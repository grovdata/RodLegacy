#include "racer_pool.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// ============================================================================
// ComputeCarPerformanceScore
// Approximates Amiga FUN_00243b9c (outer) / FUN_002435f0 (inner).
// The Amiga uses two compiled-in A4-relative tables with percentage-based
// arithmetic (muls #0x19 / divs #0x64). This is a disk-data approximation.
// See re_scoring.md for full RE findings.
// ============================================================================

static const int ENGINE_SCORE[9]  = { 0,  5, 10,  0,  5, 10,  0,  5, 10 };
static const int TRANS_SCORE[13]  = { 0,  0,  0,  5,  5,  5, 10, 10, 10, 10, 15, 15, 15 };
static const int CARB_SCORE[3]    = { 0,  5, 10 };
static const int TIRE_SCORE[3]    = { 0,  5, 10 };
static const int CLASS_BONUS[4]   = { 0, 15, 30, 45 };

int ComputeCarPerformanceScore(const SrCarSpecEntry& spec) {
    int score = 70;
    if (spec.engine         < 9)  score += ENGINE_SCORE[spec.engine];
    if (spec.transmission   < 13) score += TRANS_SCORE[spec.transmission];
    if (spec.carburetor     < 3)  score += CARB_SCORE[spec.carburetor];
    if (spec.tires          < 3)  score += TIRE_SCORE[spec.tires];
    score += spec.condition;
    if (spec.street_cred < 4)  score += CLASS_BONUS[spec.street_cred];
    return std::clamp(score, 0, 130);
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

    // Group car spec indices (0..24) by their classification byte.
    std::vector<int> tier_specs[3];
    for (int i = 0; i < SR_DATA_KING_CARSPEC_INDEX && i < static_cast<int>(car_specs.size()); ++i) {
        int c = std::min<int>(car_specs[i].street_cred, 2);
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
        if (static_cast<int>(car_specs_[i].street_cred) < beaten_classification) {
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
                       ? car_specs_[pool[winner_pool_index].car_spec_index].street_cred
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
