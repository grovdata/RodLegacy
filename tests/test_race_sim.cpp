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

uint16_t ThrottleWithShiftIfNeeded(const RaceSim& sim) {
    const CarState& player = sim.Player();
    const CarTables& tables = sim.PlayerTables();
    const bool canShiftUp = player.gear < tables.numGears - 1;
    uint16_t input = 0;
    if (canShiftUp && player.rpm >= tables.autoShiftRpm) {
        input |= INPUT_SHIFT | INPUT_THROTTLE;  // bang the next gear under power
    } else if (player.rpm >= 5600) {
        // Top gear near redline: lift off so we don't grenade the engine by
        // sitting on the limiter (the original blows after ~15 ticks held there).
    } else {
        input |= INPUT_THROTTLE;
    }
    return input;
}

// Idle on the line until the staging countdown ends, so the caller can floor it
// at the green without triggering a false start.
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

// The cop spawn is a 1-in-4 roll inside Init, so tests that need one search for
// a seed rather than reaching into the sim.
// Inverse of the above: a seed where the spawn roll does NOT arm a cop, so
// kingRace can be toggled without also toggling the cop.
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

// Arms the cop, drops the player just past the trigger segment with the
// opponent well behind, and ticks once so the cop takes over the opponent slot.
void StartCopPursuit(RaceSim& sim, const TrackData& track, const CarSetup& setup,
                     uint32_t seed) {
    sim.Init(&track, 1, setup, setup, seed);
    REQUIRE(sim.CopState() == 1);
    sim.SetCameraState(sim.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    sim.SetNpcDebugPose(sim.CopTriggerSegment() - 100, 2200);
    sim.Tick(0);
}

// Flips purist scoring on for one test case and puts it back afterwards, so a
// failure cannot leak the setting into the rest of the suite.
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

}  // namespace

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

// Same term-by-term arithmetic the 68k does, with the fixed tune term. The
// purist tune term is covered by the two cases above.
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

// Reference reading taken from the real game: a stock, correctly tuned 1940
// Chevrolet 2-dr coupe shows 92 mph. That car is SR_data car spec 0 - class
// byte 0, config 0x1000, i.e. GM automatic, 2-brl carb, stock manifold, stock
// tires - and the engine/manifold pairing scores 0 whichever way round the
// spec word's two fields are read. See top_speed.md.
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

// Second reference reading from the real game: the newspaper's 1963 Corvette
// shows 119 mph once tuned. SR_data car spec 5, config 0x1ba2, class byte 8 -
// racing 4-brl carb, racing 4-speed GM, racing slicks, engine class 2 with the
// class-4 racing manifold, which is the pairing that scores 100.
//
// roofChopped is set on a *stock* car here on purpose: no Corvette can be
// chopped, and the original hands out the 9 points for a mod the body cannot
// do. Without it the car scores 110 and the 119 does not come out.
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

    // Three notches off perfect tune is the 117 the same car reads before it
    // is tuned: the term goes (1-3)*20/8 = -5 instead of +2.
    CarSetup untuned = TunedCorvette();
    untuned.tuneOffset = 3;
    CHECK(ComputeTopSpeed(untuned) == 117);

    // The King drives this exact car (spec 25 is spec 5 again). A player who
    // buys one and pulls both bumpers gets to 122 - beating him in his own car
    // is possible but tight, which is what the strategy guides describe.
    CarSetup built = TunedCorvette();
    built.frontBumperRemoved = true;
    built.rearBumperRemoved = true;
    CHECK(ComputeTopSpeed(built) == 122);
}

// The same car with the tune term "fixed". Not what the game shows - kept as a
// pin so the two modes cannot silently converge.
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

// The body-mod freebie the Corvette reading exposed: a Corvette gets the roof
// points because it cannot be chopped, but no bumper points, because it can
// lose its bumpers like any other car. Treating the bumpers as impossible too
// would put the 1963 at 125 mph instead of 119.
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
        // Hold the opponent in the far-left lane. Without this the pair start
        // the race overlapping and the test "finishes" on the rear-end crash
        // (END_HIT_NPC carries END_FINISHED) a dozen segments off the line,
        // rather than on reaching 380.
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
    // Who wins is deliberately not asserted. The opponent rubber-bands to
    // player speed + 16 while level and sprints when jockeying for position, and
    // ThrottleWithShiftIfNeeded drives the player conservatively (lifting off
    // near the redline), so the finish is close either way. This test is about
    // the sim staying in sane ranges and reaching the flag.
    CHECK(std::abs(static_cast<int>(sim.Player().segPos) -
                   static_cast<int>(sim.Npc().segPos)) < 40);
}

TEST_CASE("Steering input counteracts curve drift") {
    TrackData track = MakeFlatTrack();
    // The road-direction array that pushes the car sideways is curve[] (A4+0x1098),
    // not elev[] - see ApplyLateralDrift. A non-zero curve[] run creates the drift.
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
    // Park the opponent far up the road. This test is about curve drift versus
    // steering input, and holding the throttle without ever steering otherwise
    // rear-ends the blocking NPC long before segment 305 - a real outcome, but
    // not the one under test here.
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
            // 510 + rand(255): always in the middle-to-late stretch of a road
            // course, which runs 200..1020.
            CHECK(road.CopTriggerSegment() >= 510);
            CHECK(road.CopTriggerSegment() < 765);
        }
    }
    // It is a 1-in-4 roll, so over 79 road races it must fire sometimes and
    // must not fire always.
    CHECK(roadSpawns > 0);
    CHECK(roadSpawns < 79);
}

TEST_CASE("Cop is cancelled unless the player is 5+ segments ahead") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    // Level with the opponent at the trigger: no cop at all. You only get
    // pulled over for leading.
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

    // Build speed with the cop held far enough back that it cannot catch us,
    // then force the capture and check the fine matches the tier we are in.
    for (int i = 0; i < 400 && sim.Player().speed < 60; ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 40, 2200);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(sim.Player().speed >= 51);
    REQUIRE_FALSE(sim.PlayerBusted());

    // Let it close first - the capture only arms once the siren is going.
    sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
    sim.Tick(ThrottleWithShiftIfNeeded(sim));
    REQUIRE(sim.CopState() == 3);

    sim.SetNpcDebugPose(sim.Player().segPos + 4, 2200);
    sim.Tick(ThrottleWithShiftIfNeeded(sim));
    REQUIRE(sim.PlayerBusted());

    // Fine tracks the speed actually recorded at capture, not the speed we
    // happened to read a tick earlier.
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

    // Run to the flag with the cop pinned close behind, so the siren is going
    // the whole way but it never gets ahead of us.
    bool sirenSounded = false;
    for (int i = 0; i < 4000 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        sirenSounded = sirenSounded || sim.CopSirenOn();
    }

    REQUIRE(sim.RaceOver());
    CHECK(sirenSounded);              // it really was chasing us
    CHECK(sim.PlayerWonAtLine());
    // The whole point: the original re-tests its failure word before ORing in
    // the caught bit, and a win is recorded in a different word, so crossing
    // the line first gets you off scot free.
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

TEST_CASE("Cop cannot be rear-ended or shove you off the road") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Sit level and laterally overlapping while closing - without the cop gate
    // this is exactly the geometry that sets END_HIT_NPC.
    for (int i = 0; i < 30 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos, sim.Player().lateral + 20);
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
        CHECK((sim.Player().endFlags & END_HIT_NPC) == 0);
    }
}

TEST_CASE("Init fully resets a sim that has already been raced") {
    // The scene objects that own a RaceSim are built once at game start and are
    // reused for every race, so re-Init has to be a complete reset - anything it
    // misses leaks into the next race.
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    RaceSim fresh;
    fresh.Init(&track, 1, setup, setup, seed);

    // Two separate dirty states, because they are mutually exclusive: being
    // caught is what switches the siren back off, so a captured sim has a
    // non-zero caught speed but a quiet siren, and vice versa. Testing only one
    // of them lets the other's reset go missing unnoticed.
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
    // Must be a seed with no cop: kingRace also suppresses the spawn roll, so
    // on a cop-armed seed this would compare "cop" against "no cop" instead of
    // King AI against ordinary AI.
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
            // Pinned 30 segments up the road - past the 13 segment threshold
            // where an ordinary opponent drops back to its cruising speed
            // (UpdateNPCEngine 0x2444c2) - with the player left crawling.
            sim.SetNpcDebugPose(sim.Player().segPos + 30, 2200);
            sim.Tick(0);
        }
        return Result{sim.Npc().speed, sim.NpcAi().targetSpeedNormal,
                      sim.NpcAi().targetSpeedRisky};
    };

    const Result ordinary = runClearAhead(false);
    const Result king = runClearAhead(true);

    // The ordinary opponent is pulled back to its cruising target and hovers
    // just above it (the ease-off assigns the target, the next tick accelerates
    // off it again).
    CHECK(ordinary.finalSpeed <= ordinary.normal + 2);
    // The King never enters that branch, so it is held to neither target and
    // runs away to its car's own limit - comfortably clear of the higher target
    // it would settle at if the branch did apply to it.
    CHECK(king.finalSpeed > king.risky + 20);
}

TEST_CASE("In a drag race the King eases to the higher of the two targets") {
    TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    // The road-race clause of the King's entry test does not apply to a drag
    // race, so here it does reach the far-ahead branch - and takes the risky
    // target where an ordinary opponent takes the normal one.
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
    // Opponent left a long way back, which is the normal state of affairs by the
    // time the cop trigger is reached: it takes over wherever the opponent is.
    sim.SetNpcDebugPose(sim.CopTriggerSegment() - 115, 2200);
    sim.Tick(0);
    REQUIRE(sim.CopState() == 2);

    const int gapAtSpawn =
        static_cast<int>(sim.Player().segPos) - static_cast<int>(sim.Npc().segPos);
    REQUIRE(gapAtSpawn > 100);

    // The cop must not inherit the challenger's performance or its concede
    // state, or it simply trails along and can never catch anyone.
    CHECK_FALSE(sim.NpcAi().gaveUp);
    CHECK(sim.NpcTables().perfScore > 70);

    for (int i = 0; i < 900 && !sim.RaceOver() && !sim.PlayerBusted(); ++i) {
        sim.Tick(ThrottleWithShiftIfNeeded(sim));
    }

    const int gapEnd =
        static_cast<int>(sim.Player().segPos) - static_cast<int>(sim.Npc().segPos);
    CHECK(gapEnd < gapAtSpawn);          // it closed
    CHECK_FALSE(sim.NpcAi().gaveUp);     // and never conceded on the way
    // Fleeing flat out is not enough to shake it: it runs to player + 32.
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
        // Nothing at all is latched: an event carrying a suppressed bit is
        // dropped whole, so a rear-end cannot leave END_FINISHED behind either.
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

    // Standing still: PlayerSkidding() (RaceLogicUpdate 0x244d50) requires
    // speed > 20 mph, so even pinning the wheel over should stay quiet.
    for (int i = 0; i < 10; ++i) {
        sim.Tick(INPUT_STEER_LEFT);
        CHECK_FALSE(sim.PlayerSkidding());
    }

    // Get rolling past 20 mph, then hold the wheel hard over. slideVel ramps
    // by 2/tick toward its +-24 clamp, so it crosses the |slideVel| >= 12
    // threshold within a handful of ticks.
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

    // Hold the throttle on the line with the selector in N. The box must stay in
    // neutral so the car never rolls: the engine revs, nothing moves.
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

    // Now hold the throttle while coasting out of gear. The needle must track
    // road speed, not run away on the rpm gain.
    for (int i = 0; i < 20 && sim.Player().speed > 0; ++i) {
        sim.Tick(INPUT_THROTTLE);
        const CarState& p = sim.Player();
        CHECK(p.rpm == p.speed * 30 + 1500);
    }
    CHECK(sim.Player().rpm < 5000);

    // Stationary in neutral, though, it must still rev - that is the start-line
    // blip, and the rpm gain is what drives it.
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

    // Flat out for far longer than the ~15 ticks that grenades a manual on the
    // limiter. The governor holds it below the redline, so it must survive.
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

}  // namespace

// The bug this fixes: `speed = rpm/ratio` was recomputed every tick and braking
// was a post-pass on top of it, so a braking car sprang back to the speed implied
// by the 1500 rpm idle floor and could never stop. In the original the two are
// alternatives, not a sequence (PlayerCarPhysics 0x23c5d6).
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

    // kingRace suppresses the cop spawn roll. This test is about the roll-out
    // past the line, and a working cop would end the race before the player
    // ever gets there.
    RaceSim sim;
    sim.Init(&track, 1, player, npc, 0x12345678u, /*kingRace=*/true);
    RunOutCountdown(sim);

    bool crossed = false;
    int16_t speedAtLine = 0;
    int rollOutTicks = 0;
    for (int i = 0; i < 4000; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
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

    // It ends only once the car has stopped, some way past the line. Whether the
    // player won is incidental here - see the drag smoke test.
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
        // Race normally to the line, then bury the throttle and hold full left
        // lock: past the line neither may have any effect.
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

}  // namespace srrace
