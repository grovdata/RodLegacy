/*
 * RodLegacy - The player's standing: Reputation counts wins and races and decides when The King will race.
 *
 * Created: 2026-03-10
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

// Street Rod 2's drive-in menu offers three (ldrivein.c FUN_00250598).
enum class RaceType { Drag, Road, Aqueduct, Grudge };

struct Reputation {
    int drag_rep = 0;
    int road_rep = 0;
    int races_completed = 0;
    // drag_rep/road_rep rise on a counted win and only fall on a loss to The King,
    // so they cannot be read as a tally.
    int drag_wins = 0;
    int road_wins = 0;
    bool last_race_won = false;
    // Street Rod 2 (ldrivein.c, docs/sr2_amiga/king.md): road_rep is the Mulholland rep, plus these.
    int aqueduct_rep = 0;
    int grudge_won = 0;
    int king_stage = 0;   // 0 drag, 1 Mulholland, 2 Aqueduct

    int races_won() const { return drag_wins + road_wins; }

    // A race "just for kicks" is not counted: no race, no win.
    void RecordRace(RaceType type, bool won, bool counted, bool vsKing, bool sr2 = false) {
        if (sr2) {
            // A drag race never counts, and a win raises its own race's rep whatever the stake.
            if (counted && type != RaceType::Drag) races_completed += 1;
            if (vsKing && !won) {
                road_rep -= 3;
                aqueduct_rep -= 2;
            }
            if (won) {
                if (type == RaceType::Drag) drag_wins += 1;
                else if (type == RaceType::Road) { road_rep += 1; road_wins += 1; }
                else if (type == RaceType::Aqueduct) aqueduct_rep += 1;
            }
            last_race_won = won;
            return;
        }
        if (counted) races_completed += 1;
        if (vsKing && !won) {
            drag_rep -= 3;
            road_rep -= 2;
        }
        if (won && counted) {
            if (type == RaceType::Drag) {
                drag_rep += 1;
                drag_wins += 1;
            } else {
                road_rep += 1;
                road_wins += 1;
            }
        }
        last_race_won = won;
    }

    //   0 = The King will race you
    //   1 = novice, no line from The King at all
    //   2 = GameText::KING_AMATEUR
    //   3 = GameText::KING_WIN_MORE
    //   4 = GameText::KING_MORE_DRAG
    //   5 = GameText::KING_MORE_ROAD
    //
    // Street Rod 2 (FUN_0024fc16) reads the Mulholland and Aqueduct reps, and adds Grudge Night:
    //   4 = GameText::KING_NEED_GRUDGE
    //   5 = GameText::KING_MORE_MULHOLLAND
    //   6 = GameText::KING_MORE_AQUEDUCT
    int GetOpponentTier(int pool_remaining, bool sr2 = false) const {
        if (pool_remaining == 0)
            return 0;
        if (sr2) {
            const int n = road_rep + aqueduct_rep;
            if (n < 3) return 1;
            if (n >= 7 && road_rep < 6 && aqueduct_rep < 6) return 3;
            if (n < 7 && road_rep < 6 && aqueduct_rep < 6) return 2;
            if (road_rep < 6) return 5;
            if (aqueduct_rep < 6) return 6;
            if (grudge_won == 0) return 4;
            return n >= races_completed / 2 ? 0 : 3;
        }

        int total = drag_rep + road_rep;

        if (total < 3)
            return 1;

        if (total >= 7 && drag_rep < 6 && road_rep < 6)
            return 3;

        if (total < 7 && drag_rep < 6 && road_rep < 6)
            return 2;
        if (drag_rep < 6)
            return 4;
        if (road_rep < 6)
            return 5;
        // Both reps clear 6: the King still wants wins to outnumber the races
        // you have spent getting them.
        if (total < races_completed / 2)
            return 3;
        return 0;
    }

    bool IsKingUnlocked(int pool_remaining, bool sr2 = false) const {
        return GetOpponentTier(pool_remaining, sr2) == 0;
    }
};
