/*
 * RodLegacy - The 21 opponents: RacerPool picks challengers and tracks their cars, looks and rivalries.
 *
 * Created: 2026-03-20
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

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

    // Rolled once and fixed for the game, so you meet the same rival in the same car.
    uint8_t color_index = 0;  // 0..12, the 13 standard paints
    uint8_t body_byte = 0;    // roof/bumper/sticker bitfield, see libp.h

    // Set once downgraded after losing a pink slip; a second pink slip eliminates them.
    bool downgraded = false;

    // races counts every race against them except ones just for kicks, opp_wins those the player did not
    // win (a ticket included). Both are zeroed when the driver gets a new car.
    uint8_t races = 0;
    uint8_t opp_wins = 0;
};

// Which appearance changes a spec can take, parallel to car_specs.
enum SpecModFlags : uint8_t {
    SPEC_CAN_CHOP_ROOF     = 1 << 0,
    SPEC_CAN_STRIP_BUMPERS = 1 << 1,
};

// Top speed: base performance * 13/10, capped at 130. The pool ranks challengers by this.
int ComputeCarPerformanceScore(const SrCarSpecEntry& spec);

class RacerPool {
public:
    RacerPool() = default;

    // 7 slots from each car-class band, easy/mid/hard. spec_mods is parallel to car_specs; empty leaves every car stock.
    void Init(const std::vector<SrCarSpecEntry>& car_specs,
              const std::vector<uint8_t>& spec_mods = {});

    // Up to SELECTION_POOL_SIZE indices sorted by |player_score - entry_score|, excluding eliminated entries
    // and, when others remain, last_chosen_index.
    std::vector<int> BuildSelectionPool(int player_score, int last_chosen_index) const;

    int SelectNextChallenger(int player_score, int last_chosen_index);

    // pink_slip: move the opponent to a lower-class car not already in the pool, or eliminate them.
    void OnRaceWon(int winner_pool_index, bool pink_slip);

    void OnRaceLost(int challenger_pool_index);

    // opponent_won is true for any race the player did not win, a ticket included.
    void RecordRace(int pool_index, bool opponent_won);

    // Past 5 races a sticker, past 10 another, past 8 stripped bumpers or a chopped roof, each on the
    // (car_class >> 1) + 1 threshold and only where the model has the sprites.
    void ApplyRivalryUpgrades(int pool_index);

    int RivalryMargin(int pool_index) const;

    int  CountActive() const;
    bool IsEliminated(int pool_index) const;
    int  GetEntryScore(int pool_index) const;

    // Status has no confirmed source in the save format, so it is left alone.
    void SetCarSpecForSlot(int pool_index, int car_spec_index) {
        if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return;
        pool[pool_index].car_spec_index = car_spec_index;
    }

    std::vector<RacerEntry> pool; // size = RACER_POOL_SIZE after Init

private:
    std::vector<SrCarSpecEntry> car_specs_;
    std::vector<uint8_t>        spec_mods_;
    std::mt19937                rng_;

    int ComputeEntryScore(int pool_index) const;
    int FindReplacementCarSpec(int beaten_classification);
};
