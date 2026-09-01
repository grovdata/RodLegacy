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

    // Appearance, rolled once when the pool is built and then fixed for the
    // rest of the game - the original stores both in the opponent's 8-byte
    // pool record (byte 2 and byte 6) and saves them, so you meet the same
    // rival in the same car every time.
    uint8_t color_index = 0;  // 0..12, the 13 standard paints
    uint8_t body_byte = 0;    // roof/bumper/sticker bitfield, see libp.h
};

// Which appearance changes a given car spec is even capable of, parallel to
// the car_specs table handed to Init. The original gates the same way, on
// whether the model has the sprites for it.
enum SpecModFlags : uint8_t {
    SPEC_CAN_CHOP_ROOF     = 1 << 0,
    SPEC_CAN_STRIP_BUMPERS = 1 << 1,
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

    // Initialize the 21-entry pool: 7 slots from each car-class band, in
    // easy/mid/hard order, matching the original's InitRacerPool. Stores
    // car_specs for score computation. The driver table is not needed - a
    // pool slot's position IS the opponent's identity.
    // spec_mods is parallel to car_specs (see SpecModFlags); pass an empty
    // vector to leave every car stock.
    void Init(const std::vector<SrCarSpecEntry>& car_specs,
              const std::vector<uint8_t>& spec_mods = {});

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
