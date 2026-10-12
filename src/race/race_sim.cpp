#include "race/race_sim.h"

#include "race/aqueduct.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace srrace {

// Street Rod 2's on-road hazards, as the generator marks them.
constexpr uint16_t HAZARD_ROCK = 0x0004;
constexpr uint16_t HAZARD_OIL  = 0x1000;

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

constexpr size_t kScoreBaseWithinBlock = 0x52;

constexpr int kScoreEngineRows = 3;
constexpr int kScoreManifoldsSr1 = 5;
constexpr int kScoreManifoldsSr2 = 9;
constexpr int kScoreManifoldsMax = kScoreManifoldsSr2;
constexpr int kScoreWeightClasses = 9;

struct ScoringTables {
    int carb[3];
    int trans[4];
    int engByManifold[kScoreEngineRows * kScoreManifoldsMax];
    int weight[kScoreWeightClasses];
    int manifolds;
};

// Street Rod's tables as shipped; the engine grid is 3 x 5, packed at the loaded width.
ScoringTables gScoring = {
    {0, 50, 100},
    {0, 33, 66, 100},
    {
        0, 0, 100, 0, 0,
        0, 50, 0, 100, 75,
        0, 0, 0, 50, 100,
    },
    {40, 44, 48, 60, 64, 68, 80, 84, 88},
    kScoreManifoldsSr1,
};

bool gUncapTopSpeed = false;
bool gSr2BugFixes = false;
int gSimHz = 12;

}

void SetUncapTopSpeed(bool enabled) {
    gUncapTopSpeed = enabled;
}

void SetSr2BugFixes(bool enabled) {
    gSr2BugFixes = enabled;
}

bool Sr2BugFixes() {
    return gSr2BugFixes;
}

bool UncapTopSpeed() {
    return gUncapTopSpeed;
}

void BlendCarView(const CarState& prev, CarState& cur, float t) {
    const int32_t a = prev.segPos * 8 + prev.scrollAcc;
    const int32_t b = cur.segPos * 8 + cur.scrollAcc;
    if (std::abs(b - a) > 256) return;
    const int32_t fine = a + static_cast<int32_t>(std::lround((b - a) * t));
    cur.segPos = fine / 8;
    cur.scrollAcc = static_cast<int16_t>(fine % 8);
    auto lerp = [t](int16_t x, int16_t y) {
        return static_cast<int16_t>(x + std::lround((y - x) * t));
    };
    cur.lateral = lerp(prev.lateral, cur.lateral);
    cur.heading = lerp(prev.heading, cur.heading);
    cur.elevAtCar = lerp(prev.elevAtCar, cur.elevAtCar);
}

void SetSimHz(int hz) {
    gSimHz = ClampValue(hz, SIM_HZ_ORIGINAL, SIM_HZ_MAX);
}

int SimHz() {
    return gSimHz;
}

// Both adjustments below match the original's buggy behaviour: lmech.c compares a field shifted down to 0 or 1 with
// the unshifted mask, so tube headers, the glass pack and the 4.11:1 differential never match. Only a missing muffler
// and the 2.78:1 differential do anything.
int Sr2AdjustRpmGain(int gain, const CarSetup& setup) {
    if (setup.mufflerRemoved) gain += gain * 5 / 100;
    else if (gSr2BugFixes && setup.muffler == 1) gain += gain * 3 / 100;
    if (gSr2BugFixes && setup.exhaust == 1) gain += gain * 5 / 100;
    if (setup.differential == 0) gain -= gain * 5 / 100;
    else if (gSr2BugFixes && setup.differential == 2) gain += gain * 5 / 100;
    return gain;
}

int Sr2AdjustScore(int score, int differential) {
    if (differential == 0) return score + score * 5 / 100;
    if (gSr2BugFixes && differential == 2) return score - score * 5 / 100;
    return score;
}

// lmech.c FUN_00258218. One raw score where Street Rod weights each term out of 100: the engine and transmission
// tables, the manifold table row picked by carburettor, the car's class, the body mods and a flat 880.
int ComputeSr2BasePerformance(const CarSetup& setup) {
    const int engineClass = ClampValue(setup.engineClass, 0, kScoreEngineRows - 1);
    const int transClass = ClampValue(setup.transClass, 0, 3);
    const int manifoldClass = ClampValue(setup.manifoldClass, 0, kScoreManifoldsSr2 - 1);
    const int carbClass = ClampValue(setup.carbClass, 0, 2);
    const int weightClass = ClampValue(setup.weightClass, 0, kScoreWeightClasses - 1);
    const int engineWear = ClampValue(setup.engineWear, 0, 10000);
    // The Amiga subtracts the byte's distance from zero, worth under 1 mph, and its zero sits one notch step right of
    // the machine's red bar (the bar is at screen x 37, step 7; tune 0 draws at step 8). The PC build's knob costs 2640
    // from perfect to worst, so fixed, each of the 16 notch steps off the bar costs 330 and the notch dead on the bar
    // (tune -16..-1) is perfect.
    const int tune = ClampValue(setup.sr2Tune, -128, 127);
    const int tuneAbs = gSr2BugFixes ? std::abs((tune + 128) / 16 - 7) * 330 : std::abs(tune);

    // gScoring.carb holds the engine score in this game (same table slot as Street Rod's carburettor points).
    int score = gScoring.carb[engineClass] * 25 + gScoring.trans[transClass] * 15 +
                gScoring.engByManifold[carbClass * kScoreManifoldsSr2 + manifoldClass] * 15 - tuneAbs +
                gScoring.weight[weightClass] * 10 + 880;
    if (setup.roofChopped) score += 800;
    if (setup.frontBumperRemoved) score += 400;
    if (setup.rearBumperRemoved) score += 100;

    score = Sr2AdjustScore(score, setup.differential);
    score /= 100;
    score = score * (10000 - engineWear) / 10000;
    const int k = transClass == 0 ? 8 : 10;
    return 70 + score * k * 3 / 100;
}

int ComputeBasePerformance(const CarSetup& setup) {
    if (gScoring.manifolds == kScoreManifoldsSr2) return ComputeSr2BasePerformance(setup);
    const int carbClass = ClampValue(setup.carbClass, 0, 2);
    const int transClass = ClampValue(setup.transClass, 0, 3);
    // Three engines: V6, 283 and 327. engine_part.h clamps the part tier to 0..2.
    const int engineClass = ClampValue(setup.engineClass, 0, kScoreEngineRows - 1);
    const int manifoldClass = ClampValue(setup.manifoldClass, 0, gScoring.manifolds - 1);
    const int weightClass = ClampValue(setup.weightClass, 0, kScoreWeightClasses - 1);
    const int tuneOffset = ClampValue(setup.tuneOffset, 0, 8);
    const int carbWear = ClampValue(setup.carbWear, 0, 10000);

    // Weighted to 100 for a perfect car: carb 25, transmission 15, engine/manifold 15, tune 20, weight 10, body mods 15.
    int pts = 0;
    pts += (gScoring.carb[carbClass] * 25) / 100;
    pts += (gScoring.trans[transClass] * 15) / 100;
    pts += (gScoring.engByManifold[engineClass * gScoring.manifolds + manifoldClass] * 15) / 100;
    pts += (((gUncapTopSpeed ? 8 : 1) - tuneOffset) * 20) / 8;
    // Divisor is the table's top entry: 88 in Street Rod, 100 in Street Rod 2.
    const int weightMax = gScoring.weight[kScoreWeightClasses - 1];
    pts += (gScoring.weight[weightClass] * 10) / (weightMax > 0 ? weightMax : 88);
    if (setup.roofChopped) {
        pts += 9;
    }
    if (setup.frontBumperRemoved) {
        pts += 5;
    }
    if (setup.rearBumperRemoved) {
        pts += 1;
    }

    // The differential adjusts the raw score, before the wear scaling and before the 70 floor is added.
    pts = Sr2AdjustScore(pts, setup.differential);

    // transFactor is the original's k * 3, and k is 8 for an automatic and 10
    // for anything else, read off the transmission's class in FUN_00258218.
    const bool automatic = (transClass == 0);
    const int transFactor = automatic ? 24 : 30;
    const int wearScaledPts = (pts * (10000 - carbWear)) / 10000;
    return 70 + (wearScaledPts * transFactor) / 100;
}

int ComputeTopSpeed(const CarSetup& setup) {
    return std::min((ComputeBasePerformance(setup) * 13) / 10, 130);
}

// The physics tables, filled in by LoadPhysicsTables from the player's own executable.
static int kTopFracByTrans[4][5] = {
    {0, 25, 50, 100, 0},
    {0, 25, 50, 100, 0},
    {0, 25, 50, 75, 100},
    {0, 30, 50, 80, 100},
};
static int kPowerBase[3][5] = {
    {500, 600, 300, 100, 70},
    {600, 700, 350, 120, 90},
    {600, 760, 380, 140, 90},
};
static int kBandStart[3] = {2900, 2900, 2500};
static int kBandEnd[3] = {4500, 5000, 5200};
static int kNumGears[4] = {3, 3, 4, 4};
static int kTireSteer[3] = {9, 8, 5};
static int kHandling[3][5] = {
    {2, 5, 8, 11, 14},
    {0, 2, 4, 6, 8},
    {0, 0, 1, 2, 4},
};

namespace {

// Where each block sits in the two executables.
struct PhysicsTableOffsets {
    size_t block;      // gear ratios, then RPM gain at +0x28, power bands at +0x46
    size_t numGears;
    size_t tireSteer;
    size_t handling;
};
constexpr PhysicsTableOffsets kStreetRodOffsets  = {0x2C9B0, 0x2CA34, 0x2CA3C, 0x2CAA0};
constexpr PhysicsTableOffsets kStreetRod2Offsets = {0x40CEC, 0x40D88, 0x40D90, 0x40F14};

constexpr size_t kPowerBaseWithinBlock = 0x28;
constexpr size_t kPowerBandWithinBlock = 0x46;

int ReadBE16At(const std::vector<uint8_t>& d, size_t at) {
    return (int(d[at]) << 8) | int(d[at + 1]);
}

} // namespace

int ScoringManifoldCount() {
    return gScoring.manifolds;
}

bool LoadScoringTables(const std::vector<uint8_t>& exeData, PhysicsTableSource from) {
    const bool sr2 = (from == PhysicsTableSource::StreetRod2);
    const PhysicsTableOffsets& off = sr2 ? kStreetRod2Offsets : kStreetRodOffsets;
    const int manifolds = sr2 ? kScoreManifoldsSr2 : kScoreManifoldsSr1;

    // engine x manifold, then the carburettor and transmission tables, and the
    // weight points sitting just past the tyre-steer table.
    const size_t engAt = off.block + kScoreBaseWithinBlock;
    const size_t carbAt = engAt + size_t(kScoreEngineRows * manifolds) * 2;
    const size_t transAt = carbAt + 3 * 2;
    const size_t weightAt = off.tireSteer + 3 * 2;
    if (exeData.size() < std::max(transAt + 4 * 2,
                                  weightAt + size_t(kScoreWeightClasses) * 2)) {
        return false;
    }

    ScoringTables t{};
    t.manifolds = manifolds;
    for (int i = 0; i < 3; ++i) t.carb[i] = ReadBE16At(exeData, carbAt + size_t(i) * 2);
    for (int i = 0; i < 4; ++i) t.trans[i] = ReadBE16At(exeData, transAt + size_t(i) * 2);
    for (int i = 0; i < kScoreWeightClasses; ++i) {
        t.weight[i] = ReadBE16At(exeData, weightAt + size_t(i) * 2);
    }
    for (int e = 0; e < kScoreEngineRows; ++e) {
        for (int m = 0; m < manifolds; ++m) {
            t.engByManifold[e * manifolds + m] =
                ReadBE16At(exeData, engAt + size_t(e * manifolds + m) * 2);
        }
    }

    // Both games score a car out of 100 per term, so every entry has to be a percentage.
    if (t.carb[0] != 0 || t.carb[1] != 50 || t.carb[2] != 100) return false;
    if (t.trans[0] != 0 || t.trans[1] != 33 || t.trans[2] != 66 || t.trans[3] != 100) {
        return false;
    }
    for (int i = 0; i < kScoreWeightClasses; ++i) {
        if (t.weight[i] < 1 || t.weight[i] > 100) return false;
        if (i > 0 && t.weight[i] <= t.weight[i - 1]) return false;
    }
    for (int i = 0; i < kScoreEngineRows * manifolds; ++i) {
        if (t.engByManifold[i] < 0 || t.engByManifold[i] > 100) return false;
    }

    gScoring = t;
    return true;
}

bool LoadPhysicsTables(const std::vector<uint8_t>& exeData, PhysicsTableSource from) {
    const PhysicsTableOffsets& off =
        (from == PhysicsTableSource::StreetRod2) ? kStreetRod2Offsets : kStreetRodOffsets;

    // Everything the loader touches has to be inside the file.
    const size_t end = std::max(off.block + kPowerBandWithinBlock + 3 * 2 * 2,
                                std::max(off.handling + 3 * 5 * 2,
                                         std::max(off.numGears + 4 * 2, off.tireSteer + 3 * 2)));
    if (exeData.size() < end) return false;

    int topFrac[4][5], power[3][5], bandStart[3], bandEnd[3];
    int gears[4], steer[3], handling[3][5];

    for (int t = 0; t < 4; t++)
        for (int g = 0; g < 5; g++)
            topFrac[t][g] = ReadBE16At(exeData, off.block + size_t(t * 5 + g) * 2);
    for (int e = 0; e < 3; e++)
        for (int g = 0; g < 5; g++)
            power[e][g] = ReadBE16At(exeData,
                                     off.block + kPowerBaseWithinBlock + size_t(e * 5 + g) * 2);
    for (int e = 0; e < 3; e++) {
        const size_t at = off.block + kPowerBandWithinBlock + size_t(e) * 4;
        bandStart[e] = ReadBE16At(exeData, at);
        bandEnd[e] = ReadBE16At(exeData, at + 2);
    }
    for (int t = 0; t < 4; t++) gears[t] = ReadBE16At(exeData, off.numGears + size_t(t) * 2);
    for (int i = 0; i < 3; i++) steer[i] = ReadBE16At(exeData, off.tireSteer + size_t(i) * 2);
    for (int i = 0; i < 3; i++)
        for (int g = 0; g < 5; g++)
            handling[i][g] = ReadBE16At(exeData, off.handling + size_t(i * 5 + g) * 2);

    // Cheap shape checks.
    for (int e = 0; e < 3; e++) {
        if (bandStart[e] <= 0 || bandEnd[e] <= bandStart[e]) return false;
        for (int g = 0; g < 5; g++) {
            if (power[e][g] <= 0) return false;
        }
    }
    for (int t = 0; t < 4; t++) {
        if (gears[t] < 3 || gears[t] > 5) return false;
        // Every transmission starts in neutral and tops out at 100%.
        if (topFrac[t][0] != 0 || topFrac[t][3] < topFrac[t][1]) return false;
    }
    for (int i = 0; i < 3; i++) {
        if (steer[i] <= 0 || steer[i] > 20) return false;
    }

    memcpy(kTopFracByTrans, topFrac, sizeof(kTopFracByTrans));
    memcpy(kPowerBase, power, sizeof(kPowerBase));
    memcpy(kBandStart, bandStart, sizeof(kBandStart));
    memcpy(kBandEnd, bandEnd, sizeof(kBandEnd));
    memcpy(kNumGears, gears, sizeof(kNumGears));
    memcpy(kTireSteer, steer, sizeof(kTireSteer));
    memcpy(kHandling, handling, sizeof(kHandling));
    return true;
}

void BuildCarTables(const CarSetup& setup, CarTables& tables) {
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

        const int power = Sr2AdjustRpmGain((base * kPowerBase[carbClass][g]) / 100, setup);
        // SR2 compares the shifted tire class with 0x800, so slicks never get the 3 there either.
        const int row0Mul = (g == 1) ? ((tireClass == 2 && (gScoring.manifolds != kScoreManifoldsSr2 || gSr2BugFixes)) ? 3 : 2) : 1;
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

void RaceSim::Init(const TrackData* newTrack, RaceKind newKind,
                   const CarSetup& newPlayerSetup, const CarSetup& newNpcSetup,
                   uint32_t rngSeed, bool isKingRace) {
    track = newTrack;
    kingRace = isKingRace;
    kind = newKind;
    window = RaceWindowFor(kind);
    finishSeg = window.finish;
    fallDir = 0;
    // Drop a rollover left half played by the last race.
    rollFrameCount = 0;
    rollFrameIndex = 0;
    aqueductRoll = 0;
    aqueductVjerk = 0;
    obstacleBlocked = false;
    obstacleLatch = false;
    aqueductBlockHit = false;
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
    // Mulholland starts at 370 and a grudge race at 9.
    player.segPos = window.start;
    npc.segPos = window.start;
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

    // The light turns green 110..212 vblanks (50 Hz) after "Get ready"; leaving early is a false start.
    phase = RacePhase::Countdown;
    countdownTicks = std::max(1, (110 + rng.Next(103)) * gSimHz / 50);
    npcLaunchOffset = 0;

    // Road races only, never the King's: 1 in 4, trigger at 510 + rand(255). From a derived seed so the extra
    // draws don't shift every later roll the races are regression-tested against.
    copState = 0;
    copTriggerSeg = 0;
    copSpawnEvent = false;
    copSirenOn = false;
    copCaughtSpeed = 0;
    RaceRng copRng(rngSeed ^ 0x5a17c09u);
    if (IsRoadRace() && !kingRace && copRng.Next(4) == 0) {
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
            if (player.speed > 0 || player.segPos > window.start) {
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
        // A handicapped opponent with the slower dial gets their green first.
        if (npcLaunchOffset < 0 && countdownTicks <= -npcLaunchOffset) {
            NpcAiUpdate();
            NpcEngine();
            UpdateScroll(npc);
        }
        return;
    }

    if ((player.endFlags & END_FAILURE_MASK) == 0) {
        PlayerPhysics(inputBits);
        UpdateSteering(inputBits, steerAmount);
    }

    // The other side of the handicap: the player had the slower dial, so the
    // opponent is held on the line for the difference.
    if (npcLaunchOffset > 0) {
        --npcLaunchOffset;
    } else {
        NpcAiUpdate();
        NpcEngine();
    }
    UpdateCopPursuit();

    UpdateScroll(player);
    if (npcLaunchOffset <= 0) {
        UpdateScroll(npc);
    }

    if (tireBlowSeg != 0 && player.segPos > tireBlowSeg) {
        player.endFlags |= END_TIRES_BLOWN;
    }

    ApplyLateralDrift(player, playerTables.handling);
    // jroad.c FUN_00246a80: on Mulholland the drift cannot carry the camera more than 0x96 past the edges.
    if (kind == RaceKind::Mulholland) {
        player.lateral = static_cast<int16_t>(ClampValue<int>(player.lateral, edgeLeft - 0x96, edgeRight + 0x96));
    }

    const int playerSeg = ClampTrackIndex(player.segPos);
    const int playerSegAhead = ClampTrackIndex(player.segPos + 4);
    if (kind == RaceKind::Mulholland) {
        RoadEdgesSr2(track->flags[playerSeg], false, edgeLeft, edgeRight);
        RoadEdgesSr2(track->flags[playerSegAhead], false, edgeLeftAhead, edgeRightAhead);
    } else {
        RoadEdges(track->flags[playerSeg], edgeLeft, edgeRight);
        RoadEdges(track->flags[playerSegAhead], edgeLeftAhead, edgeRightAhead);
    }

    CheckEdgeCollision();
    // Then the grudge lane.
    if (kind == RaceKind::Grudge && player.segPos < finishSeg &&
        player.lateral < ((edgeLeft + edgeRight) / 2) + GRUDGE_CENTRE_LINE_MARGIN) {
        player.endFlags |= END_CROSSED_LINE;
    }
    // Nothing to hit on Grudge Night: the opponent is simulated but off screen.
    if (player.segPos < finishSeg && kind != RaceKind::Grudge) {
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
    // lrace.c FUN_00259ad2: about to leave a segment with a block two ahead, the car is held at 0x38 of 0x40 -
    // if the block is really in its way. The frame after a hold is not checked.
    if (&car == &player && kind == RaceKind::Aqueduct && aqueductTrack != nullptr) {
        obstacleBlocked = false;
        const int ahead = static_cast<int>(car.segPos) + AQU_OBSTACLE_LOOKAHEAD;
        if (!obstacleLatch) {
            obstacleLatch = true;
        } else if (car.scrollAcc + car.scrollStep > AQU_OBSTACLE_STOP_SUBPOS / 8 && ahead < AQUEDUCT_SEGMENTS &&
                   AqueductObstacleBlocks(aqueductTrack->flags[ahead]) &&
                   AqueductObstacleHits(aqueductTrack->flags[ahead], car.lateral)) {
            car.scrollAcc = AQU_OBSTACLE_STOP_SUBPOS / 8;
            car.scrollStep = 0;
            obstacleLatch = false;
            obstacleBlocked = true;
        }
    }
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

    // The original latches from two sites.
    if (finishSnapshot && player.speed < 1) {
        if (playerAheadAtFinish) {
            npc.endFlags |= END_FINISHED;
        } else {
            player.endFlags |= END_FINISHED;
        }
    }
}

// An event carrying a suppressed bit is dropped whole, or a suppressed crash would still end the race.
void RaceSim::LogCrash(const char* why) const {
    std::fprintf(stderr, "Crash: %s seg=%d lateral=%d speed=%d edges=%d..%d\n", why, int(player.segPos),
             int(player.lateral), int(player.speed), int(edgeLeft), int(edgeRight));
}

void RaceSim::AddPlayerEndFlags(uint16_t flags) {
    if (invulnerable) {
        constexpr uint16_t kSuppressed = END_WALL_HIT | END_HIT_NPC | END_ENGINE_BLOWN;
        if ((flags & kSuppressed) != 0) {
            return;
        }
    }
    player.endFlags |= flags;
}

// Street Rod 2's two on-road hazards, both Mulholland only.
void RaceSim::CheckOnRoadHazards() {
    if (kind != RaceKind::Mulholland) return;

    const int seg = ClampTrackIndex(player.segPos);
    const int lane = track->lane[seg];
    if (lane <= 0) return;

    const uint16_t flags = track->flags[seg];
    const bool rock = (flags & HAZARD_ROCK) != 0;
    const bool oil = (flags & HAZARD_OIL) != 0;
    if (!rock && !oil) return;

    const int at = edgeLeft + ((edgeRight - edgeLeft) * lane) / 16;
    if (at < player.lateral - 30 || at > player.lateral + 70) return;

    if (rock) {
        // Harmless if you are slow enough to have seen it coming.
        if (player.speed > 30) {
            LogCrash("rock");
            AddPlayerEndFlags(END_WALL_HIT);
        }
        return;
    }

    // Oil never ends the race. Over 50 it turns the heading (0x2678c4) by 0x28 toward the wheels that hit it,
    // which the drift turns into a slide; over 20 it costs speed.
    if (player.speed > 50) {
        // ponytail: the original picks the side from sprite overlap with screen windows at cam-61/-31 (left wheels)
        // and cam+128/+158 (right); the midpoint between them stands in for that test.
        const int side = at < player.lateral + 48 ? -1 : 1;
        player.heading = static_cast<int16_t>(player.heading - 0x28 * side);
    }
    if (player.speed > 20) {
        player.speed = static_cast<int16_t>(player.speed - 10);
    }
}

// Riding up the channel is allowed, so none of the road's edge logic applies.
void RaceSim::CheckAqueductWalls() {
    player.offRoad = false;

    // jroad.c FUN_00247154: a block that reached the car's scanline last frame is a crash.
    if (aqueductBlockHit) {
        aqueductBlockHit = false;
        LogCrash("aqueduct block");
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }

    // A patch on the slope is solid: riding into one wrecks the car.
    if (aqueductTrack != nullptr && !obstacleBlocked) {
        const int seg = static_cast<int>(player.segPos);
        if (seg >= 0 && seg < AQUEDUCT_SEGMENTS &&
            AqueductSlopePatchHits(aqueductTrack->flags[seg], player.lateral)) {
            AddPlayerEndFlags(END_WALL_HIT);
            return;
        }
    }

    // A rollover already under way plays out to the end.
    if (rollFrameIndex < rollFrameCount) {
        const AqueductRollFrame& f = rollFrames[rollFrameIndex++];
        aqueductRoll = f.roll;
        aqueductVjerk = f.vjerk;
        player.lateral = static_cast<int16_t>(player.lateral + f.camXDelta);
        player.heading = static_cast<int16_t>(player.heading + f.driftDelta);
        if (rollFrameIndex >= rollFrameCount) {
            rollFrameCount = 0;
            rollFrameIndex = 0;
            aqueductRoll = 0;
            aqueductVjerk = 0;
            AddPlayerEndFlags(END_WALL_HIT);
        }
        return;
    }

    const int wallState = AqueductWallState(player.lateral);
    if (wallState != 0) {
        // Leaving the concrete wrecks the car.
        rollFrameCount = AqueductRollover(wallState, player.speed, player.heading,
                                          rollFrames,
                                          static_cast<int>(std::size(rollFrames)));
        rollFrameIndex = 0;
        return;
    }

    // On the slope the camera nudge is what walks the car back down.
    const AqueductWallRide ride = AqueductRideWall(player.lateral);
    if (ride.camXDelta != 0) {
        player.lateral = static_cast<int16_t>(player.lateral + ride.camXDelta);
    }
}

// What scraping an edge costs, jroad.c's `if (speed < 11) speed = 0 else -= 10`.
void RaceSim::ShedEdgeSpeed() {
    player.speed = static_cast<int16_t>(player.speed < 11 ? 0 : player.speed - 10);
}

void RaceSim::CheckEdgeCollision() {
    if (kind == RaceKind::Aqueduct) {
        CheckAqueductWalls();
        return;
    }

    const int seg = ClampTrackIndex(player.segPos);
    const uint16_t leftObj = track->left[seg];
    const uint16_t rightObj = track->right[seg];
    const uint16_t flags = track->flags[seg];
    const bool mulholland = kind == RaceKind::Mulholland;

    CheckOnRoadHazards();
    if ((player.endFlags & END_FAILURE_MASK) != 0) {
        return;
    }

    // Mulholland narrows the road, and taking a narrowing at speed ends the
    // race however well placed the car is.
    if (mulholland && player.speed > 60) {
        const uint16_t ahead = track->flags[ClampTrackIndex(player.segPos + 1)];
        if ((ahead & 0x0060) != 0) {
            LogCrash("narrowing over 60 mph");
            AddPlayerEndFlags(END_WALL_HIT);
            return;
        }
    }

    const int carLeft = player.lateral - 30;
    const int carRight = player.lateral + 70;
    const bool offLeft = carLeft < edgeLeft;
    const bool offRight = carRight > edgeRight;
    player.offRoad = offLeft || offRight;

    // The grudge strip's right-hand limit, jroad.c 0x246860's `race_type & 0x0c` arm.
    if (kind == RaceKind::Grudge && carRight >= edgeRight - GRUDGE_RIGHT_LIMIT) {
        if (player.speed > EDGE_DROP_MAX_MPH) {
            AddPlayerEndFlags(END_WALL_HIT);
        } else {
            player.lateral = static_cast<int16_t>(player.lateral - 200);
            player.heading = static_cast<int16_t>(player.heading + 0x14);
            ShedEdgeSpeed();
        }
        return;
    }

    // A Mulholland rock pile (object bit 0x10) reaches onto the road: jroad.c FUN_0024665c crashes you on it
    // anywhere left of 0x79e or right of 0xb22, on the road or off it.
    if (mulholland && (((leftObj & 0x0010) != 0 && carLeft < 0x79e) ||
                       ((rightObj & 0x0010) != 0 && carRight > 0xb22))) {
        LogCrash("rock pile");
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }

    if (!player.offRoad) {
        return;
    }

    // Mulholland's drivable corridor is tighter: 150 against 260 everywhere else.
    const int tolerance = mulholland ? 150 : 260;

    if (mulholland) {
        const int16_t wallL = track->wallLeft[seg];
        const int16_t wallR = track->wallRight[seg];

        // A positive shoulder is a barrier, and it stops the car well before
        // the general tolerance does.
        if (offLeft && wallL > 0 && carLeft < edgeLeft - tolerance / 4) {
            LogCrash("left barrier");
            AddPlayerEndFlags(END_WALL_HIT);
            return;
        }
        if (offRight && wallR > 0 && carRight > edgeRight + tolerance / 2) {
            LogCrash("right barrier");
            AddPlayerEndFlags(END_WALL_HIT);
            return;
        }

        // Bit 15 of the right object word ONE segment ahead marks the edge the ground falls away from.
        const uint16_t rightAhead = track->right[ClampTrackIndex(player.segPos + 1)];
        if (offRight && (rightAhead & 0x8000) != 0) {
            if (player.speed > EDGE_DROP_MAX_MPH) {
                LogCrash("right drop");
                AddPlayerEndFlags(END_WALL_HIT);
                return;
            }
            player.lateral = static_cast<int16_t>(player.lateral - 300);
            player.heading = static_cast<int16_t>(player.heading + 0x1e);
            ShedEdgeSpeed();
        }
    }

    if (carLeft < (edgeLeft - tolerance) || carRight > (edgeRight + tolerance)) {
        // Over a negative wall the car goes over the edge; Racing plays the fall.
        if (mulholland && fallDir == 0) {
            if (offLeft && track->wallLeft[seg] < 0) fallDir = -1;
            if (offRight && track->wallRight[seg] < 0) fallDir = 1;
        }
        LogCrash("off the road");
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }

    if (offLeft && (leftObj & (0x0010 | 0x1000)) != 0) {
        LogCrash("left roadside object");
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }
    if (offRight && (rightObj & (0x0010 | 0x1000)) != 0) {
        LogCrash("right roadside object");
        AddPlayerEndFlags(END_WALL_HIT);
        return;
    }
    // The top flags nibble is an overhead arch. Mulholland has none: SR2's FUN_0024665c has no arch test.
    if (!mulholland && (flags & 0xf000) != 0) {
        LogCrash("overhead arch");
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

    // On Grudge Night the opponent is simulated but never drawn, so the banding above.
    if (kind == RaceKind::Grudge) {
        target = npcAi.lateralTarget;
    }
    npcAi.lateralTarget = static_cast<int16_t>(target);
    if (kind == RaceKind::Aqueduct && aqueductTrack != nullptr) {
        // The player has not moved yet this tick, so this is the channel the last frame was drawn with.
        const AqueductChannel drawn = AqueductRideWall(player.lateral).channel;
        npc.lateral = static_cast<int16_t>(
            npc.lateral + AqueductOpponentStep(npc.lateral, npcAi.lateralTarget - npc.lateral,
                                               static_cast<int>(npc.segPos), player.lateral,
                                               static_cast<int>(player.segPos), drawn, aqueductTrack->flags));
    } else {
        npc.lateral = static_cast<int16_t>(npc.lateral + ((npcAi.lateralTarget - npc.lateral) / 8));
    }

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

        if (segDiff < 13 || (IsRoadRace() && kingRace)) {
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
