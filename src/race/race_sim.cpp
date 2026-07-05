#include "race/race_sim.h"

#include <algorithm>
#include <cstdlib>

namespace srrace {

namespace {

template <typename T>
T ClampValue(T v, T lo, T hi) {
    return std::max(lo, std::min(v, hi));
}

int Sign(int value) {
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

int ClampTrackIndex(int seg) {
    return ClampValue(seg, 0, TRACK_SEGMENTS - 1);
}

}  // namespace

int ComputeBasePerformance(const CarSetup& setup) {
    static const int kCarbPts[3] = {0, 50, 100};
    static const int kTransPts[4] = {0, 33, 66, 100};
    static const int kEngByManifold[20] = {
        0, 0, 100, 0, 0,
        0, 50, 0, 100, 75,
        0, 0, 0, 50, 100,
        0, 50, 100, 0, 33
    };
    static const int kWeightPts[8] = {40, 44, 48, 60, 64, 68, 80, 84};

    const int carbClass = ClampValue(setup.carbClass, 0, 2);
    const int transClass = ClampValue(setup.transClass, 0, 3);
    const int engineClass = ClampValue(setup.engineClass, 0, 3);
    const int manifoldClass = ClampValue(setup.manifoldClass, 0, 4);
    const int weightClass = ClampValue(setup.weightClass, 0, 7);
    const int tuneOffset = ClampValue(setup.tuneOffset, 0, 8);
    const int carbWear = ClampValue(setup.carbWear, 0, 10000);

    int pts = 0;
    pts += (kCarbPts[carbClass] * 25) / 100;
    pts += (kTransPts[transClass] * 15) / 100;
    pts += (kEngByManifold[engineClass * 5 + manifoldClass] * 15) / 100;
    pts += ((8 - tuneOffset) * 20) / 8;
    pts += (kWeightPts[weightClass] * 10) / 88;
    if (setup.bumpersStripped) {
        pts += 9;
    }

    const bool automatic = (transClass == 0);
    const int transFactor = automatic ? 24 : 30;
    const int wearScaledPts = (pts * (10000 - carbWear)) / 10000;
    return 70 + (wearScaledPts * transFactor) / 100;
}

void BuildCarTables(const CarSetup& setup, CarTables& tables) {
    static const int kTopFracByTrans[4][5] = {
        {0, 25, 50, 100, 0},
        {0, 25, 50, 100, 0},
        {0, 25, 50, 75, 100},
        {0, 30, 50, 80, 100},
    };
    static const int kPowerBase[3][5] = {
        {500, 600, 300, 100, 70},
        {600, 700, 350, 120, 90},
        {600, 760, 380, 140, 90},
    };
    static const int kBandStart[3] = {2900, 2900, 2500};
    static const int kBandEnd[3] = {4500, 5000, 5200};
    static const int kNumGears[4] = {3, 3, 4, 4};
    static const int kTireSteer[3] = {9, 8, 5};
    static const int kHandling[3][5] = {
        {2, 5, 8, 11, 14},
        {0, 2, 4, 6, 8},
        {0, 0, 1, 2, 4},
    };

    const int carbClass = ClampValue(setup.carbClass, 0, 2);
    const int transClass = ClampValue(setup.transClass, 0, 3);
    const int tireClass = ClampValue(setup.tireClass, 0, 2);

    const int base = ComputeBasePerformance(setup);
    const int topSpeed = std::min((base * 13) / 10, 130);

    tables.automatic = (transClass == 0);
    tables.numGears = kNumGears[transClass];
    tables.autoShiftRpm = static_cast<int16_t>(kBandEnd[carbClass]);
    tables.perfScore = static_cast<int16_t>(base);

    for (int g = 0; g < 5; ++g) {
        const int fracSpeed = (kTopFracByTrans[transClass][g] * topSpeed) / 100;
        const int speedFloor = std::max(fracSpeed, 20);
        tables.ratio[g] = static_cast<int16_t>(570000 / speedFloor);

        const int power = (base * kPowerBase[carbClass][g]) / 100;
        const int row0Mul = (g == 1) ? ((tireClass == 2) ? 3 : 2) : 1;
        const int row0Div = (g == 0) ? 1 : 5;
        const int row2Div = (g == 0) ? 1 : 10;
        const int rowLoss = (g == 0) ? power : (power / (5 - g));

        tables.rows[g][0].rpmThreshold = static_cast<int16_t>(kBandStart[carbClass]);
        tables.rows[g][1].rpmThreshold = static_cast<int16_t>(kBandEnd[carbClass]);
        tables.rows[g][2].rpmThreshold = 5700;

        tables.rows[g][0].rpmGain = static_cast<int16_t>((power / row0Div) * row0Mul);
        tables.rows[g][1].rpmGain = static_cast<int16_t>(power);
        tables.rows[g][2].rpmGain = static_cast<int16_t>(power / row2Div);

        tables.rows[g][0].rpmLoss = static_cast<int16_t>(rowLoss);
        tables.rows[g][1].rpmLoss = static_cast<int16_t>(rowLoss);
        tables.rows[g][2].rpmLoss = static_cast<int16_t>(rowLoss);
    }

    if (setup.supercharger) {
        for (int r = 0; r < 3; ++r) {
            tables.rows[0][r].rpmGain = static_cast<int16_t>(tables.rows[0][r].rpmGain * 2);
        }
    }

    const int skillPenalty = ((130 - ClampValue(setup.driverSkill, 0, 130)) * 5) / 50;
    tables.steeringStrength = static_cast<int16_t>(kTireSteer[tireClass] + skillPenalty);

    const int wearBucket = ClampValue(setup.tireWear / 2000, 0, 4);
    tables.handling = static_cast<int16_t>(kHandling[tireClass][wearBucket]);
}

int SpeedToScrollStep(int speedMph) {
    if (speedMph <= 0) {
        return 0;
    }
    if (speedMph <= 13) {
        return 1;
    }
    if (speedMph <= 27) {
        return 2;
    }
    if (speedMph <= 41) {
        return 3;
    }
    if (speedMph <= 55) {
        return 4;
    }
    if (speedMph <= 69) {
        return 5;
    }
    if (speedMph <= 83) {
        return 6;
    }
    if (speedMph <= 97) {
        return 7;
    }
    if (speedMph >= 130) {
        return 16;
    }
    return 8 + ((speedMph - 98) / 4);
}

void RaceSim::Init(const TrackData* newTrack, int newRaceType,
                   const CarSetup& newPlayerSetup, const CarSetup& newNpcSetup,
                   uint32_t rngSeed) {
    track = newTrack;
    raceType = (newRaceType == 0) ? 0 : 1;
    finishSeg = (raceType == 0) ? DRAG_FINISH_SEG : ROAD_FINISH_SEG;
    playerSetup = newPlayerSetup;
    npcSetup = newNpcSetup;
    rng = RaceRng(rngSeed);

    BuildCarTables(playerSetup, playerTables);
    BuildCarTables(npcSetup, npcTables);

    player = CarState{};
    npc = CarState{};
    player.rpm = 1500;
    npc.rpm = 1500;
    player.gear = 0;
    npc.gear = 0;
    player.lateral = 2560;
    npc.lateral = 2400;
    player.segPos = RACE_START_SEG;
    npc.segPos = RACE_START_SEG;
    player.endFlags = 0;
    npc.endFlags = 0;

    npcAi = NpcState{};
    npcAi.lateralTarget = 2400;
    npcAi.targetSpeedNormal = static_cast<int16_t>((npcTables.perfScore * 70) / 100);
    npcAi.targetSpeedRisky = static_cast<int16_t>((npcTables.perfScore * 80) / 100);
    npcAi.stallTicks = 0;
    npcAi.gaveUp = false;

    if (track != nullptr) {
        int seg = ClampTrackIndex(player.segPos);
        RoadEdges(track->flags[seg], edgeLeft, edgeRight);
        seg = ClampTrackIndex(player.segPos + 4);
        RoadEdges(track->flags[seg], edgeLeftAhead, edgeRightAhead);
    } else {
        edgeLeft = 1500;
        edgeRight = 3300;
        edgeLeftAhead = 1500;
        edgeRightAhead = 3300;
    }
}

void RaceSim::Tick(uint16_t inputBits) {
    if (track == nullptr) {
        return;
    }

    if ((player.endFlags & (END_ENGINE_BLOWN | END_TRANS_DROPPED | END_WALL_HIT | END_STALLED)) == 0) {
        PlayerPhysics(inputBits);
        UpdateSteering(inputBits);
    }

    NpcAiUpdate();
    NpcEngine();

    UpdateScroll(player);
    UpdateScroll(npc);

    ApplyLateralDrift(player, playerTables.handling);

    const int playerSeg = ClampTrackIndex(player.segPos);
    const int playerSegAhead = ClampTrackIndex(player.segPos + 4);
    RoadEdges(track->flags[playerSeg], edgeLeft, edgeRight);
    RoadEdges(track->flags[playerSegAhead], edgeLeftAhead, edgeRightAhead);

    CheckEdgeCollision();
}

void RaceSim::PlayerPhysics(uint16_t inputBits) {
    const bool throttle = (inputBits & INPUT_THROTTLE) != 0;
    const bool brake = (inputBits & INPUT_BRAKE) != 0;
    const bool shift = (inputBits & INPUT_SHIFT) != 0;
    EngineTick(player, playerTables, throttle, brake, shift);
}

void RaceSim::EngineTick(CarState& car, const CarTables& tables, bool throttle,
                         bool brake, bool shift) {
    // Gear 0 is neutral (original A4+0x4b22 "0 = neutral/1st index base");
    // driving gears are 1..numGears.
    const int gear = ClampValue<int>(car.gear, 0, tables.numGears);
    const PowerRow* gearRows = tables.rows[gear];
    int rowIndex = 0;
    if (car.rpm >= gearRows[1].rpmThreshold) {
        rowIndex = 1;
    }
    if (car.rpm >= gearRows[2].rpmThreshold) {
        rowIndex = 2;
    }
    const int gain = gearRows[rowIndex].rpmGain;
    const int loss = gearRows[rowIndex].rpmLoss;

    const bool pastFinish = (car.segPos >= finishSeg);
    if (pastFinish) {
        throttle = false;
    }

    if (throttle) {
        car.rpm = static_cast<int16_t>(car.rpm + gain);
    } else if (car.rpm > 1500) {
        car.rpm = static_cast<int16_t>(std::max(1500, car.rpm - loss));
    }

    const int rpmCap = tables.automatic ? 5690 : 5700;
    if (car.rpm > rpmCap) {
        car.rpm = static_cast<int16_t>(rpmCap);
    }

    if (throttle && car.rpm >= rpmCap) {
        car.redlineTicks++;
        int wearNow = 0;
        if (&car == &player) {
            wearNow = ClampValue(playerSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        } else {
            wearNow = ClampValue(npcSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        }
        const int blowLimit = std::max(1, 15 - (wearNow / 1000));
        if (car.redlineTicks >= blowLimit) {
            car.endFlags |= END_ENGINE_BLOWN;
        }
    } else {
        car.redlineTicks = 0;
    }

    if (car.rpm > 5000) {
        car.engineWearDelta += 1;
        car.transWearDelta += 1;
    }
    if (((car.tick + 1) % 16) == 0) {
        car.engineWearDelta += 1;
    }

    bool doShift = false;
    if (shift) {
        doShift = (throttle && car.gear < tables.numGears) || (!throttle && car.gear > 0);
    } else if (tables.automatic && throttle) {
        // Automatic: engage first gear from neutral, then upshift at the
        // carb power-band rpm.
        if (car.gear == 0 || (car.rpm >= tables.autoShiftRpm && car.gear < tables.numGears)) {
            doShift = true;
        }
    }

    if (doShift) {
        car.transWearDelta += (car.rpm < 5001) ? 3 : 15;
        if (throttle) {
            car.gear = static_cast<int16_t>(std::min(car.gear + 1, tables.numGears));
        } else {
            car.gear = static_cast<int16_t>(std::max(car.gear - 1, 0));
        }

        if (car.gear > 0) {
            const int newGearRatio = tables.ratio[car.gear];
            car.rpm = static_cast<int16_t>(std::max((newGearRatio * car.speed) / 100, 1505));
            if (car.rpm > 5700) {
                car.rpm = 5700;
            }
        }
    }

    if (car.gear == 0) {
        // Neutral: engine revs freely, the car coasts down.
        if (car.speed > 0) {
            car.speed = static_cast<int16_t>(car.speed - 1);
        }
    } else {
        const int ratio = std::max<int>(tables.ratio[car.gear], 1);
        car.speed = static_cast<int16_t>((car.rpm * 100) / ratio);
    }

    const int curRatio = std::max<int>(tables.ratio[std::max<int>(car.gear, 1)], 1);
    if ((brake || car.offRoad) && car.speed > 8) {
        car.speed = static_cast<int16_t>(std::max(0, car.speed - (car.speed / 16 + 2)));
        if (car.gear > 0) {
            car.rpm = static_cast<int16_t>(std::max((curRatio * car.speed) / 100, 1500));
        }
    }

    if (pastFinish) {
        car.speed = static_cast<int16_t>(std::max(car.speed - 15, 0));
        if (car.gear > 0) {
            car.rpm = static_cast<int16_t>(std::max((curRatio * car.speed) / 100, 1500));
        }
    }

    car.speed = static_cast<int16_t>(ClampValue<int>(car.speed, 0, 130));
    car.distanceAcc += car.speed;
    car.tick += 1;
}

void RaceSim::UpdateSteering(uint16_t inputBits) {
    int steer = 0;
    if ((inputBits & INPUT_STEER_LEFT) != 0) {
        steer -= 1;
    }
    if ((inputBits & INPUT_STEER_RIGHT) != 0) {
        steer += 1;
    }

    if (steer == 0) {
        player.steerVel = 0;
    } else {
        if (Sign(player.steerVel) != 0 && Sign(player.steerVel) != steer) {
            player.steerVel = 0;
        }
        player.steerVel = static_cast<int16_t>(ClampValue<int>(player.steerVel + 2 * steer, -24, 24));
    }

    const int curveValue = track->curve[ClampTrackIndex(player.segPos)];
    const int steerScale = std::max<int>(playerTables.steeringStrength, 1);
    const int headingDelta = ((player.steerVel * steerScale) / 9) + Sign(player.steerVel);
    player.heading = static_cast<int16_t>(player.heading - headingDelta);
    player.heading = static_cast<int16_t>(ClampValue<int>(player.heading, curveValue - 200, curveValue + 200));
}

void RaceSim::ApplyLateralDrift(CarState& car, int16_t handling) {
    const int seg = ClampTrackIndex(car.segPos);
    const int curve = track->curve[seg];
    const int drift = (car.heading - curve) * (car.scrollStep + handling) / 4;
    car.lateral = static_cast<int16_t>(car.lateral - drift);
}

void RaceSim::UpdateScroll(CarState& car) {
    car.scrollStep = static_cast<int16_t>(SpeedToScrollStep(car.speed));
    car.scrollAcc = static_cast<int16_t>(car.scrollAcc + car.scrollStep);
    if (car.scrollAcc > 7) {
        car.segPos += (car.scrollAcc >> 3);
        car.scrollAcc = static_cast<int16_t>(car.scrollAcc & 7);
    }
    if (car.segPos >= finishSeg) {
        car.endFlags |= END_FINISHED;
    }
}

void RaceSim::CheckEdgeCollision() {
    const int seg = ClampTrackIndex(player.segPos);
    const uint16_t leftObj = track->left[seg];
    const uint16_t rightObj = track->right[seg];
    const uint16_t flags = track->flags[seg];

    const int carLeft = player.lateral - 30;
    const int carRight = player.lateral + 70;
    const bool offLeft = carLeft < edgeLeft;
    const bool offRight = carRight > edgeRight;
    player.offRoad = offLeft || offRight;

    if (!player.offRoad) {
        return;
    }

    if (carLeft < (edgeLeft - 260) || carRight > (edgeRight + 260)) {
        player.endFlags |= END_WALL_HIT;
        return;
    }

    if (offLeft && (leftObj & (0x0010 | 0x1000)) != 0) {
        player.endFlags |= END_WALL_HIT;
        return;
    }
    if (offRight && (rightObj & (0x0010 | 0x1000)) != 0) {
        player.endFlags |= END_WALL_HIT;
        return;
    }
    if ((flags & 0xf000) != 0) {
        player.endFlags |= END_WALL_HIT;
    }
}

void RaceSim::NpcAiUpdate() {
    const int npcSeg = ClampTrackIndex(npc.segPos);
    int16_t npcEdgeLeft = 1500;
    int16_t npcEdgeRight = 3300;
    RoadEdges(track->flags[npcSeg], npcEdgeLeft, npcEdgeRight);
    int target = ((npcEdgeLeft + npcEdgeRight) / 2) + 100;

    const int segDiff = npc.segPos - player.segPos;
    if (std::abs(segDiff) <= 5) {
        target = (player.lateral < 2400) ? (target + 250) : (target - 250);
    }

    npcAi.lateralTarget = static_cast<int16_t>(target);
    npc.lateral = static_cast<int16_t>(npc.lateral + ((npcAi.lateralTarget - npc.lateral) / 8));

    const int raceThreeQuarter = RACE_START_SEG + ((finishSeg - RACE_START_SEG) * 3) / 4;
    if (!npcAi.gaveUp && player.segPos > raceThreeQuarter && player.segPos > npc.segPos + 2) {
        npcAi.gaveUp = true;
        npc.endFlags |= END_GAVE_UP;
    }
}

void RaceSim::NpcEngine() {
    const int segDiff = npc.segPos - player.segPos;
    if (segDiff >= 4 && rng.Next(64) == 0) {
        npc.rpm = static_cast<int16_t>(std::max(1500, npc.rpm - 300));
        npcAi.stallTicks = 2;
    }

    int targetSpeed = (player.segPos > npc.segPos) ? npcAi.targetSpeedRisky : npcAi.targetSpeedNormal;
    if (std::abs(segDiff) <= 5) {
        targetSpeed += 16;
    }
    targetSpeed = ClampValue(targetSpeed, 0, 130);

    bool throttle = !npcAi.gaveUp && (npc.speed < targetSpeed);
    if (npcAi.stallTicks > 0) {
        throttle = false;
        npcAi.stallTicks -= 1;
    }

    const bool shift = throttle &&
        ((npc.gear == 0) ||
         (npc.gear < npcTables.numGears && npc.rpm >= npcTables.autoShiftRpm));
    EngineTick(npc, npcTables, throttle, false, shift);
}

}  // namespace srrace
