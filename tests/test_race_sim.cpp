#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "parsers/libp.h"
#include "race/race_sim.h"
#include "race/race_types.h"

namespace srrace {
namespace {

TrackData MakeFlatTrack() {
    TrackData track{};
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        track.left[i] = 0;
        track.right[i] = 0;
        track.flags[i] = 0;
        track.elev[i] = 0;
        track.curve[i] = 0;
    }
    return track;
}

// Shifts at or under 5000 rpm: above that a manual rolls the ~3% money-shift, which drops the box and made
// these tests fail at random. Tests that want a dropped box power-shift explicitly.
uint16_t ThrottleWithShiftIfNeeded(const RaceSim& sim) {
    const CarState& player = sim.Player();
    const CarTables& tables = sim.PlayerTables();
    const bool canShiftUp = player.gear < tables.numGears - 1;
    // Low gears gain ~500 rpm a tick, so trigger well under 5000.
    const int shiftRpm = std::min<int>(tables.autoShiftRpm, 3600);
    uint16_t input = 0;
    if (canShiftUp && player.rpm >= shiftRpm) {
        input |= INPUT_SHIFT | INPUT_THROTTLE;
    } else if (player.rpm >= 5600) {
        // Lift near redline, or the engine blows after ~15 ticks on the limiter.
    } else {
        input |= INPUT_THROTTLE;
    }
    return input;
}

// Idle through the countdown so the caller can floor it at the green.
void RunOutCountdown(RaceSim& sim) {
    for (int i = 0; i < 400 && sim.InCountdown(); ++i) {
        sim.Tick(0);
    }
}

// A reasonable mid-range car, shared by the cop tests below.
CarSetup CopTestSetup() {
    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;
    setup.driverSkill = 130;
    return setup;
}

// A seed where the 1-in-4 spawn roll does not arm a cop, so kingRace can be toggled alone.
uint32_t SeedWithoutCop(const TrackData& track, const CarSetup& setup) {
    for (uint32_t seed = 1; seed < 500; ++seed) {
        RaceSim probe;
        probe.Init(&track, 1, setup, setup, seed);
        if (probe.CopState() == 0) {
            return seed;
        }
    }
    return 0;
}

uint32_t SeedThatArmsCop(const TrackData& track, const CarSetup& setup) {
    for (uint32_t seed = 1; seed < 500; ++seed) {
        RaceSim probe;
        probe.Init(&track, 1, setup, setup, seed);
        if (probe.CopState() == 1) {
            return seed;
        }
    }
    return 0;
}

// Arms the cop, drops the player past the trigger with the opponent well behind, and ticks once.
void StartCopPursuit(RaceSim& sim, const TrackData& track, const CarSetup& setup,
                     uint32_t seed) {
    sim.Init(&track, 1, setup, setup, seed);
    REQUIRE(sim.CopState() == 1);
    sim.SetCameraState(sim.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    sim.SetNpcDebugPose(sim.CopTriggerSegment() - 100, 2200);
    sim.Tick(0);
}

// Restored afterwards so it cannot leak into the rest of the suite.
struct PuristScope {
    bool previous;
    explicit PuristScope(bool enabled) : previous(PuristScoring()) {
        SetPuristScoring(enabled);
    }
    ~PuristScope() { SetPuristScoring(previous); }
};

// Every scoring term at its best: carb 25 + trans 15 + engine/manifold 15 +
// weight 10 + roof 9 + front bumper 5 + rear bumper 1 = 80, plus the tune term.
CarSetup PerfectCar() {
    CarSetup setup{};
    setup.carbClass = 2;
    setup.transClass = 3;
    setup.engineClass = 2;
    setup.manifoldClass = 4;
    setup.weightClass = 8;
    setup.tireClass = 2;
    setup.tuneOffset = 0;
    setup.carbWear = 0;
    setup.roofChopped = true;
    setup.frontBumperRemoved = true;
    setup.rearBumperRemoved = true;
    return setup;
}

}

TEST_CASE("ComputeBasePerformance all-stock worst setup uses integer math") {
    CarSetup setup{};
    setup.carbClass = 0;
    setup.transClass = 0;
    setup.engineClass = 0;
    setup.manifoldClass = 0;
    setup.weightClass = 0;
    setup.tuneOffset = 8;
    setup.carbWear = 0;

    // pts = 0 + 0 + 0 + ((8-8)*20/8 = 0) + (40*10/88 = 4) = 4
    // base = 70 + (4 * 24 / 100) = 70 (integer division)
    CHECK(ComputeBasePerformance(setup) == 70);

    // Purist: the tune term becomes (1-8)*20/8 = -17, so pts = -13 and
    // base = 70 + (-13 * 24 / 100) = 67.
    PuristScope purist(true);
    CHECK(ComputeBasePerformance(setup) == 67);
}

TEST_CASE("ComputeBasePerformance perfect car lands on the 100-point ceiling") {
    // 80 points of parts and body plus the intended 20 for a perfect tune.
    CHECK(ComputeBasePerformance(PerfectCar()) == 70 + (100 * 30) / 100);  // 100

    // Purist: the tune term gives 2 instead of 20, so pts = 82.
    PuristScope purist(true);
    CHECK(ComputeBasePerformance(PerfectCar()) == 70 + (82 * 30) / 100);  // 94
}

// The 68k's term-by-term arithmetic, with the fixed tune term.
TEST_CASE("ComputeBasePerformance matches the formula term by term") {
    CarSetup setup{};
    setup.carbClass = 2;
    setup.transClass = 3;
    setup.engineClass = 1;
    setup.manifoldClass = 3;
    setup.weightClass = 7;
    setup.tuneOffset = 3;
    setup.carbWear = 500;
    setup.roofChopped = true;
    setup.rearBumperRemoved = true;

    static const int kCarbPts[3] = {0, 50, 100};
    static const int kTransPts[4] = {0, 33, 66, 100};
    static const int kEngByManifold[20] = {
        0, 0, 100, 0, 0,
        0, 50, 0, 100, 75,
        0, 0, 0, 50, 100,
        0, 50, 100, 0, 33
    };
    static const int kWeightPts[9] = {40, 44, 48, 60, 64, 68, 80, 84, 88};

    int pts = 0;
    pts += (kCarbPts[setup.carbClass] * 25) / 100;
    pts += (kTransPts[setup.transClass] * 15) / 100;
    pts += (kEngByManifold[setup.engineClass * 5 + setup.manifoldClass] * 15) / 100;
    pts += ((8 - setup.tuneOffset) * 20) / 8;
    pts += (kWeightPts[setup.weightClass] * 10) / 88;
    pts += 9;  // roof chopped
    pts += 1;  // rear bumper off
    const int expected = 70 + (((pts * (10000 - setup.carbWear)) / 10000) * 30) / 100;

    CHECK(ComputeBasePerformance(setup) == expected);
}

TEST_CASE("ComputeTopSpeed is base * 13/10 capped at 130") {
    CarSetup setup{};
    setup.carbClass = 0;
    setup.transClass = 1;
    setup.engineClass = 0;
    setup.manifoldClass = 0;
    setup.weightClass = 0;
    setup.tuneOffset = 0;

    const int base = ComputeBasePerformance(setup);
    CHECK(ComputeTopSpeed(setup) == (base * 13) / 10);
}

// Reference reading from the real game: a stock, tuned 1940 Chevrolet 2-dr coupe (spec 0) shows 92 mph.
TEST_CASE("Stock 1940 Chevrolet 2-dr coupe reads 92 mph in purist mode") {
    CarSetup chevy{};
    chevy.carbClass = 0;      // 2-brl
    chevy.transClass = 0;     // GM automatic
    chevy.engineClass = 1;    // V6
    chevy.manifoldClass = 0;  // stock
    chevy.tireClass = 0;
    chevy.weightClass = 0;    // spec class byte
    chevy.tuneOffset = 0;     // "with right tuning"

    PuristScope purist(true);
    CHECK(ComputeTopSpeed(chevy) == 92);
}

// Reference reading: the tuned 1963 Corvette (spec 5) shows 119 mph. roofChopped is set on purpose: a
// Corvette cannot be chopped, and the original hands out the 9 points; without them it scores 110.
CarSetup TunedCorvette() {
    CarSetup vette{};
    vette.carbClass = 2;      // 4-brl racing
    vette.transClass = 3;     // racing 4-speed
    vette.engineClass = 2;    // GM V-8 327
    vette.manifoldClass = 4;  // GM racing manifold
    vette.tireClass = 2;      // racing slicks
    vette.weightClass = 8;    // spec class byte
    vette.tuneOffset = 0;
    vette.roofChopped = true;  // cannot be chopped -> scored as though it were
    return vette;
}

TEST_CASE("Tuned 1963 Corvette reads 119 mph in purist mode") {
    PuristScope purist(true);
    CHECK(ComputeTopSpeed(TunedCorvette()) == 119);

    // Three notches off gives the 117 the car reads before tuning.
    CarSetup untuned = TunedCorvette();
    untuned.tuneOffset = 3;
    CHECK(ComputeTopSpeed(untuned) == 117);

    // The King drives this car; pulling both bumpers gets a player to 122.
    CarSetup built = TunedCorvette();
    built.frontBumperRemoved = true;
    built.rearBumperRemoved = true;
    CHECK(ComputeTopSpeed(built) == 122);
}

// The fixed tune term, kept as a pin so the two modes cannot silently converge.
TEST_CASE("Stock 1940 Chevrolet 2-dr coupe reads 97 mph with the fixed tune term") {
    CarSetup chevy{};
    chevy.carbClass = 0;
    chevy.transClass = 0;
    chevy.engineClass = 1;
    chevy.manifoldClass = 0;
    chevy.tireClass = 0;
    chevy.weightClass = 0;
    chevy.tuneOffset = 0;

    PuristScope purist(false);
    CHECK(ComputeTopSpeed(chevy) == 97);
}

// A Corvette gets the roof points because it cannot be chopped, but no bumper points, since it can lose them.
TEST_CASE("Corvettes cannot be chopped but can lose their bumpers") {
    for (AmigaLibAsset body : {CHEVROLET_CORVETTE_1956_BODY,
                               CHEVROLET_CORVETTE_1961_BODY,
                               CHEVROLET_CORVETTE_1963_BODY}) {
        CHECK_FALSE(CarQuery::SrLibQueryCanChopRoof(body));
        CHECK(CarQuery::SrLibQueryCanStripBumpers(body));
    }
    CHECK(CarQuery::SrLibQueryCanChopRoof(CHEVROLET_2DR_COUPE_1940_BODY));
    CHECK(CarQuery::SrLibQueryCanStripBumpers(CHEVROLET_2DR_COUPE_1940_BODY));

    // Actually pulling both bumpers is worth 6 points, so 122 not 119.
    CarSetup stripped = TunedCorvette();
    stripped.frontBumperRemoved = true;
    stripped.rearBumperRemoved = true;
    PuristScope purist(true);
    CHECK(ComputeTopSpeed(stripped) == 122);
}

TEST_CASE("A perfect car reaches exactly the 130 mph cap") {
    CHECK(ComputeTopSpeed(PerfectCar()) == 130);

    // Purist: the tune bug leaves the cap unreachable and the best car at 122.
    PuristScope purist(true);
    CHECK(ComputeTopSpeed(PerfectCar()) == 122);
}

TEST_CASE("No legal set of parts exceeds the cap in either mode") {
    for (int mode = 0; mode < 2; ++mode) {
        PuristScope purist(mode == 1);
        int highest = 0;
        for (int carb = 0; carb <= 2; ++carb) {
            for (int trans = 0; trans <= 3; ++trans) {
                for (int eng = 0; eng <= 3; ++eng) {
                    for (int mani = 0; mani <= 4; ++mani) {
                        for (int weight = 0; weight <= 8; ++weight) {
                            CarSetup s{};
                            s.carbClass = carb;
                            s.transClass = trans;
                            s.engineClass = eng;
                            s.manifoldClass = mani;
                            s.weightClass = weight;
                            s.roofChopped = true;
                            s.frontBumperRemoved = true;
                            s.rearBumperRemoved = true;
                            highest = std::max(highest, ComputeTopSpeed(s));
                        }
                    }
                }
            }
        }
        CHECK(highest == (mode == 1 ? 122 : 130));
    }
}

TEST_CASE("BuildCarTables gears the car to exactly its top speed") {
    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 3;
    setup.weightClass = 4;

    CarTables tables{};
    BuildCarTables(setup, tables);

    const int topSpeed = ComputeTopSpeed(setup);
    CHECK(tables.topSpeed == topSpeed);
    // rpm = ratio * mph / 100, so redline in top gear is the top speed.
    CHECK((5700 * 100) / tables.ratio[tables.numGears] == topSpeed);
}

TEST_CASE("BuildCarTables sets 4spd ratios and thresholds") {
    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;

    CarTables tables{};
    BuildCarTables(setup, tables);

    CHECK(tables.numGears == 4);
    CHECK(tables.autoShiftRpm == 5000);
    CHECK(tables.rows[1][0].rpmThreshold == 2900);
    CHECK(tables.rows[1][1].rpmThreshold == 5000);
    CHECK(tables.rows[1][2].rpmThreshold == 5700);

    CHECK(tables.ratio[1] > tables.ratio[2]);
    CHECK(tables.ratio[2] > tables.ratio[3]);
    CHECK(tables.ratio[3] > tables.ratio[4]);
}

TEST_CASE("SpeedToScrollStep boundary mapping") {
    CHECK(SpeedToScrollStep(0) == 0);
    CHECK(SpeedToScrollStep(1) == 1);
    CHECK(SpeedToScrollStep(13) == 1);
    CHECK(SpeedToScrollStep(14) == 2);
    CHECK(SpeedToScrollStep(97) == 7);
    CHECK(SpeedToScrollStep(98) == 8);
    CHECK(SpeedToScrollStep(129) == 15);
    CHECK(SpeedToScrollStep(130) == 16);
    CHECK(SpeedToScrollStep(200) == 16);
}

TEST_CASE("Drag race smoke test reaches finish with sane ranges") {
    TrackData track = MakeFlatTrack();

    CarSetup playerSetup{};
    playerSetup.transClass = 2;
    playerSetup.carbClass = 1;
    playerSetup.engineClass = 1;
    playerSetup.manifoldClass = 1;
    playerSetup.tireClass = 1;
    playerSetup.weightClass = 4;
    playerSetup.driverSkill = 130;

    CarSetup npcSetup = playerSetup;
    npcSetup.transClass = 0;

    RaceSim sim;
    sim.Init(&track, 0, playerSetup, npcSetup, 0x12345678u);
    RunOutCountdown(sim);

    bool finished = false;
    for (int i = 0; i < 6000; ++i) {
        // Hold the opponent in the far-left lane, or the pair overlap and the race ends on a rear-end crash.
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));

        const CarState& now = sim.Player();
        CHECK(now.speed >= 0);
        CHECK(now.speed <= 130);
        CHECK(now.rpm >= 1400);
        CHECK(now.rpm <= 5700);

        if (sim.RaceOver()) {
            finished = true;
            break;
        }
    }

    CHECK(finished);
    CHECK((sim.Player().endFlags & END_FAILURE_MASK) == 0);
    CHECK(sim.Player().segPos > DRAG_FINISH_SEG);
    CHECK(sim.Player().speed == 0);
    // Who wins is not asserted: the finish is close either way. This checks the sim stays sane and reaches the flag.
    CHECK(std::abs(static_cast<int>(sim.Player().segPos) -
                   static_cast<int>(sim.Npc().segPos)) < 40);
}

// The curve branch caps the opponent once, at perfScore - aggression, re-armed on the next straight.
TEST_CASE("The opponent brakes for a corner, once, by its driver's aggression") {
    TrackData track = MakeFlatTrack();
    // A one-segment kink far enough down the course for the opponent to exceed the cap.
    track.curve[801] = 1;

    CarSetup setup = CopTestSetup();
    setup.aggression = 12;   // 0x0c, the worst driver: the biggest penalty
    // A slower player, so the opponent is clear ahead and takes the full penalty.
    CarSetup slowPlayer;

    RaceSim sim;
    sim.Init(&track, 1, slowPlayer, setup, SeedWithoutCop(track, setup));
    RunOutCountdown(sim);

    const int perf = sim.NpcTables().perfScore;
    int caps = 0;
    int cappedTo = -1;
    for (int i = 0; i < 8000 && sim.Npc().segPos < 805; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);   // keep the pair apart
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if (sim.NpcAi().branch == NpcBranch::CorneringCap) {
            ++caps;
            cappedTo = sim.Npc().speed;
        }
    }

    REQUIRE(caps > 0);
    CHECK(caps == 1);   // the latch: one brake for the corner, not one per tick
    // Full penalty while it is ahead of the player, half while it is not.
    CHECK((cappedTo == perf - 12 || cappedTo == perf - 6));
}

// The opponent upshifts at band end minus 100 per point of reaction, so none uses the top of its band.
TEST_CASE("The opponent upshifts short of the band end, by its driver's reaction") {
    TrackData track = MakeFlatTrack();
    CarSetup setup = CopTestSetup();   // carb class 1 -> band end 5000

    auto peakRpmBeforeShift = [&](int reaction) {
        CarSetup npc = setup;
        npc.reaction = reaction;
        RaceSim sim;
        sim.Init(&track, 1, setup, npc, SeedWithoutCop(track, npc));
        RunOutCountdown(sim);
        int peak = 0;
        const int startGear = sim.Npc().gear;
        for (int i = 0; i < 400 && sim.Npc().gear <= startGear + 1; ++i) {
            sim.Tick(ThrottleWithShiftIfNeeded(sim));
            if (sim.Npc().gear <= startGear + 1) peak = std::max(peak, int(sim.Npc().rpm));
        }
        return peak;
    };

    const int best = peakRpmBeforeShift(0x0a);   // The King
    const int worst = peakRpmBeforeShift(0x14);  // the slowest driver in SR_data

    // 5000 - 1000 and 5000 - 2000, reached on the tick the shift fires.
    CHECK(best >= 4000);
    CHECK(best < 5000);
    CHECK(worst < best);
    CHECK(worst >= 3000);
}

TEST_CASE("Steering input counteracts curve drift") {
    TrackData track = MakeFlatTrack();
    // The drift comes from curve[], not elev[].
    for (int i = 300; i <= 340; ++i) {
        track.curve[i] = 4;
    }

    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;
    setup.driverSkill = 130;

    CarSetup npcSetup = setup;
    npcSetup.transClass = 0;

    RaceSim noSteer;
    noSteer.Init(&track, 1, setup, npcSetup, 0x1122u);
    RunOutCountdown(noSteer);
    // Park the opponent far up the road, or holding the throttle rear-ends it before segment 305.
    noSteer.SetNpcDebugPose(900, 2200);
    for (int i = 0; i < 4000 && noSteer.Player().segPos < 305; ++i) {
        noSteer.Tick(ThrottleWithShiftIfNeeded(noSteer));
    }
    REQUIRE(noSteer.Player().segPos >= 305);
    const int lateralStartNoSteer = noSteer.Player().lateral;
    int monotonicDir = 0;
    int prevLateral = noSteer.Player().lateral;
    int noSteerTicksInCurve = 0;
    while (noSteer.Player().segPos <= 340 && noSteerTicksInCurve < 300) {
        noSteer.Tick(ThrottleWithShiftIfNeeded(noSteer));
        const int delta = noSteer.Player().lateral - prevLateral;
        prevLateral = noSteer.Player().lateral;
        noSteerTicksInCurve++;
        if (delta == 0) {
            continue;
        }
        const int dir = (delta > 0) ? 1 : -1;
        if (monotonicDir == 0) {
            monotonicDir = dir;
        } else {
            CHECK(dir == monotonicDir);
        }
    }
    REQUIRE(noSteerTicksInCurve > 0);
    const int driftNoSteer = noSteer.Player().lateral - lateralStartNoSteer;
    CHECK(driftNoSteer != 0);
    CHECK(monotonicDir != 0);

    RaceSim withSteerAdaptive;
    withSteerAdaptive.Init(&track, 1, setup, npcSetup, 0x3344u);
    RunOutCountdown(withSteerAdaptive);
    withSteerAdaptive.SetNpcDebugPose(900, 2200);   // as above: keep the opponent out of it
    for (int i = 0; i < 4000 && withSteerAdaptive.Player().segPos < 305; ++i) {
        withSteerAdaptive.Tick(ThrottleWithShiftIfNeeded(withSteerAdaptive));
    }
    REQUIRE(withSteerAdaptive.Player().segPos >= 305);
    const int lateralStartWithSteer = withSteerAdaptive.Player().lateral;
    int adaptiveTicksInCurve = 0;
    int prevAdaptiveLateral = withSteerAdaptive.Player().lateral;
    while (withSteerAdaptive.Player().segPos <= 340 && adaptiveTicksInCurve < 300) {
        uint16_t input = ThrottleWithShiftIfNeeded(withSteerAdaptive);
        const int currentLateral = withSteerAdaptive.Player().lateral;
        if (currentLateral > prevAdaptiveLateral) {
            input |= INPUT_STEER_LEFT;
        } else if (currentLateral < prevAdaptiveLateral) {
            input |= INPUT_STEER_RIGHT;
        }
        withSteerAdaptive.Tick(input);
        prevAdaptiveLateral = currentLateral;
        adaptiveTicksInCurve++;
    }
    REQUIRE(adaptiveTicksInCurve > 0);
    const int driftWithSteer = withSteerAdaptive.Player().lateral - lateralStartWithSteer;

    CHECK(std::abs(driftWithSteer) <= std::abs(driftNoSteer));
}

TEST_CASE("Cop spawns only on plain road races") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    int roadSpawns = 0;
    for (uint32_t seed = 1; seed < 80; ++seed) {
        RaceSim drag;
        drag.Init(&track, 0, setup, setup, seed);
        CHECK(drag.CopState() == 0);          // never on the drag strip

        RaceSim king;
        king.Init(&track, 1, setup, setup, seed, /*kingRace=*/true);
        CHECK(king.CopState() == 0);          // never in the King's race

        RaceSim road;
        road.Init(&track, 1, setup, setup, seed);
        if (road.CopState() == 1) {
            ++roadSpawns;
            // 510 + rand(255), in the back half of a course that runs 200..1020.
            CHECK(road.CopTriggerSegment() >= 510);
            CHECK(road.CopTriggerSegment() < 765);
        }
    }
    // A 1-in-4 roll: over 79 road races it must fire sometimes, never always.
    CHECK(roadSpawns > 0);
    CHECK(roadSpawns < 79);
}

TEST_CASE("Cop is cancelled unless the player is 5+ segments ahead") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    // Level with the opponent at the trigger: no cop. You only get pulled over for leading.
    RaceSim level;
    level.Init(&track, 1, setup, setup, seed);
    REQUIRE(level.CopState() == 1);
    level.SetCameraState(level.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    level.SetNpcDebugPose(level.CopTriggerSegment() + 1, 2200);
    level.Tick(0);
    CHECK(level.CopState() == 0);
    CHECK_FALSE(level.CopChasing());

    // Well clear of the opponent: the cop takes over the opponent slot.
    RaceSim leading;
    StartCopPursuit(leading, track, setup, seed);
    CHECK(leading.CopState() == 2);
    CHECK(leading.CopChasing());
    CHECK(leading.ConsumeCopSpawn());        // one-shot for the sprite swap
    CHECK_FALSE(leading.ConsumeCopSpawn());  // and only once
    CHECK_FALSE(leading.CopSirenOn());       // not close enough yet
}

TEST_CASE("Cop siren starts when it closes, and catching you ends the race") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Closed to within 15 segments behind -> siren.
    sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
    sim.Tick(0);
    CHECK(sim.CopState() == 3);
    CHECK(sim.CopSirenOn());
    CHECK_FALSE(sim.PlayerBusted());   // alongside is not caught

    // More than 2 segments ahead of us -> pulled over.
    sim.SetNpcDebugPose(sim.Player().segPos + 4, 2200);
    sim.Tick(0);
    CHECK(sim.PlayerBusted());
    CHECK(sim.PlayerFailed());
    CHECK_FALSE(sim.PlayerWrecked());  // a ticket does not bend metal
    CHECK_FALSE(sim.CopSirenOn());     // siren stops on capture
    CHECK(sim.RaceOver());
    // Caught crawling, so the cheapest tier.
    CHECK(sim.CopCaughtSpeed() < 51);
    CHECK(sim.SpeedingFine() == 20);
}

TEST_CASE("Speeding fine is tiered on the speed at capture") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Build speed with the cop held back, then force the capture and check the fine tier.
    for (int i = 0; i < 400 && sim.Player().speed < 60; ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 40, 2200);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(sim.Player().speed >= 51);
    REQUIRE_FALSE(sim.PlayerBusted());

    // The capture only arms once the siren is going.
    sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
    sim.Tick(ThrottleWithShiftIfNeeded(sim));
    REQUIRE(sim.CopState() == 3);

    sim.SetNpcDebugPose(sim.Player().segPos + 4, 2200);
    sim.Tick(ThrottleWithShiftIfNeeded(sim));
    REQUIRE(sim.PlayerBusted());

    // The fine uses the speed recorded at capture.
    const int16_t caught = sim.CopCaughtSpeed();
    CHECK(caught >= 51);
    CHECK(sim.SpeedingFine() == (caught < 81 ? 50 : 75));
}

TEST_CASE("Winning the race shakes the cop - no ticket") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // The cop pinned close behind all the way to the flag, never ahead.
    bool sirenSounded = false;
    for (int i = 0; i < 4000 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        sirenSounded = sirenSounded || sim.CopSirenOn();
    }

    REQUIRE(sim.RaceOver());
    CHECK(sirenSounded);              // it really was chasing us
    CHECK(sim.PlayerWonAtLine());
    // A win is recorded in a different word than the failure bits, so crossing first gets you off.
    CHECK_FALSE(sim.PlayerBusted());
    CHECK(sim.SpeedingFine() == 0);
    CHECK_FALSE(sim.CopSirenOn());
}

TEST_CASE("Crashing while chased gets you a ticket on top") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Put the car way off the road so the edge check wrecks it.
    sim.SetCameraState(sim.Player().segPos, 0, 0, 100, 0);
    sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
    sim.Tick(0);

    REQUIRE(sim.PlayerWrecked());
    CHECK(sim.PlayerBusted());       // ticket added on top of the wreck
    CHECK(sim.SpeedingFine() > 0);
    CHECK_FALSE(sim.CopSirenOn());
}

// END_TIRES_BLOWN is not an in-race outcome bit; reading it as the race ending handed out a ticket.
TEST_CASE("Blowing the tires while a cop is out is not a bust") {
    TrackData track = MakeFlatTrack();
    CarSetup worn = CopTestSetup();
    worn.tireWear = 10000;

    uint32_t seed = 0;
    for (uint32_t s = 1; s <= 2000 && seed == 0; ++s) {
        RaceSim probe;
        probe.Init(&track, 1, worn, worn, s);
        if (probe.CopState() == 1 && probe.TireBlowSegment() != 0 &&
            probe.TireBlowSegment() > probe.CopTriggerSegment()) {
            seed = s;
        }
    }
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, worn, seed);
    REQUIRE(sim.CopState() == 2);

    // Jump the car past the blowout segment, leaving the cop where it spawned.
    sim.SetCameraState(sim.TireBlowSegment() + 1, 0, 0, 2400, 0);
    sim.Tick(0);

    REQUIRE(sim.PlayerTiresBlown());
    CHECK_FALSE(sim.PlayerBusted());
    CHECK_FALSE(sim.RaceOver());
}

TEST_CASE("Cop cannot be rear-ended or shove you off the road") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Level and overlapping while closing: the geometry that sets END_HIT_NPC without the cop gate.
    for (int i = 0; i < 30 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos, sim.Player().lateral + 20);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        CHECK((sim.Player().endFlags & END_HIT_NPC) == 0);
    }
}

TEST_CASE("Init fully resets a sim that has already been raced") {
    // A RaceSim is reused for every race, so re-Init must be a complete reset.
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim fresh;
    fresh.Init(&track, 1, setup, setup, seed);

    // Two dirty states, since capture switches the siren off: test both resets.
    SUBCASE("left mid-pursuit with the siren going") {
        RaceSim used;
        StartCopPursuit(used, track, setup, seed);
        used.SetNpcDebugPose(used.Player().segPos - 10, 2200);
        used.Tick(0);
        REQUIRE(used.CopState() == 3);
        REQUIRE(used.CopSirenOn());          // genuinely dirty

        used.Init(&track, 1, setup, setup, seed);
        CHECK_FALSE(used.CopSirenOn());
        CHECK_FALSE(used.CopChasing());
        CHECK(used.CopState()          == fresh.CopState());
        CHECK(used.CopTriggerSegment() == fresh.CopTriggerSegment());
        CHECK(used.InCountdown());
        CHECK_FALSE(used.ConsumeCopSpawn());
    }

    SUBCASE("left busted at speed") {
        RaceSim used;
        StartCopPursuit(used, track, setup, seed);
        for (int i = 0; i < 400 && used.Player().speed < 60; ++i) {
            used.SetNpcDebugPose(used.Player().segPos - 40, 2200);
            used.Tick(ThrottleWithShiftIfNeeded(used));
            if (used.RaceOver()) {
                break;
            }
        }
        REQUIRE(used.Player().speed >= 51);
        used.SetNpcDebugPose(used.Player().segPos - 10, 2200);
        used.Tick(ThrottleWithShiftIfNeeded(used));
        used.SetNpcDebugPose(used.Player().segPos + 4, 2200);
        used.Tick(ThrottleWithShiftIfNeeded(used));
        REQUIRE(used.PlayerBusted());
        REQUIRE(used.CopCaughtSpeed() >= 51);   // genuinely dirty
        REQUIRE(used.SpeedingFine() > 0);

        used.Init(&track, 1, setup, setup, seed);
        CHECK(used.CopCaughtSpeed() == 0);
        CHECK(used.SpeedingFine() == 0);
        CHECK_FALSE(used.PlayerBusted());
        CHECK_FALSE(used.RaceOver());
        CHECK(used.Player().endFlags == 0);
        CHECK(used.Npc().endFlags == 0);
        CHECK(used.Player().segPos     == fresh.Player().segPos);
        CHECK(used.Player().speed      == fresh.Player().speed);
        CHECK(used.Player().lateral    == fresh.Player().lateral);
        CHECK(used.Npc().segPos        == fresh.Npc().segPos);
        CHECK(used.Npc().lateral       == fresh.Npc().lateral);
        CHECK(used.CountdownTicks()    == fresh.CountdownTicks());
        CHECK(used.PlayerWonAtLine()   == fresh.PlayerWonAtLine());
        CHECK(used.InCountdown());
    }
}

TEST_CASE("The King does not ease off when it gets clear in a road race") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    // A seed with no cop, since kingRace also suppresses the spawn roll.
    const uint32_t seed = SeedWithoutCop(track, setup);
    REQUIRE(seed != 0);

    struct Result {
        int finalSpeed;
        int normal;
        int risky;
    };
    auto runClearAhead = [&](bool king) {
        RaceSim sim;
        sim.Init(&track, 1, setup, setup, seed, king);
        REQUIRE(sim.CopState() == 0);
        RunOutCountdown(sim);
        for (int i = 0; i < 400 && !sim.RaceOver(); ++i) {
            // Pinned 30 segments ahead, past the 13-segment threshold where an ordinary opponent drops to cruise.
            sim.SetNpcDebugPose(sim.Player().segPos + 30, 2200);
            sim.Tick(0);
        }
        return Result{sim.Npc().speed, sim.NpcAi().targetSpeedNormal,
                      sim.NpcAi().targetSpeedRisky};
    };

    const Result ordinary = runClearAhead(false);
    const Result king = runClearAhead(true);

    // The ordinary opponent hovers just above its cruising target.
    CHECK(ordinary.finalSpeed <= ordinary.normal + 2);
    // The King never enters that branch and runs to his car's own limit.
    CHECK(king.finalSpeed > king.risky + 20);
}

TEST_CASE("In a drag race the King eases to the higher of the two targets") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    // In a drag race the King does reach the far-ahead branch, taking the risky target.
    auto finalSpeedClearAhead = [&](bool king) {
        RaceSim sim;
        sim.Init(&track, 0, setup, setup, 0x77u, king);
        RunOutCountdown(sim);
        for (int i = 0; i < 300 && !sim.RaceOver(); ++i) {
            sim.SetNpcDebugPose(sim.Player().segPos + 30, 2200);
            sim.Tick(0);
        }
        return std::pair<int, int>{sim.Npc().speed, sim.NpcAi().targetSpeedNormal};
    };

    const auto ordinary = finalSpeedClearAhead(false);
    const auto king = finalSpeedClearAhead(true);
    CHECK(ordinary.first <= ordinary.second + 2);   // settled on normal
    CHECK(king.first > ordinary.first);             // settled on risky instead
}

TEST_CASE("A spawned cop pursues and runs down a fleeing player") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, seed);
    sim.SetCameraState(sim.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    // The cop takes over wherever the opponent is, usually a long way back.
    sim.SetNpcDebugPose(sim.CopTriggerSegment() - 115, 2200);
    sim.Tick(0);
    REQUIRE(sim.CopState() == 2);

    const int gapAtSpawn =
        static_cast<int>(sim.Player().segPos) - static_cast<int>(sim.Npc().segPos);
    REQUIRE(gapAtSpawn > 100);

    // The cop must not inherit the challenger's performance or concede state.
    CHECK_FALSE(sim.NpcAi().gaveUp);
    CHECK(sim.NpcTables().perfScore > 70);

    for (int i = 0; i < 900 && !sim.RaceOver() && !sim.PlayerBusted(); ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
    }

    const int gapEnd =
        static_cast<int>(sim.Player().segPos) - static_cast<int>(sim.Npc().segPos);
    CHECK(gapEnd < gapAtSpawn);          // it closed
    CHECK_FALSE(sim.NpcAi().gaveUp);     // and never conceded on the way
    // Fleeing flat out is not enough: it runs to player + 32.
    CHECK(sim.Npc().speed > sim.Player().speed);
}

TEST_CASE("Invulnerable suppresses crashes and blown engines, nothing else") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    SUBCASE("a wall hit that would normally end the race is dropped whole") {
        RaceSim normal;
        normal.Init(&track, 1, setup, setup, 0x2468u);
        normal.SetCameraState(300, 0, 0, 100, 0);   // far off the left edge
        normal.SetNpcDebugPose(900, 2200);
        normal.Tick(0);
        // A wall hit ends the race through the failure mask, not END_FINISHED.
        REQUIRE(normal.PlayerWrecked());
        REQUIRE(normal.PlayerFailed());

        RaceSim tough;
        tough.Init(&track, 1, setup, setup, 0x2468u);
        tough.SetInvulnerable(true);
        tough.SetCameraState(300, 0, 0, 100, 0);
        tough.SetNpcDebugPose(900, 2200);
        tough.Tick(0);
        CHECK_FALSE(tough.PlayerWrecked());
        CHECK_FALSE(tough.PlayerFailed());
        // An event carrying a suppressed bit is dropped whole.
        CHECK(tough.Player().endFlags == 0);
    }

    SUBCASE("sitting on the limiter no longer grenades the engine") {
        RaceSim tough;
        tough.Init(&track, 1, setup, setup, 0xbeefu);
        tough.SetInvulnerable(true);
        RunOutCountdown(tough);
        for (int i = 0; i < 600 && !tough.RaceOver(); ++i) {
            tough.SetNpcDebugPose(900, 2200);
            tough.Tick(INPUT_THROTTLE);   // never shifting: pinned at the redline
        }
        CHECK((tough.Player().endFlags & END_ENGINE_BLOWN) == 0);
    }

    SUBCASE("the cop and a false start still end the race") {
        const uint32_t seed = SeedThatArmsCop(track, setup);
        REQUIRE(seed != 0);

        RaceSim busted;
        StartCopPursuit(busted, track, setup, seed);
        busted.SetInvulnerable(true);
        busted.SetNpcDebugPose(busted.Player().segPos - 10, 2200);
        busted.Tick(0);
        busted.SetNpcDebugPose(busted.Player().segPos + 4, 2200);
        busted.Tick(0);
        CHECK(busted.PlayerBusted());
        CHECK(busted.RaceOver());

        RaceSim jumped;
        jumped.Init(&track, 1, setup, setup, 0xabcu);
        jumped.SetInvulnerable(true);
        REQUIRE(jumped.InCountdown());
        for (int i = 0; i < 20 && jumped.InCountdown(); ++i) {
            jumped.Tick(INPUT_THROTTLE | INPUT_SHIFT);
        }
        CHECK((jumped.Player().endFlags & END_STALLED) != 0);
    }
}

TEST_CASE("Tire screech gate: hard steering at speed, not at a standstill") {
    TrackData track = MakeFlatTrack();

    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;
    setup.driverSkill = 130;

    CarSetup npcSetup = setup;
    npcSetup.transClass = 0;

    RaceSim sim;
    sim.Init(&track, 1, setup, npcSetup, 0x5566u);
    RunOutCountdown(sim);
    sim.SetNpcDebugPose(900, 2200);  // keep the opponent out of the way

    // PlayerSkidding() needs speed > 20 mph.
    for (int i = 0; i < 10; ++i) {
        sim.Tick(INPUT_STEER_LEFT);
        CHECK_FALSE(sim.PlayerSkidding());
    }

    // slideVel ramps 2/tick toward +-24, crossing 12 within a few ticks.
    for (int i = 0; i < 200 && sim.Player().speed <= 25; ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
    }
    REQUIRE(sim.Player().speed > 20);

    bool skidded = false;
    for (int i = 0; i < 20; ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim) | INPUT_STEER_RIGHT);
        if (sim.PlayerSkidding()) {
            skidded = true;
            break;
        }
    }
    CHECK(skidded);
    CHECK(std::abs(sim.Player().slideVel) >= 12);
}

TEST_CASE("Rolling before the green is a false start") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0xabcu);
    REQUIRE(sim.InCountdown());
    // Slam it in gear and floor it before the light.
    for (int i = 0; i < 20 && sim.InCountdown(); ++i) {
        sim.Tick(INPUT_THROTTLE | INPUT_SHIFT);
    }
    CHECK((sim.Player().endFlags & END_STALLED) != 0);
}

TEST_CASE("An automatic can rev on the line without jumping the start") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0xabcu);
    REQUIRE(sim.PlayerTables().automatic);
    REQUIRE(sim.InCountdown());

    // Throttle on the line in N: the engine revs, nothing moves.
    for (int i = 0; i < 20 && sim.InCountdown(); ++i) {
        sim.Tick(INPUT_THROTTLE);
    }
    CHECK(sim.Player().gear == 0);
    CHECK(sim.Player().speed == 0);
    CHECK((sim.Player().endFlags & END_STALLED) == 0);
    CHECK(sim.Player().rpm > 1500);   // it did actually rev
}

TEST_CASE("An automatic engages drive on the shift input and pulls away") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0xabcu);
    RunOutCountdown(sim);

    sim.Tick(INPUT_THROTTLE | INPUT_SHIFT);   // N -> D
    CHECK(sim.Player().gear > 0);
    for (int i = 0; i < 60; ++i) {
        sim.Tick(INPUT_THROTTLE);             // no further shift input needed
    }
    CHECK(sim.Player().speed > 0);
    CHECK(sim.Player().gear > 1);             // and it upshifts on its own
}

TEST_CASE("Rolling in neutral pins the rpm to road speed, standing still it revs") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x2468u);
    RunOutCountdown(sim);
    sim.SetNpcDebugPose(900, 2200);

    // Build some speed in first, then drop it back into neutral.
    for (int i = 0; i < 12; ++i) {
        sim.Tick(INPUT_THROTTLE | (i == 0 ? INPUT_SHIFT : 0));
    }
    REQUIRE(sim.Player().gear > 0);
    REQUIRE(sim.Player().speed > 0);
    while (sim.Player().gear > 0) {
        sim.Tick(INPUT_SHIFT);           // no throttle: shift down
    }
    REQUIRE(sim.Player().speed > 0);

    // Coasting out of gear the needle tracks road speed.
    for (int i = 0; i < 20 && sim.Player().speed > 0; ++i) {
        sim.Tick(INPUT_THROTTLE);
        const CarState& p = sim.Player();
        CHECK(p.rpm == p.speed * 30 + 1500);
    }
    CHECK(sim.Player().rpm < 5000);

    // Stationary in neutral it must still rev: the start-line blip.
    RaceSim standing;
    standing.Init(&track, 1, setup, setup, 0x2468u);
    RunOutCountdown(standing);
    REQUIRE(standing.Player().gear == 0);
    REQUIRE(standing.Player().speed == 0);
    const int16_t rpmBefore = standing.Player().rpm;
    for (int i = 0; i < 4; ++i) {
        standing.Tick(INPUT_THROTTLE);
    }
    CHECK(standing.Player().speed == 0);
    CHECK(standing.Player().rpm > rpmBefore);
}

TEST_CASE("An automatic cannot be blown by holding the throttle") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0x77u);
    RunOutCountdown(sim);
    sim.Tick(INPUT_THROTTLE | INPUT_SHIFT);   // engage drive

    // Far longer than the ~15 ticks that grenade a manual; the governor holds it under redline.
    for (int i = 0; i < 400; ++i) {
        sim.Tick(INPUT_THROTTLE);
    }
    CHECK((sim.Player().endFlags & END_ENGINE_BLOWN) == 0);
    CHECK(sim.Player().rpm <= 5690);
}

TEST_CASE("A manual held on the limiter still grenades") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0x77u);
    RunOutCountdown(sim);
    // Never shift: first gear pins against the limiter and lets go.
    bool blown = false;
    for (int i = 0; i < 400; ++i) {
        sim.Tick(INPUT_THROTTLE);
        if ((sim.Player().endFlags & END_ENGINE_BLOWN) != 0) {
            blown = true;
            break;
        }
    }
    CHECK(blown);
}

TEST_CASE("A maxed-wear engine grenades during the race") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.engineWear = 9899;  // 0x26ab; one tick of extra wear pushes it over

    RaceSim sim;
    sim.Init(&track, 0, setup, setup, 0x5u);
    RunOutCountdown(sim);
    bool blown = false;
    for (int i = 0; i < 300; ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if ((sim.Player().endFlags & END_ENGINE_BLOWN) != 0) {
            blown = true;
            break;
        }
    }
    CHECK(blown);
}

TEST_CASE("A worn manual transmission drops on a hard shift") {
    TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed
    setup.transWear = 9900;   // > 0x26ab: drops on the first shift

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x9u);
    RunOutCountdown(sim);
    bool dropped = false;
    for (int i = 0; i < 300; ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if ((sim.Player().endFlags & END_TRANS_DROPPED) != 0) {
            dropped = true;
            break;
        }
    }
    CHECK(dropped);
}

namespace {

CarSetup FastSetup() {
    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;
    setup.driverSkill = 130;
    return setup;
}

}

// Speed recomputed from rpm every tick made a braking car spring back to the idle speed. In the original
// the two are alternatives.
TEST_CASE("Braking brings a car to a complete stop") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x1234u);
    RunOutCountdown(sim);
    for (int i = 0; i < 200; ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
    }
    REQUIRE(sim.Player().speed > 50);

    for (int i = 0; i < 400 && sim.Player().speed > 0; ++i) {
        sim.Tick(INPUT_BRAKE);
    }
    CHECK(sim.Player().speed == 0);
}

TEST_CASE("Crossing the line starts a roll-out, it does not end the race") {
    TrackData track = MakeFlatTrack();
    CarSetup player = FastSetup();
    CarSetup npc = FastSetup();
    npc.transClass = 0;
    npc.carbClass = 0;
    npc.engineClass = 0;
    npc.weightClass = 7;

    // kingRace suppresses the cop, which would end the race before the roll-out.
    RaceSim sim;
    sim.Init(&track, 1, player, npc, 0x12345678u, /*kingRace=*/true);
    RunOutCountdown(sim);

    bool crossed = false;
    int16_t speedAtLine = 0;
    int rollOutTicks = 0;
    for (int i = 0; i < 4000; ++i) {
        // Held well behind: parked level in the next lane is the side-swipe view state.
        sim.SetNpcDebugPose(sim.Player().segPos - 40, 1550);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if (!crossed && sim.Player().segPos >= ROAD_FINISH_SEG) {
            crossed = true;
            speedAtLine = sim.Player().speed;
            // Still racing the instant the line goes by, and still moving.
            CHECK_FALSE(sim.RaceOver());
        } else if (crossed) {
            ++rollOutTicks;
        }
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(crossed);
    CHECK(speedAtLine > 50);

    // It ends only once the car has stopped past the line.
    CHECK(sim.RaceOver());
    CHECK(sim.Player().speed == 0);
    CHECK(sim.Player().segPos > ROAD_FINISH_SEG);
    CHECK(rollOutTicks > 1);
}

TEST_CASE("The roll-out cuts throttle and steering and cannot be crashed out") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x777u);
    RunOutCountdown(sim);

    int rollOutTicks = 0;
    int16_t topRollOutSpeed = 0;
    for (int i = 0; i < 4000; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
        // Past the line, throttle and steering have no effect.
        const bool rolling = sim.RollingOut();
        sim.Tick(rolling ? static_cast<uint16_t>(INPUT_THROTTLE | INPUT_STEER_LEFT)
                         : ThrottleWithShiftIfNeeded(sim));
        if (rolling) {
            ++rollOutTicks;
            CHECK(sim.Player().steerVel == 0);
            topRollOutSpeed = std::max(topRollOutSpeed, sim.Player().speed);
        }
        if (sim.RaceOver()) {
            break;
        }
    }
    CHECK(rollOutTicks > 0);
    // Speed only ever fell during the roll-out, throttle notwithstanding.
    CHECK(topRollOutSpeed < 130);
    CHECK(sim.RaceOver());
    // No crash, no failure: the roll-out ended the race cleanly.
    CHECK((sim.Player().endFlags & END_FAILURE_MASK) == 0);
}

TEST_CASE("The roll-out autopilot walks the car into a lane") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x99u);
    RunOutCountdown(sim);

    for (int i = 0; i < 4000; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
        sim.Tick(sim.RollingOut() ? 0 : ThrottleWithShiftIfNeeded(sim));
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(sim.RaceOver());
    // Opponent held left of 0x961, so the player is steered to lane 0xa00.
    CHECK(sim.Player().lateral == 0xa00);
    // Heading snapped to the road direction (0 on a flat track), so the car
    // tracked straight instead of drifting while it braked.
    CHECK(sim.Player().heading == 0);
}

TEST_CASE("Losing at the line still runs the roll-out and reports a loss") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    RaceSim sim;
    sim.Init(&track, 1, setup, setup, 0x4242u);
    RunOutCountdown(sim);

    for (int i = 0; i < 4000; ++i) {
        // Keep the opponent a clear 6 segments up the road and out of our lane,
        // so it takes the line first.
        sim.SetNpcDebugPose(sim.Player().segPos + 6, 1550);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(sim.RaceOver());
    CHECK_FALSE(sim.PlayerWonAtLine());
    CHECK(sim.Player().speed == 0);
}

// Shifts with the throttle pinned, which is what rolls the money-shift.
uint16_t PowerShift(const RaceSim& sim) {
    const CarState& player = sim.Player();
    const CarTables& tables = sim.PlayerTables();
    if (player.gear < tables.numGears - 1 && player.rpm >= tables.autoShiftRpm) {
        return INPUT_SHIFT | INPUT_THROTTLE;
    }
    return INPUT_THROTTLE;
}

int CountDroppedBoxes(int transClass, int seeds) {
    int dropped = 0;
    for (int s = 0; s < seeds; ++s) {
        CarSetup setup{};
        setup.transClass = transClass;
        setup.carbClass = 1;
        setup.engineClass = 1;
        setup.manifoldClass = 1;
        setup.tireClass = 1;
        setup.weightClass = 4;
        setup.driverSkill = 130;

        TrackData track = MakeFlatTrack();
        RaceSim sim;
        sim.Init(&track, 0, setup, setup, 0x1000u + static_cast<uint32_t>(s) * 7919u);
        RunOutCountdown(sim);
        for (int i = 0; i < 4000; ++i) {
            sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
            sim.Tick(PowerShift(sim));
            if (sim.RaceOver()) break;
        }
        if (sim.Player().endFlags & END_TRANS_DROPPED) dropped++;
    }
    return dropped;
}

TEST_CASE("Power-shifting a manual above 5000 rpm drops the transmission") {
    // Manual, throttle held, rpm > 5000, then PRNG(3277) < transWear/100 + 100: ~3% a shift when fresh.
    const int dropped = CountDroppedBoxes(2, 60);
    CHECK(dropped > 0);
    CHECK(dropped < 60);  // and not so likely that every race ends this way

    // No roll at all on an automatic.
    CHECK(CountDroppedBoxes(0, 60) == 0);
}


// Stock tires wear about twice as fast, and the road course more than the drag strip.
TEST_CASE("TireWearForRace: stock tires wear fastest, road more than drag") {
    CHECK(TireWearForRace(0, true,  0) == 700);   // stock, road
    CHECK(TireWearForRace(0, false, 0) == 300);   // stock, drag
    CHECK(TireWearForRace(1, true,  0) == 400);   // brand name, road
    CHECK(TireWearForRace(2, true,  0) == 400);   // slicks score the same as brand
    CHECK(TireWearForRace(2, false, 0) == 200);   // slicks, drag
    // The roll is added on top, so a race is never worse than base + 299.
    CHECK(TireWearForRace(0, true, 299) == 999);
}

// A fresh set never blows; a worn one only in the back half, never past the line.
TEST_CASE("tire blowout is rolled once and lands before the finish") {
    TrackData track = MakeFlatTrack();
    CarSetup fresh;
    CarSetup worn;
    worn.tireWear = 10000;

    RaceSim sim;
    sim.Init(&track, 1, fresh, fresh, 12345u);
    CHECK(sim.TireBlowSegment() == 0);

    int blowouts = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        sim.Init(&track, 1, worn, fresh, seed);
        const int32_t seg = sim.TireBlowSegment();
        if (seg != 0) {
            ++blowouts;
            CHECK(seg >= 610);
            CHECK(seg < sim.FinishSegment());
        }
    }
    CHECK(blowouts > 0);
}

// A flat pull replaces the curve drift.
TEST_CASE("blown tires pull the car aside at a fixed 25 per tick") {
    TrackData track = MakeFlatTrack();
    CarSetup worn;
    worn.tireWear = 10000;

    RaceSim sim;
    uint32_t seed = 0;
    for (uint32_t s = 1; s <= 500 && seed == 0; ++s) {
        sim.Init(&track, 1, worn, worn, s);
        if (sim.TireBlowSegment() != 0) seed = s;
    }
    REQUIRE(seed != 0);

    sim.Init(&track, 1, worn, worn, seed);
    // Just past the blowout segment instead of driving the course.
    sim.SetCameraState(sim.TireBlowSegment() + 1, 0, 0, 2560, 0);
    sim.Tick(0);
    REQUIRE(sim.PlayerTiresBlown());

    const int16_t before = sim.Player().lateral;
    sim.Tick(0);
    CHECK(sim.Player().lateral - before == 25);
}


TEST_CASE("Analog steering: the stick sets how far the wheel goes over") {
    TrackData track = MakeFlatTrack();

    CarSetup setup{};
    setup.transClass = 2;
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.manifoldClass = 1;
    setup.tireClass = 1;
    setup.weightClass = 4;
    setup.driverSkill = 130;

    CarSetup npcSetup = setup;
    npcSetup.transClass = 0;

    // Steering in short bursts: held too long the car leaves the road and the sim freezes.
    auto rolling = [&](RaceSim& sim) {
        sim.Init(&track, 1, setup, npcSetup, 0x5566u);
        RunOutCountdown(sim);
        sim.SetNpcDebugPose(900, 2200);
        for (int i = 0; i < 200 && sim.Player().speed <= 25; ++i) {
            sim.Tick(ThrottleWithShiftIfNeeded(sim));
        }
        REQUIRE(sim.Player().speed > 20);
        REQUIRE(sim.Phase() == RacePhase::Running);
    };

    // steerVel ramps 2/tick, so 12 ticks is exactly full lock from centre.
    const int kTicksToFullLock = 12;

    SUBCASE("full deflection is the original's ramp to +-24") {
        RaceSim full, original;
        rolling(full);
        rolling(original);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            full.Tick(INPUT_STEER_RIGHT, 1.0f);
            original.Tick(INPUT_STEER_RIGHT);
        }
        REQUIRE(full.Phase() == RacePhase::Running);
        CHECK(full.Player().steerVel == 24);
        // The defaulted argument has to be exactly the old behaviour.
        CHECK(full.Player().steerVel == original.Player().steerVel);
        CHECK(full.Player().slideVel == original.Player().slideVel);
        CHECK(full.Player().heading == original.Player().heading);
    }

    SUBCASE("half a stick settles at half lock and holds there") {
        RaceSim sim;
        rolling(sim);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        }
        REQUIRE(sim.Phase() == RacePhase::Running);
        CHECK(sim.Player().steerVel == 12);
        // Still held: it stays put rather than creeping on to full lock.
        for (int i = 0; i < 5; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
            CHECK(sim.Player().steerVel == 12);
        }
    }

    SUBCASE("a lighter touch turns the car less") {
        RaceSim light, hard;
        rolling(light);
        rolling(hard);
        const int16_t start = light.Player().heading;
        REQUIRE(hard.Player().heading == start);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            light.Tick(INPUT_STEER_RIGHT, 0.25f);
            hard.Tick(INPUT_STEER_RIGHT, 1.0f);
        }
        CHECK(light.Player().steerVel == 6);
        CHECK(hard.Player().steerVel == 24);
        // Heading counts down as the car turns, so less lock = less travelled.
        CHECK(std::abs(light.Player().heading - start) <
              std::abs(hard.Player().heading - start));
    }

    SUBCASE("easing the stick back unwinds instead of snapping") {
        RaceSim sim;
        rolling(sim);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 1.0f);
        }
        REQUIRE(sim.Phase() == RacePhase::Running);
        REQUIRE(sim.Player().steerVel == 24);

        // Down by 2 a tick, the same rate it wound on, not straight to 12.
        sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        CHECK(sim.Player().steerVel == 22);
        sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        CHECK(sim.Player().steerVel == 20);

        for (int i = 0; i < 5 && sim.Phase() == RacePhase::Running; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        }
        CHECK(sim.Player().steerVel == 12);
    }

    SUBCASE("a centred stick is no steering at all") {
        RaceSim sim;
        rolling(sim);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.0f);
        }
        CHECK(sim.Player().steerVel == 0);
    }
}

TEST_CASE("BlendCarView steps the drawn car between two sim steps") {
    CarState prev, cur;
    prev.segPos = 300; prev.scrollAcc = 6; prev.lateral = 2400; prev.heading = 0;
    cur.segPos = 302;  cur.scrollAcc = 2;  cur.lateral = 2500;  cur.heading = 20;

    CarState half = cur;
    BlendCarView(prev, half, 0.5f);
    // 2406 .. 2418 eighths, halfway is 2412 = segment 301, scroll 4.
    CHECK(half.segPos == 301);
    CHECK(half.scrollAcc == 4);
    CHECK(half.lateral == 2450);
    CHECK(half.heading == 10);

    CarState start = cur, end = cur;
    BlendCarView(prev, start, 0.0f);
    BlendCarView(prev, end, 1.0f);
    CHECK(start.segPos == 300);
    CHECK(start.scrollAcc == 6);
    CHECK(end.segPos == 302);
    CHECK(end.scrollAcc == 2);

    // A teleport is drawn where it landed.
    CarState jump = cur;
    jump.segPos = 900;
    BlendCarView(prev, jump, 0.5f);
    CHECK(jump.segPos == 900);
}

}
