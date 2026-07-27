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
        track.curve[i] = 0;
        track.elev[i] = 0;
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
        sim.Tick(ThrottleWithShiftIfNeeded(sim));

        const CarState& now = sim.Player();
        CHECK(now.speed >= 0);
        CHECK(now.speed <= 130);
        CHECK(now.rpm >= 1400);
        CHECK(now.rpm <= 5700);

        if ((now.endFlags & END_FINISHED) != 0) {
            finished = true;
            break;
        }
    }

    CHECK(finished);
    CHECK((sim.Player().endFlags & END_FINISHED) != 0);
}

TEST_CASE("Steering input counteracts curve drift") {
    TrackData track = MakeFlatTrack();
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

}  // namespace srrace
