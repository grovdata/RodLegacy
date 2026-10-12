#include <doctest/doctest.h>
#include "race/reputation.h"

namespace {
void Win(Reputation& r, RaceType t) { r.RecordRace(t, true, true, false); }
void Loss(Reputation& r) { r.RecordRace(RaceType::Drag, false, true, false); }
}

TEST_SUITE("Reputation::RecordRace") {

TEST_CASE("drag win increments drag_rep") {
    Reputation r;
    Win(r, RaceType::Drag);
    CHECK(r.drag_rep == 1);
    CHECK(r.road_rep == 0);
}

TEST_CASE("road win increments road_rep") {
    Reputation r;
    Win(r, RaceType::Road);
    CHECK(r.drag_rep == 0);
    CHECK(r.road_rep == 1);
}

TEST_CASE("win increments races_completed and sets last_race_won") {
    Reputation r;
    Win(r, RaceType::Drag);
    CHECK(r.races_completed == 1);
    CHECK(r.last_race_won == true);
}

TEST_CASE("an ordinary loss only counts the race") {
    Reputation r;
    Win(r, RaceType::Drag);
    Loss(r);
    CHECK(r.drag_rep == 1);
    CHECK(r.road_rep == 0);
    CHECK(r.races_completed == 2);
    CHECK(r.last_race_won == false);
}

TEST_CASE("a race just for kicks counts nothing") {
    Reputation r;
    r.RecordRace(RaceType::Drag, true, false, false);
    r.RecordRace(RaceType::Road, false, false, false);
    CHECK(r.drag_rep == 0);
    CHECK(r.road_rep == 0);
    CHECK(r.races_completed == 0);
}

TEST_CASE("a loss to The King costs 3 drag and 2 road") {
    Reputation r;
    r.RecordRace(RaceType::Road, false, true, true);
    CHECK(r.drag_rep == -3);
    CHECK(r.road_rep == -2);
    CHECK(r.races_completed == 1);
    r.RecordRace(RaceType::Drag, false, true, true);
    CHECK(r.drag_rep == -6);
    CHECK(r.road_rep == -4);
}

}

TEST_SUITE("Reputation::GetOpponentTier") {

TEST_CASE("fresh game: tier 1 (total < 3)") {
    Reputation r;
    CHECK(r.GetOpponentTier(10) == 1);
}

TEST_CASE("pool exhausted: tier 0 regardless of reps") {
    Reputation r;
    CHECK(r.GetOpponentTier(0) == 0);
}

TEST_CASE("pool exhausted takes priority over rep-based tiers") {
    Reputation r;
    Win(r, RaceType::Drag);
    Win(r, RaceType::Road);
    CHECK(r.GetOpponentTier(0) == 0);
}

TEST_CASE("tier 1: total below 3") {
    Reputation r;
    Win(r, RaceType::Drag);   // drag=1 road=0 total=1
    CHECK(r.GetOpponentTier(5) == 1);
    Win(r, RaceType::Road);   // drag=1 road=1 total=2
    CHECK(r.GetOpponentTier(5) == 1);
}

TEST_CASE("tier 2: 3 <= total < 7, both reps below 6") {
    Reputation r;
    Win(r, RaceType::Drag);
    Win(r, RaceType::Drag);
    Win(r, RaceType::Road);   // drag=2 road=1 total=3
    CHECK(r.GetOpponentTier(5) == 2);

    Win(r, RaceType::Drag);
    Win(r, RaceType::Road);
    Win(r, RaceType::Road);   // drag=3 road=3 total=6
    CHECK(r.GetOpponentTier(5) == 2);
}

TEST_CASE("tier 3: total >= 7, neither rep at 6 yet") {
    Reputation r;
    for (int i = 0; i < 5; i++) Win(r, RaceType::Drag);
    for (int i = 0; i < 5; i++) Win(r, RaceType::Road); // 5/5 total=10
    CHECK(r.GetOpponentTier(5) == 3);
}

TEST_CASE("tier 4: drag_rep short of 6") {
    Reputation r;
    for (int i = 0; i < 7; i++) Win(r, RaceType::Road); // road=7 drag=0
    CHECK(r.GetOpponentTier(5) == 4);
    Reputation s;
    for (int i = 0; i < 6; i++) Win(s, RaceType::Road); // road=6 total=6 < 7
    CHECK(s.GetOpponentTier(5) == 4);
}

TEST_CASE("tier 5: drag_rep at 6, road_rep short") {
    Reputation r;
    for (int i = 0; i < 7; i++) Win(r, RaceType::Drag); // drag=7 road=0
    CHECK(r.GetOpponentTier(5) == 5);
}

TEST_CASE("tier 3: both reps at 6 but wins lag races completed") {
    Reputation r;
    for (int i = 0; i < 6; i++) Win(r, RaceType::Drag);
    for (int i = 0; i < 6; i++) Win(r, RaceType::Road);
    for (int i = 0; i < 14; i++) Loss(r);
    // drag=6 road=6 total=12 races=26 -> 12 < 13
    CHECK(r.GetOpponentTier(5) == 3);
    Win(r, RaceType::Drag); // total=13 races=27 -> 13 >= 13
    CHECK(r.GetOpponentTier(5) == 0);
}

}

TEST_SUITE("Reputation::IsKingUnlocked") {

TEST_CASE("King not unlocked at start") {
    Reputation r;
    CHECK_FALSE(r.IsKingUnlocked(10));
}

TEST_CASE("King unlocked when pool is empty") {
    Reputation r;
    CHECK(r.IsKingUnlocked(0));
}

TEST_CASE("King unlocked when both reps >= 6 and total >= races_completed/2") {
    Reputation r;
    // 6 drag + 6 road wins, races=12: total 12 >= 6, so The King
    for (int i = 0; i < 6; i++) Win(r, RaceType::Drag);
    for (int i = 0; i < 6; i++) Win(r, RaceType::Road);
    CHECK(r.IsKingUnlocked(5));
}

TEST_CASE("King not unlocked if only one rep >= 6") {
    Reputation r;
    for (int i = 0; i < 6; i++) Win(r, RaceType::Drag); // drag=6, road=0
    CHECK_FALSE(r.IsKingUnlocked(5));
}

TEST_CASE("losing to The King locks him again") {
    Reputation r;
    for (int i = 0; i < 6; i++) Win(r, RaceType::Drag);
    for (int i = 0; i < 6; i++) Win(r, RaceType::Road);
    // A loss to The King: drag=3, road=4. Not King.
    r.RecordRace(RaceType::Road, false, true, true);
    CHECK_FALSE(r.IsKingUnlocked(5));
}

}

TEST_SUITE("Reputation SR2") {
TEST_CASE("Mulholland and Aqueduct wins raise their own reps; drag races never count") {
    Reputation r;
    r.RecordRace(RaceType::Drag, true, true, false, true);
    CHECK(r.races_completed == 0);
    CHECK(r.drag_wins == 1);
    r.RecordRace(RaceType::Road, true, false, false, true);   // just for kicks: a win still counts for rep
    CHECK(r.road_rep == 1);
    CHECK(r.races_completed == 0);
    r.RecordRace(RaceType::Aqueduct, true, true, false, true);
    CHECK(r.aqueduct_rep == 1);
    CHECK(r.races_completed == 1);
}

TEST_CASE("the King wants both reps, then Grudge Night, then wins over races") {
    Reputation r;
    r.road_rep = 1; r.aqueduct_rep = 1;
    CHECK(r.GetOpponentTier(5, true) == 1);
    r.road_rep = 3; r.aqueduct_rep = 2;
    CHECK(r.GetOpponentTier(5, true) == 2);
    r.road_rep = 4; r.aqueduct_rep = 4;
    CHECK(r.GetOpponentTier(5, true) == 3);
    r.road_rep = 5; r.aqueduct_rep = 6;
    CHECK(r.GetOpponentTier(5, true) == 5);
    r.road_rep = 6; r.aqueduct_rep = 5;
    CHECK(r.GetOpponentTier(5, true) == 6);
    r.aqueduct_rep = 6;
    CHECK(r.GetOpponentTier(5, true) == 4);
    r.grudge_won = 1;
    r.races_completed = 30;
    CHECK(r.GetOpponentTier(5, true) == 3);
    r.races_completed = 24;
    CHECK(r.GetOpponentTier(5, true) == 0);
    // Every pool racer beaten: he races whatever the reps.
    CHECK(Reputation{}.GetOpponentTier(0, true) == 0);
}

TEST_CASE("losing to the King costs 3 Mulholland and 2 Aqueduct") {
    Reputation r;
    r.road_rep = 6; r.aqueduct_rep = 6;
    r.RecordRace(RaceType::Road, false, true, true, true);
    CHECK(r.road_rep == 3);
    CHECK(r.aqueduct_rep == 4);
}
}
