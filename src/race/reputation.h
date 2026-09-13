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

    // Penalties confirmed against the Amiga original (ChallengeRaceLoop
    // 0023e60c): a loss costs road standing more than drag standing,
    // whichever kind of race it was. These were the other way round here.
    void RecordLoss() {
        road_rep -= 3;
        drag_rep -= 2;
        races_completed += 1;
        last_race_won = false;
    }

    // Returns opponent difficulty tier:
    //   0 = The King (pool exhausted, or both reps maxed)
    //   1 = novice  (total < 3)
    //   2 = easy    (3 <= total < 7, both reps < 6)
    //   3 = mid     (3 <= total < 7, one rep >= 6)
    //   4 = hard    (total >= 7, drag_rep >= 6, road_rep < 6)
    //   5 = hardest (total >= 7, drag_rep < 6)
    // pool_remaining: number of unbeaten regular opponents still in pool
    int GetOpponentTier(int pool_remaining) const {
        if (pool_remaining == 0)
            return 0;

        int total = drag_rep + road_rep;

        if (drag_rep >= 6 && road_rep >= 6 && total >= races_completed / 2)
            return 0;

        if (total < 3)
            return 1;

        if (total < 7) {
            if (drag_rep < 6 && road_rep < 6)
                return 2;
            return 3;
        }

        // total >= 7
        if (road_rep < 6)
            return 4;
        return 5;
    }

    bool IsKingUnlocked(int pool_remaining) const {
        return GetOpponentTier(pool_remaining) == 0;
    }
};
