/*
 * RodLegacy - The player's standing: Reputation counts wins and races and decides when The King will race.
 *
 * Created: 2026-03-10
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

enum class RaceType { Drag, Road };

struct Reputation {
    int drag_rep = 0;
    int road_rep = 0;
    int races_completed = 0;
    // drag_rep/road_rep rise on a counted win and only fall on a loss to The King,
    // so they cannot be read as a tally.
    int drag_wins = 0;
    int road_wins = 0;
    bool last_race_won = false;

    int races_won() const { return drag_wins + road_wins; }

    // A race "just for kicks" is not counted: no race, no win.
    void RecordRace(RaceType type, bool won, bool counted, bool vsKing) {
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
    int GetOpponentTier(int pool_remaining) const {
        if (pool_remaining == 0)
            return 0;

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

    bool IsKingUnlocked(int pool_remaining) const {
        return GetOpponentTier(pool_remaining) == 0;
    }
};
