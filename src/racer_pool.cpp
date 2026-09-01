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
    // The config word's fields drop straight into CarSetup because the spec
    // uses the same bit layout as the Amiga's parts table, confirmed against
    // two readings off the running game (spec 0 = 92 mph, spec 5 = 119 mph).
    // See RodLegacyResearch/docs/driving/top_speed.md.
    return setup;
}

int ComputeCarPerformanceScore(const SrCarSpecEntry& spec) {
    return srrace::ComputeTopSpeed(CarSetupFromSpec(spec));
}

// ============================================================================
// RacerPool
// ============================================================================

namespace {

// The original's per-slot appearance roll (FUN_0023eac0). Every bound below is
// taken from its disassembly; its random(n) returns 0..n-1.
//
//   threshold = (car_class >> 1) + 1        class 0..8 -> 1..5
//   bit 3     = (spec config word & 7) == 2
//   sticker   : random(20) < threshold, then random(9) into the high nibble
//   bumpers   : random(25) < threshold, only if the model has the sprites
//   roof      : random(30) < threshold, only if the model has the sprites
//
// So faster cars are modified more often - at class 8 a sticker is 25% likely,
// at class 0 it is 5%.
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

} // namespace

void RacerPool::Init(const std::vector<SrCarSpecEntry>& car_specs,
                     const std::vector<uint8_t>& spec_mods) {
    car_specs_ = car_specs;
    pool.clear();
    pool.reserve(RACER_POOL_SIZE);

    // Group car specs into three tiers by their class byte, which runs 0..8 -
    // so three classes per tier. Over the shipped SR_data that splits 9/8/8,
    // enough to fill the pool's 7 slots per tier.
    //
    // The filter is "for sale" rather than an index cap: the table can hold
    // more than one car set (see State::mergedCarSpecs), and each set carries
    // its own King's car at price 0. Over the stock table alone this picks out
    // exactly 0..24, the same set the old cap did.
    std::vector<int> tier_specs[3];
    for (int i = 0; i < static_cast<int>(car_specs.size()); ++i) {
        if (!car_specs[i].ForSale()) continue;
        int c = std::min<int>(car_specs[i].car_class / 3, 2);
        tier_specs[c].push_back(i);
    }

    rng_ = std::mt19937(std::random_device{}());

    // Fill pool: 7 slots per class band, matching the original's InitRacerPool
    // (0023ebfa), which fills A4-0x2c38 as 7 records from each band in
    // easy/mid/hard order.
    //
    // The driver is the slot itself: the original's 8-byte pool record holds
    // no driver field (byte 1 is the car spec, byte 2 is random, the tail is
    // appearance flags), so opponent identity is the record's position. This
    // used to group drivers by their SR_data tier byte and take the first 7 of
    // each, which is not what the original does and does not fit the data:
    // the shipped tiers are 4/7/10, so the easy band ran 3 slots short and
    // back-filled them with indices that collided with real mid-tier drivers,
    // putting 3 drivers in the pool twice and leaving drivers 18-20 out
    // entirely.
    //
    // Cars are drawn at random without replacement within each band, as the
    // original's per-slot filler does (FUN_0023eac0): it picks a random entry
    // from the band, retries while that entry is already taken, then marks it
    // taken. Shuffling and taking a prefix gives the same distribution without
    // the retry loop - and cannot spin forever on a band of fewer than 7,
    // which the original would.
    for (int tier = 0; tier < 3; ++tier) {
        std::vector<int> band = tier_specs[tier];
        std::shuffle(band.begin(), band.end(), rng_);
        for (int j = 0; j < 7; ++j) {
            RacerEntry entry;
            entry.driver_index = static_cast<int>(pool.size());
            // A band with fewer than 7 cars cannot fill its slots; the stock
            // table splits 9/8/8 so this only guards non-stock car sets.
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
    for (int i = 0; i < static_cast<int>(car_specs_.size()); ++i) {
        if (!car_specs_[i].ForSale()) continue;
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
