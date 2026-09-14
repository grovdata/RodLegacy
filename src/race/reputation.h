#pragma once

enum class RaceType { Drag, Road };

struct Reputation {
    int drag_rep = 0;
    int road_rep = 0;
    int races_completed = 0;
    // Wins are counted separately: drag_rep/road_rep rise by 1 on a win but fall
    // by 3/2 on a loss, so they are reputation and cannot be read as a tally.
    int drag_wins = 0;
    int road_wins = 0;
    bool last_race_won = false;

    int races_won() const { return drag_wins + road_wins; }

    void RecordWin(RaceType type) {
        if (type == RaceType::Drag) {
            drag_rep += 1;
            drag_wins += 1;
        } else {
            road_rep += 1;
            road_wins += 1;
        }
        races_completed += 1;
        last_race_won = true;
    }

    // ChallengeRaceLoop 0023e60c: a loss costs drag standing more than road
    // standing, whichever kind of race it was. A4-0x4678 (drag, -3) and
    // A4-0x4676 (road, -2) were labelled the wrong way round in the
    // decompile; the race-type flag A4+0x19de is 1 for road (it is the branch
    // that burns 15 fuel and rolls for a cop), 0 for drag.
    void RecordLoss() {
        drag_rep -= 3;
        road_rep -= 2;
        races_completed += 1;
        last_race_won = false;
    }

    // GetOpponentDifficulty (0023d03a), transcribed branch for branch.
    //   0 = The King will race you
    //   1 = novice, no line from The King at all
    //   2 = "I don't got time for amateurs like you!"
    //   3 = "You're doing OK kid, but you're gonna need to win more races!"
    //   4 = "You're gonna hafta win more drag races!"
    //   5 = "You're gonna hafta win more road races!"
    // pool_remaining: number of unbeaten regular opponents still in pool
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
