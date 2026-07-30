#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

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
    setup.bumpersStripped = false;

    // pts = 0 + 0 + 0 + 0 + (40*10/88=4) = 4
    // base = 70 + 4 * 24 / 100 = 70 (integer division)
    CHECK(ComputeBasePerformance(setup) == 70);
}

TEST_CASE("ComputeBasePerformance maxed-ish setup matches formula") {
    CarSetup setup{};
    setup.carbClass = 2;
    setup.transClass = 3;
    setup.engineClass = 1;
    setup.manifoldClass = 3;
    setup.weightClass = 7;
    setup.tuneOffset = 0;
    setup.carbWear = 500;
    setup.bumpersStripped = true;

    static const int kCarbPts[3] = {0, 50, 100};
    static const int kTransPts[4] = {0, 33, 66, 100};
    static const int kEngByManifold[20] = {
        0, 0, 100, 0, 0,
        0, 50, 0, 100, 75,
        0, 0, 0, 50, 100,
        0, 50, 100, 0, 33
    };
    static const int kWeightPts[8] = {40, 44, 48, 60, 64, 68, 80, 84};

    int pts = 0;
    pts += (kCarbPts[setup.carbClass] * 25) / 100;
    pts += (kTransPts[setup.transClass] * 15) / 100;
    pts += (kEngByManifold[setup.engineClass * 5 + setup.manifoldClass] * 15) / 100;
    pts += ((8 - setup.tuneOffset) * 20) / 8;
    pts += (kWeightPts[setup.weightClass] * 10) / 88;
    pts += 9;
    const int expected = 70 + (((pts * (10000 - setup.carbWear)) / 10000) * 30) / 100;

    CHECK(ComputeBasePerformance(setup) == expected);
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
    CHECK(sim.PlayerWonAtLine());
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

    RaceSim sim;
    sim.Init(&track, 1, player, npc, 0x12345678u);
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

    // It ends only once the car has stopped, some way past the line.
    CHECK(sim.RaceOver());
    CHECK(sim.Player().speed == 0);
    CHECK(sim.Player().segPos > ROAD_FINISH_SEG);
    CHECK(rollOutTicks > 1);
    CHECK(sim.PlayerWonAtLine());
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
