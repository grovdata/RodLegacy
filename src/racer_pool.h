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

// The listed car's top speed in mph (Amiga ComputeCarTopSpeed 0x243b9c):
// unpack the spec config word, score it via srrace::ComputeBasePerformance,
// then base * 13/10 capped at 130. The pool ranks challengers by this, which
// is the same number the original shows as "Max speed".
// See RodLegacyResearch/docs/driving/top_speed.md.
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
    int  GetEntryScore(int pool_index) const;

    // For loading a save's known-opponents record (trailer chunk 6): sets
    // which car spec that pool slot's driver currently drives. Status
    // (active/challenging/eliminated) has no confirmed source in the save
    // format, so it's left untouched here -- callers loading a fresh save
    // should reset status themselves if they want every entry active.
    void SetCarSpecForSlot(int pool_index, int car_spec_index) {
        if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return;
        pool[pool_index].car_spec_index = car_spec_index;
    }

    std::vector<RacerEntry> pool; // size = RACER_POOL_SIZE after Init

private:
    std::vector<SrCarSpecEntry> car_specs_;
    std::mt19937                rng_;

    int ComputeEntryScore(int pool_index) const;
    int FindReplacementCarSpec(int beaten_classification) const;
};
