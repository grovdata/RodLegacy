#pragma once
// Race simulation port: BuildCarPhysicsTables (0x2437f0), base performance
// (0x2435f0), PlayerCarPhysics (0x23c5d6), UpdateRaceScroll (0x244818),
// AdvanceTrackSegment (0x23b4a0), UpdateNPCEngine (0x2444c2),
// UpdateNPCAI (0x244870), CheckTrackEdgeCollision (0x23b620).
// See RodLegacyResearch/docs/driving/physics.md and track-data.md.
// One Tick() = one original frame (the Amiga ran this once per rendered
// frame; we tick at a fixed 50 Hz).

#include <cstdint>
#include <cstdlib>

#include "race/race_types.h"

namespace srrace {

// Input word bits (original A4-0x3f0c).
constexpr uint16_t INPUT_THROTTLE = 0x01;
constexpr uint16_t INPUT_BRAKE = 0x02;
constexpr uint16_t INPUT_STEER_LEFT = 0x04;   // steer -1
constexpr uint16_t INPUT_STEER_RIGHT = 0x08;  // steer +1
constexpr uint16_t INPUT_SHIFT = 0x10;        // consumed each tick

// End/crash flag bits (original A4+0x4bc6).
constexpr uint16_t END_FINISHED = 0x01;
constexpr uint16_t END_HIT_NPC = 0x02;  // rear-ended the opponent (RaceLogicUpdate |= 3)
constexpr uint16_t END_ENGINE_BLOWN = 0x04;
// NPC-only, and not an original flag: our own "the opponent conceded in the last
// quarter" marker (NpcAiUpdate). It sits on the bit the original spends on its
// crash-damage marker, which we do not implement.
constexpr uint16_t END_GAVE_UP = 0x08;
constexpr uint16_t END_TRANS_DROPPED = 0x10;
constexpr uint16_t END_WALL_HIT = 0x20;
// Pulled over for speeding. The original's own bit: the `| 0x41` in
// RaceLogicUpdate is the only place 0x40 is ever set in the whole binary, so it
// means this and nothing else. See RodLegacyResearch/docs/driving/cop.md.
constexpr uint16_t END_COP_CAUGHT = 0x40;
constexpr uint16_t END_STALLED = 0x80;  // also used for a jumped-start false start

// Player failure states that freeze the car and end the race as a loss.
constexpr uint16_t END_FAILURE_MASK =
    END_HIT_NPC | END_ENGINE_BLOWN | END_TRANS_DROPPED | END_WALL_HIT | END_STALLED |
    END_COP_CAUGHT;

// The two impacts that bend metal and put the wrecked interior on screen.
constexpr uint16_t END_WRECK_MASK = END_WALL_HIT | END_HIT_NPC;

// Race phase: staging countdown -> running -> ended (matches the UpdatePhysics
// 0x2364e8 state machine: launch light, then the race, then a frozen car).
enum class RacePhase : uint8_t { Countdown, Running, Ended };

// Car configuration in original part-class terms.
struct CarSetup {
    int carbClass = 0;      // 0=2bbl, 1=4bbl, 2=racing
    int transClass = 2;     // 0=auto, 1=3spd, 2=4spd, 3=racing
    int engineClass = 2;    // 0..3 row in eng_x_mani matrix; V6=1, V8=2
    int manifoldClass = 0;  // 0..4 column in eng_x_mani; stock=0, perf=3, racing=4
    int tireClass = 0;      // 0=stock, 1=brand, 2=racing slicks
    int weightClass = 4;    // 0..7, indexes weight_pts
    int tuneOffset = 0;     // |tune distance from perfect|, 0..8
    int carbWear = 0;       // wear values 0..10000
    int engineWear = 0;
    int transWear = 0;
    int tireWear = 0;
    bool bumpersStripped = false;
    bool supercharger = false;
    int driverSkill = 130;  // 0..130 (0x82); player uses 130
};

struct PowerRow {
    int16_t rpmThreshold;
    int16_t rpmGain;
    int16_t rpmLoss;
};

// Tables built once per race per car (original A4+0x4b1e..0x4b9e block).
// Gear 0 is neutral; driving gears are 1..numGears.
struct CarTables {
    int numGears = 4;          // driving gears, 3 or 4
    bool automatic = false;
    int16_t ratio[5] = {0};    // rpm = ratio * mph / 100, per gear (index 0 unused)
    PowerRow rows[5][3] = {};  // torque curve rows per gear
    int16_t autoShiftRpm = 4500;  // carb band end (redline tab)
    int16_t steeringStrength = 9;
    int16_t handling = 2;      // drift factor from tire class + wear
    int16_t perfScore = 70;    // base performance 70..200 (capped 130 for AI)
};

// Base performance score (FUN_002435f0). Returns ~70..200.
int ComputeBasePerformance(const CarSetup& setup);

// Fills tables from the setup (BuildCarPhysicsTables).
void BuildCarTables(const CarSetup& setup, CarTables& tables);

// Per-car dynamic state.
struct CarState {
    int16_t speed = 0;       // mph
    int16_t rpm = 1500;      // idle 1500, redline 5700
    int16_t gear = 0;        // 0 = neutral, 1..numGears driving
    // Automatic selector (original A4+0x4b2e, 1 = N, 2 = D). Unused on manuals.
    bool autoDrive = false;
    int16_t steerVel = 0;    // input integrator, +-24 (A4+0x4b0c)
    int16_t slideVel = 0;    // decaying steer momentum (A4+0x4b0e)
    int16_t heading = 0;     // road-direction the car is pointing
    int16_t elevAtCar = 0;  // snapshot of track.elev[pos] on segment advance
    int16_t lateral = 2560;  // road spans 1500..3300
    int32_t segPos = RACE_START_SEG;
    int16_t scrollAcc = 0;
    int16_t scrollStep = 0;
    uint16_t endFlags = 0;   // END_* bits
    bool offRoad = false;
    int32_t tick = 0;
    int32_t redlineTicks = 0;   // held-at-redline counter
    int32_t distanceAcc = 0;
    // wear accumulated during the race, added to part wear (0..10000)
    int32_t engineWearDelta = 0;
    int32_t transWearDelta = 0;
    int32_t tireWearDelta = 0;
};

// NPC AI extras.
struct NpcState {
    int16_t lateralTarget = 2400;
    int16_t targetSpeedNormal = 0;  // perf*70/100
    int16_t targetSpeedRisky = 0;   // perf*80/100
    int16_t stallTicks = 0;
    bool gaveUp = false;
};

class RaceSim {
public:
    // track must outlive the sim. raceType: 0 = drag (finish 380),
    // 1 = road (finish 1020).
    // kingRace suppresses the cop entirely (InitRaceState 0x244250 gates the
    // spawn roll on A4+0x19da == 0).
    void Init(const TrackData* track, int raceType,
              const CarSetup& playerSetup, const CarSetup& npcSetup,
              uint32_t rngSeed, bool kingRace = false);

    // Advances one frame. inputBits = INPUT_* mask.
    void Tick(uint16_t inputBits);

    const CarState& Player() const { return player; }
    const CarState& Npc() const { return npc; }
    const CarTables& PlayerTables() const { return playerTables; }
    const CarTables& NpcTables() const { return npcTables; }
    const NpcState& NpcAi() const { return npcAi; }
    int RaceType() const { return raceType; }
    int FinishSegment() const { return finishSeg; }
    RacePhase Phase() const { return phase; }
    bool InCountdown() const { return phase == RacePhase::Countdown; }
    int CountdownTicks() const { return countdownTicks; }

    // Crossing the line does NOT end the race. The original runs a roll-out
    // past the finish (throttle and steering cut, brake forced on, the car
    // autopiloted back into a lane) and only stops the frame loop once the car
    // has come to a standstill - see UpdateRaceScroll 0x244818 and
    // RodLegacyResearch/docs/driving/physics.md. RaceOver() is that loop-exit
    // condition; PlayerWonAtLine() is the verdict latched as the line was
    // crossed, which is what decides the outcome.
    bool RaceOver() const {
        return ((player.endFlags | npc.endFlags) & END_FINISHED) != 0;
    }
    bool PlayerWonAtLine() const { return playerAheadAtFinish; }
    bool RollingOut() const { return player.segPos > finishSeg; }

    bool PlayerFinished() const { return (player.endFlags & END_FINISHED) != 0; }
    // Any race-ending failure, not just a crash: a blown engine, a dropped
    // transmission and a jumped start all end the race without wrecking the car.
    // Use PlayerWrecked() for the ones that bend metal.
    bool PlayerFailed() const {
        return (player.endFlags & END_FAILURE_MASK) != 0;
    }
    bool PlayerWrecked() const { return (player.endFlags & END_WRECK_MASK) != 0; }

    // True for one tick after a side-swipe with the opponent, for the audio
    // layer to hang the impact sound on. Consuming it clears it.
    bool ConsumeNpcBump() {
        const bool hit = npcBumpEvent;
        npcBumpEvent = false;
        return hit;
    }
    bool NpcFinished() const { return (npc.endFlags & END_FINISHED) != 0; }

    // Flyby / debug camera: sets player pose without running physics.
    void SetCameraState(int32_t segPos, int16_t scrollAcc, int16_t heading,
                        int16_t lateral, int16_t elevAtCar);
    void SetNpcDebugPose(int32_t segPos, int16_t lateral);

    // Road edges at the player's current segment (updated per tick).
    int16_t RoadEdgeLeft() const { return edgeLeft; }
    int16_t RoadEdgeRight() const { return edgeRight; }

    // Tire screech gate (RaceLogicUpdate 0x244d50): the original triggers the
    // squeal sample only when the slide momentum is at least half its clamp
    // (|slideVel| >= 12 of +-24) AND the car is doing more than 20 mph. Below
    // that it is silent - there is a dead zone down to |slideVel| < 10 where
    // the call is a no-op (param 0 short-circuits before AudioDevice_PlaySample
    // in the original), so nothing plays and nothing needs to be stopped.
    bool PlayerSkidding() const {
        return std::abs(player.slideVel) >= 12 && player.speed > 20;
    }

    // --- Cop pursuit (see RodLegacyResearch/docs/driving/cop.md) ---
    // 0 = no cop this race, 1 = armed and waiting for the trigger segment,
    // 2 = the cop has taken over the opponent slot and is chasing,
    // 3 = close enough that the siren is going.
    int CopState() const { return copState; }
    int32_t CopTriggerSegment() const { return copTriggerSeg; }
    // The opponent car IS the cop from state 2 on, so the renderer should be
    // drawing the police sprites.
    bool CopChasing() const { return copState >= 2; }
    bool CopSirenOn() const { return copSirenOn; }
    // True for one tick when the cop takes over, for the sprite swap.
    bool ConsumeCopSpawn() {
        const bool spawned = copSpawnEvent;
        copSpawnEvent = false;
        return spawned;
    }
    bool PlayerBusted() const { return (player.endFlags & END_COP_CAUGHT) != 0; }
    // Ticket price, tiered on the speed at the moment of capture
    // (ProcessRaceOutcome 0x23eaa2). The original adds the crash-repair bill on
    // top when its damage bit is set; we have no equivalent of that bit.
    int SpeedingFine() const {
        if (!PlayerBusted()) return 0;
        if (copCaughtSpeed < 51) return 20;
        if (copCaughtSpeed < 81) return 50;
        return 75;
    }
    int16_t CopCaughtSpeed() const { return copCaughtSpeed; }

private:
    void PlayerPhysics(uint16_t inputBits);
    void UpdateScroll(CarState& car);
    void PostFinishAutopilot();
    void CheckFinish();
    void UpdateSteering(uint16_t inputBits);
    void ApplyLateralDrift(CarState& car, int16_t handling);
    void CheckEdgeCollision();
    void CheckNpcContact();
    void UpdateCopPursuit();
    void ApplyCopRaceEnd();
    void NpcEngine();
    void NpcAiUpdate();
    void EngineTick(CarState& car, const CarTables& tables, bool throttle,
                    bool brake, bool shift);

    const TrackData* track = nullptr;
    int raceType = 1;
    int finishSeg = ROAD_FINISH_SEG;
    CarSetup playerSetup, npcSetup;
    CarTables playerTables, npcTables;
    CarState player, npc;
    NpcState npcAi;
    // Side-swipe bookkeeping (original A4-0x39a4 / A4-0x39a2 / A4-0x1386).
    int16_t npcContactTicks = 0;   // consecutive ticks running alongside the NPC
    int16_t bumpStunTicks = 0;     // throttle cut for a couple of ticks after an impact
    bool npcBumpEvent = false;     // one-shot for the audio layer
    // Finish bookkeeping. `finishSnapshot` is the original's A4-0x3994 latch,
    // set the first tick either car is at or past the line; `playerAheadAtFinish`
    // is A4+0x4cc4, the win/loss verdict taken at that instant.
    bool finishSnapshot = false;
    bool playerAheadAtFinish = false;
    // Cop pursuit (original A4+0x4cb6 state / A4+0x4cb8 trigger segment).
    int16_t copState = 0;
    int32_t copTriggerSeg = 0;
    bool copSpawnEvent = false;
    bool copSirenOn = false;
    int16_t copCaughtSpeed = 0;   // speed at capture; sets the fine tier
    int16_t edgeLeft = 1500, edgeRight = 3300;
    int16_t edgeLeftAhead = 1500, edgeRightAhead = 3300;
    RaceRng rng;
    RacePhase phase = RacePhase::Running;
    int countdownTicks = 0;
};

// Speed (mph) -> scroll step 0..16 (SpeedToScrollStep 0x24476c).
int SpeedToScrollStep(int speedMph);

}  // namespace srrace
