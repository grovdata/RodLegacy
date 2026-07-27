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

    // Staging countdown before the green (UpdatePhysics 0x2364e8 picks a random
    // launch time). ~2-3s at 50 Hz; leaving the line early is a false start.
    phase = RacePhase::Countdown;
    countdownTicks = 100 + rng.Next(50);

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

void RaceSim::SetCameraState(int32_t segPos, int16_t scrollAcc, int16_t heading,
                             int16_t lateral, int16_t curveAtCar) {
    if (track == nullptr) {
        return;
    }
    player.segPos = ClampTrackIndex(segPos);
    player.scrollAcc = static_cast<int16_t>(ClampValue<int>(scrollAcc, 0, 7));
    player.scrollStep = 0;
    player.speed = 0;
    player.rpm = 1500;
    player.heading = heading;
    player.lateral = lateral;
    player.curveAtCar = curveAtCar;
    player.endFlags &= static_cast<uint16_t>(~END_FINISHED);
    player.offRoad = false;
    phase = RacePhase::Running;  // debug camera is not staged

    const int seg = ClampTrackIndex(player.segPos);
    const int segAhead = ClampTrackIndex(player.segPos + 4);
    RoadEdges(track->flags[seg], edgeLeft, edgeRight);
    RoadEdges(track->flags[segAhead], edgeLeftAhead, edgeRightAhead);
}

void RaceSim::SetNpcDebugPose(int32_t segPos, int16_t lateral) {
    npc.segPos = ClampTrackIndex(segPos);
    npc.lateral = lateral;
}

void RaceSim::Tick(uint16_t inputBits) {
    if (track == nullptr) {
        return;
    }

    // A player failure (crash, blown engine, dropped trans, false start) freezes
    // the car; the race is over (UpdatePhysics returns non-zero on flags 0x4bc6).
    if (phase == RacePhase::Ended) {
        return;
    }

    if (phase == RacePhase::Countdown) {
        // The engine can rev on the line, but if the car actually rolls before
        // the green it is a false start (UpdatePhysics 0x2364e8 state 1).
        if ((player.endFlags & END_FAILURE_MASK) == 0) {
            PlayerPhysics(inputBits);
            UpdateSteering(inputBits);
            UpdateScroll(player);
            if (player.speed > 0 || player.segPos > RACE_START_SEG) {
                player.endFlags |= END_STALLED;  // jumped the start
                phase = RacePhase::Ended;
            }
        }
        if (countdownTicks > 0) {
            --countdownTicks;
            if (countdownTicks == 0) {
                phase = RacePhase::Running;
            }
        }
        return;
    }

    if ((player.endFlags & END_FAILURE_MASK) == 0) {
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

    if ((player.endFlags & END_FAILURE_MASK) != 0) {
        phase = RacePhase::Ended;
    }
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

    // Engine grenades once total wear passes 0x26ab (PlayerCarPhysics 0x23c5d6
    // checks this every tick, independent of the redline-hold blow above).
    const int baseEngineWear = (&car == &player) ? playerSetup.engineWear : npcSetup.engineWear;
    if (baseEngineWear + static_cast<int>(car.engineWearDelta) > 0x26ab) {
        car.endFlags |= END_ENGINE_BLOWN;
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

        // Transmission drops (PlayerCarPhysics 0x23c5d6): worn past 0x26ab, or a
        // money-shift - a manual box banged under throttle above 5000 rpm, at a
        // wear-scaled random chance. The original rolls a 15-bit random against
        // wear/100+100, so it is rare when fresh (~0.3%/shift) and grows with wear.
        const int baseTransWear = (&car == &player) ? playerSetup.transWear : npcSetup.transWear;
        const int totalTransWear = baseTransWear + static_cast<int>(car.transWearDelta);
        const bool moneyShift = !tables.automatic && throttle && car.rpm > 5000 &&
                                rng.Next(32768) < (totalTransWear / 100 + 100);
        if (totalTransWear > 0x26ab || moneyShift) {
            car.endFlags |= END_TRANS_DROPPED;
        }

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

    // Steer/slide velocity model (PlayerCarPhysics 0x23c5d6). steerVel is the
    // input integrator (+-24); slideVel carries the momentum and decays, so the
    // car keeps drifting briefly after the stick is released or reversed.
    int sv = player.steerVel;
    int pv = player.slideVel;
    if (steer != 0 && (Sign(sv) == 0 || Sign(sv) == steer)) {
        // Re-steering the same way recovers the decayed slide momentum.
        if (Sign(pv) == steer && std::abs(sv) < std::abs(pv)) {
            sv = pv;
        }
        sv = ClampValue<int>(sv + 2 * steer, -24, 24);
        pv = sv;
    } else {
        sv = 0;  // released, or reversed direction
    }
    // slideVel decays ~1/4 + 1 toward zero each tick (68k ASR = arithmetic shift).
    if (pv != 0) {
        pv += (pv < 0) ? ((-pv >> 2) + 1) : -((pv >> 2) + 1);
    }
    player.steerVel = static_cast<int16_t>(sv);
    player.slideVel = static_cast<int16_t>(pv);

    // Heading follows steer velocity (RenderTrackFrame 0x23b77c):
    //   heading -= steerVel/2 + 1
    // only while the car is moving and its heading stays within +-200 of the
    // road direction. (No steering-strength scaling here; the original uses a
    // fixed /2.)
    if (sv != 0 && player.speed != 0) {
        const int curve = track->curve[ClampTrackIndex(player.segPos)];
        if (std::abs(player.heading - sv - curve) < 200) {
            player.heading = static_cast<int16_t>(player.heading - ((sv >> 1) + 1));
        }
    }
}

void RaceSim::ApplyLateralDrift(CarState& car, int16_t handling) {
    const int seg = ClampTrackIndex(car.segPos);
    const int curve = track->curve[seg];
    const int drift = (car.heading - curve) * (car.scrollStep + handling) / 4;
    car.lateral = static_cast<int16_t>(car.lateral - drift);
}

void RaceSim::UpdateScroll(CarState& car) {
    const int32_t prevSeg = car.segPos;
    car.scrollStep = static_cast<int16_t>(SpeedToScrollStep(car.speed));
    car.scrollAcc = static_cast<int16_t>(car.scrollAcc + car.scrollStep);
    if (car.scrollAcc > 7) {
        car.segPos += (car.scrollAcc >> 3);
        car.scrollAcc = static_cast<int16_t>(car.scrollAcc & 7);
    }
    if (car.segPos != prevSeg && track != nullptr) {
        car.curveAtCar = track->curve[ClampTrackIndex(car.segPos)];
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
    // Aim for the midpoint of the road a few segments ahead (UpdateNPCAI 0x244870
    // steers to the average of the look-ahead edges, not a fixed center).
    const int aheadSeg = ClampTrackIndex(npc.segPos + 4);
    int16_t aheadLeft = 1500;
    int16_t aheadRight = 3300;
    RoadEdges(track->flags[aheadSeg], aheadLeft, aheadRight);
    const int center = (aheadLeft + aheadRight) / 2;
    int target = center;

    const int segDiff = npc.segPos - player.segPos;
    if (std::abs(segDiff) <= 5) {
        // Block the player's line when ahead, swing to the open side when behind
        // (UpdateNPCAI swerves +300 to block / -200 to give room).
        const bool playerLeft = player.lateral < center;
        if (segDiff >= 0) {
            target = center + (playerLeft ? -200 : 300);
        } else {
            target = center + (playerLeft ? 300 : -200);
        }
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
