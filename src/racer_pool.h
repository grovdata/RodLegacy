#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include "parsers/libp.h"

static constexpr int RACER_POOL_SIZE      = 21;
static constexpr int RACER_KING_SLOT      = 21; // Not in pool[]; accessed separately
static constexpr int SELECTION_POOL_SIZE  = 9;

struct RacerEntry {
    int    driver_index;    // index into SrDriverData table (0-20)
    int    car_spec_index;  // index into SrCarSpecEntry table (0-24)
    int8_t status;          // 0=active, 1=currently challenging, -1=eliminated
};

// Mirrors Amiga ComputeCarPerformanceScore (FUN_00243b9c / FUN_002435f0).
// Formula: base 70 + engine + trans + carb + tire + condition + class_bonus, clamped [0,130].
// TODO: replace placeholder lookup table values with exact values from Amiga FUN_002435f0.
int ComputeCarPerformanceScore(const SrCarSpecEntry& spec);

class RacerPool {
public:
    RacerPool() = default;

    // Initialize 21-entry pool (7 per driver tier). Stores car_specs for score computation.
    void Init(const std::vector<SrDriverData>& drivers,
              const std::vector<SrCarSpecEntry>& car_specs);

    // Returns up to SELECTION_POOL_SIZE pool indices sorted by |player_score - entry_score|.
    // Excludes eliminated (status==-1) entries.
    // If >1 candidate remains, also excludes last_chosen_index.
    std::vector<int> BuildSelectionPool(int player_score, int last_chosen_index) const;

    // Randomly pick from BuildSelectionPool, sets winner's status=1. Returns pool index.
    int SelectNextChallenger(int player_score, int last_chosen_index);

    // Call after player wins.
    // pink_slip=false: opponent returns to pool (status=0).
    // pink_slip=true: find a lower-classification car spec not already in pool.
    //   Found → replace opponent's car spec and keep active.
    //   Not found → eliminate opponent (status=-1).
    void OnRaceWon(int winner_pool_index, bool pink_slip);

    // Call after player loses. Resets challenger status to 0.
    void OnRaceLost(int challenger_pool_index);

    int  CountActive() const;
    bool IsEliminated(int pool_index) const;

    std::vector<RacerEntry> pool; // size = RACER_POOL_SIZE after Init

private:
    std::vector<SrCarSpecEntry> car_specs_;
    std::mt19937                rng_;

    int ComputeEntryScore(int pool_index) const;
    int FindReplacementCarSpec(int beaten_classification) const;
};
