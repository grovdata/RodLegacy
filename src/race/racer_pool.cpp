#include "race/racer_pool.h"
#include "race/race_sim.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// Top speed: base performance * 13/10 capped at 130 mph, the original's "Max speed".

srrace::CarSetup CarSetupFromSpec(const SrCarSpecEntry& spec) {
    srrace::CarSetup setup{};
    setup.carbClass     = spec.CarbClass();
    setup.transClass    = spec.TransClass();
    setup.engineClass   = spec.EngineClass();
    setup.manifoldClass = spec.ManifoldClass();
    setup.tireClass     = spec.TireClass();
    setup.weightClass   = spec.car_class;  // 0..8, indexes weight_pts directly
    // A listed car is untuned, unworn and stock. The config word shares the parts table's bit layout.
    return setup;
}

int ComputeCarPerformanceScore(const SrCarSpecEntry& spec) {
    return srrace::ComputeTopSpeed(CarSetupFromSpec(spec));
}

namespace {

// The original's per-slot appearance roll; its random(n) returns 0..n-1.
//
//   threshold = (car_class >> 1) + 1        class 0..8 -> 1..5
//   bit 3     = (spec config word & 7) == 2
//   sticker   : random(20) < threshold, then random(9) into the high nibble
//   bumpers   : random(25) < threshold, only if the model has the sprites
//   roof      : random(30) < threshold, only if the model has the sprites
uint8_t RollBodyByte(const SrCarSpecEntry& spec, uint8_t mods, std::mt19937& rng) {
    auto roll = [&rng](int n) {
        return static_cast<int>(std::uniform_int_distribution<int>(0, n - 1)(rng));
    };
    const int threshold = (spec.car_class >> 1) + 1;

    uint8_t body = ((spec.ConfigWord() & 7) == 2) ? CAR_BODY_SPEC_CONFIG_BIT : 0;

    if (roll(20) < threshold) {
        // One-based on purpose: 0 leaves the car unstickered (see libp.h).
        body |= static_cast<uint8_t>(roll(9) << 4);
    }
    if (mods & SPEC_CAN_STRIP_BUMPERS) {
        if (roll(25) < threshold) body |= CAR_BODY_BUMPERS_STRIPPED;
    }
    if (mods & SPEC_CAN_CHOP_ROOF) {
        if (roll(30) < threshold) body |= CAR_BODY_ROOF_CHOPPED;
    }
    return body;
}

}

void RacerPool::Init(const std::vector<SrCarSpecEntry>& car_specs,
                     const std::vector<uint8_t>& spec_mods) {
    car_specs_ = car_specs;
    spec_mods_ = spec_mods;   // kept for the appearance re-roll in OnRaceWon
    pool.clear();
    pool.reserve(RACER_POOL_SIZE);

    // Three tiers of three classes each. Filtered on "for sale" since each merged set carries its own King's car.
    std::vector<int> tier_specs[3];
    for (int i = 0; i < static_cast<int>(car_specs.size()); ++i) {
        if (!car_specs[i].ForSale()) continue;
        int c = std::min<int>(car_specs[i].car_class / 3, 2);
        tier_specs[c].push_back(i);
    }

    rng_ = std::mt19937(std::random_device{}());

    // 7 slots per band, easy/mid/hard. The driver is the slot itself: the pool record has no driver field.
    // Cars are drawn without replacement per band; shuffling avoids the original's retry loop.
    for (int tier = 0; tier < 3; ++tier) {
        std::vector<int> band = tier_specs[tier];
        std::shuffle(band.begin(), band.end(), rng_);
        for (int j = 0; j < 7; ++j) {
            RacerEntry entry;
            entry.driver_index = static_cast<int>(pool.size());
            // Only non-stock car sets can have a band of fewer than 7.
            entry.car_spec_index = (j < static_cast<int>(band.size())) ? band[j] : 0;
            entry.status = 0;

            const int spec = entry.car_spec_index;
            if (spec >= 0 && spec < static_cast<int>(car_specs.size())) {
                const uint8_t mods = (spec < static_cast<int>(spec_mods.size()))
                                     ? spec_mods[spec] : 0;
                entry.body_byte = RollBodyByte(car_specs[spec], mods, rng_);
            }
            // 13 standard paints, picked per opponent - the original's
            // random(13) into pool record byte 2.
            entry.color_index = static_cast<uint8_t>(
                std::uniform_int_distribution<int>(0, 12)(rng_));

            pool.push_back(entry);
        }
    }
}

int RacerPool::GetEntryScore(int pool_index) const {
    return ComputeEntryScore(pool_index);
}

int RacerPool::ComputeEntryScore(int pool_index) const {
    if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return 70;
    int spec_idx = pool[pool_index].car_spec_index;
    if (spec_idx < 0 || spec_idx >= static_cast<int>(car_specs_.size())) return 70;
    return ComputeCarPerformanceScore(car_specs_[spec_idx]);
}

std::vector<int> RacerPool::BuildSelectionPool(int player_score, int last_chosen_index) const {
    std::vector<std::pair<int, int>> candidates; // (delta, pool_index)
    for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
        if (pool[i].status == -1) continue;
        int delta = std::abs(player_score - ComputeEntryScore(i));
        candidates.push_back({delta, i});
    }
    std::stable_sort(candidates.begin(), candidates.end());

    std::vector<int> result;
    result.reserve(SELECTION_POOL_SIZE);
    for (int i = 0; i < static_cast<int>(candidates.size()) && i < SELECTION_POOL_SIZE; ++i) {
        result.push_back(candidates[i].second);
    }

    if (result.size() > 1 && last_chosen_index >= 0) {
        auto it = std::find(result.begin(), result.end(), last_chosen_index);
        if (it != result.end()) result.erase(it);
    }

    return result;
}

int RacerPool::SelectNextChallenger(int player_score, int last_chosen_index) {
    auto sel = BuildSelectionPool(player_score, last_chosen_index);
    if (sel.empty()) return -1;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(sel.size()) - 1);
    int idx = sel[dist(rng_)];
    pool[idx].status = 1;
    return idx;
}

int RacerPool::FindReplacementCarSpec(int beaten_classification) {
    // Every spec of a strictly lower class, even ones another racer drives.
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(car_specs_.size()); ++i) {
        if (!car_specs_[i].ForSale()) continue;
        if (static_cast<int>(car_specs_[i].car_class) < beaten_classification) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) return -1;
    return candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(rng_)];
}

void RacerPool::OnRaceWon(int winner_pool_index, bool pink_slip) {
    if (winner_pool_index < 0 || winner_pool_index >= static_cast<int>(pool.size())) return;

    if (!pink_slip) {
        pool[winner_pool_index].status = 0;
        return;
    }

    RacerEntry& entry = pool[winner_pool_index];

    // One downgrade per driver; a second pink slip eliminates them.
    if (entry.downgraded) {
        entry.status = -1;
        return;
    }

    int beaten_class = (entry.car_spec_index < static_cast<int>(car_specs_.size()))
                       ? car_specs_[entry.car_spec_index].car_class
                       : 0;

    int replacement = FindReplacementCarSpec(beaten_class);
    if (replacement >= 0) {
        entry.car_spec_index = replacement;
        // A new car gets fresh paint and appearance, and the rivalry counters reset.
        const uint8_t mods = (replacement < static_cast<int>(spec_mods_.size()))
                             ? spec_mods_[replacement] : 0;
        entry.body_byte = RollBodyByte(car_specs_[replacement], mods, rng_);
        entry.color_index = static_cast<uint8_t>(
            std::uniform_int_distribution<int>(0, 12)(rng_));
        entry.races = 0;
        entry.opp_wins = 0;
        entry.downgraded = true;
        entry.status = 0;
    } else {
        // Nothing cheaper left to put them in: they are out for good.
        entry.status = -1;
    }
}

void RacerPool::OnRaceLost(int challenger_pool_index) {
    if (challenger_pool_index >= 0 && challenger_pool_index < static_cast<int>(pool.size())) {
        pool[challenger_pool_index].status = 0;
    }
}

void RacerPool::RecordRace(int pool_index, bool opponent_won) {
    if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return;
    RacerEntry& entry = pool[pool_index];
    if (entry.races < 255) entry.races += 1;
    if (opponent_won && entry.opp_wins < 255) entry.opp_wins += 1;
}

int RacerPool::RivalryMargin(int pool_index) const {
    if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return 0;
    return pool[pool_index].opp_wins * 2 - pool[pool_index].races;
}

void RacerPool::ApplyRivalryUpgrades(int pool_index) {
    if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return;
    RacerEntry& entry = pool[pool_index];
    if (entry.car_spec_index < 0 ||
        entry.car_spec_index >= static_cast<int>(car_specs_.size())) {
        return;
    }
    const SrCarSpecEntry& spec = car_specs_[entry.car_spec_index];
    const int races = entry.races;
    const int threshold = (spec.car_class >> 1) + 1;
    auto roll = [this](int n) {
        return static_cast<int>(std::uniform_int_distribution<int>(0, n - 1)(rng_));
    };

    // Past 5 races a bare car gets a sticker; past 10 an existing one can be swapped. One-based, 0 = none.
    const int sticker = (entry.body_byte & 0xF0) >> 4;
    if ((sticker == 0 && races > 5) || (sticker != 0 && races > 10)) {
        if (roll(20) < threshold) {
            entry.body_byte = static_cast<uint8_t>((entry.body_byte & 0x0F) |
                                                   (roll(9) << 4));
        }
    }

    const uint8_t mods = (entry.car_spec_index < static_cast<int>(spec_mods_.size()))
                         ? spec_mods_[entry.car_spec_index] : 0;
    if (races > 8 && (mods & SPEC_CAN_STRIP_BUMPERS)) {
        if (roll(25) < threshold) entry.body_byte |= CAR_BODY_BUMPERS_STRIPPED;
    }
    if (races > 8 && (mods & SPEC_CAN_CHOP_ROOF)) {
        if (roll(30) < threshold) entry.body_byte |= CAR_BODY_ROOF_CHOPPED;
    }
}

int RacerPool::CountActive() const {
    int count = 0;
    for (const auto& e : pool) {
        if (e.status != -1) ++count;
    }
    return count;
}

bool RacerPool::IsEliminated(int pool_index) const {
    if (pool_index < 0 || pool_index >= static_cast<int>(pool.size())) return true;
    return pool[pool_index].status == -1;
}
