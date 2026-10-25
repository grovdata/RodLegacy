#include <doctest/doctest.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "core/save_loader.h"

// A minimal synthetic .sav: one car with 3 installed parts, one spare part, a full trailer.
namespace {

void AppendU16BE(std::vector<uint8_t>& d, uint16_t v) {
    d.push_back(static_cast<uint8_t>(v >> 8));
    d.push_back(static_cast<uint8_t>(v & 0xFF));
}

void AppendU32BE(std::vector<uint8_t>& d, uint32_t v) {
    d.push_back(static_cast<uint8_t>(v >> 24));
    d.push_back(static_cast<uint8_t>(v >> 16));
    d.push_back(static_cast<uint8_t>(v >> 8));
    d.push_back(static_cast<uint8_t>(v & 0xFF));
}

void AppendPartRecord(std::vector<uint8_t>& d, uint16_t value, uint16_t wearRaw, uint8_t variant) {
    AppendU16BE(d, value);
    AppendU16BE(d, wearRaw);
    d.push_back(0);        // reserved
    d.push_back(variant);
    AppendU32BE(d, 0);     // next-ptr: NULL, terminates the list
}

std::vector<uint8_t> BuildSyntheticSave() {
    std::vector<uint8_t> d;

    const char name[] = "TESTDRIVER";
    for (char c : std::string(name)) d.push_back(static_cast<uint8_t>(c));
    while (d.size() < 14) d.push_back(0);       // NUL-pad to 14
    AppendU16BE(d, 0);                          // 0x0E-0x0F reserved
    AppendU32BE(d, 4321);                       // 0x10-0x13 bankroll
    AppendU16BE(d, 0x000A);                     // 0x14-0x15 constant
    AppendU32BE(d, 0); AppendU32BE(d, 0); AppendU32BE(d, 0); // 0x16/0x1A/0x1E noise
    AppendU16BE(d, 1);                          // 0x22-0x23 owned car count = 1
    AppendU32BE(d, 0); AppendU32BE(d, 0);        // 0x24/0x28 noise
    AppendU16BE(d, 0);                          // 0x2C-0x2D
    REQUIRE(d.size() == 46);

    // --- Segment 2: one active car record (52 bytes) ---
    size_t carStart = d.size();
    AppendU16BE(d, 999);   // value
    d.push_back(0);        // reserved
    d.push_back(5);        // specIndex
    d.push_back(3);        // colorIndex
    d.push_back(0);        // unmapped rel+5
    // Slots 0 (Transmission), 1 (Engine) and 6 (Tires) populated.
    uint32_t nonZeroPtr = 1;
    AppendU32BE(d, nonZeroPtr); // 0x06 slot0 Transmission
    AppendU32BE(d, nonZeroPtr); // 0x0A slot1 Engine
    AppendU32BE(d, 0);          // 0x0E slot2 Manifold (empty)
    AppendU32BE(d, 0);          // 0x12 slot3 Carb (empty)
    AppendU32BE(d, 0);          // 0x16 slot4 (empty)
    AppendU32BE(d, 0);          // 0x1A slot5 (empty)
    AppendU32BE(d, nonZeroPtr); // 0x1E slot6 Tires
    while (d.size() < carStart + 0x22) d.push_back(0); // pad to tuning screws
    for (int i = 0; i < 6; ++i) d.push_back(3);         // 0x22-0x27 tuning screws, baseline 3
    AppendU16BE(d, 0);      // 0x28-0x29 unmapped
    d.push_back(0x01);      // 0x2A wire state: plugged
    d.push_back(static_cast<uint8_t>(-3)); // 0x2B tuning knob: -3
    d.push_back(0);         // 0x2C unmapped
    d.push_back(45);        // 0x2D fuel = 4.5 gal
    d.push_back(0x24);      // 0x2E body byte: sticker=2, roof chopped (0x04)
    d.push_back(0);         // 0x2F unmapped
    AppendU32BE(d, 0);      // 0x30 next-car ptr (pointer garbage)
    REQUIRE(d.size() == carStart + 52);

    AppendPartRecord(d, 250, 150, 9);   // slot0 Transmission: $250, 1% worn, variant 9
    AppendPartRecord(d, 500, 0, 6);     // slot1 Engine: $500, 0% worn, variant 6 (GM V8 327)
    AppendPartRecord(d, 80, 250, 41);   // slot6 Tires: $80, 2% worn, variant 41

    // --- Segment 3: owned roster (carcount-1 = 0 records) ---

    // --- Segment 4: flat spare-parts list (one entry) ---
    AppendPartRecord(d, 60, 0xFF80, 18); // stock/racing carb-style sentinel wear

    // --- Trailer (190 bytes, always the last 190 bytes of the file) ---
    size_t trailerStart = d.size();
    // road_rep is negative: it is read as a signed int16.
    AppendU16BE(d, 10);      // races completed
    AppendU16BE(d, 0xFFFD);  // road_rep = -3
    AppendU16BE(d, 6);       // drag_rep = 6
    AppendU16BE(d, 1);       // last race won
    REQUIRE(d.size() == trailerStart + 8);
    for (int i = 0; i < 22; ++i) {
        d.push_back(0);                          // byte 0: always 0x00
        d.push_back(static_cast<uint8_t>(i));     // byte 1: car spec this driver drives
        d.push_back(0);                          // byte 2: paint
        d.push_back(i % 3 == 0 ? 1 : 0);         // byte 3: met
        for (int b = 0; b < 4; ++b) d.push_back(0); // remaining 4 bytes, undecoded
    }
    AppendU32BE(d, 12345); // elapsed ticks
    AppendU16BE(d, 4);     // chunk 8: road-course template index
    REQUIRE(d.size() == trailerStart + 190);

    return d;
}

std::filesystem::path WriteTempSave(const std::vector<uint8_t>& bytes) {
    auto path = std::filesystem::temp_directory_path() / "rodlegacy_test_save.sav";
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}

TEST_CASE("SaveGameLoader: parses a synthetic save matching save_game.md exactly") {
    auto path = WriteTempSave(BuildSyntheticSave());
    LoadedSave save = SaveGameLoader::Load(path);
    std::filesystem::remove(path);

    CHECK(save.name == "TESTDRIVER");
    CHECK(save.bankroll == 4321);
    CHECK(save.ownedCarCount == 1);
    REQUIRE(save.hasActiveCar);
    CHECK(save.roster.empty());

    const auto& car = save.activeCar;
    CHECK(car.value == 999);
    CHECK(car.specIndex == 5);
    CHECK(car.colorIndex == 3);
    for (uint8_t screw : car.tuningScrews) CHECK(screw == 3);
    CHECK(car.wireState == 0x01);
    CHECK(car.tuningKnob == -3);
    CHECK(car.fuelTenths == 45);
    CHECK(car.bodyByte == 0x24);

    REQUIRE(car.sublists[0].size() == 1); // Transmission
    CHECK(car.sublists[0][0].value == 250);
    CHECK(car.sublists[0][0].wearRaw == 150);
    CHECK(car.sublists[0][0].variant == 9);

    REQUIRE(car.sublists[1].size() == 1); // Engine
    CHECK(car.sublists[1][0].value == 500);
    CHECK(car.sublists[1][0].variant == 6);

    CHECK(car.sublists[2].empty()); // Manifold
    CHECK(car.sublists[3].empty()); // Carb
    CHECK(car.sublists[4].empty());
    CHECK(car.sublists[5].empty());

    REQUIRE(car.sublists[6].size() == 1); // Tires
    CHECK(car.sublists[6][0].value == 80);
    CHECK(car.sublists[6][0].wearRaw == 250);
    CHECK(car.sublists[6][0].variant == 41);

    REQUIRE(save.spareParts.size() == 1);
    CHECK(save.spareParts[0].value == 60);
    CHECK(save.spareParts[0].wearRaw == 0xFF80);
    CHECK(save.spareParts[0].variant == 18);

    CHECK(save.racesCompleted == 10);
    CHECK(save.roadRep == -3);
    CHECK(save.dragRep == 6);
    CHECK(save.lastRaceWon == true);
    for (int i = 0; i < 22; ++i) CHECK(save.opponentCarSpec[i] == i);
    for (int i = 0; i < 22; ++i) CHECK(save.opponentMet[i] == (i % 3 == 0));

    CHECK(save.elapsedTicks == 12345);
    // The template is rolled once per profile, so it has to survive the round trip.
    CHECK(save.trackVariant == 4);
}

TEST_CASE("SaveGameLoader: throws on a too-small file rather than reading past the end") {
    std::vector<uint8_t> tooSmall(10, 0);
    auto path = WriteTempSave(tooSmall);
    CHECK_THROWS_AS(SaveGameLoader::Load(path), SaveLoadError);
    std::filesystem::remove(path);
}

TEST_CASE("SaveGameLoader: throws on a missing file") {
    CHECK_THROWS_AS(SaveGameLoader::Load("/nonexistent/path/that/does/not/exist.sav"), SaveLoadError);
}
