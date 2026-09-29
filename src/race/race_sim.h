#pragma once
// One Tick() = one original frame.

#include <cstdint>
#include <cstdlib>

#include "race/race_types.h"

namespace srrace {

// The original steps once per rendered frame and times the race as 6 steps a second.
constexpr int SIM_HZ_ORIGINAL = 6;
constexpr int SIM_HZ_MAX = 15;
// Race speed in sim steps a second, clamped to SIM_HZ_ORIGINAL..SIM_HZ_MAX.
void SetSimHz(int hz);
int SimHz();

// Input word bits.
constexpr uint16_t INPUT_THROTTLE = 0x01;
constexpr uint16_t INPUT_BRAKE = 0x02;
constexpr uint16_t INPUT_STEER_LEFT = 0x04;   // steer -1
constexpr uint16_t INPUT_STEER_RIGHT = 0x08;  // steer +1
constexpr uint16_t INPUT_SHIFT = 0x10;        // consumed each tick

// End/crash flag bits.
constexpr uint16_t END_FINISHED = 0x01;
constexpr uint16_t END_HIT_NPC = 0x02;  // rear-ended the opponent (RaceLogicUpdate |= 3)
constexpr uint16_t END_ENGINE_BLOWN = 0x04;
// Not a failure bit: the car pulls hard to one side for the rest of the race. At the finish it picks the
// blown-tire wordings and makes the garage fit a fresh set.
constexpr uint16_t END_TIRES_BLOWN = 0x08;
constexpr uint16_t END_TRANS_DROPPED = 0x10;
constexpr uint16_t END_WALL_HIT = 0x20;
// The only place the original sets 0x40.
constexpr uint16_t END_COP_CAUGHT = 0x40;
constexpr uint16_t END_STALLED = 0x80;  // also used for a jumped-start false start
// NPC-only and not an original flag: the opponent conceded in the last quarter.
constexpr uint16_t END_GAVE_UP = 0x0100;

// Player failure states that freeze the car and end the race as a loss.
constexpr uint16_t END_FAILURE_MASK =
    END_HIT_NPC | END_ENGINE_BLOWN | END_TRANS_DROPPED | END_WALL_HIT | END_STALLED |
    END_COP_CAUGHT;

// The two impacts that bend metal and put the wrecked interior on screen.
constexpr uint16_t END_WRECK_MASK = END_WALL_HIT | END_HIT_NPC;

// Tires wear once per race, only past TIRE_WEAR_MIN_SEGMENTS. Stock tires wear about twice as fast, and
// the road course more than the strip. `roll300` is rand(300). Units are 0..10000.
constexpr int TIRE_WEAR_MIN_SEGMENTS = 200;
inline int TireWearForRace(int tireClass, bool isRoadRace, int roll300) {
    const int base = (tireClass == 0) ? (isRoadRace ? 700 : 300)
                                      : (isRoadRace ? 400 : 200);
    return base + roll300;
}

// Staging countdown (launch light) -> running -> ended (frozen car).
enum class RacePhase : uint8_t { Countdown, Running, Ended };

struct CarSetup {
    int carbClass = 0;      // 0=2bbl, 1=4bbl, 2=racing
    int transClass = 2;     // 0=auto, 1=3spd, 2=4spd, 3=racing
    int engineClass = 2;    // 0..3 row in eng_x_mani; part tier V6=0, 283=1, 327=2
    int manifoldClass = 0;  // 0..4 column in eng_x_mani; stock=0, perf=3, racing=4
    int tireClass = 0;      // 0=stock, 1=brand, 2=racing slicks
    int weightClass = 4;    // SR_data car-spec class byte 0..8, indexes weight_pts
    int tuneOffset = 0;     // |tune distance from perfect|, 0..8
    int carbWear = 0;       // wear values 0..10000
    int engineWear = 0;
    int transWear = 0;
    int tireWear = 0;
    // Body mods. A mod the body cannot take scores as if it were done.
    bool roofChopped = false;         // +9
    bool frontBumperRemoved = false;  // +5
    bool rearBumperRemoved = false;   // +1
    bool supercharger = false;
    int driverSkill = 130;  // 0..130 (0x82); player uses 130
    // Driver record byte [2]: 0x04 (best) .. 0x0c (worst), for the side-swipe roll and cornering cap.
    int aggression = 8;
    // Driver record byte [1]: 0x0a (best) .. 0x14 (worst). The opponent's shift point, band end - reaction * 100.
    // Manuals only.
    int reaction = 10;
};

struct PowerRow {
    int16_t rpmThreshold;
    int16_t rpmGain;
    int16_t rpmLoss;
};

// Gear 0 is neutral; driving gears are 1..numGears.
struct CarTables {
    int numGears = 4;          // driving gears, 3 or 4
    bool automatic = false;
    int16_t ratio[5] = {0};    // rpm = ratio * mph / 100, per gear (index 0 unused)
    PowerRow rows[5][3] = {};  // torque curve rows per gear
    int16_t autoShiftRpm = 4500;  // carb band end (redline tab)
    int16_t steeringStrength = 9;
    int16_t handling = 2;      // drift factor from tire class + wear
    int16_t perfScore = 70;    // base performance 70..100
    int16_t topSpeed = 91;     // mph, ComputeTopSpeed - the gear ratios key off it
};

// Purist: tune scored as (1 - |tune|) * 20/8 as on the 68k (the stock 1940 Chevy reads 92 mph). Default is
// (8 - |tune|) * 20/8, which lets a perfect car reach the 130 cap.
void SetPuristScoring(bool enabled);
bool PuristScoring();

// 70 + a 0..100 point score scaled by the transmission, so 70..100 (67..94 in purist mode).
int ComputeBasePerformance(const CarSetup& setup);

// base * 13/10, capped at 130: the car info screen's "Max speed", the pool's match value, and what the top
// gear is built to reach at redline.
int ComputeTopSpeed(const CarSetup& setup);

void BuildCarTables(const CarSetup& setup, CarTables& tables);

struct CarState {
    int16_t speed = 0;       // mph
    int16_t rpm = 1500;      // idle 1500, redline 5700
    int16_t gear = 0;        // 0 = neutral, 1..numGears driving
    // 1 = N, 2 = D. Unused on manuals.
    bool autoDrive = false;
    int16_t steerVel = 0;    // input integrator, +-24
    int16_t slideVel = 0;    // decaying steer momentum
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
};

// Blends the drawn position of a car from the previous sim step towards the current one, t in 0..1.
// A jump longer than 32 segments (a debug pose, a cruise start) is shown as it is.
void BlendCarView(const CarState& prev, CarState& cur, float t);

// What the opponent's engine logic decided last tick, for the debug UI only.
enum class NpcBranch : uint8_t {
    Accelerating,   // nothing capping it
    HoldingLevel,   // eased off to player speed + 16 while alongside
    CruisingAhead,  // eased off to its cruise, well clear of the player
    CopClosingCap,  // cop holding station at player speed + 32
    CorneringHold,  // in a curve, rpm untouched
    CorneringCap,   // entering a curve, backed off to perf - aggression
};

struct NpcState {
    int16_t lateralTarget = 2400;
    int16_t targetSpeedNormal = 0;  // perf*70/100
    int16_t targetSpeedRisky = 0;   // perf*80/100
    int16_t stallTicks = 0;
    bool gaveUp = false;
    // Sticky lane preference, a latch arming one lane swap after falling far behind, and one arming a side-swipe.
    bool preferLeft = false;
    bool wasDroppedBack = false;
    bool swervedToBlock = false;
    // Fires on the first curve segment and re-arms on the next straight.
    bool corneringCapped = false;

    // 0/1 = alongside to our left, 2..8 = ahead in 16 gap-unit steps, 9..12 = behind in the mirror. Moves at most
    // one step per frame so the sprite does not snap between sizes.
    int16_t viewState = 0;
    int16_t prevViewState = 0;
    bool viewHidden = false;    // walked off the end of the range: draw nothing
    bool inContactRange = false;
    bool rearEnding = false;

    NpcBranch branch = NpcBranch::Accelerating;
    bool throttle = false;
    bool shifted = false;
    int16_t branchTarget = 0;   // the speed it eased to, when it eased
};

class RaceSim {
public:
    // track must outlive the sim. raceType: 0 = drag (finish 380), 1 = road (finish 1020). kingRace suppresses the cop.
    void Init(const TrackData* track, int raceType,
              const CarSetup& playerSetup, const CarSetup& npcSetup,
              uint32_t rngSeed, bool kingRace = false);

    // steerAmount 0..1; 1.0 reproduces the original's held-direction steering. Only an analog stick with
    // analog_steering=1 passes anything else.
    void Tick(uint16_t inputBits, float steerAmount = 1.0f);

    const CarState& Player() const { return player; }
    const CarState& Npc() const { return npc; }
    const CarTables& PlayerTables() const { return playerTables; }
    const CarTables& NpcTables() const { return npcTables; }
    const NpcState& NpcAi() const { return npcAi; }
    int RaceType() const { return raceType; }
    int FinishSegment() const { return finishSeg; }
    // A cruise starts further back and runs to the drag finish at the arch.
    void BeginCruiseAt(int seg) {
        player.segPos = seg;
        npc.segPos = seg;
        // A cruise has no start line: the staging countdown would call any movement a false start.
        phase = RacePhase::Running;
        countdownTicks = 0;
    }
    int32_t PlayerSegment() const { return player.segPos; }
    RacePhase Phase() const { return phase; }
    bool InCountdown() const { return phase == RacePhase::Countdown; }
    int CountdownTicks() const { return countdownTicks; }

    // Crossing the line does not end the race: a roll-out follows until the car stops. PlayerWonAtLine() is the
    // verdict latched at the line.
    bool RaceOver() const {
        return ((player.endFlags | npc.endFlags) & END_FINISHED) != 0;
    }
    bool PlayerWonAtLine() const { return playerAheadAtFinish; }
    bool RollingOut() const { return player.segPos > finishSeg; }

    // Any race-ending failure; PlayerWrecked() is the subset that bends metal.
    bool PlayerFailed() const {
        return (player.endFlags & END_FAILURE_MASK) != 0;
    }
    bool PlayerWrecked() const { return (player.endFlags & END_WRECK_MASK) != 0; }

    // True for one tick after a side-swipe; consuming clears it.
    bool ConsumeNpcBump() {
        const bool hit = npcBumpEvent;
        npcBumpEvent = false;
        return hit;
    }

    // Suppresses crashes and blown engines; not the cop, the finish or a false start.
    void SetInvulnerable(bool on) { invulnerable = on; }
    bool Invulnerable() const { return invulnerable; }

    // Flyby / debug camera: sets player pose without running physics.
    void SetCameraState(int32_t segPos, int16_t scrollAcc, int16_t heading,
                        int16_t lateral, int16_t elevAtCar);
    void SetNpcDebugPose(int32_t segPos, int16_t lateral);

    // |slideVel| >= 12 of +-24 and over 20 mph.
    bool PlayerSkidding() const {
        return std::abs(player.slideVel) >= 12 && player.speed > 20;
    }

    // 0 = no cop this race, 1 = armed, 2 = chasing in the opponent slot, 3 = siren.
    int CopState() const { return copState; }
    int32_t CopTriggerSegment() const { return copTriggerSeg; }
    // 0 = they hold this race.
    int32_t TireBlowSegment() const { return tireBlowSeg; }
    bool PlayerTiresBlown() const { return (player.endFlags & END_TIRES_BLOWN) != 0; }
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
    // Tiered on the speed at capture. The original adds a crash bill on its damage bit, which we lack.
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
    void UpdateSteering(uint16_t inputBits, float steerAmount);
    void ApplyLateralDrift(CarState& car, int16_t handling);
    // One place for the invulnerable toggle to filter.
    void AddPlayerEndFlags(uint16_t flags);
    void CheckEdgeCollision();
    void CheckNpcContact();
    void SpawnCop();
    void UpdateCopPursuit();
    void ApplyCopRaceEnd();
    // Re-gears the opponent and snaps it to a speed.
    void NpcSettleAtSpeed(int targetSpeed);
    void NpcEngine();
    void NpcAiUpdate();
    // In the sim because the AI and the contact test both read it.
    void UpdateNpcViewState();
    void EngineTick(CarState& car, const CarTables& tables, bool throttle,
                    bool brake, bool shift);

    const TrackData* track = nullptr;
    int raceType = 1;
    int finishSeg = ROAD_FINISH_SEG;
    CarSetup playerSetup, npcSetup;
    CarTables playerTables, npcTables;
    CarState player, npc;
    NpcState npcAi;
    int16_t npcContactTicks = 0;   // consecutive ticks running alongside the NPC
    bool npcScrapeAlt = false;     // only every other scrape costs it a segment
    int16_t bumpStunTicks = 0;     // throttle cut for a couple of ticks after an impact
    bool npcBumpEvent = false;     // one-shot for the audio layer
    // finishSnapshot is set the first tick either car reaches the line; playerAheadAtFinish is the verdict then.
    bool finishSnapshot = false;
    bool playerAheadAtFinish = false;
    // Re-derived when the cop takes the slot, which brings a different car and driver.
    int16_t npcShiftRpm = 0;
    int16_t copState = 0;
    int32_t copTriggerSeg = 0;
    // 0 = they hold this race. Rolled at Init.
    int32_t tireBlowSeg = 0;
    bool copSpawnEvent = false;
    bool copSirenOn = false;
    int16_t copCaughtSpeed = 0;   // speed at capture; sets the fine tier
    bool invulnerable = false;
    bool kingRace = false;
    int16_t edgeLeft = 1500, edgeRight = 3300;
    int16_t edgeLeftAhead = 1500, edgeRightAhead = 3300;
    RaceRng rng;
    RacePhase phase = RacePhase::Running;
    int countdownTicks = 0;
};

// Speed (mph) -> scroll step 0..16.
int SpeedToScrollStep(int speedMph);

}
