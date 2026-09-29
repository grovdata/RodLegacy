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
    //   2 = "I don't got time for amateurs like you!"
    //   3 = "You're doing OK kid, but you're gonna need to win more races!"
    //   4 = "You're gonna hafta win more drag races!"
    //   5 = "You're gonna hafta win more road races!"
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
