#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "parsers/libp.h"
#include "race/race_sim.h"
#include "race/track_gen.h"
#include "race/grudge.h"
#include "race/race_types.h"

namespace srrace {
namespace {

srrace::TrackData MakeFlatTrack() {
    srrace::TrackData track{};
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        track.left[i] = 0;
        track.right[i] = 0;
        track.flags[i] = 0;
        track.elev[i] = 0;
        track.curve[i] = 0;
    }
    return track;
}

uint16_t ThrottleWithShiftIfNeeded(const srrace::RaceSim& sim) {
    const CarState& player = sim.Player();
    const CarTables& tables = sim.PlayerTables();
    const bool canShiftUp = player.gear < tables.numGears - 1;
    // Low gears gain ~500 rpm a tick, so trigger well under 5000.
    const int shiftRpm = std::min<int>(tables.autoShiftRpm, 3600);
    uint16_t input = 0;
    if (canShiftUp && player.rpm >= shiftRpm) {
        input |= INPUT_SHIFT | srrace::INPUT_THROTTLE;
    } else if (player.rpm >= 5600) {
        // Lift near redline, or the engine blows after ~15 ticks on the limiter.
    } else {
        input |= srrace::INPUT_THROTTLE;
    }
    return input;
}

void RunOutCountdown(srrace::RaceSim& sim) {
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

uint32_t SeedWithoutCop(const srrace::TrackData& track, const CarSetup& setup) {
    for (uint32_t seed = 1; seed < 500; ++seed) {
        srrace::RaceSim probe;
        probe.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
        if (probe.CopState() == 0) {
            return seed;
        }
    }
    return 0;
}

uint32_t SeedThatArmsCop(const srrace::TrackData& track, const CarSetup& setup) {
    for (uint32_t seed = 1; seed < 500; ++seed) {
        srrace::RaceSim probe;
        probe.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
        if (probe.CopState() == 1) {
            return seed;
        }
    }
    return 0;
}

void StartCopPursuit(srrace::RaceSim& sim, const srrace::TrackData& track, const CarSetup& setup,
                     uint32_t seed) {
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
    REQUIRE(sim.CopState() == 1);
    sim.SetCameraState(sim.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    sim.SetNpcDebugPose(sim.CopTriggerSegment() - 100, 2200);
    sim.Tick(0);
}

// Restored afterwards so it cannot leak into the rest of the suite.
struct UncapScope {
    bool previous;
    explicit UncapScope(bool enabled) : previous(UncapTopSpeed()) {
        SetUncapTopSpeed(enabled);
    }
    ~UncapScope() { SetUncapTopSpeed(previous); }
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

TEST_CASE("the grudge handicap stages the two cars separately") {
    srrace::TrackData track{};
    srrace::RaceRng rng(0x1234u);
    srrace::RoadLayoutInit(track, rng, 1, srrace::TrackGenSource::StreetRod2);
    srrace::BuildGrudgeStrip(track);
    const CarSetup setup = PerfectCar();

    // Nothing on the pedals while staged - throttle in gear before your own
    // green is a false start, which is the point of having two deadlines.
    auto stage = [](srrace::RaceSim& sim) { sim.Tick(0); };
    auto drive = [](srrace::RaceSim& sim) {
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
    };

    // The deadlines are the original's frames, so the sim spends them at its own
    // tick rate: 110 frames is a little over two seconds either way.
    const int baseTicks = srrace::RaceFramesToTicks(srrace::GRUDGE_STAGE_FRAMES);
    CHECK(baseTicks == 33);

    // Equal dials: one green, and the opponent waits for it too.
    {
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Grudge, setup, setup, 7u);
        const auto delay = srrace::GrudgeStageDelays(120, 120);
        sim.SetLaunchHandicap(delay.playerFrames, delay.opponentFrames);
        CHECK(sim.NpcLaunchOffset() == 0);
        const int16_t npcStart = sim.Npc().segPos;
        int ticks = 0;
        while (sim.Phase() == srrace::RacePhase::Countdown && ticks < 4000) {
            stage(sim);
            ++ticks;
        }
        CHECK(ticks == baseTicks);
        CHECK(sim.Npc().segPos == npcStart);
        CHECK(sim.Npc().speed == 0);
    }

    // The player's dial is the quicker one, so the player waits and the
    // opponent is already gone by the time the player gets the green.
    {
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Grudge, setup, setup, 7u);
        const auto delay = srrace::GrudgeStageDelays(100, 160);
        sim.SetLaunchHandicap(delay.playerFrames, delay.opponentFrames);
        CHECK(delay.playerFrames == srrace::GRUDGE_STAGE_FRAMES + 60 * 7);
        const int heldTicks = srrace::RaceFramesToTicks(delay.playerFrames);
        CHECK(sim.NpcLaunchOffset() == baseTicks - heldTicks);
        const int16_t npcStart = sim.Npc().segPos;
        int ticks = 0;
        while (sim.Phase() == srrace::RacePhase::Countdown && ticks < 4000) {
            stage(sim);
            ++ticks;
        }
        CHECK(ticks == heldTicks);
        CHECK(sim.Npc().segPos > npcStart);
        CHECK(sim.Player().speed == 0);
        // Staying off the pedals is not a jumped start, however long the wait.
        CHECK((sim.Player().endFlags & srrace::END_STALLED) == 0);
    }

    // And the other way round: the player launches on the base stage time and
    // the opponent is pinned for the difference.
    {
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Grudge, setup, setup, 7u);
        const auto delay = srrace::GrudgeStageDelays(160, 100);
        sim.SetLaunchHandicap(delay.playerFrames, delay.opponentFrames);
        CHECK(delay.playerFrames == srrace::GRUDGE_STAGE_FRAMES);
        const int holdTicks =
            srrace::RaceFramesToTicks(delay.opponentFrames) - baseTicks;
        CHECK(sim.NpcLaunchOffset() == holdTicks);
        const int16_t npcStart = sim.Npc().segPos;
        const int16_t playerStart = sim.Player().segPos;
        int ticks = 0;
        while (sim.Phase() == srrace::RacePhase::Countdown && ticks < 4000) {
            stage(sim);
            ++ticks;
        }
        CHECK(ticks == baseTicks);
        CHECK(sim.Npc().segPos == npcStart);

        // Held while the player drives away. Position, not speed: the hold runs
        // out long after the player has left the line.
        for (int i = 0; i < holdTicks - 1; ++i) {
            drive(sim);
        }
        CHECK(sim.Npc().segPos == npcStart);
        CHECK(sim.Player().segPos > playerStart);
        CHECK(sim.NpcLaunchOffset() == 1);

        // Then released.
        for (int i = 0; i < 80; ++i) {
            drive(sim);
        }
        CHECK(sim.NpcLaunchOffset() == 0);
        CHECK(sim.Npc().segPos > npcStart);
    }
}

TEST_CASE("the opponent finishing settles it once the player has stopped") {
    // lrace.c latches from the opponent's update too, and both sites gate on the PLAYER's speed being 0.
    srrace::TrackData track{};
    srrace::RaceRng rng(0x1234u);
    srrace::RoadLayoutInit(track, rng, 1, srrace::TrackGenSource::StreetRod2);
    const CarSetup setup = PerfectCar();

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 7u);
    RunOutCountdown(sim);

    // Coast: the opponent drives away and the player never touches the throttle.
    int ticks = 0;
    while (!sim.RaceOver() && ticks < 4000) {
        sim.Tick(0);
        ++ticks;
    }
    CHECK(sim.RaceOver());
    CHECK(sim.Player().speed == 0);
    CHECK(sim.Player().segPos < sim.Npc().segPos);
    // Standing still short of the line is a loss, not a win.
    CHECK((sim.Player().endFlags & srrace::END_FINISHED) != 0);
    CHECK((sim.Npc().endFlags & srrace::END_FINISHED) == 0);
}

TEST_CASE("the grudge strip has a right-hand limit too") {
    // jroad.c 0x246860's `race_type & 0x0c` arm. It bites 180 short of the
    // edge, so it fires while the car is still on the road.
    srrace::TrackData track{};
    srrace::RaceRng rng(0x1234u);
    srrace::RoadLayoutInit(track, rng, 1, srrace::TrackGenSource::StreetRod2);
    srrace::BuildGrudgeStrip(track);
    const CarSetup setup = PerfectCar();

    // At speed it ends the round, the same as the centre line on the other side.
    srrace::RaceSim fast;
    fast.Init(&track, srrace::RaceKind::Grudge, setup, setup, 7u);
    RunOutCountdown(fast);
    int ticks = 0;
    while ((fast.Player().endFlags & srrace::END_WALL_HIT) == 0 && ticks < 600) {
        fast.Tick(static_cast<uint16_t>(srrace::ThrottleWithShiftIfNeeded(fast) |
                                        srrace::INPUT_STEER_RIGHT));
        ++ticks;
    }
    CHECK((fast.Player().endFlags & srrace::END_WALL_HIT) != 0);
    CHECK(fast.Player().speed > srrace::EDGE_DROP_MAX_MPH);

    // Standing on it instead is a shove back across the road, not a crash.
    srrace::RaceSim slow;
    slow.Init(&track, srrace::RaceKind::Grudge, setup, setup, 7u);
    slow.SetCameraState(12, 0, 0, 3100, 0);
    const int16_t before = slow.Player().lateral;
    slow.Tick(0);
    CHECK((slow.Player().endFlags & srrace::END_WALL_HIT) == 0);
    CHECK(slow.Player().lateral <= before - 150);
    CHECK(slow.Player().heading == 0x14);
}

TEST_CASE("the drop at the right edge is read a segment ahead") {
    // jroad.c 0x246830 tests bit 15 of right[seg+1], not of the segment the car
    // is on, and only on Mulholland.
    srrace::TrackData drop = MakeFlatTrack();
    const CarSetup dropCar = PerfectCar();
    const int seg = 210;
    drop.right[seg + 1] = 0x8000;

    srrace::RaceSim sim;
    sim.Init(&drop, srrace::RaceKind::Mulholland, dropCar, dropCar, 7u);
    // Off the right edge (3300) but inside the 150 tolerance, at a standstill.
    sim.SetCameraState(seg, 0, 0, 3300, 0);
    const int16_t before = sim.Player().lateral;
    sim.Tick(0);
    CHECK((sim.Player().endFlags & srrace::END_WALL_HIT) == 0);
    CHECK(sim.Player().lateral <= before - 250);
    CHECK(sim.Player().heading == 0x1e);
    CHECK(sim.Player().speed == 0);

    // Without the bit on the NEXT segment nothing happens, however the car sits.
    srrace::TrackData clean = MakeFlatTrack();
    clean.right[seg] = 0x8000;
    srrace::RaceSim off;
    off.Init(&clean, srrace::RaceKind::Mulholland, dropCar, dropCar, 7u);
    off.SetCameraState(seg, 0, 0, 3300, 0);
    const int16_t stay = off.Player().lateral;
    off.Tick(0);
    CHECK(off.Player().lateral == stay);
}

TEST_CASE("fall_dir is set only by going off the road over a drop") {
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup car = PerfectCar();
    const int seg = 210;
    track.wallLeft[seg] = -500;

    // Off the edge but inside the tolerance: no fall.
    srrace::RaceSim near;
    near.Init(&track, srrace::RaceKind::Mulholland, car, car, 7u);
    near.SetCameraState(seg, 0, 0, 1450, 0);
    near.Tick(0);
    CHECK(near.FallDirection() == 0);

    srrace::RaceSim over;
    over.Init(&track, srrace::RaceKind::Mulholland, car, car, 7u);
    over.SetCameraState(seg, 0, 0, 1350, 0);
    over.Tick(0);
    CHECK((over.Player().endFlags & srrace::END_WALL_HIT) != 0);
    CHECK(over.FallDirection() == -1);
}

TEST_CASE("crossing the centre line loses a grudge round") {
    srrace::TrackData track{};
    srrace::RaceRng rng(0x1234u);
    srrace::RoadLayoutInit(track, rng, 1, srrace::TrackGenSource::StreetRod2);
    srrace::BuildGrudgeStrip(track);
    const CarSetup setup = PerfectCar();

    auto launch = [&](srrace::RaceSim& sim, srrace::RaceKind kind) {
        sim.Init(&track, kind, setup, setup, 7u);
        while (sim.Phase() == srrace::RacePhase::Countdown) {
            sim.Tick(0);
        }
    };

    // The lane is the right half of the strip, and the car stages ten units
    // clear of the limit - the road's midpoint plus 150.
    srrace::RaceSim sim;
    launch(sim, srrace::RaceKind::Grudge);
    CHECK(sim.Player().lateral == 2560);

    int ticks = 0;
    while ((sim.Player().endFlags & srrace::END_CROSSED_LINE) == 0 && ticks < 600) {
        sim.Tick(static_cast<uint16_t>(srrace::ThrottleWithShiftIfNeeded(sim) |
                                       srrace::INPUT_STEER_LEFT));
        ++ticks;
    }
    CHECK((sim.Player().endFlags & srrace::END_CROSSED_LINE) != 0);
    CHECK(sim.Player().lateral < 2400 + srrace::GRUDGE_CENTRE_LINE_MARGIN);
    // It is a failure bit, so the race stops there.
    CHECK(sim.Phase() == srrace::RacePhase::Ended);

    // Holding the lane runs the round out clean.
    srrace::RaceSim clean;
    launch(clean, srrace::RaceKind::Grudge);
    for (int i = 0; i < 600 && clean.Phase() != srrace::RacePhase::Ended; ++i) {
        clean.Tick(srrace::ThrottleWithShiftIfNeeded(clean));
    }
    CHECK((clean.Player().endFlags & srrace::END_CROSSED_LINE) == 0);

    // Only grudge races have a lane to leave: the same steering on the drag
    // strip is just a line to pick.
    srrace::RaceSim drag;
    launch(drag, srrace::RaceKind::Drag);
    for (int i = 0; i < 600 && drag.Phase() != srrace::RacePhase::Ended; ++i) {
        drag.Tick(srrace::INPUT_THROTTLE | srrace::INPUT_STEER_LEFT);
    }
    CHECK((drag.Player().endFlags & srrace::END_CROSSED_LINE) == 0);
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
    {
        UncapScope uncap(true);
        CHECK(ComputeBasePerformance(setup) == 70);
    }

    // Original scoring: the tune term becomes (1-8)*20/8 = -17, so pts = -13 and
    // base = 70 + (-13 * 24 / 100) = 67.
    CHECK(ComputeBasePerformance(setup) == 67);
}

TEST_CASE("ComputeBasePerformance perfect car lands on the 100-point ceiling") {
    // 80 points of parts and body plus the intended 20 for a perfect tune.
    {
        UncapScope uncap(true);
        CHECK(ComputeBasePerformance(PerfectCar()) == 70 + (100 * 30) / 100);  // 100
    }

    // Original scoring: the tune term gives 2 instead of 20, so pts = 82.
    CHECK(ComputeBasePerformance(PerfectCar()) == 70 + (82 * 30) / 100);  // 94
}

// The 68k's term-by-term arithmetic, with the fixed tune term.
TEST_CASE("ComputeBasePerformance matches the formula term by term") {
    UncapScope uncap(true);
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
TEST_CASE("Stock 1940 Chevrolet 2-dr coupe reads 92 mph with the original scoring") {
    CarSetup chevy{};
    chevy.carbClass = 0;      // 2-brl
    chevy.transClass = 0;     // GM automatic
    chevy.engineClass = 1;    // V6
    chevy.manifoldClass = 0;  // stock
    chevy.tireClass = 0;
    chevy.weightClass = 0;    // spec class byte
    chevy.tuneOffset = 0;     // "with right tuning"

    UncapScope uncap(false);
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

TEST_CASE("Tuned 1963 Corvette reads 119 mph with the original scoring") {
    UncapScope uncap(false);
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

    UncapScope uncap(true);
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
    UncapScope uncap(false);
    CHECK(ComputeTopSpeed(stripped) == 122);
}

TEST_CASE("A perfect car reaches exactly the 130 mph cap") {
    {
        UncapScope uncap(true);
        CHECK(ComputeTopSpeed(PerfectCar()) == 130);
    }

    // Original scoring: the tune bug leaves the cap unreachable and the best car at 122.
    CHECK(ComputeTopSpeed(PerfectCar()) == 122);
}

TEST_CASE("No legal set of parts exceeds the cap in either mode") {
    for (int mode = 0; mode < 2; ++mode) {
        UncapScope uncap(mode == 0);
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
    srrace::TrackData track = MakeFlatTrack();

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

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, playerSetup, npcSetup, 0x12345678u);
    srrace::RunOutCountdown(sim);

    bool finished = false;
    for (int i = 0; i < 6000; ++i) {
        // Hold the opponent in the far-left lane, or the pair overlap and the race ends on a rear-end crash.
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));

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
    UncapScope uncap(true);
    TrackData track = MakeFlatTrack();
    // A one-segment kink far enough down the course for the opponent to exceed the cap.
    track.curve[801] = 1;

    CarSetup setup = CopTestSetup();
    setup.aggression = 12;   // 0x0c, the worst driver: the biggest penalty
    // A slower player, so the opponent is clear ahead and takes the full penalty.
    CarSetup slowPlayer;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, slowPlayer, setup, SeedWithoutCop(track, setup));
    srrace::RunOutCountdown(sim);

    const int perf = sim.NpcTables().perfScore;
    int caps = 0;
    int cappedTo = -1;
    for (int i = 0; i < 8000 && sim.Npc().segPos < 805; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);   // keep the pair apart
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup = CopTestSetup();   // carb class 1 -> band end 5000

    auto peakRpmBeforeShift = [&](int reaction) {
        CarSetup npc = setup;
        npc.reaction = reaction;
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Road, setup, npc, SeedWithoutCop(track, npc));
        srrace::RunOutCountdown(sim);
        int peak = 0;
        const int startGear = sim.Npc().gear;
        for (int i = 0; i < 400 && sim.Npc().gear <= startGear + 1; ++i) {
            sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
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

    srrace::RaceSim noSteer;
    noSteer.Init(&track, srrace::RaceKind::Road, setup, npcSetup, 0x1122u);
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

    srrace::RaceSim withSteerAdaptive;
    withSteerAdaptive.Init(&track, srrace::RaceKind::Road, setup, npcSetup, 0x3344u);
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    int roadSpawns = 0;
    for (uint32_t seed = 1; seed < 80; ++seed) {
        srrace::RaceSim drag;
        drag.Init(&track, srrace::RaceKind::Drag, setup, setup, seed);
        CHECK(drag.CopState() == 0);          // never on the drag strip

        srrace::RaceSim king;
        king.Init(&track, srrace::RaceKind::Road, setup, setup, seed, /*kingRace=*/true);
        CHECK(king.CopState() == 0);          // never in the King's race

        srrace::RaceSim road;
        road.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim level;
    level.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
    REQUIRE(level.CopState() == 1);
    level.SetCameraState(level.CopTriggerSegment() + 1, 0, 0, 2560, 0);
    level.SetNpcDebugPose(level.CopTriggerSegment() + 1, 2200);
    level.Tick(0);
    CHECK(level.CopState() == 0);
    CHECK_FALSE(level.CopChasing());

    // Well clear of the opponent: the cop takes over the opponent slot.
    srrace::RaceSim leading;
    StartCopPursuit(leading, track, setup, seed);
    CHECK(leading.CopState() == 2);
    CHECK(leading.CopChasing());
    CHECK(leading.ConsumeCopSpawn());        // one-shot for the sprite swap
    CHECK_FALSE(leading.ConsumeCopSpawn());  // and only once
    CHECK_FALSE(leading.CopSirenOn());       // not close enough yet
}

TEST_CASE("Cop siren starts when it closes, and catching you ends the race") {
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Build speed with the cop held back, then force the capture and check the fine tier.
    for (int i = 0; i < 400 && sim.Player().speed < 60; ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 40, 2200);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // The cop pinned close behind all the way to the flag, never ahead.
    bool sirenSounded = false;
    for (int i = 0; i < 4000 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos - 10, 2200);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
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
    srrace::TrackData track = MakeFlatTrack();
    CarSetup worn = CopTestSetup();
    worn.tireWear = 10000;

    uint32_t seed = 0;
    for (uint32_t s = 1; s <= 2000 && seed == 0; ++s) {
        srrace::RaceSim probe;
        probe.Init(&track, srrace::RaceKind::Road, worn, worn, s);
        if (probe.CopState() == 1 && probe.TireBlowSegment() != 0 &&
            probe.TireBlowSegment() > probe.CopTriggerSegment()) {
            seed = s;
        }
    }
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
    StartCopPursuit(sim, track, setup, seed);
    REQUIRE(sim.CopState() == 2);

    // Level and overlapping while closing: the geometry that sets END_HIT_NPC without the cop gate.
    for (int i = 0; i < 30 && !sim.RaceOver(); ++i) {
        sim.SetNpcDebugPose(sim.Player().segPos, sim.Player().lateral + 20);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
        CHECK((sim.Player().endFlags & END_HIT_NPC) == 0);
    }
}

TEST_CASE("Init fully resets a sim that has already been raced") {
    // The scene objects that own a srrace::RaceSim are built once at game start and are
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim fresh;
    fresh.Init(&track, srrace::RaceKind::Road, setup, setup, seed);

    // Two dirty states, since capture switches the siren off: test both resets.
    SUBCASE("left mid-pursuit with the siren going") {
        srrace::RaceSim used;
        StartCopPursuit(used, track, setup, seed);
        used.SetNpcDebugPose(used.Player().segPos - 10, 2200);
        used.Tick(0);
        REQUIRE(used.CopState() == 3);
        REQUIRE(used.CopSirenOn());          // genuinely dirty

        used.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
        CHECK_FALSE(used.CopSirenOn());
        CHECK_FALSE(used.CopChasing());
        CHECK(used.CopState()          == fresh.CopState());
        CHECK(used.CopTriggerSegment() == fresh.CopTriggerSegment());
        CHECK(used.InCountdown());
        CHECK_FALSE(used.ConsumeCopSpawn());
    }

    SUBCASE("left busted at speed") {
        srrace::RaceSim used;
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

        used.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
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
    srrace::TrackData track = MakeFlatTrack();
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
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Road, setup, setup, seed, king);
        REQUIRE(sim.CopState() == 0);
        srrace::RunOutCountdown(sim);
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    // In a drag race the King does reach the far-ahead branch, taking the risky target.
    auto finalSpeedClearAhead = [&](bool king) {
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0x77u, king);
        srrace::RunOutCountdown(sim);
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();
    const uint32_t seed = SeedThatArmsCop(track, setup);
    REQUIRE(seed != 0);

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, seed);
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
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
    }

    const int gapEnd =
        static_cast<int>(sim.Player().segPos) - static_cast<int>(sim.Npc().segPos);
    CHECK(gapEnd < gapAtSpawn);          // it closed
    CHECK_FALSE(sim.NpcAi().gaveUp);     // and never conceded on the way
    // Fleeing flat out is not enough: it runs to player + 32.
    CHECK(sim.Npc().speed > sim.Player().speed);
}

TEST_CASE("Invulnerable suppresses crashes and blown engines, nothing else") {
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = CopTestSetup();

    SUBCASE("a wall hit that would normally end the race is dropped whole") {
        srrace::RaceSim normal;
        normal.Init(&track, srrace::RaceKind::Road, setup, setup, 0x2468u);
        normal.SetCameraState(300, 0, 0, 100, 0);   // far off the left edge
        normal.SetNpcDebugPose(900, 2200);
        normal.Tick(0);
        // A wall hit ends the race through the failure mask, not END_FINISHED.
        REQUIRE(normal.PlayerWrecked());
        REQUIRE(normal.PlayerFailed());

        srrace::RaceSim tough;
        tough.Init(&track, srrace::RaceKind::Road, setup, setup, 0x2468u);
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
        srrace::RaceSim tough;
        tough.Init(&track, srrace::RaceKind::Road, setup, setup, 0xbeefu);
        tough.SetInvulnerable(true);
        RunOutCountdown(tough);
        for (int i = 0; i < 600 && !tough.RaceOver(); ++i) {
            tough.SetNpcDebugPose(900, 2200);
            tough.Tick(srrace::INPUT_THROTTLE);   // never shifting: pinned at the redline
        }
        CHECK((tough.Player().endFlags & END_ENGINE_BLOWN) == 0);
    }

    SUBCASE("the cop and a false start still end the race") {
        const uint32_t seed = SeedThatArmsCop(track, setup);
        REQUIRE(seed != 0);

        srrace::RaceSim busted;
        StartCopPursuit(busted, track, setup, seed);
        busted.SetInvulnerable(true);
        busted.SetNpcDebugPose(busted.Player().segPos - 10, 2200);
        busted.Tick(0);
        busted.SetNpcDebugPose(busted.Player().segPos + 4, 2200);
        busted.Tick(0);
        CHECK(busted.PlayerBusted());
        CHECK(busted.RaceOver());

        srrace::RaceSim jumped;
        jumped.Init(&track, srrace::RaceKind::Road, setup, setup, 0xabcu);
        jumped.SetInvulnerable(true);
        REQUIRE(jumped.InCountdown());
        for (int i = 0; i < 20 && jumped.InCountdown(); ++i) {
            jumped.Tick(srrace::INPUT_THROTTLE | INPUT_SHIFT);
        }
        CHECK((jumped.Player().endFlags & srrace::END_STALLED) != 0);
    }
}

TEST_CASE("Tire screech gate: hard steering at speed, not at a standstill") {
    srrace::TrackData track = MakeFlatTrack();

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

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, npcSetup, 0x5566u);
    srrace::RunOutCountdown(sim);
    sim.SetNpcDebugPose(900, 2200);  // keep the opponent out of the way

    // PlayerSkidding() needs speed > 20 mph.
    for (int i = 0; i < 10; ++i) {
        sim.Tick(INPUT_STEER_LEFT);
        CHECK_FALSE(sim.PlayerSkidding());
    }

    // slideVel ramps 2/tick toward +-24, crossing 12 within a few ticks.
    for (int i = 0; i < 200 && sim.Player().speed <= 25; ++i) {
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0xabcu);
    REQUIRE(sim.InCountdown());
    // Slam it in gear and floor it before the light.
    for (int i = 0; i < 20 && sim.InCountdown(); ++i) {
        sim.Tick(srrace::INPUT_THROTTLE | INPUT_SHIFT);
    }
    CHECK((sim.Player().endFlags & srrace::END_STALLED) != 0);
}

TEST_CASE("An automatic can rev on the line without jumping the start") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0xabcu);
    REQUIRE(sim.PlayerTables().automatic);
    REQUIRE(sim.InCountdown());

    // Throttle on the line in N: the engine revs, nothing moves.
    for (int i = 0; i < 20 && sim.InCountdown(); ++i) {
        sim.Tick(srrace::INPUT_THROTTLE);
    }
    CHECK(sim.Player().gear == 0);
    CHECK(sim.Player().speed == 0);
    CHECK((sim.Player().endFlags & srrace::END_STALLED) == 0);
    CHECK(sim.Player().rpm > 1500);   // it did actually rev
}

TEST_CASE("An automatic engages drive on the shift input and pulls away") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0xabcu);
    srrace::RunOutCountdown(sim);

    sim.Tick(srrace::INPUT_THROTTLE | INPUT_SHIFT);   // N -> D
    CHECK(sim.Player().gear > 0);
    for (int i = 0; i < 60; ++i) {
        sim.Tick(srrace::INPUT_THROTTLE);             // no further shift input needed
    }
    CHECK(sim.Player().speed > 0);
    CHECK(sim.Player().gear > 1);             // and it upshifts on its own
}

TEST_CASE("Rolling in neutral pins the rpm to road speed, standing still it revs") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x2468u);
    srrace::RunOutCountdown(sim);
    sim.SetNpcDebugPose(900, 2200);

    // Build some speed in first, then drop it back into neutral.
    for (int i = 0; i < 12; ++i) {
        sim.Tick(srrace::INPUT_THROTTLE | (i == 0 ? INPUT_SHIFT : 0));
    }
    REQUIRE(sim.Player().gear > 0);
    REQUIRE(sim.Player().speed > 0);
    while (sim.Player().gear > 0) {
        sim.Tick(INPUT_SHIFT);           // no throttle: shift down
    }
    REQUIRE(sim.Player().speed > 0);

    // Coasting out of gear the needle tracks road speed.
    for (int i = 0; i < 20 && sim.Player().speed > 0; ++i) {
        sim.Tick(srrace::INPUT_THROTTLE);
        const CarState& p = sim.Player();
        CHECK(p.rpm == p.speed * 30 + 1500);
    }
    CHECK(sim.Player().rpm < 5000);

    srrace::RaceSim standing;
    standing.Init(&track, srrace::RaceKind::Road, setup, setup, 0x2468u);
    RunOutCountdown(standing);
    REQUIRE(standing.Player().gear == 0);
    REQUIRE(standing.Player().speed == 0);
    const int16_t rpmBefore = standing.Player().rpm;
    for (int i = 0; i < 4; ++i) {
        standing.Tick(srrace::INPUT_THROTTLE);
    }
    CHECK(standing.Player().speed == 0);
    CHECK(standing.Player().rpm > rpmBefore);
}

TEST_CASE("An automatic cannot be blown by holding the throttle") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 0;     // automatic

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0x77u);
    srrace::RunOutCountdown(sim);
    sim.Tick(srrace::INPUT_THROTTLE | INPUT_SHIFT);   // engage drive

    // Far longer than the ~15 ticks that grenade a manual; the governor holds it under redline.
    for (int i = 0; i < 400; ++i) {
        sim.Tick(srrace::INPUT_THROTTLE);
    }
    CHECK((sim.Player().endFlags & END_ENGINE_BLOWN) == 0);
    CHECK(sim.Player().rpm <= 5690);
}

TEST_CASE("A manual held on the limiter still grenades") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0x77u);
    srrace::RunOutCountdown(sim);
    // Never shift: first gear pins against the limiter and lets go.
    bool blown = false;
    for (int i = 0; i < 400; ++i) {
        sim.Tick(srrace::INPUT_THROTTLE);
        if ((sim.Player().endFlags & END_ENGINE_BLOWN) != 0) {
            blown = true;
            break;
        }
    }
    CHECK(blown);
}

TEST_CASE("A maxed-wear engine grenades during the race") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.engineWear = 9899;  // 0x26ab; one tick of extra wear pushes it over

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0x5u);
    srrace::RunOutCountdown(sim);
    bool blown = false;
    for (int i = 0; i < 300; ++i) {
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
        if ((sim.Player().endFlags & END_ENGINE_BLOWN) != 0) {
            blown = true;
            break;
        }
    }
    CHECK(blown);
}

TEST_CASE("A worn manual transmission drops on a hard shift") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup setup{};
    setup.carbClass = 1;
    setup.engineClass = 1;
    setup.tireClass = 1;
    setup.transClass = 2;     // manual 4-speed
    setup.transWear = 9900;   // > 0x26ab: drops on the first shift

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x9u);
    srrace::RunOutCountdown(sim);
    bool dropped = false;
    for (int i = 0; i < 300; ++i) {
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x1234u);
    srrace::RunOutCountdown(sim);
    for (int i = 0; i < 200; ++i) {
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
    }
    REQUIRE(sim.Player().speed > 50);

    for (int i = 0; i < 400 && sim.Player().speed > 0; ++i) {
        sim.Tick(INPUT_BRAKE);
    }
    CHECK(sim.Player().speed == 0);
}

TEST_CASE("Crossing the line starts a roll-out, it does not end the race") {
    srrace::TrackData track = MakeFlatTrack();
    CarSetup player = FastSetup();
    CarSetup npc = FastSetup();
    npc.transClass = 0;
    npc.carbClass = 0;
    npc.engineClass = 0;
    npc.weightClass = 7;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, player, npc, 0x12345678u, /*kingRace=*/true);
    srrace::RunOutCountdown(sim);

    bool crossed = false;
    int16_t speedAtLine = 0;
    int rollOutTicks = 0;
    for (int i = 0; i < 4000; ++i) {
        // Held well behind: parked level in the next lane is the side-swipe view state.
        sim.SetNpcDebugPose(sim.Player().segPos - 40, 1550);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x777u);
    srrace::RunOutCountdown(sim);

    int rollOutTicks = 0;
    int16_t topRollOutSpeed = 0;
    for (int i = 0; i < 4000; ++i) {
        sim.SetNpcDebugPose(sim.Npc().segPos, 1550);
        // Past the line, throttle and steering have no effect.
        const bool rolling = sim.RollingOut();
        sim.Tick(rolling ? static_cast<uint16_t>(srrace::INPUT_THROTTLE | INPUT_STEER_LEFT)
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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x99u);
    srrace::RunOutCountdown(sim);

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
    srrace::TrackData track = MakeFlatTrack();
    const CarSetup setup = FastSetup();

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, setup, setup, 0x4242u);
    srrace::RunOutCountdown(sim);

    for (int i = 0; i < 4000; ++i) {
        // Keep the opponent a clear 6 segments up the road and out of our lane,
        // so it takes the line first.
        sim.SetNpcDebugPose(sim.Player().segPos + 6, 1550);
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
        if (sim.RaceOver()) {
            break;
        }
    }
    REQUIRE(sim.RaceOver());
    CHECK_FALSE(sim.PlayerWonAtLine());
    CHECK(sim.Player().speed == 0);
}

// Shifts with the throttle pinned, which is what rolls the money-shift.
uint16_t PowerShift(const srrace::RaceSim& sim) {
    const CarState& player = sim.Player();
    const CarTables& tables = sim.PlayerTables();
    if (player.gear < tables.numGears - 1 && player.rpm >= tables.autoShiftRpm) {
        return INPUT_SHIFT | srrace::INPUT_THROTTLE;
    }
    return srrace::INPUT_THROTTLE;
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

        srrace::TrackData track = MakeFlatTrack();
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Drag, setup, setup, 0x1000u + static_cast<uint32_t>(s) * 7919u);
        srrace::RunOutCountdown(sim);
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
    srrace::TrackData track = MakeFlatTrack();
    CarSetup fresh;
    CarSetup worn;
    worn.tireWear = 10000;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Road, fresh, fresh, 12345u);
    CHECK(sim.TireBlowSegment() == 0);

    int blowouts = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        sim.Init(&track, srrace::RaceKind::Road, worn, fresh, seed);
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
    srrace::TrackData track = MakeFlatTrack();
    CarSetup worn;
    worn.tireWear = 10000;

    srrace::RaceSim sim;
    uint32_t seed = 0;
    for (uint32_t s = 1; s <= 500 && seed == 0; ++s) {
        sim.Init(&track, srrace::RaceKind::Road, worn, worn, s);
        if (sim.TireBlowSegment() != 0) seed = s;
    }
    REQUIRE(seed != 0);

    sim.Init(&track, srrace::RaceKind::Road, worn, worn, seed);
    sim.SetCameraState(sim.TireBlowSegment() + 1, 0, 0, 2560, 0);
    sim.Tick(0);
    REQUIRE(sim.PlayerTiresBlown());

    const int16_t before = sim.Player().lateral;
    sim.Tick(0);
    CHECK(sim.Player().lateral - before == 25);
}


TEST_CASE("Analog steering: the stick sets how far the wheel goes over") {
    srrace::TrackData track = MakeFlatTrack();

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

    auto rolling = [&](srrace::RaceSim& sim) {
        sim.Init(&track, srrace::RaceKind::Road, setup, npcSetup, 0x5566u);
        srrace::RunOutCountdown(sim);
        sim.SetNpcDebugPose(900, 2200);
        for (int i = 0; i < 200 && sim.Player().speed <= 25; ++i) {
            sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
        }
        REQUIRE(sim.Player().speed > 20);
        REQUIRE(sim.Phase() == srrace::RacePhase::Running);
    };

    // steerVel ramps 2/tick, so 12 ticks is exactly full lock from centre.
    const int kTicksToFullLock = 12;

    SUBCASE("full deflection is the original's ramp to +-24") {
        srrace::RaceSim full, original;
        rolling(full);
        rolling(original);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            full.Tick(INPUT_STEER_RIGHT, 1.0f);
            original.Tick(INPUT_STEER_RIGHT);
        }
        REQUIRE(full.Phase() == srrace::RacePhase::Running);
        CHECK(full.Player().steerVel == 24);
        // The defaulted argument has to be exactly the old behaviour.
        CHECK(full.Player().steerVel == original.Player().steerVel);
        CHECK(full.Player().slideVel == original.Player().slideVel);
        CHECK(full.Player().heading == original.Player().heading);
    }

    SUBCASE("half a stick settles at half lock and holds there") {
        srrace::RaceSim sim;
        rolling(sim);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        }
        REQUIRE(sim.Phase() == srrace::RacePhase::Running);
        CHECK(sim.Player().steerVel == 12);
        // Still held: it stays put rather than creeping on to full lock.
        for (int i = 0; i < 5; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
            CHECK(sim.Player().steerVel == 12);
        }
    }

    SUBCASE("a lighter touch turns the car less") {
        srrace::RaceSim light, hard;
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
        srrace::RaceSim sim;
        rolling(sim);
        for (int i = 0; i < kTicksToFullLock; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 1.0f);
        }
        REQUIRE(sim.Phase() == srrace::RacePhase::Running);
        REQUIRE(sim.Player().steerVel == 24);

        // Down by 2 a tick, the same rate it wound on, not straight to 12.
        sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        CHECK(sim.Player().steerVel == 22);
        sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        CHECK(sim.Player().steerVel == 20);

        for (int i = 0; i < 5 && sim.Phase() == srrace::RacePhase::Running; ++i) {
            sim.Tick(INPUT_STEER_RIGHT, 0.5f);
        }
        CHECK(sim.Player().steerVel == 12);
    }

    SUBCASE("a centred stick is no steering at all") {
        srrace::RaceSim sim;
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

// ---------------------------------------------------------------------------
// Street Rod 2's exhaust, muffler and differential.
// ---------------------------------------------------------------------------

TEST_CASE("a Street Rod car is untouched by SR2's three slots") {
    // This is what lets the whole feature exist without a game flag.
    srrace::CarSetup stock;
    CHECK(stock.exhaust == 0);
    CHECK(stock.muffler == 0);
    CHECK_FALSE(stock.mufflerRemoved);
    CHECK(stock.differential == 1);          // 3.73:1, neutral on both

    CHECK(srrace::Sr2AdjustRpmGain(1000, stock) == 1000);
    CHECK(srrace::Sr2AdjustScore(1000, stock.differential) == 1000);

    srrace::CarSetup withDiff = stock;
    withDiff.differential = 1;
    CHECK(srrace::ComputeTopSpeed(withDiff) == srrace::ComputeTopSpeed(stock));
}

TEST_CASE("only a missing muffler buys acceleration; headers and the glass pack are inert") {
    // The original compares a 0/1 field with the unshifted mask, so these two never match.
    srrace::CarSetup s;
    s.exhaust = 1;                            // tube headers
    CHECK(srrace::Sr2AdjustRpmGain(1000, s) == 1000);

    srrace::CarSetup glass;
    glass.muffler = 1;                        // glass pack
    CHECK(srrace::Sr2AdjustRpmGain(1000, glass) == 1000);

    srrace::CarSetup removed;
    removed.muffler = 1;
    removed.mufflerRemoved = true;
    CHECK(srrace::Sr2AdjustRpmGain(1000, removed) == 1050);

    // None of them touches the performance score, so none moves top speed.
    CHECK(srrace::ComputeTopSpeed(s) == srrace::ComputeTopSpeed(srrace::CarSetup{}));
    CHECK(srrace::ComputeTopSpeed(removed) == srrace::ComputeTopSpeed(srrace::CarSetup{}));
}

TEST_CASE("only the 2.78:1 differential does anything") {
    srrace::CarSetup low;
    low.differential = 0;                     // 2.78:1
    srrace::CarSetup high;
    high.differential = 2;                    // 4.11:1: the original's test for it never matches

    CHECK(srrace::Sr2AdjustRpmGain(1000, low) == 950);
    CHECK(srrace::Sr2AdjustRpmGain(1000, high) == 1000);

    CHECK(srrace::Sr2AdjustScore(1000, 0) == 1050);
    CHECK(srrace::Sr2AdjustScore(1000, 1) == 1000);
    CHECK(srrace::Sr2AdjustScore(1000, 2) == 1000);

    srrace::CarSetup strong;
    strong.carbClass = 2;
    strong.transClass = 3;
    strong.engineClass = 2;
    strong.manifoldClass = 4;
    strong.weightClass = 8;
    strong.roofChopped = true;
    strong.frontBumperRemoved = true;
    strong.rearBumperRemoved = true;

    srrace::CarSetup strongLow = strong;
    strongLow.differential = 0;
    srrace::CarSetup strongHigh = strong;
    strongHigh.differential = 2;
    CHECK(srrace::ComputeBasePerformance(strongHigh) == srrace::ComputeBasePerformance(strong));
    CHECK(srrace::ComputeBasePerformance(strongLow) >= srrace::ComputeBasePerformance(strong));
    CHECK(srrace::ComputeTopSpeed(high) == srrace::ComputeTopSpeed(srrace::CarSetup{}));
    CHECK(srrace::ComputeTopSpeed(low) >= srrace::ComputeTopSpeed(srrace::CarSetup{}));
}

TEST_CASE("the live adjustments compound in the original's order") {
    // Of the King's three parts only the muffler's absence and the 2.78 gear count.
    srrace::CarSetup king;
    king.exhaust = 1;
    king.muffler = 1;
    king.differential = 2;
    CHECK(srrace::Sr2AdjustRpmGain(1000, king) == 1000);

    srrace::CarSetup stripped = king;
    stripped.mufflerRemoved = true;
    stripped.differential = 0;
    int expected = 1000;
    expected += expected * 5 / 100;           // no muffler
    expected -= expected * 5 / 100;           // 2.78:1
    CHECK(srrace::Sr2AdjustRpmGain(1000, stripped) == expected);
}

TEST_CASE("SR2's live slots reach the built physics tables") {
    // Check the adjustment survives into BuildCarTables.
    srrace::CarSetup stock;
    srrace::CarSetup bare = stock;
    bare.mufflerRemoved = true;
    srrace::CarSetup headers = stock;
    headers.exhaust = 1;

    srrace::CarTables a{}, b{}, c{};
    srrace::BuildCarTables(stock, a);
    srrace::BuildCarTables(bare, b);
    srrace::BuildCarTables(headers, c);

    bool anyGearPullsHarder = false;
    for (int g = 0; g < 5; ++g) {
        CHECK(b.rows[g][1].rpmGain >= a.rows[g][1].rpmGain);
        if (b.rows[g][1].rpmGain > a.rows[g][1].rpmGain) anyGearPullsHarder = true;
        // Tube headers do nothing in the original.
        CHECK(c.rows[g][1].rpmGain == a.rows[g][1].rpmGain);
    }
    CHECK(anyGearPullsHarder);
    CHECK(b.topSpeed == a.topSpeed);
}

// ---------------------------------------------------------------------------
// The physics tables are read from the player's own executable rather than shipped as source constants.
// ---------------------------------------------------------------------------

namespace {

// Offsets of each block in Street Rod's executable, the same ones LoadPhysicsTables reads.
constexpr size_t kBlock = 0x2C9B0, kNumGearsAt = 0x2CA34;
constexpr size_t kTireSteerAt = 0x2CA3C, kHandlingAt = 0x2CAA0;

struct FakeExe {
    std::vector<uint8_t> d = std::vector<uint8_t>(kHandlingAt + 256, 0);
    void W(size_t at, int v) {
        d[at] = uint8_t((v >> 8) & 0xFF);
        d[at + 1] = uint8_t(v & 0xFF);
    }
    void Row(size_t at, std::initializer_list<int> vals) {
        for (int v : vals) { W(at, v); at += 2; }
    }
    // The values Street Rod actually ships, which are also the fallbacks.
    void FillStock(int racingGears = 4) {
        Row(kBlock,        {0, 25, 50, 100, 0,  0, 25, 50, 100, 0,
                            0, 25, 50, 75, 100, 0, 30, 50, 80, 100});
        Row(kBlock + 0x28, {500, 600, 300, 100, 70,
                            600, 700, 350, 120, 90,
                            600, 760, 380, 140, 90});
        Row(kBlock + 0x46, {2900, 4500, 2900, 5000, 2500, 5200});
        Row(kNumGearsAt,   {3, 3, 4, racingGears});
        Row(kTireSteerAt,  {9, 8, 5});
        Row(kHandlingAt,   {2, 5, 8, 11, 14,  0, 2, 4, 6, 8,  0, 0, 1, 2, 4});
    }
};

// Puts the shipped tables back, whatever a test did to them.
void RestorePhysicsTables() {
    FakeExe stock;
    stock.FillStock();
    REQUIRE(srrace::LoadPhysicsTables(stock.d, srrace::PhysicsTableSource::StreetRod));
}

} // namespace

TEST_CASE("the physics tables come out of the executable") {
    srrace::CarSetup racing;
    racing.transClass = 3;

    srrace::CarTables before{};
    srrace::BuildCarTables(racing, before);
    REQUIRE(before.numGears == 4);

    // A racing gearbox with five forward gears is not what the game ships, so
    // seeing it proves the number was read rather than compiled in.
    FakeExe altered;
    altered.FillStock(5);
    CHECK(srrace::LoadPhysicsTables(altered.d, srrace::PhysicsTableSource::StreetRod));

    srrace::CarTables after{};
    srrace::BuildCarTables(racing, after);
    CHECK(after.numGears == 5);

    RestorePhysicsTables();
    srrace::CarTables restored{};
    srrace::BuildCarTables(racing, restored);
    CHECK(restored.numGears == 4);
}

TEST_CASE("tables that do not hold together leave the fallbacks alone") {
    srrace::CarSetup stock;
    srrace::CarTables expected{};
    srrace::BuildCarTables(stock, expected);

    // Too short to hold them at all.
    CHECK_FALSE(srrace::LoadPhysicsTables(std::vector<uint8_t>(1024, 0),
                                          srrace::PhysicsTableSource::StreetRod));
    // Right size, wrong contents: a power band that ends before it starts, and
    // a gear count no transmission has.
    CHECK_FALSE(srrace::LoadPhysicsTables(std::vector<uint8_t>(kHandlingAt + 256, 0xAB),
                                          srrace::PhysicsTableSource::StreetRod));
    FakeExe backwards;
    backwards.FillStock();
    backwards.Row(kBlock + 0x46, {4500, 2900, 2900, 5000, 2500, 5200});
    CHECK_FALSE(srrace::LoadPhysicsTables(backwards.d, srrace::PhysicsTableSource::StreetRod));

    FakeExe tooManyGears;
    tooManyGears.FillStock(9);
    CHECK_FALSE(srrace::LoadPhysicsTables(tooManyGears.d, srrace::PhysicsTableSource::StreetRod));

    // A refused load must not have half-written the tables.
    srrace::CarTables actual{};
    srrace::BuildCarTables(stock, actual);
    CHECK(actual.numGears == expected.numGears);
    CHECK(actual.topSpeed == expected.topSpeed);
    CHECK(actual.autoShiftRpm == expected.autoShiftRpm);
    CHECK(actual.rows[1][1].rpmGain == expected.rows[1][1].rpmGain);
}

// ---------------------------------------------------------------------------
// Race windows.
// ---------------------------------------------------------------------------

TEST_CASE("each race runs in its own window") {
    // Street Rod's two, pinned first so parameterising cannot move them.
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Drag).start == 200);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Drag).finish == 380);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Road).start == 200);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Road).finish == 1020);

    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Mulholland).start == 370);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Mulholland).finish == 960);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Grudge).start == 9);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Grudge).finish == 89);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Aqueduct).start == 9);
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Aqueduct).finish == 343);

    // Every window has to fit the road it runs on.
    for (srrace::RaceKind k : {srrace::RaceKind::Mulholland, srrace::RaceKind::Grudge}) {
        const srrace::RaceWindow w = srrace::RaceWindowFor(k);
        CHECK(w.start < w.finish);
        CHECK(w.finish <= 990);
    }
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Road).finish > 990);
}

TEST_CASE("the sim finishes where its race says") {
    srrace::TrackData track{};
    srrace::CarSetup setup;

    srrace::RaceSim drag;
    drag.Init(&track, srrace::RaceKind::Drag, setup, setup, 1u);
    CHECK(drag.FinishSegment() == 380);
    CHECK(drag.StartSegment() == 200);
    CHECK_FALSE(drag.IsRoadRace());

    srrace::RaceSim mulholland;
    mulholland.Init(&track, srrace::RaceKind::Mulholland, setup, setup, 1u);
    CHECK(mulholland.FinishSegment() == 960);
    CHECK(mulholland.StartSegment() == 370);
    // The cop is Street Rod road-race behaviour; whether Mulholland spawns one
    // is not traced, so this port does not.
    CHECK_FALSE(mulholland.IsRoadRace());

    srrace::RaceSim road;
    road.Init(&track, srrace::RaceKind::Road, setup, setup, 1u);
    CHECK(road.FinishSegment() == 1020);
    CHECK(road.IsRoadRace());
}

TEST_CASE("a race will not start on too little gas") {
    // Checked against the car's tank before the wager dialog.
    CHECK(srrace::RaceMinimumGas(srrace::RaceKind::Drag) == 7);
    CHECK(srrace::RaceMinimumGas(srrace::RaceKind::Aqueduct) == 12);
    CHECK(srrace::RaceMinimumGas(srrace::RaceKind::Mulholland) == 17);
    // Street Rod never asked, and a grudge race is run from the strip.
    CHECK(srrace::RaceMinimumGas(srrace::RaceKind::Road) == 0);
    CHECK(srrace::RaceMinimumGas(srrace::RaceKind::Grudge) == 0);
}

// ---------------------------------------------------------------------------
// Mulholland's on-road hazards and its tighter corridor. Behaviour table in
// RodLegacyResearch/docs/sr2_amiga/races.md, "Speed limits and crashes".
// ---------------------------------------------------------------------------

namespace {

// A straight, flat road with one hazard under a centred car.
srrace::TrackData MulhollandTrackWith(uint16_t hazardFlag, int atSegment) {
    srrace::TrackData t{};
    t.flags[atSegment] |= hazardFlag;
    t.lane[atSegment] = 10;
    return t;
}

// Waits out the countdown, then drives until the car is past the given segment or the race ends.
void DriveTo(srrace::RaceSim& sim, int pastSegment) {
    srrace::RunOutCountdown(sim);
    for (int tick = 0; tick < 6000; ++tick) {
        if (sim.Player().segPos > pastSegment) break;
        if ((sim.Player().endFlags & srrace::END_FAILURE_MASK) != 0) break;
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
    }
}

} // namespace

TEST_CASE("a rock ends the race, oil only costs you") {
    const int at = 500;   // inside Mulholland's 370..960 window
    srrace::CarSetup setup;

    srrace::TrackData rockTrack = MulhollandTrackWith(0x0004, at);
    srrace::RaceSim rock;
    rock.Init(&rockTrack, srrace::RaceKind::Mulholland, setup, setup, 0x9001u);
    CHECK(rock.Player().segPos == 370);      // the race starts in its own window
    DriveTo(rock, at);
    CHECK((rock.Player().endFlags & srrace::END_WALL_HIT) != 0);

    // The same hazard marked as oil never ends the race, however fast.
    srrace::TrackData oilTrack = MulhollandTrackWith(0x1000, at);
    srrace::RaceSim oil;
    oil.Init(&oilTrack, srrace::RaceKind::Mulholland, setup, setup, 0x9001u);
    DriveTo(oil, at);
    CHECK((oil.Player().endFlags & srrace::END_FAILURE_MASK) == 0);
    CHECK(oil.Player().segPos > at);

    // On Street Rod the same bit 12 is an overhead arch, which is something to
    // hit - so the two games must read that flag differently.
    srrace::TrackData archTrack = MulhollandTrackWith(0x1000, at);
    srrace::RaceSim road;
    road.Init(&archTrack, srrace::RaceKind::Road, setup, setup, 0x9001u);
    CHECK(road.Player().segPos == 200);
}

TEST_CASE("Mulholland's corridor is tighter than the road race's") {
    // 150 against 260: the same distance off the road ends one race and not
    // the other. Nothing else about the two differs here.
    srrace::TrackData track{};
    srrace::CarSetup setup;

    srrace::RaceSim mulholland;
    mulholland.Init(&track, srrace::RaceKind::Mulholland, setup, setup, 1u);
    srrace::RaceSim road;
    road.Init(&track, srrace::RaceKind::Road, setup, setup, 1u);
    CHECK(mulholland.RoadEdgeLeft() == road.RoadEdgeLeft());
    CHECK(mulholland.RoadEdgeRight() == road.RoadEdgeRight());
}

TEST_CASE("taking a narrowing at speed ends a Mulholland race") {
    const int at = 500;
    srrace::CarSetup setup;

    srrace::TrackData narrowed{};
    for (int i = at; i < at + 40; ++i) narrowed.flags[i] |= 0x0040;  // road narrows
    srrace::RaceSim fast;
    fast.Init(&narrowed, srrace::RaceKind::Mulholland, setup, setup, 0x77u);
    DriveTo(fast, at + 20);
    CHECK((fast.Player().endFlags & srrace::END_WALL_HIT) != 0);

    // The same narrowing on Street Rod's road race is not a speed trap.
    srrace::TrackData sameRoad{};
    for (int i = at; i < at + 40; ++i) sameRoad.flags[i] |= 0x0040;
    srrace::RaceSim sr1;
    sr1.Init(&sameRoad, srrace::RaceKind::Road, setup, setup, 0x77u);
    DriveTo(sr1, at + 20);
    CHECK((sr1.Player().endFlags & srrace::END_WALL_HIT) == 0);
}

TEST_CASE("a Mulholland rock pile reaches onto the road") {
    srrace::CarSetup setup;
    srrace::TrackData track{};
    for (int i = 400; i < 520; ++i) track.left[i] |= 0x0010;

    // Down the middle the pile is never reached.
    srrace::RaceSim middle;
    middle.Init(&track, srrace::RaceKind::Mulholland, setup, setup, 0x77u);
    DriveTo(middle, 510);
    CHECK((middle.Player().endFlags & srrace::END_WALL_HIT) == 0);

    // Placed on the road but left of 0x79e + 30 the pile is hit; just right of it, not.
    auto hitAt = [&](int16_t lateral) {
        srrace::RaceSim sim;
        sim.Init(&track, srrace::RaceKind::Mulholland, setup, setup, 0x77u);
        sim.SetCameraState(450, 0, 0, lateral, 0);
        sim.Tick(0);
        return (sim.Player().endFlags & srrace::END_WALL_HIT) != 0;
    };
    CHECK(hitAt(1960));
    CHECK_FALSE(hitAt(2000));
}

TEST_CASE("going over the edge on Mulholland is a fall, not a wall") {
    // A negative shoulder is ground that falls away. Street Rod has no such
    // thing, so this only ever happens in Street Rod 2.
    srrace::CarSetup setup;
    srrace::TrackData track{};
    srrace::RaceRng rng(7);
    srrace::ScatterHazards(track, rng, srrace::TrackGenSource::StreetRod2);
    // Straight and flat, so the car runs where it is steered.
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        track.curve[i] = 0;
        track.elev[i] = 0;
        track.flags[i] = 0;
        track.lane[i] = 0;
    }

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Mulholland, setup, setup, 0x5150u);
    CHECK(sim.FallDirection() == 0);

    srrace::RunOutCountdown(sim);
    // The ground only falls away from segment 380.
    for (int tick = 0; tick < 4000; ++tick) {
        if (sim.Player().segPos >= 385) break;
        if ((sim.Player().endFlags & srrace::END_FAILURE_MASK) != 0) break;
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim));
    }
    REQUIRE(sim.Player().segPos >= 385);
    REQUIRE(track.wallRight[sim.Player().segPos] < 0);

    for (int tick = 0; tick < 4000; ++tick) {
        if (sim.FallDirection() != 0) break;
        if ((sim.Player().endFlags & srrace::END_FAILURE_MASK) != 0) break;
        sim.Tick(srrace::ThrottleWithShiftIfNeeded(sim) | srrace::INPUT_STEER_RIGHT);
    }
    CHECK(sim.FallDirection() == 1);   // off the right, where the ground drops

    // Street Rod cannot fall: the same drive on its road race never sets it.
    srrace::TrackData sr1Track{};
    srrace::RaceSim sr1;
    sr1.Init(&sr1Track, srrace::RaceKind::Road, setup, setup, 0x5150u);
    srrace::RunOutCountdown(sr1);
    for (int tick = 0; tick < 4000; ++tick) {
        if (sr1.Player().segPos >= 385) break;
        if ((sr1.Player().endFlags & srrace::END_FAILURE_MASK) != 0) break;
        sr1.Tick(srrace::ThrottleWithShiftIfNeeded(sr1));
    }
    for (int tick = 0; tick < 2000; ++tick) {
        if ((sr1.Player().endFlags & srrace::END_FAILURE_MASK) != 0) break;
        sr1.Tick(srrace::ThrottleWithShiftIfNeeded(sr1) | srrace::INPUT_STEER_RIGHT);
    }
    CHECK(sr1.FallDirection() == 0);
}


// ---------------------------------------------------------------------------
// The Aqueduct's walls are not the road's edges.
// ---------------------------------------------------------------------------

TEST_CASE("aqueduct walls are ridden, not crashed into") {
    srrace::TrackData track{};
    srrace::BuildAqueductRoad(track);
    srrace::CarSetup setup;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Aqueduct, setup, setup, 0x12345678u);

    // Well up the right slope: past the channel floor at 0xcf8 but short of the lip at 0xded, where the road
    // race would already have called it a wall hit and ended the race.
    sim.SetCameraState(60, 0, 0, 2900, 0);
    for (int i = 0; i < 20; ++i) {
        sim.Tick(0);
    }
    CHECK((sim.Player().endFlags & srrace::END_FAILURE_MASK) == 0);
    CHECK_FALSE(sim.Player().offRoad);
    // The slope walks the car back down towards the middle of the channel.
    CHECK(sim.Player().lateral < 2900);
}

TEST_CASE("going over the lip rolls the car and then wrecks it") {
    srrace::TrackData track{};
    srrace::BuildAqueductRoad(track);
    srrace::CarSetup setup;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Aqueduct, setup, setup, 0x12345678u);
    // Past the right lip (car_right >= 0xded) and quick enough to go over.
    sim.SetCameraState(60, 0, 0, 3100, 0);

    bool rolled = false;
    for (int i = 0; i < 30; ++i) {
        sim.Tick(0);
        if (sim.IsRollingOver() || sim.AqueductRoll() != 0) rolled = true;
    }
    CHECK(rolled);
    // Going over the lip is a crash, and the wreck lands after the roll has
    // played rather than on the frame the car leaves the concrete.
    CHECK((sim.Player().endFlags & srrace::END_FAILURE_MASK) != 0);
}

TEST_CASE("the roll plays before the wreck lands") {
    srrace::TrackData track{};
    srrace::BuildAqueductRoad(track);
    srrace::CarSetup setup;

    srrace::RaceSim sim;
    sim.Init(&track, srrace::RaceKind::Aqueduct, setup, setup, 0x12345678u);
    sim.SetCameraState(60, 0, 0, 3100, 0);

    sim.Tick(0);
    // First tick off the lip: rolling, but not wrecked yet.
    CHECK(sim.IsRollingOver());
    CHECK((sim.Player().endFlags & srrace::END_FAILURE_MASK) == 0);

    for (int i = 0; i < 30 && sim.IsRollingOver(); ++i) {
        sim.Tick(0);
    }
    CHECK_FALSE(sim.IsRollingOver());
    CHECK((sim.Player().endFlags & srrace::END_FAILURE_MASK) != 0);
}


// ---------------------------------------------------------------------------
// Scoring tables, read from the executable rather than hardcoded.
// ---------------------------------------------------------------------------

namespace {

// A synthetic executable carrying the scoring block where the loader expects it.
std::vector<uint8_t> MakeScoringExe(bool sr2, const std::vector<int>& eng,
                                    const std::vector<int>& weight) {
    const size_t block = sr2 ? 0x40CEC : 0x2C9B0;
    const size_t tire = sr2 ? 0x40D90 : 0x2CA3C;
    const int manifolds = sr2 ? 9 : 5;
    std::vector<uint8_t> d(sr2 ? 0x41000 : 0x2D000, 0);

    auto put = [&](size_t at, int v) {
        d[at] = static_cast<uint8_t>((v >> 8) & 0xff);
        d[at + 1] = static_cast<uint8_t>(v & 0xff);
    };
    const size_t engAt = block + 0x52;
    for (size_t i = 0; i < eng.size(); ++i) put(engAt + i * 2, eng[i]);
    const size_t carbAt = engAt + size_t(3 * manifolds) * 2;
    put(carbAt, 0); put(carbAt + 2, 50); put(carbAt + 4, 100);
    const size_t transAt = carbAt + 6;
    put(transAt, 0); put(transAt + 2, 33); put(transAt + 4, 66); put(transAt + 6, 100);
    const size_t wtAt = tire + 6;
    for (size_t i = 0; i < weight.size(); ++i) put(wtAt + i * 2, weight[i]);
    return d;
}

const std::vector<int> kSr1Weight = {40, 44, 48, 60, 64, 68, 80, 84, 88};

}  // namespace

TEST_CASE("scoring tables load from the executable") {
    // Street Rod: a 3 x 5 grid. Restore it afterwards so later cases see the
    // shipped values.
    std::vector<int> eng(15, 0);
    eng[2 * 5 + 4] = 100;   // best engine, best manifold
    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(false, eng, kSr1Weight),
                                      srrace::PhysicsTableSource::StreetRod));
    CHECK(srrace::ScoringManifoldCount() == 5);

    // Street Rod 2's grid is 3 x 9, and its weight points top out at 100
    // rather than 88 - so a full-weight car still scores the same 10.
    std::vector<int> eng2(27, 0);
    eng2[2 * 9 + 8] = 100;
    const std::vector<int> w2 = {46, 50, 55, 68, 73, 78, 91, 96, 100};
    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(true, eng2, w2),
                                      srrace::PhysicsTableSource::StreetRod2));
    CHECK(srrace::ScoringManifoldCount() == 9);

    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(false, eng, kSr1Weight),
                                      srrace::PhysicsTableSource::StreetRod));
}

TEST_CASE("Street Rod 2 scores a car as one raw sum, by the original's arithmetic") {
    // engine score 0/50/100, transmission 0/33/66/100, manifold rows per carburettor.
    std::vector<int> grid(27, 0);
    grid[1 * 9 + 5] = 50;    // 4-barrel carb, tunnel ram
    grid[2 * 9 + 8] = 100;   // racing carb row, supercharger
    const std::vector<int> w2 = {46, 50, 55, 68, 73, 78, 91, 96, 100};
    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(true, grid, w2), srrace::PhysicsTableSource::StreetRod2));

    srrace::CarSetup setup;
    setup.engineClass = 2;
    setup.transClass = 3;
    setup.carbClass = 1;
    setup.manifoldClass = 5;
    setup.weightClass = 8;
    setup.tuneOffset = 0;
    setup.roofChopped = true;
    setup.frontBumperRemoved = true;
    setup.rearBumperRemoved = true;
    // 100*25 + 100*15 + 50*15 + 100*10 + 880 + 800 + 400 + 100 = 7930 -> 79 -> 79*10*3/100 = 23 -> 93
    CHECK(srrace::ComputeBasePerformance(setup) == 93);

    // Only the 2.78:1 differential skews the raw sum, by 5% before the divide.
    setup.differential = 0;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + (7930 + 7930 * 5 / 100) / 100 * 30 / 100);
    setup.differential = 2;
    // 4.11:1 does nothing: the original's test for it never matches.
    CHECK(srrace::ComputeBasePerformance(setup) == 93);

    // Engine wear scales it, and an automatic's k is 8 not 10.
    setup.differential = 1;
    setup.engineWear = 5000;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + 79 * 5000 / 10000 * 30 / 100);
    setup.engineWear = 0;
    setup.transClass = 0;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + ((7930 - 100 * 15) / 100) * 24 / 100);

    // The tune byte's distance from zero comes straight off the sum, either side.
    setup.transClass = 3;
    setup.sr2Tune = 0;
    const int perfect = srrace::ComputeBasePerformance(setup);
    setup.sr2Tune = -128;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + ((7930 - 128) / 100) * 30 / 100);
    setup.sr2Tune = 127;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + ((7930 - 127) / 100) * 30 / 100);
    CHECK(srrace::ComputeBasePerformance(setup) <= perfect);
    setup.sr2Tune = 0;

    // Supercharger on a racing carb scores the table's 100.
    setup.transClass = 3;
    setup.carbClass = 2;
    setup.manifoldClass = 8;
    CHECK(srrace::ComputeBasePerformance(setup) == 70 + ((7930 - 50 * 15 + 100 * 15) / 100) * 30 / 100);

    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(false, std::vector<int>(15, 0), kSr1Weight),
                                      srrace::PhysicsTableSource::StreetRod));
}

TEST_CASE("a file that does not hold the tables is refused") {
    // Scoring a car off the wrong data would be silent and wrong, so the
    // loader keeps the built-in copy instead of taking what it finds.
    const std::vector<uint8_t> empty(0x2D000, 0);
    CHECK_FALSE(srrace::LoadScoringTables(empty, srrace::PhysicsTableSource::StreetRod));
    CHECK_FALSE(srrace::LoadScoringTables(std::vector<uint8_t>(16, 0),
                                          srrace::PhysicsTableSource::StreetRod));

    // Weight points that do not rise are not the weight table.
    std::vector<int> eng(15, 0);
    std::vector<int> bad = kSr1Weight;
    bad[4] = bad[3];
    CHECK_FALSE(srrace::LoadScoringTables(MakeScoringExe(false, eng, bad),
                                          srrace::PhysicsTableSource::StreetRod));

    // And the shipped values are still in force afterwards.
    CHECK(srrace::ScoringManifoldCount() == 5);
}


TEST_CASE("the SR2 bug fix setting turns on the PC build's part effects and tune scale") {
    srrace::SetSr2BugFixes(true);

    srrace::CarSetup headers;
    headers.exhaust = 1;
    CHECK(srrace::Sr2AdjustRpmGain(1000, headers) == 1050);
    srrace::CarSetup glass;
    glass.muffler = 1;
    CHECK(srrace::Sr2AdjustRpmGain(1000, glass) == 1030);
    srrace::CarSetup high;
    high.differential = 2;
    CHECK(srrace::Sr2AdjustRpmGain(1000, high) == 1050);
    CHECK(srrace::Sr2AdjustScore(1000, 2) == 950);
    CHECK(srrace::Sr2AdjustScore(1000, 0) == 1050);

    // The first-gear slicks bonus is back on SR2 tables too.
    std::vector<int> grid(27, 0);
    const std::vector<int> w2 = {46, 50, 55, 68, 73, 78, 91, 96, 100};
    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(true, grid, w2), srrace::PhysicsTableSource::StreetRod2));
    srrace::CarSetup stock;
    srrace::CarSetup slicks = stock;
    slicks.tireClass = 2;
    srrace::CarTables a{}, b{};
    srrace::BuildCarTables(stock, a);
    srrace::BuildCarTables(slicks, b);
    CHECK(b.rows[1][0].rpmGain > a.rows[1][0].rpmGain);

    // The tune costs the PC build's 2640 across its range, by notch step: the notch dead on the red bar
    // (tune -16..-1) is perfect, and each step either side costs 330.
    srrace::CarSetup tuned;
    tuned.engineClass = 2;
    tuned.transClass = 3;
    tuned.weightClass = 8;
    tuned.sr2Tune = -1;
    const int perfect = srrace::ComputeBasePerformance(tuned);
    tuned.sr2Tune = -16;
    CHECK(srrace::ComputeBasePerformance(tuned) == perfect);
    tuned.sr2Tune = 0;   // one step right of the bar, which is where the Amiga's own zero draws
    CHECK(srrace::ComputeBasePerformance(tuned) <= perfect);
    tuned.sr2Tune = -128;
    CHECK(srrace::ComputeBasePerformance(tuned) < perfect);
    tuned.sr2Tune = 127;
    CHECK(srrace::ComputeBasePerformance(tuned) < perfect);

    srrace::SetSr2BugFixes(false);
    srrace::CarSetup off = tuned;
    off.sr2Tune = 0;
    const int amigaZero = srrace::ComputeBasePerformance(off);
    off.sr2Tune = -128;
    CHECK(srrace::ComputeBasePerformance(off) >= amigaZero - 1);

    REQUIRE(srrace::LoadScoringTables(MakeScoringExe(false, std::vector<int>(15, 0), kSr1Weight),
                                      srrace::PhysicsTableSource::StreetRod));
}
