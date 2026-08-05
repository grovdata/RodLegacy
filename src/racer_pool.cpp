#include "racer_pool.h"
#include "race/race_sim.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// ============================================================================
// ComputeCarPerformanceScore
// The Amiga's ComputeCarTopSpeed (FUN_00243b9c): unpack the car spec's config
// word into part classes, score them with FUN_002435f0
// (srrace::ComputeBasePerformance), and turn that into a top speed of
// base * 13/10 capped at 0x82 (130) mph. It is the same number the original
// prints as "Max speed %3d mph", so a pool match is a match on top speed.
// ============================================================================

srrace::CarSetup CarSetupFromSpec(const SrCarSpecEntry& spec) {
    srrace::CarSetup setup{};
    setup.carbClass     = spec.CarbClass();
    setup.transClass    = spec.TransClass();
    setup.engineClass   = spec.EngineClass();
    setup.manifoldClass = spec.ManifoldClass();
    setup.tireClass     = spec.TireClass();
    setup.weightClass   = spec.car_class;  // 0..8, indexes weight_pts directly
    // The spec carries no tune or wear data - a listed car is untuned and
    // unworn, so those keep CarSetup's defaults. Body mods likewise: a spec
    // car is stock, which is what the original's synthetic record gives it.
    //
    // CAVEAT: the config word's BIT LAYOUT is verified, but mapping its fields
    // onto CarSetup's classes is not. The Amiga expands each field into a part
    // index (`brand + class*3 + base`, FUN_00243b9c) before scoring, so
    // SR_data's engine field is an engine *variant* 0..7 while CarSetup's
    // engineClass is a 0..3 row of the engine/manifold matrix; manifold is 0..3
    // here against 0..4 there. ComputeBasePerformance clamps both, so this is
    // safe but may compress the top of the range.
    return setup;
}

int ComputeCarPerformanceScore(const SrCarSpecEntry& spec) {
    return srrace::ComputeTopSpeed(CarSetupFromSpec(spec));
}

// ============================================================================
// RacerPool
// ============================================================================

void RacerPool::Init(const std::vector<SrDriverData>& drivers,
                     const std::vector<SrCarSpecEntry>& car_specs) {
    car_specs_ = car_specs;
    pool.clear();
    pool.reserve(RACER_POOL_SIZE);

    // Group driver indices (0..20) by their tier byte.
    std::vector<int> tier_drivers[3];
    for (int i = 0; i < SR_DATA_KING_DRIVER_INDEX && i < static_cast<int>(drivers.size()); ++i) {
        int t = std::min<int>(drivers[i].tier, 2);
        tier_drivers[t].push_back(i);
    }

    // Group car spec indices (0..24) into three tiers by their class byte, which
    // runs 0..8 - so three classes per tier. Over the shipped SR_data that
    // splits 9/8/8, enough to fill the pool's 7 slots per tier.
    std::vector<int> tier_specs[3];
    for (int i = 0; i < SR_DATA_KING_CARSPEC_INDEX && i < static_cast<int>(car_specs.size()); ++i) {
        int c = std::min<int>(car_specs[i].car_class / 3, 2);
        tier_specs[c].push_back(i);
    }

    // Fill pool: 7 slots per tier.
    for (int tier = 0; tier < 3; ++tier) {
        for (int j = 0; j < 7; ++j) {
            RacerEntry entry;
            entry.driver_index = (j < static_cast<int>(tier_drivers[tier].size()))
                                 ? tier_drivers[tier][j]
                                 : (tier * 7 + j);
            entry.car_spec_index = (j < static_cast<int>(tier_specs[tier].size()))
                                   ? tier_specs[tier][j]
                                   : 0;
            entry.status = 0;
            pool.push_back(entry);
        }
    }

    rng_ = std::mt19937(std::random_device{}());
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

    // Remove last_chosen if there are alternatives.
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

int RacerPool::FindReplacementCarSpec(int beaten_classification) const {
    // Collect spec indices currently in pool (non-eliminated).
    std::vector<int> used;
    for (const auto& e : pool) {
        if (e.status != -1) used.push_back(e.car_spec_index);
    }
    for (int i = 0; i < SR_DATA_KING_CARSPEC_INDEX && i < static_cast<int>(car_specs_.size()); ++i) {
        if (static_cast<int>(car_specs_[i].car_class) < beaten_classification) {
            if (std::find(used.begin(), used.end(), i) == used.end()) {
                return i;
            }
        }
    }
    return -1;
}

void RacerPool::OnRaceWon(int winner_pool_index, bool pink_slip) {
    if (winner_pool_index < 0 || winner_pool_index >= static_cast<int>(pool.size())) return;

    if (!pink_slip) {
        pool[winner_pool_index].status = 0;
        return;
    }

    int beaten_class = (pool[winner_pool_index].car_spec_index < static_cast<int>(car_specs_.size()))
                       ? car_specs_[pool[winner_pool_index].car_spec_index].car_class
                       : 0;

    int replacement = FindReplacementCarSpec(beaten_class);
    if (replacement >= 0) {
        pool[winner_pool_index].car_spec_index = replacement;
        pool[winner_pool_index].status = 0;
    } else {
        pool[winner_pool_index].status = -1;
    }
}

void RacerPool::OnRaceLost(int challenger_pool_index) {
    if (challenger_pool_index >= 0 && challenger_pool_index < static_cast<int>(pool.size())) {
        pool[challenger_pool_index].status = 0;
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
