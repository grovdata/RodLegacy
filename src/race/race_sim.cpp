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

bool gPuristScoring = false;

}  // namespace

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

    // The terms are weighted to add up to 100 for a perfect car: carb 25,
    // transmission 15, engine/manifold 15, tune 20, weight 10, body mods 15.
    int pts = 0;
    pts += (kCarbPts[carbClass] * 25) / 100;
    pts += (kTransPts[transClass] * 15) / 100;
    pts += (kEngByManifold[engineClass * 5 + manifoldClass] * 15) / 100;
    // The tune term is the one place we do not follow the 68k by default. The
    // original writes (1 - |tune|), which tops out at +2 instead of the +20
    // the rest of the scale is built around and leaves the 130 mph cap
    // unreachable; --purist restores it. See top_speed.md.
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

    // Starting lanes. The player sits at a fixed 0xa00, and the opponent is
    // placed 200 left of the road centre - NOT on the centre line (the original
    // race init does `lateral = ((edgeLeft + edgeRight) >> 1) - 200` and seeds
    // its steering target with the same value). That matters: centring the
    // opponent leaves only a 160-unit gap, inside the 180 that CheckNpcContact
    // counts as laterally overlapping, so the pair start the race already
    // touching and the first car to pull ahead "rear-ends" the other.
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
    npcAi.targetSpeedNormal = static_cast<int16_t>((npcTables.perfScore * 70) / 100);
    npcAi.targetSpeedRisky = static_cast<int16_t>((npcTables.perfScore * 80) / 100);
    npcAi.stallTicks = 0;
    npcAi.gaveUp = false;

    // Staging countdown before the green (UpdatePhysics 0x2364e8 picks a random
    // launch time). ~2-3s at 50 Hz; leaving the line early is a false start.
    phase = RacePhase::Countdown;
    countdownTicks = 100 + rng.Next(50);

    // Cop spawn roll (InitRaceState 0x244250). Road races only, never the
    // King's, 1 in 4, and the trigger sits at 510 + rand(255) - which on a road
    // course running 200..1020 always lands in the middle-to-late stretch.
    //
    // Drawn from a seed derived off the main one rather than from `rng` itself.
    // The original spends draws from its shared stream here, but ours is not
    // bit-identical to the original's anyway, so there is no fidelity to gain -
    // and taking two draws at init would shift every later draw in the race,
    // silently re-rolling NPC behaviour and the wear/failure checks that the
    // existing races are tuned and regression-tested against.
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
    // The cop machine runs straight after the NPC engine and BEFORE the finish
    // is detected, exactly as in RaceLogicUpdate 0x244d50. The order matters:
    // ApplyCopRaceEnd below has to see the finish result of this same tick.
    UpdateCopPursuit();

    UpdateScroll(player);
    UpdateScroll(npc);

    ApplyLateralDrift(player, playerTables.handling);

    const int playerSeg = ClampTrackIndex(player.segPos);
    const int playerSegAhead = ClampTrackIndex(player.segPos + 4);
    RoadEdges(track->flags[playerSeg], edgeLeft, edgeRight);
    RoadEdges(track->flags[playerSegAhead], edgeLeftAhead, edgeRightAhead);

    CheckEdgeCollision();
    // All car-to-car contact is gated on the player still being short of the
    // line (RaceLogicUpdate 0x244d50 tests `pos < finish` before either the
    // rear-end or the side-swipe), so the roll-out cannot be crashed out.
    if (player.segPos < finishSeg) {
        CheckNpcContact();
    }
    CheckFinish();
    ApplyCopRaceEnd();

    // Last, so the AI and the contact test above read the state the original's
    // render pass left behind on the PREVIOUS frame - RaceLogicUpdate 0x244d50
    // acts on its flags one frame after RenderNPCCarAndUpdateCopState sets them.
    UpdateNpcViewState();

    if ((player.endFlags & END_FAILURE_MASK) != 0) {
        phase = RacePhase::Ended;
    }
}

// The state half of RenderNPCCarAndUpdateCopState 0x237ea8. A raw state is read
// off the fine gap, then walked toward - at most one step per frame - through a
// transition table. The rate limit is the whole point: the states pick both the
// sprite and its projected distance, so snapping to the raw value makes the
// opponent jump between sizes and positions whenever the gap crosses a boundary.
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
        // Level. Overlapping laterally reads as 2 unless we are already behind,
        // and only a clear gap to our LEFT gets the alongside view.
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
            // prevViewState == raw means the gap has come straight back to where
            // it was two frames ago, and the original snaps rather than crawling.
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

// The opponent slot becomes the police car (SpawnCopCar 0x244ca2). The original
// swaps the car record for a fixed cop one, re-gears the car so it carries on at
// the speed it already had, and recomputes the AI's target speeds from that
// car's performance - a police interceptor is not limited to whatever the
// challenger happened to be driving.
void RaceSim::SpawnCop() {
    // Car spec 25 in SR_data, which the original scores the cop against: GM,
    // racing carburettor, racing transmission, racing slicks. See
    // RodLegacyResearch/docs/formats/sr_data.md.
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

    // 60% and 80% of the cop's performance, against the 70/80 an ordinary
    // opponent uses (the multipliers are 0x1770/0x1f40 over 0x2710 in the
    // original). Its performance is high enough that both still beat a typical
    // challenger.
    npcAi.targetSpeedNormal = static_cast<int16_t>((npcTables.perfScore * 60) / 100);
    npcAi.targetSpeedRisky = static_cast<int16_t>((npcTables.perfScore * 80) / 100);

    // A cop does not concede. Without this it inherits the challenger's
    // give-up state, which by this point in the race it has usually entered -
    // the conditions for conceding and for the cop appearing nearly coincide.
    npcAi.gaveUp = false;
    npcAi.stallTicks = 0;
    npc.endFlags &= static_cast<uint16_t>(~END_GAVE_UP);

    // Keep the speed it was carrying; the gear is re-derived from the new tables.
    npc.speed = speedBefore;
    copSpawnEvent = true;
}

// Cop pursuit state machine (RaceLogicUpdate 0x244d50). The cop does not spawn
// as a third car: from state 2 it IS the opponent, because the original swaps
// the opponent's car record and its 13 sprite pointers for the police ones
// (SpawnCop 0x244ca2). Here that swap is just a flag the renderer reads.
void RaceSim::UpdateCopPursuit() {
    if (copState == 0) {
        return;
    }
    const int pSeg = static_cast<int>(player.segPos);
    const int nSeg = static_cast<int>(npc.segPos);

    if (copState == 1) {
        if (pSeg >= copTriggerSeg) {
            if (pSeg < nSeg + 5) {
                // Not far enough clear of the opponent: no cop at all this race.
                // You only get pulled over for leading.
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

    // State 3: caught the moment it is more than 2 segments ahead of us.
    if (pSeg < nSeg - 2) {
        copCaughtSpeed = player.speed;
        player.endFlags |= (END_FINISHED | END_COP_CAUGHT);
        copSirenOn = false;
    }
}

// The tail of RaceLogicUpdate, run after the finish has been resolved. This is
// where winning saves you: the original re-tests its failure word before ORing
// in the caught bit, and UpdateRaceScroll 0x244818 records a WIN in a different
// word (0x4c8e) than a LOSS (0x4bc6). So a win leaves the failure word at zero,
// skips the OR and only silences the siren. See docs/driving/cop.md.
void RaceSim::ApplyCopRaceEnd() {
    if (copState < 2) {
        return;
    }
    const bool playerEnded = player.endFlags != 0;
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
    // A side-swipe knocks the throttle out for a couple of ticks (original
    // A4-0x39a2, decremented at the top of PlayerCarPhysics 0x23c5d6).
    if (bumpStunTicks > 0) {
        bumpStunTicks -= 1;
        throttle = false;
    }
    EngineTick(player, playerTables, throttle, brake, shift);
}

void RaceSim::EngineTick(CarState& car, const CarTables& tables, bool throttle,
                         bool brake, bool shift) {
    // Gear 0 is neutral (original A4+0x4b22 "0 = neutral/1st index base");
    // driving gears are 1..numGears.
    const int gear = ClampValue<int>(car.gear, 0, tables.numGears);
    const PowerRow* gearRows = tables.rows[gear];
    // rpmThreshold is the row's UPPER bound: the three rows span [0, bandStart],
    // [bandStart, bandEnd] and [bandEnd, 5700]. The original walks them with
    // `while (row[i].upper < rpm) ++i` capped at 2 (BuildCarPhysicsTables
    // 0x2437f0 lays the bounds out in pairs, PlayerCarPhysics 0x23c5d6 walks
    // them), i.e. the first row whose upper bound still covers the current rpm.
    // We were comparing against rows 1 and 2 instead of 0 and 1, which shifted
    // the whole power band up one row: full power was applied from bandEnd all
    // the way to the limiter, where the original drops to a tenth of it. That is
    // what made the engine so easy to wind past the redline.
    int rowIndex = 0;
    if (car.rpm > gearRows[0].rpmThreshold) {
        rowIndex = 1;
    }
    if (car.rpm > gearRows[1].rpmThreshold) {
        rowIndex = 2;
    }
    const int gain = gearRows[rowIndex].rpmGain;
    const int loss = gearRows[rowIndex].rpmLoss;

    // Roll-out past the line (PlayerCarPhysics 0x23c5d6, at the very top of the
    // function): once `finish < pos` - strictly past, so the finish segment
    // itself is still raced - the throttle and the steering are taken away, the
    // brake is forced on, and speed sheds a flat 15 mph a tick on top of the
    // braking below. The forced brake is the point: it puts the speed model in
    // its decay branch, which is the only path that reaches 0, and a standstill
    // is what ends the race (CheckFinish).
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

    // An automatic is governed ten rpm below the redline (PlayerCarPhysics
    // 0x23c5d6 caps it at 0x163a = 5690 in the `automatic` branch), while the
    // over-rev blow counts ticks spent past a FIXED 0x1644 = 5700. The gap is the
    // point: a governed automatic can never reach the threshold, so it cannot be
    // blown by holding the throttle. Testing the counter against the cap instead
    // of against 5700 blew automatics the instant they hit the governor - the
    // exact opposite of the original.
    constexpr int kRedlineRpm = 5700;
    const int rpmCap = tables.automatic ? 5690 : kRedlineRpm;
    if (car.rpm > rpmCap) {
        car.rpm = static_cast<int16_t>(rpmCap);
    }

    if (throttle && car.rpm >= kRedlineRpm) {
        car.redlineTicks++;
        int wearNow = 0;
        if (&car == &player) {
            wearNow = ClampValue(playerSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        } else {
            wearNow = ClampValue(npcSetup.engineWear + static_cast<int>(car.engineWearDelta), 0, 10000);
        }
        const int blowLimit = std::max(1, 15 - (wearNow / 1000));
        if (car.redlineTicks >= blowLimit) {
            if (&car == &player) {
                AddPlayerEndFlags(END_ENGINE_BLOWN);
            } else {
                car.endFlags |= END_ENGINE_BLOWN;
            }
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
        // Automatic in D: engage first gear from neutral, then upshift at the
        // carb power-band rpm. The original gates this on the selector being past
        // N (PlayerCarPhysics 0x23c5d6: `automatic && 1 < A4+0x4b2e`).
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

    // Player only: the NPC has no selector (the original drives it from
    // UpdateNPCEngine 0x2444c2, and NpcEngine below synthesises a shift on every
    // upshift, which would toggle a selector straight back to N).
    if (tables.automatic && &car == &player) {
        // Automatic selector, N <-> D (original A4+0x4b2e, PlayerCarPhysics
        // 0x23c5d6). The shift input toggles it; it drops back to N whenever the
        // box ends up out of gear or the car coasts to a crawl off the throttle;
        // and while it is in N the box is held in neutral. That last rule is what
        // lets an automatic sit on the line and rev without rolling - the throttle
        // used to engage first gear straight out of neutral, which moved the car
        // and read as a jumped start.
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

    // Speed. This is one if/else in the original, NOT a drive step followed by a
    // braking pass (PlayerCarPhysics 0x23c5d6):
    //
    //   if !braking && (!offroad || speed < 9):   drive - speed follows rpm
    //   else:                                     decay - rpm follows speed
    //
    // Which way the dependency runs is the whole game. In the decay branch speed
    // is never re-derived from rpm, so it walks down to 0 while rpm bottoms out
    // at its 1500 idle floor. Running the two as sequential steps instead let the
    // next tick recompute `speed = rpm/ratio` from that floored rpm, which pinned
    // a braking car at ~14 mph forever - nothing could ever come to a stop, and
    // the roll-out past the finish had no way to terminate.
    const int curRatio = std::max<int>(tables.ratio[std::max<int>(car.gear, 1)], 1);
    const bool decaying = brake || (car.offRoad && car.speed >= 9);
    if (!decaying) {
        if (car.gear == 0) {
            // Neutral. Standing still the engine revs freely - that is the blip
            // on the start line, and the rpm gain above is all that moves the
            // needle. ROLLING, though, the original pins the needle straight to
            // road speed (`speed -= 1; rpm = speed*30 + 1500`), so you cannot
            // wind it up while coasting out of gear.
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

void RaceSim::UpdateSteering(uint16_t inputBits) {
    int steer = 0;
    if ((inputBits & INPUT_STEER_LEFT) != 0) {
        steer -= 1;
    }
    if ((inputBits & INPUT_STEER_RIGHT) != 0) {
        steer += 1;
    }
    // Past the line the wheel is taken away (PlayerCarPhysics clears its steering
    // flag, which zeroes steerVel and leaves only the slide decay running). The
    // autopilot writes slideVel directly on each segment advance.
    if (player.segPos > finishSeg) {
        steer = 0;
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
        // Heading is measured against the road-direction array, which in this
        // engine is curve[] (A4+0x1098) - the same array that drives the lateral
        // drift and the horizontal road projection (RenderTrackFrame 0x23b77c).
        const int roadDir = track->curve[ClampTrackIndex(player.segPos)];
        if (std::abs(player.heading - sv - roadDir) < 200) {
            player.heading = static_cast<int16_t>(player.heading - ((sv >> 1) + 1));
        }
    }
}

void RaceSim::ApplyLateralDrift(CarState& car, int16_t handling) {
    const int seg = ClampTrackIndex(car.segPos);
    // Lateral drift uses the road-direction array curve[] (A4+0x1098), NOT elev[]
    // - physics.md had this backwards. This is the same array the horizontal road
    // projection reads, so the road and the car stay coupled through a bend.
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
        // The original hangs the roll-out autopilot off the segment advance, not
        // off the frame (AdvanceTrackSegment 0x23b4a0), so it only steps when the
        // car crosses into a new segment.
        if (&car == &player && player.segPos > finishSeg) {
            PostFinishAutopilot();
        }
    }
}

// Steering past the finish line (AdvanceTrackSegment 0x23b4a0, the `finish < pos`
// tail). The player no longer has the wheel: the car is walked into a lane and
// its heading is snapped to the road direction so it tracks straight while it
// brakes. This also runs for the cop takeover, which reaches the same block.
void RaceSim::PostFinishAutopilot() {
    // Lane choice depends on where the opponent sits, so the two cars part to
    // opposite sides for the drive-off: opponent left of 0x961 -> we take 0xa00,
    // otherwise we take 0x820.
    const int16_t lane = (npc.lateral < 0x961) ? 0xa00 : 0x820;
    const int delta = std::abs(static_cast<int>(player.lateral) - lane);
    if (delta < 0x65) {
        player.lateral = lane;
    } else {
        // Step to within 100 of the lane on whichever side we are; the next
        // segment lands exactly on it.
        player.lateral = static_cast<int16_t>(lane + ((player.lateral <= lane) ? -100 : 100));
    }

    const int roadDir = track->curve[ClampTrackIndex(player.segPos)];
    if (roadDir != player.heading) {
        int slide = (roadDir - player.heading) * -2;
        slide = ClampValue<int>(slide, -24, 24);
        player.slideVel = static_cast<int16_t>(slide);
    }
    // Heading snaps to the road, so ApplyLateralDrift stops pushing the car
    // sideways for the rest of the roll-out.
    player.heading = static_cast<int16_t>(roadDir);
}

// The race is over once the car has stopped past the line (UpdateRaceScroll
// 0x244818). The win/loss verdict is NOT decided here - it was latched the first
// tick either car reached the finish (ComputeNPCPosition 0x243f1c).
void RaceSim::CheckFinish() {
    if (!finishSnapshot &&
        (player.segPos >= finishSeg || npc.segPos >= finishSeg)) {
        const int gap = (static_cast<int>(npc.segPos) - static_cast<int>(player.segPos)) * 8 +
                        (npc.scrollAcc - player.scrollAcc);
        playerAheadAtFinish = (gap <= 0);
        finishSnapshot = true;
    }

    if (finishSnapshot && player.segPos >= finishSeg && player.speed < 1) {
        // The original flags whichever car "finished" in the losing sense, and
        // the frame loop exits on either flag word being set.
        if (playerAheadAtFinish) {
            npc.endFlags |= END_FINISHED;
        } else {
            player.endFlags |= END_FINISHED;
        }
    }
}

// An event carrying a suppressed bit is dropped whole, END_FINISHED included -
// otherwise a suppressed crash would still end the race, just without the cause.
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

// Car-to-car contact (RaceLogicUpdate 0x244d50). Nothing here measures the gap
// itself: the whole thing runs off the two flags the view-state pass leaves
// behind, `inContactRange` (A4-0x1386) and `rearEnding` (A4-0x1384). That matters
// because the view state is rate-limited, so it has to SETTLE on 2 before a
// rear-end can fire - a gap that flicks through the window for one frame does
// not total your car.
//
// Rear-ending sets endFlags |= 3 (bit 0 ends the race, bit 1 is the crash) - the
// wreck screen. Otherwise the opponent has to be in view state 0, alongside and
// clear to our LEFT, and then it splits on which way we are steering:
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

    // While the cop is chasing you cannot total yourself on it: the original
    // gates the rear-end branch on `copState <= 1` and falls through to the
    // side-contact path instead.
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
        // Leaning left onto it. Only bites if the road is not already carrying us
        // away from it - the bend must not be turning right, or we must be left
        // of the road centre.
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

    // The cop never shoves you off the road (same `copState < 2` gate).
    if (!npcAi.swervedToBlock || npcContactTicks <= 3 || copState >= 2) {
        return;
    }
    // Rolled rather than bumped every tick, against the driver's aggression byte
    // (0x00244f96: rand(rec[2] / 4 + 2)). Lower is a better driver, so the good
    // ones lean on you roughly a third of the time and the worst a fifth.
    if (rng.Next(ClampValue(npcSetup.aggression, 0, 120) / 4 + 2) == 0) {
        // Always +300: view state 0 only ever has the opponent on our left.
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
    // Aim for the midpoint of the road a few segments ahead (UpdateNPCAI 0x244870
    // steers to the average of the look-ahead edges, not a fixed center).
    const int aheadSeg = ClampTrackIndex(npc.segPos + 4);
    int16_t aheadLeft = 1500;
    int16_t aheadRight = 3300;
    RoadEdges(track->flags[aheadSeg], aheadLeft, aheadRight);
    const int center = (aheadLeft + aheadRight) / 2;

    // Most of UpdateNPCAI's segDiff bands leave the target where it was and only
    // a few write it. Defaulting it to the road centre every tick is what let the
    // opponent be walked across the player's lane off the line.
    int target = npcAi.lateralTarget;

    const int segDiff = npc.segPos - player.segPos;
    // The render view state, from the previous frame - the same one-frame lag the
    // original carries by reading A4-0x1388 here. 0/1 = alongside to our left,
    // 2..8 = ahead, 9..12 = behind.
    const int view = npcAi.viewState;

    // The offset lane every band outside +-3 segments uses. The side comes from a
    // sticky preference flipped by the road's curvature (A4-0x39a8), NOT from
    // which side of the centre line the player is on - the original never steers
    // the opponent at the player, and doing so parked it inside the 180 units
    // CheckNpcContact reads as overlapping. A narrowed road cancels the offset.
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

    // The two cars never collide in the original: nothing anywhere in the Amiga
    // binary compares the two laterals for overlap, and END_WALL_HIT comes only
    // from CheckTrackEdgeCollision (0x23b620), which tests the player against the
    // road edges and the roadside-object bits. What the original does instead is
    // refuse the pass. Once the cars are roughly level (segDiff in [-3,3)) and the
    // NPC is the faster of the two, it stops aiming at the road centre and lines
    // up a pass; if it is behind and no side has room it aborts and locks itself
    // one or two segments behind the player, so it sits on your bumper and cannot
    // get by. Original: UpdateNPCAI 0x244870.
    if (segDiff >= 13) {
        // Gone; the target is left alone.
    } else if (segDiff >= 7) {
        npcAi.swervedToBlock = true;
        offsetLane();
    } else if (segDiff >= 3) {
        // Coming back up past the player after having been dropped: one lane
        // swap, then the latch is spent.
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
            // Level, no quicker, and sitting off our left: move further away.
            // Every other case here leaves the target untouched, which is what
            // keeps the opponent in its starting lane off the line.
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

    // A police pursuit never concedes. The conditions for conceding and for the
    // cop appearing nearly coincide - both want the player well clear and late
    // in the race - so without this the cop gives up moments after taking over.
    const int raceThreeQuarter = RACE_START_SEG + ((finishSeg - RACE_START_SEG) * 3) / 4;
    if (copState < 2 && !npcAi.gaveUp &&
        player.segPos > raceThreeQuarter && player.segPos > npc.segPos + 2) {
        npcAi.gaveUp = true;
        npc.endFlags |= END_GAVE_UP;
    }
}

// FUN_0024441c. The original does not decelerate an opponent toward a target:
// it walks up from first gear to whichever gear puts the resulting rpm inside
// its power band, then assigns the speed outright.
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

// UpdateNPCEngine 0x2444c2. Three outcomes rather than one speed cap: ease off
// to a target, hold station under the cop's closing cap, or accelerate. Which
// one applies depends on where the opponent is relative to the player, and the
// whole thing only runs on straights - through a curve the original leaves the
// rpm alone, so the car simply holds its speed.
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
        // The King never eases off for being far ahead in a road race: it stays
        // in the level branch whatever the gap, so nothing throttles it back.
        if (segDiff < 13 || (raceType == 1 && kingRace)) {
            // Level with the player: run 16 mph up on them, but never below the
            // car's own cruising pace. This is the rubber band that keeps a
            // close race close.
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
            // Well clear ahead: drop back to a cruise. The King is the exception
            // and keeps the higher target (see docs/driving/king.md).
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
            // The cop's closing cap gates the acceleration itself: over the cap
            // it settles back to it instead of accelerating further.
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
                                  npc.rpm >= npcTables.autoShiftRpm));
                npcAi.throttle = throttle;
                npcAi.shifted = shift;
                EngineTick(npc, npcTables, throttle, false, shift);
            }
        }
    }

    // NOT PORTED: through a curve the original also caps the opponent once, at
    // perfScore minus its driver skill byte (halved when it is behind). Our
    // CarSetup::driverSkill is on a different scale to that byte, so the cap
    // would be a guess; the opponent currently holds its speed through curves.

    // Past the line the opponent sheds a flat 5 mph a tick, or 10 while it is
    // still close to the player, applied to the speed it had coming in.
    if (npc.segPos >= finishSeg) {
        const int drop = (player.segPos + 5 < npc.segPos) ? 5 : 10;
        npc.speed = (drop < speedBefore) ? static_cast<int16_t>(speedBefore - drop) : 0;
    }
}

}  // namespace srrace
