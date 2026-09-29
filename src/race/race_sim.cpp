#include "race/race_sim.h"

#include <algorithm>
#include <cmath>
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

bool gPuristScoring = false;

}

void SetPuristScoring(bool enabled) {
    gPuristScoring = enabled;
}

bool PuristScoring() {
    return gPuristScoring;
}

int ComputeBasePerformance(const CarSetup& setup) {
    static const int kCarbPts[3] = {0, 50, 100};
    static const int kTransPts[4] = {0, 33, 66, 100};
    static const int kEngByManifold[20] = {
        0, 0, 100, 0, 0,
        0, 50, 0, 100, 75,
        0, 0, 0, 50, 100,
        0, 50, 100, 0, 33
    };
    static const int kWeightPts[9] = {40, 44, 48, 60, 64, 68, 80, 84, 88};

    const int carbClass = ClampValue(setup.carbClass, 0, 2);
    const int transClass = ClampValue(setup.transClass, 0, 3);
    const int engineClass = ClampValue(setup.engineClass, 0, 3);
    const int manifoldClass = ClampValue(setup.manifoldClass, 0, 4);
    const int weightClass = ClampValue(setup.weightClass, 0, 8);
    const int tuneOffset = ClampValue(setup.tuneOffset, 0, 8);
    const int carbWear = ClampValue(setup.carbWear, 0, 10000);

    // Weighted to 100 for a perfect car: carb 25, transmission 15, engine/manifold 15, tune 20, weight 10, body mods 15.
    int pts = 0;
    pts += (kCarbPts[carbClass] * 25) / 100;
    pts += (kTransPts[transClass] * 15) / 100;
    pts += (kEngByManifold[engineClass * 5 + manifoldClass] * 15) / 100;
    // The one deviation by default: the original's (1 - |tune|) tops out at +2 and leaves the 130 cap unreachable.
    // --purist restores it.
    pts += (((gPuristScoring ? 1 : 8) - tuneOffset) * 20) / 8;
    pts += (kWeightPts[weightClass] * 10) / 88;
    if (setup.roofChopped) {
        pts += 9;
    }
    if (setup.frontBumperRemoved) {
        pts += 5;
    }
    if (setup.rearBumperRemoved) {
        pts += 1;
    }

    const bool automatic = (transClass == 0);
    const int transFactor = automatic ? 24 : 30;
    const int wearScaledPts = (pts * (10000 - carbWear)) / 10000;
    return 70 + (wearScaledPts * transFactor) / 100;
}

int ComputeTopSpeed(const CarSetup& setup) {
    return std::min((ComputeBasePerformance(setup) * 13) / 10, 130);
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
    const int topSpeed = ComputeTopSpeed(setup);

    tables.automatic = (transClass == 0);
    tables.numGears = kNumGears[transClass];
    tables.autoShiftRpm = static_cast<int16_t>(kBandEnd[carbClass]);
    tables.perfScore = static_cast<int16_t>(base);
    tables.topSpeed = static_cast<int16_t>(topSpeed);

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

// Band end minus 100 per point of reaction: the best driver leaves 1000 rpm unused, the worst 2000.
// Automatics are exempt.
int16_t NpcShiftRpm(const CarSetup& setup, const CarTables& tables) {
    if (tables.automatic) {
        return tables.autoShiftRpm;
    }
    const int drop = ClampValue(setup.reaction, 0, 40) * 100;
    return static_cast<int16_t>(std::max(1500, tables.autoShiftRpm - drop));
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
                   uint32_t rngSeed, bool isKingRace) {
    track = newTrack;
    kingRace = isKingRace;
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
    player.segPos = RACE_START_SEG;
    npc.segPos = RACE_START_SEG;
    player.endFlags = 0;
    npc.endFlags = 0;

    npcContactTicks = 0;
    npcScrapeAlt = false;
    bumpStunTicks = 0;
    npcBumpEvent = false;
    finishSnapshot = false;
    playerAheadAtFinish = false;
    player.autoDrive = false;   // automatics stage in N
    npc.autoDrive = false;

    // The opponent starts 200 left of the road centre, not on it: centred, the 160 gap is inside the 180 that
    // CheckNpcContact counts as overlapping.
    {
        int startSeg = ClampTrackIndex(RACE_START_SEG);
        int16_t startLeft = 1500;
        int16_t startRight = 3300;
        if (track != nullptr) {
            RoadEdges(track->flags[startSeg], startLeft, startRight);
        }
        const int16_t npcStart = static_cast<int16_t>(((startLeft + startRight) >> 1) - 200);
        player.lateral = 2560;
        npc.lateral = npcStart;

        npcAi = NpcState{};
        npcAi.lateralTarget = npcStart;
    }
    npcShiftRpm = NpcShiftRpm(npcSetup, npcTables);
    npcAi.targetSpeedNormal = static_cast<int16_t>((npcTables.perfScore * 70) / 100);
    npcAi.targetSpeedRisky = static_cast<int16_t>((npcTables.perfScore * 80) / 100);
    npcAi.stallTicks = 0;
    npcAi.gaveUp = false;

    // Random launch time, ~2-3s at 50 Hz; leaving early is a false start.
    phase = RacePhase::Countdown;
    countdownTicks = 100 + rng.Next(50);

    // Road races only, never the King's: 1 in 4, trigger at 510 + rand(255). From a derived seed so the extra
    // draws don't shift every later roll the races are regression-tested against.
    copState = 0;
    copTriggerSeg = 0;
    copSpawnEvent = false;
    copSirenOn = false;
    copCaughtSpeed = 0;
    RaceRng copRng(rngSeed ^ 0x5a17c09u);
    if (raceType == 1 && !kingRace && copRng.Next(4) == 0) {
        copState = 1;
        copTriggerSeg = 510 + copRng.Next(255);
    }

    // Tires under 80% worn hold; past that `wear + rand(2000) > 9500` once per race, placed at
    //   blowSeg = 200 + (finish-200)/2 + rand((finish-200)/4)
    // Own RNG stream, as for the cop.
    tireBlowSeg = 0;
    if (playerSetup.tireWear >= 8000) {
        RaceRng tireRng(rngSeed ^ 0x71235u);
        if (playerSetup.tireWear + static_cast<int>(tireRng.Next(2000)) > 9500) {
            const int half = (finishSeg - 200) / 2;
            tireBlowSeg = 200 + half + static_cast<int>(tireRng.Next(std::max(1, half / 2)));
        }
    }

    if (track != nullptr) {
        int seg = ClampTrackIndex(player.segPos);
        player.elevAtCar = track->elev[seg];
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
                             int16_t lateral, int16_t elevAtCar) {
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
    player.elevAtCar = elevAtCar;
    player.endFlags &= static_cast<uint16_t>(~END_FINISHED);
    npc.endFlags &= static_cast<uint16_t>(~END_FINISHED);
    finishSnapshot = false;
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

void RaceSim::Tick(uint16_t inputBits, float steerAmount) {
    if (track == nullptr) {
        return;
    }

    // A player failure freezes the car and ends the race.
    if (phase == RacePhase::Ended) {
        return;
    }

    if (phase == RacePhase::Countdown) {
        // The engine can rev on the line, but rolling before the green is a false start.
        if ((player.endFlags & END_FAILURE_MASK) == 0) {
            PlayerPhysics(inputBits);
            UpdateSteering(inputBits, steerAmount);
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
        UpdateSteering(inputBits, steerAmount);
    }

    NpcAiUpdate();
    NpcEngine();
    // Before the finish check: ApplyCopRaceEnd must see this tick's finish result.
    UpdateCopPursuit();

    UpdateScroll(player);
    UpdateScroll(npc);

    if (tireBlowSeg != 0 && player.segPos > tireBlowSeg) {
        player.endFlags |= END_TIRES_BLOWN;
    }

    ApplyLateralDrift(player, playerTables.handling);

    const int playerSeg = ClampTrackIndex(player.segPos);
    const int playerSegAhead = ClampTrackIndex(player.segPos + 4);
    RoadEdges(track->flags[playerSeg], edgeLeft, edgeRight);
    RoadEdges(track->flags[playerSegAhead], edgeLeftAhead, edgeRightAhead);

    CheckEdgeCollision();
    // All car-to-car contact needs the player short of the line, so the roll-out cannot be crashed out.
    if (player.segPos < finishSeg) {
        CheckNpcContact();
    }
    CheckFinish();
    ApplyCopRaceEnd();

    // Last, so the AI and contact test read the previous frame's state, as in the original.
    UpdateNpcViewState();

    if ((player.endFlags & END_FAILURE_MASK) != 0) {
        phase = RacePhase::Ended;
    }
}

// A raw state is read off the gap, then walked toward at most one step per frame, so the sprite doesn't jump
// between sizes and positions.
void RaceSim::UpdateNpcViewState() {
    const int longGap = (static_cast<int>(npc.segPos) - static_cast<int>(player.segPos)) * 8 +
                        (npc.scrollAcc - player.scrollAcc);
    const int latGap = npc.lateral - player.lateral;

    int raw = 0;
    if (longGap >= 0x78) {
        raw = -1;                                   // too far up the road to draw
    } else if (longGap >= 0x10) {
        raw = std::min(((longGap + 8) >> 4) + 2, 8);  // ahead, one frame per 16 units
    } else if (longGap >= 9) {
        raw = (latGap < -180) ? 1 : 3;
    } else if (longGap >= -7) {
        // Overlapping laterally reads as 2 unless already behind; only a clear gap to our left gets the alongside view.
        if (std::abs(latGap) < 180) {
            raw = (npcAi.viewState < 9) ? 2 : 9;
        } else {
            raw = (latGap >= 0) ? 2 : 0;
        }
    } else {
        raw = ((0x10 - longGap) >> 4) + 8;          // behind us: 9 and up
    }

    const int cur = npcAi.viewState;
    int next = raw;
    if (cur != raw) {
        if (raw < 0) {
            if (cur == 8) {                          // walked out the far end
                npcAi.viewHidden = true;
                return;
            }
            next = cur + 1;
        } else if (raw > 12) {
            if (cur == 12) {                         // walked out the mirror end
                npcAi.viewHidden = true;
                return;
            }
            next = cur + 1;
        } else if (npcAi.prevViewState != raw) {
            // The gap came straight back to where it was two frames ago, so snap.
            if ((raw <= 8 && cur > 9) || (raw >= 9 && cur <= 8 && cur > 0)) {
                next = cur - 1;
            } else if (cur == 0) {
                next = (raw >= 9) ? 9 : 1;
            } else if (cur == 9) {
                next = (raw >= 9) ? 10 : ((latGap < 0) ? 0 : 2);
            } else if (cur == 1) {
                next = (raw < 1) ? 0 : 3;
            } else if (cur == 3 && raw == 1) {
                next = raw;
            } else {
                next = (raw > cur) ? cur + 1 : cur - 1;
            }
        }
    }

    npcAi.viewHidden = false;
    npcAi.inContactRange = (next == 0) || (next == 2 && longGap < 11);
    npcAi.rearEnding = (next == 2 && longGap < 11 && std::abs(latGap) < 180 &&
                        SpeedToScrollStep(npc.speed) < SpeedToScrollStep(player.speed));
    npcAi.prevViewState = static_cast<int16_t>(cur);
    npcAi.viewState = static_cast<int16_t>(next);
}

// The cop is re-geared to keep its speed and gets its own target speeds, not the challenger's.
void RaceSim::SpawnCop() {
    // Car spec 25: GM, racing carb, racing transmission, racing slicks.
    CarSetup copSetup{};
    copSetup.carbClass = 2;
    copSetup.transClass = 3;
    copSetup.engineClass = 2;
    copSetup.manifoldClass = 4;
    copSetup.tireClass = 2;
    copSetup.driverSkill = 130;
    npcSetup = copSetup;

    const int16_t speedBefore = npc.speed;
    BuildCarTables(npcSetup, npcTables);

    // 60% and 80% of the cop's performance, against an ordinary opponent's 70/80.
    npcAi.targetSpeedNormal = static_cast<int16_t>((npcTables.perfScore * 60) / 100);
    npcAi.targetSpeedRisky = static_cast<int16_t>((npcTables.perfScore * 80) / 100);

    // Otherwise it inherits the challenger's give-up state, which usually coincides with the cop appearing.
    npcAi.gaveUp = false;
    npcAi.stallTicks = 0;
    npc.endFlags &= static_cast<uint16_t>(~END_GAVE_UP);

    // Keep the speed it was carrying; the gear is re-derived from the new tables.
    npc.speed = speedBefore;
    npcShiftRpm = NpcShiftRpm(npcSetup, npcTables);
    copSpawnEvent = true;
}

// From state 2 the opponent is the cop; the swap is a flag the renderer reads.
void RaceSim::UpdateCopPursuit() {
    if (copState == 0) {
        return;
    }
    const int pSeg = static_cast<int>(player.segPos);
    const int nSeg = static_cast<int>(npc.segPos);

    if (copState == 1) {
        if (pSeg >= copTriggerSeg) {
            if (pSeg < nSeg + 5) {
                // Not far enough clear of the opponent: no cop this race. You only get pulled over for leading.
                copState = 0;
            } else {
                copState = 2;
                SpawnCop();
            }
        }
        return;
    }

    if (copState == 2) {
        if (pSeg <= nSeg + 15) {   // closed to within 15 segments
            copState = 3;
            copSirenOn = true;
        }
        return;
    }

    // State 3: caught the moment it is more than 2 segments ahead.
    if (pSeg < nSeg - 2) {
        copCaughtSpeed = player.speed;
        player.endFlags |= (END_FINISHED | END_COP_CAUGHT);
        copSirenOn = false;
    }
}

// Winning saves you: a win is recorded in a different word than the failure bits, so the caught bit is never
// ORed in and only the siren stops.
void RaceSim::ApplyCopRaceEnd() {
    if (copState < 2) {
        return;
    }
    // The original only folds the tire flag into its outcome word after the race; testing it here handed out a
    // ticket the moment the tires let go.
    const bool playerEnded = (player.endFlags & ~END_TIRES_BLOWN) != 0;
    const bool playerWon = (npc.endFlags & END_FINISHED) != 0;
    if (!playerEnded && !playerWon) {
        return;   // race still running
    }
    if (playerEnded && (player.endFlags & END_COP_CAUGHT) == 0) {
        // Lost, crashed or blew up while being chased: ticket on top of it.
        copCaughtSpeed = player.speed;
        player.endFlags |= END_COP_CAUGHT;
    }
    // A win reaches here with playerEnded false, so no ticket - just quiet.
    copSirenOn = false;
}

void RaceSim::PlayerPhysics(uint16_t inputBits) {
    bool throttle = (inputBits & INPUT_THROTTLE) != 0;
    const bool brake = (inputBits & INPUT_BRAKE) != 0;
    const bool shift = (inputBits & INPUT_SHIFT) != 0;
    // A side-swipe knocks the throttle out for a couple of ticks.
    if (bumpStunTicks > 0) {
        bumpStunTicks -= 1;
        throttle = false;
    }
    EngineTick(player, playerTables, throttle, brake, shift);
}

void RaceSim::EngineTick(CarState& car, const CarTables& tables, bool throttle,
                         bool brake, bool shift) {
    // Gear 0 is neutral; driving gears are 1..numGears.
    const int gear = ClampValue<int>(car.gear, 0, tables.numGears);
    const PowerRow* gearRows = tables.rows[gear];
    // rpmThreshold is each row's upper bound: [0, bandStart], [bandStart, bandEnd], [bandEnd, 5700]. Use the first
    // row whose bound covers the rpm.
    int rowIndex = 0;
    if (car.rpm > gearRows[0].rpmThreshold) {
        rowIndex = 1;
    }
    if (car.rpm > gearRows[1].rpmThreshold) {
        rowIndex = 2;
    }
    const int gain = gearRows[rowIndex].rpmGain;
    const int loss = gearRows[rowIndex].rpmLoss;

    // Strictly past the finish, throttle and steering are taken away and the brake is forced on, shedding a flat
    // 15 mph a tick. The forced brake is the only path to 0, and a standstill ends the race.
    const bool pastFinish = (car.segPos > finishSeg);
    if (pastFinish) {
        throttle = false;
        brake = true;
        if (car.speed > 15) {
            car.speed = static_cast<int16_t>(car.speed - 15);
        }
    }

    if (throttle) {
        car.rpm = static_cast<int16_t>(car.rpm + gain);
    } else if (car.rpm > 1500) {
        car.rpm = static_cast<int16_t>(std::max(1500, car.rpm - loss));
    }

    // Automatics are governed at 5690 while the blow counts ticks past a fixed 5700, so they can't be blown by revving.
    constexpr int kRedlineRpm = 5700;
    if (tables.automatic && car.rpm > 5690) {
        car.rpm = 5690;
    }

    // Tested before clamping, so a tick landing exactly on the limiter costs nothing; sitting on it holds the count.
    const bool overRevved = (car.rpm > kRedlineRpm);
    if (car.rpm > kRedlineRpm) {
        car.rpm = static_cast<int16_t>(kRedlineRpm);
    }

    if (throttle && overRevved) {
        car.redlineTicks++;
        int wearNow = 0;
        if (&car == &player) {
            wearNow = ClampValue(playerSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        } else {
            wearNow = ClampValue(npcSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        }
        // Strictly greater: the limit-th tick still survives.
        const int blowLimit = std::max(1, 15 - (wearNow / 1000));
        if (car.redlineTicks > blowLimit) {
            if (&car == &player) {
                AddPlayerEndFlags(END_ENGINE_BLOWN);
            } else {
                car.endFlags |= END_ENGINE_BLOWN;
            }
        }
    } else if (car.rpm < kRedlineRpm) {
        car.redlineTicks = 0;
    }

    if (car.rpm > 5000) {
        car.engineWearDelta += 1;
        car.transWearDelta += 1;
    }
    if (((car.tick + 1) % 16) == 0) {
        car.engineWearDelta += 1;
    }

    // The engine part blows past 0x26ab wear (outcome 0x05), independent of the redline hold.
    const int baseEngineWear = (&car == &player) ? playerSetup.engineWear : npcSetup.engineWear;
    if (baseEngineWear + static_cast<int>(car.engineWearDelta) > 0x26ab) {
        if (&car == &player) {
            AddPlayerEndFlags(END_ENGINE_BLOWN);
        } else {
            car.endFlags |= END_ENGINE_BLOWN;
        }
    }

    bool doShift = false;
    if (shift) {
        doShift = (throttle && car.gear < tables.numGears) || (!throttle && car.gear > 0);
    } else if (tables.automatic && throttle && (car.autoDrive || &car != &player)) {
        // Only past N does an automatic engage first and upshift at the carb band rpm.
        if (car.gear == 0 || (car.rpm >= tables.autoShiftRpm && car.gear < tables.numGears)) {
            doShift = true;
        }
    }

    if (doShift) {
        car.transWearDelta += (car.rpm < 5001) ? 3 : 15;

        // Worn past 0x26ab, or a money-shift: a manual banged under throttle above 5000 rpm, PRNG(3277) against wear,
        // ~3% a shift when fresh.
        const int baseTransWear = (&car == &player) ? playerSetup.transWear : npcSetup.transWear;
        const int totalTransWear = baseTransWear + static_cast<int>(car.transWearDelta);
        const bool moneyShift = !tables.automatic && throttle && car.rpm > 5000 &&
                                rng.Next(3277) < (totalTransWear / 100 + 100);
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

    // Player only: NpcEngine synthesises a shift on every upshift, which would toggle a selector back to N.
    if (tables.automatic && &car == &player) {
        // The shift input toggles N/D. It drops to N out of gear or coasting to a crawl, and N holds the box in
        // neutral so an automatic can rev on the line without rolling.
        if (shift) {
            car.autoDrive = !car.autoDrive;
        }
        const bool coastingToStop = (car.speed < 5 && !throttle && car.gear > 0);
        if (coastingToStop || car.gear == 0) {
            car.autoDrive = false;
        }
        if (!car.autoDrive) {
            car.gear = 0;
        }
    }

    // One if/else, not a drive step then a braking pass:
    //
    //   if !braking && (!offroad || speed < 9):   drive - speed follows rpm
    //   else:                                     decay - rpm follows speed
    //
    // In decay speed is never re-derived from rpm, so it walks down to 0 while rpm floors at idle.
    const int curRatio = std::max<int>(tables.ratio[std::max<int>(car.gear, 1)], 1);
    const bool decaying = brake || (car.offRoad && car.speed >= 9);
    if (!decaying) {
        if (car.gear == 0) {
            // Standing, the engine revs freely: the start-line blip. Rolling, the needle is pinned to road speed.
            if (car.speed > 0) {
                car.speed = static_cast<int16_t>(car.speed - 1);
                car.rpm = static_cast<int16_t>(car.speed * 30 + 1500);
            }
        } else {
            const int ratio = std::max<int>(tables.ratio[car.gear], 1);
            car.speed = static_cast<int16_t>((car.rpm * 100) / ratio);
        }
    } else {
        car.speed = static_cast<int16_t>(std::max(0, car.speed - (car.speed / 16 + 2)));
        car.rpm = static_cast<int16_t>(std::max((curRatio * car.speed) / 100, 1500));
    }

    car.speed = static_cast<int16_t>(ClampValue<int>(car.speed, 0, 130));
    car.distanceAcc += car.speed;
    car.tick += 1;
}

void RaceSim::UpdateSteering(uint16_t inputBits, float steerAmount) {
    int steer = 0;
    if ((inputBits & INPUT_STEER_LEFT) != 0) {
        steer -= 1;
    }
    if ((inputBits & INPUT_STEER_RIGHT) != 0) {
        steer += 1;
    }
    // Past the line the wheel is taken away; the autopilot writes slideVel on each segment advance.
    if (player.segPos > finishSeg) {
        steer = 0;
    }

    // steerVel is the input integrator (+-24); slideVel carries the momentum and decays. steerAmount moves the clamp
    // in rather than the ramp, so half a stick settles at half lock. At 1.0 every step is the original's.
    const int steerLimit = ClampValue<int>(
        static_cast<int>(std::lround(24.0f * ClampValue<float>(steerAmount, 0.0f, 1.0f))),
        0, 24);

    int sv = player.steerVel;
    int pv = player.slideVel;
    if (steer != 0 && (Sign(sv) == 0 || Sign(sv) == steer)) {
        // Re-steering the same way recovers the decayed slide momentum.
        if (Sign(pv) == steer && std::abs(sv) < std::abs(pv)) {
            sv = pv;
        }
        int target = sv + 2 * steer;
        // Unwind at the same rate it wound on; at steerLimit 24 this is the original's clamp.
        if (std::abs(target) > steerLimit) {
            target = Sign(sv) * std::max(steerLimit, std::abs(sv) - 2);
        }
        sv = ClampValue<int>(target, -24, 24);
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

    //   heading -= steerVel/2 + 1
    // only while moving and within +-200 of the road direction.
    if (sv != 0 && player.speed != 0) {
        // The road direction is curve[], the same array driving the drift and the horizontal projection.
        const int roadDir = track->curve[ClampTrackIndex(player.segPos)];
        if (std::abs(player.heading - sv - roadDir) < 200) {
            player.heading = static_cast<int16_t>(player.heading - ((sv >> 1) + 1));
        }
    }
}

void RaceSim::ApplyLateralDrift(CarState& car, int16_t handling) {
    // Blown tires replace the curve drift with a flat 25 a frame to one side. You can steer against it.
    if (&car == &player && (player.endFlags & END_TIRES_BLOWN) != 0) {
        car.lateral = static_cast<int16_t>(car.lateral + 25);
        return;
    }
    const int seg = ClampTrackIndex(car.segPos);
    // Lateral drift uses curve[], not elev[], keeping the road and the car coupled through a bend.
    const int roadDir = track->curve[seg];
    const int drift = (car.heading - roadDir) * (car.scrollStep + handling) / 4;
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
        car.elevAtCar = track->elev[ClampTrackIndex(car.segPos)];
        // The roll-out autopilot steps on segment advance, not per frame.
        if (&car == &player && player.segPos > finishSeg) {
            PostFinishAutopilot();
        }
    }
}

// The car is walked into a lane and its heading snapped to the road so it tracks straight while braking.
// Also runs for the cop takeover.
void RaceSim::PostFinishAutopilot() {
    // The cars part to opposite sides: opponent left of 0x961 -> we take 0xa00, otherwise 0x820.
    const int16_t lane = (npc.lateral < 0x961) ? 0xa00 : 0x820;
    const int delta = std::abs(static_cast<int>(player.lateral) - lane);
    if (delta < 0x65) {
        player.lateral = lane;
    } else {
        // Within 100 of the lane; the next segment lands on it.
        player.lateral = static_cast<int16_t>(lane + ((player.lateral <= lane) ? -100 : 100));
    }

    const int roadDir = track->curve[ClampTrackIndex(player.segPos)];
    if (roadDir != player.heading) {
        int slide = (roadDir - player.heading) * -2;
        slide = ClampValue<int>(slide, -24, 24);
        player.slideVel = static_cast<int16_t>(slide);
    }
    // Snapped to the road, so the drift stops pushing.
    player.heading = static_cast<int16_t>(roadDir);
}

// Over once the car has stopped past the line. The verdict was latched the first tick either car reached it.
void RaceSim::CheckFinish() {
    if (!finishSnapshot &&
        (player.segPos >= finishSeg || npc.segPos >= finishSeg)) {
        const int gap = (static_cast<int>(npc.segPos) - static_cast<int>(player.segPos)) * 8 +
                        (npc.scrollAcc - player.scrollAcc);
        playerAheadAtFinish = (gap <= 0);
        finishSnapshot = true;
    }

    if (finishSnapshot && player.segPos >= finishSeg && player.speed < 1) {
        // Whichever car "finished" in the losing sense is flagged; either flag word exits the loop.
        if (playerAheadAtFinish) {
            npc.endFlags |= END_FINISHED;
        } else {
            player.endFlags |= END_FINISHED;
        }
    }
}

// An event carrying a suppressed bit is dropped whole, or a suppressed crash would still end the race.
void RaceSim::AddPlayerEndFlags(uint16_t flags) {
    if (invulnerable) {
        constexpr uint16_t kSuppressed = END_WALL_HIT | END_HIT_NPC | END_ENGINE_BLOWN;
        if ((flags & kSuppressed) != 0) {
            return;
        }
    }
    player.endFlags |= flags;
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
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }

    if (offLeft && (leftObj & (0x0010 | 0x1000)) != 0) {
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }
    if (offRight && (rightObj & (0x0010 | 0x1000)) != 0) {
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }
    if ((flags & 0xf000) != 0) {
        AddPlayerEndFlags(END_WALL_HIT);
    }
}

// Runs off the two view-state flags, not the gap itself. The view state is rate-limited, so it must settle on 2
// before a rear-end fires; a one-frame flick does not total the car.
//
// Rear-ending sets endFlags |= 3. Otherwise, with the opponent alongside and clear to our left:
//   steering left into it -> it scrapes and, every other tick, loses a segment
//   otherwise             -> if it swerved into a blocking lane, it eventually
//                            shoves us right: lateral +300, heading +-15,
//                            throttle cut for two ticks.
void RaceSim::CheckNpcContact() {
    if ((player.endFlags & END_FAILURE_MASK) != 0) {
        npcContactTicks = 0;
        return;
    }
    if (!npcAi.inContactRange) {
        return;
    }

    // Chased by the cop you cannot total yourself on it.
    if (npcAi.rearEnding && copState < 2) {
        AddPlayerEndFlags(END_FINISHED | END_HIT_NPC);
        return;
    }

    if (npcAi.viewState != 0) {
        npcContactTicks = 0;
        return;
    }
    if (player.scrollStep <= 1) {
        npcContactTicks = 0;   // crawling does not count as contact
        return;
    }
    npcContactTicks = static_cast<int16_t>(npcContactTicks + 1);

    if (player.steerVel < 0) {
        // Only bites if the road is not carrying us away from it.
        const int seg = ClampTrackIndex(player.segPos);
        const int segNext = ClampTrackIndex(player.segPos + 1);
        const int center = (edgeLeft + edgeRight) >> 1;
        if (track->curve[segNext] <= track->curve[seg] || player.lateral <= center) {
            npcBumpEvent = true;
            if (!npcScrapeAlt) {
                npc.segPos -= 1;   // every other scrape costs it a segment
                npcScrapeAlt = true;
            } else {
                npcScrapeAlt = false;
            }
        }
        return;
    }

    // The cop never shoves you off the road.
    if (!npcAi.swervedToBlock || npcContactTicks <= 3 || copState >= 2) {
        return;
    }
    // rand(aggression / 4 + 2): good drivers lean on you about a third of the time, the worst a fifth.
    if (rng.Next(ClampValue(npcSetup.aggression, 0, 120) / 4 + 2) == 0) {
        // Always +300: view state 0 only has the opponent on our left.
        player.lateral = static_cast<int16_t>(player.lateral + 300);
        if (player.speed > 20) {
            bumpStunTicks = 2;
        }
        npcAi.swervedToBlock = false;
        player.heading = static_cast<int16_t>(player.heading + ((rng.Next(2) == 0) ? -15 : 15));
        npcBumpEvent = true;
    }
    npcContactTicks = 0;
}

void RaceSim::NpcAiUpdate() {
    // The midpoint of the road a few segments ahead, not a fixed centre.
    const int aheadSeg = ClampTrackIndex(npc.segPos + 4);
    int16_t aheadLeft = 1500;
    int16_t aheadRight = 3300;
    RoadEdges(track->flags[aheadSeg], aheadLeft, aheadRight);
    const int center = (aheadLeft + aheadRight) / 2;

    // Most bands leave the target where it was; defaulting to the centre walked the opponent across our lane.
    int target = npcAi.lateralTarget;

    const int segDiff = npc.segPos - player.segPos;
    // From the previous frame. 0/1 = alongside to our left, 2..8 = ahead, 9..12 = behind.
    const int view = npcAi.viewState;

    // The side comes from a sticky preference flipped by curvature, never from the player's side, which would
    // park it inside the contact window. A narrowed road cancels the offset.
    const int curveHere = track->curve[ClampTrackIndex(player.segPos + 4)];
    const int curveNext = track->curve[ClampTrackIndex(player.segPos + 5)];
    const bool straight = (curveHere == curveNext);
    const bool narrow = (track->flags[ClampTrackIndex(npc.segPos + 4)] & 0xe0) != 0;
    auto offsetLane = [&]() {
        if (narrow) {
            target = center;
        } else if (straight) {
            target = center + (npcAi.preferLeft ? -200 : 300);
        } else if (curveHere < curveNext) {
            npcAi.preferLeft = true;
            target = center - 200;
        } else {
            npcAi.preferLeft = false;
            target = center + 300;
        }
    };

    // The cars never collide in the original; it refuses the pass instead. Roughly level and faster, the NPC lines
    // up a pass; behind with no room, it locks itself one or two segments back, on your bumper.
    if (segDiff >= 13) {
        // Gone; the target is left alone.
    } else if (segDiff >= 7) {
        npcAi.swervedToBlock = true;
        offsetLane();
    } else if (segDiff >= 3) {
        // Coming back up after being dropped: one lane swap, then the latch is spent.
        if (straight && !narrow) {
            if (npcAi.wasDroppedBack) {
                target = center + (npcAi.preferLeft ? 300 : -200);
                npcAi.wasDroppedBack = false;
                npcAi.preferLeft = !npcAi.preferLeft;
            }
        } else {
            offsetLane();
        }
    } else if (segDiff >= -3) {
        if (SpeedToScrollStep(player.speed) < SpeedToScrollStep(npc.speed)) {
            if (view >= 9) {
                // Behind and quicker: commit to a side, or give the pass up.
                const int leftLane = player.lateral - 330;
                if (edgeLeft + 480 < leftLane) {
                    npcAi.preferLeft = true;
                    target = leftLane;
                    if (player.lateral - npc.lateral < 280) {
                        npc.segPos = player.segPos - 1;
                    }
                } else if (player.lateral + 430 < edgeRight - 30) {
                    npcAi.preferLeft = false;
                    target = player.lateral + 630;
                    if (npc.lateral - player.lateral < 580) {
                        npc.segPos = player.segPos - 2;
                    }
                } else {
                    // Neither side has room at all.
                    npc.segPos = player.segPos - 2;
                }
            } else if (view == 0 || view == 2) {
                // Level and quicker: hold a lane clear of the player's line.
                target = (npc.lateral < player.lateral) ? (player.lateral - 330)
                                                        : (player.lateral + 530);
            }
        } else if (view == 0) {
            // Every other case leaves the target alone, keeping the opponent in its starting lane off the line.
            target = player.lateral - 330;
        }
    } else if (segDiff >= -5) {
        npcAi.swervedToBlock = true;
        offsetLane();
    } else {
        npcAi.wasDroppedBack = true;
        target = center;
    }

    npcAi.lateralTarget = static_cast<int16_t>(target);
    npc.lateral = static_cast<int16_t>(npc.lateral + ((npcAi.lateralTarget - npc.lateral) / 8));

    // A police pursuit never concedes; otherwise the cop gives up moments after taking over.
    const int raceThreeQuarter = RACE_START_SEG + ((finishSeg - RACE_START_SEG) * 3) / 4;
    if (copState < 2 && !npcAi.gaveUp &&
        player.segPos > raceThreeQuarter && player.segPos > npc.segPos + 2) {
        npcAi.gaveUp = true;
        npc.endFlags |= END_GAVE_UP;
    }
}

// Walks up from first gear to the one whose rpm sits in the power band, then assigns the speed outright.
void RaceSim::NpcSettleAtSpeed(int targetSpeed) {
    targetSpeed = ClampValue(targetSpeed, 0, 130);
    int gear = 1;
    while (gear < npcTables.numGears &&
           (targetSpeed * npcTables.ratio[gear]) / 100 > npcTables.rows[gear][2].rpmThreshold) {
        ++gear;
    }
    npc.gear = static_cast<int16_t>(gear);
    npc.speed = static_cast<int16_t>(targetSpeed);
    npc.rpm = static_cast<int16_t>(
        ClampValue((targetSpeed * npcTables.ratio[gear]) / 100, 1500, 5700));
}

// Ease off to a target, hold under the cop's closing cap, or accelerate. Straights only; in a curve the rpm is
// left alone.
void RaceSim::NpcEngine() {
    const int segDiff = static_cast<int>(npc.segPos) - static_cast<int>(player.segPos);
    if (segDiff >= 4 && rng.Next(64) == 0) {
        npc.rpm = static_cast<int16_t>(std::max(1500, npc.rpm - 300));
        npcAi.stallTicks = 2;
    }

    const int seg = ClampTrackIndex(npc.segPos);
    const int segNext = ClampTrackIndex(npc.segPos + 1);
    const bool onStraight = track->curve[seg] == track->curve[segNext];

    const int16_t speedBefore = npc.speed;
    bool settled = false;
    npcAi.branch = onStraight ? NpcBranch::Accelerating : NpcBranch::CorneringHold;
    npcAi.throttle = false;
    npcAi.shifted = false;
    npcAi.branchTarget = 0;

    if (onStraight) {
        npcAi.corneringCapped = false;   // re-arm the corner brake

        // The King never eases off for being far ahead in a road race.
        if (segDiff < 13 || (raceType == 1 && kingRace)) {
            // Level: 16 mph up on the player, never below its own cruise. The rubber band that keeps a close race close.
            if (segDiff > -5 && segDiff < 5) {
                int target = static_cast<int>(player.speed) + 16;
                if (target < npcAi.targetSpeedNormal) {
                    target = npcAi.targetSpeedNormal;
                }
                if (target < npc.speed) {
                    NpcSettleAtSpeed(target);
                    settled = true;
                    npcAi.branch = NpcBranch::HoldingLevel;
                    npcAi.branchTarget = static_cast<int16_t>(target);
                }
            }
        } else {
            // Well clear ahead: drop back to a cruise. The King keeps the higher target.
            const int target = (player.speed < 97 && !kingRace) ? npcAi.targetSpeedNormal
                                                                : npcAi.targetSpeedRisky;
            if (target < npc.speed) {
                NpcSettleAtSpeed(target);
                settled = true;
                npcAi.branch = NpcBranch::CruisingAhead;
                npcAi.branchTarget = static_cast<int16_t>(target);
            }
        }

        if (!settled) {
            // Over the cop's closing cap it settles back instead of accelerating.
            if (copState >= 2 && player.speed >= 51 && npc.speed > player.speed + 32) {
                NpcSettleAtSpeed(static_cast<int>(player.speed) + 32);
                settled = true;
                npcAi.branch = NpcBranch::CopClosingCap;
                npcAi.branchTarget = static_cast<int16_t>(player.speed + 32);
            } else {
                bool throttle = !npcAi.gaveUp;
                if (npcAi.stallTicks > 0) {
                    throttle = false;
                    npcAi.stallTicks -= 1;
                }
                const bool shift =
                    throttle && ((npc.gear == 0) ||
                                 (npc.gear < npcTables.numGears &&
                                  npc.rpm >= npcShiftRpm));
                npcAi.throttle = throttle;
                npcAi.shifted = shift;
                EngineTick(npc, npcTables, throttle, false, shift);
            }
        }
    } else if (!npcAi.corneringCapped) {
        // Entering a curve it brakes once to perfScore - aggression, halved while it is not ahead, so a chaser
        // corners harder than a leader.
        npcAi.corneringCapped = true;
        int penalty = ClampValue(npcSetup.aggression, 0, 120);
        if (npc.segPos <= player.segPos) {
            penalty /= 2;
        }
        const int cap = npcTables.perfScore - penalty;
        if (cap < npc.speed) {
            NpcSettleAtSpeed(cap);
            npcAi.branch = NpcBranch::CorneringCap;
            npcAi.branchTarget = static_cast<int16_t>(cap);
        }
    }

    // Past the line it sheds a flat 5 mph a tick, or 10 while close to the player.
    if (npc.segPos >= finishSeg) {
        const int drop = (player.segPos + 5 < npc.segPos) ? 5 : 10;
        npc.speed = (drop < speedBefore) ? static_cast<int16_t>(speedBefore - drop) : 0;
    }
}

}
