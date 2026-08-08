#include <doctest/doctest.h>
#include "parsers/libp.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// DOS HOT_DATA parsing. Layout: RodLegacyResearch/docs/formats/hot_data_dos.md.

namespace {

void PutU16LE(std::vector<uint8_t>& buf, size_t at, uint16_t v) {
    buf[at] = static_cast<uint8_t>(v & 0xFF);
    buf[at + 1] = static_cast<uint8_t>(v >> 8);
}

// A HOT_DATA with every table present and one classified ad per buyable car.
// Car i gets price 100+i, tier i%9, and sprite numbers derived from i so the
// parser cannot pass by returning a constant.
std::vector<uint8_t> MakeSyntheticHotData() {
    std::vector<uint8_t> buf(HOT_DATA_TEXT_START, 0);

    for (int i = 0; i < HOT_DATA_CAR_COUNT; ++i) {
        const size_t spec = HOT_DATA_CARSPEC_START + i * 10;
        PutU16LE(buf, spec, static_cast<uint16_t>(100 + i));
        buf[spec + 2] = static_cast<uint8_t>(i % 9);       // tier
        buf[spec + 3] = static_cast<uint8_t>(i + 1);       // listing order
        buf[spec + 4] = 0x02 | (1 << 4) | (3 << 6);        // Ford, tires 1, trans 3
        buf[spec + 5] = 2 | (4 << 3) | (1 << 6);           // carb 2, manifold 4, engine 1
        PutU16LE(buf, spec + 8, static_cast<uint16_t>(HOT_DATA_SPRITE_BASE + 40 + i));

        const size_t spr = HOT_DATA_CARSPRITES_START + i * 10;
        PutU16LE(buf, spr, 2);
        // The King (last entry) gets no variants, like the real files.
        const bool king = (i == HOT_DATA_CAR_COUNT - 1);
        PutU16LE(buf, spr + 2, king ? 0 : static_cast<uint16_t>(HOT_DATA_SPRITE_BASE + 200 + i));
        PutU16LE(buf, spr + 4, king ? 0 : static_cast<uint16_t>(HOT_DATA_SPRITE_BASE + 182));
        PutU16LE(buf, spr + 6, king ? 0 : static_cast<uint16_t>(HOT_DATA_SPRITE_BASE + 100 + i));
        PutU16LE(buf, spr + 8, king ? 0 : static_cast<uint16_t>(HOT_DATA_SPRITE_BASE + 150 + i));

        for (int f = 0; f < 12; ++f) {
            buf[HOT_DATA_OFFSETS_START + i * 12 + f] = static_cast<uint8_t>(i + f);
        }
        for (int f = 0; f < 20; ++f) {
            buf[HOT_DATA_CLICKBOX_START + i * 20 + f] = static_cast<uint8_t>(i * 2 + f);
        }
        for (int f = 0; f < 6; ++f) {
            buf[HOT_DATA_GASSTATION_START + i * 6 + f] = static_cast<uint8_t>(i * 3 + f);
        }
    }

    // Text blob: name line then a price line, per buyable car.
    size_t adOffset = 0;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        PutU16LE(buf, HOT_DATA_CARSPEC_START + i * 10 + 6, static_cast<uint16_t>(adOffset));
        const std::string name = "19" + std::to_string(40 + i) + " Testmobile.";
        const std::string price = "Runs. $" + std::to_string(100 + i);
        for (const std::string& s : {name, price}) {
            buf.insert(buf.end(), s.begin(), s.end());
            buf.push_back(0);
            adOffset += s.size() + 1;
        }
    }
    return buf;
}

std::vector<uint8_t> LoadHotData(const char* relative) {
    const std::string prefixes[] = {"../", "../../"};
    for (const std::string& prefix : prefixes) {
        std::ifstream f(prefix + relative, std::ios::binary);
        if (f) {
            return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
        }
    }
    return {};
}

} // namespace

TEST_SUITE("HotDataDos") {

TEST_CASE("car spec fields round-trip through the Amiga-shaped struct") {
    const std::vector<uint8_t> buf = MakeSyntheticHotData();
    DataParser parser;
    const auto specs = parser.ParseCarSpecTableDOS(buf);
    REQUIRE(specs.size() == HOT_DATA_CAR_COUNT);

    for (int i = 0; i < HOT_DATA_CAR_COUNT; ++i) {
        CHECK(specs[i].Price() == 100 + i);
        CHECK(specs[i].car_class == i % 9);
        CHECK(specs[i].Brand() == 1);          // Ford: mask 2, collapsed by >>13
        CHECK(specs[i].TireClass() == 1);
        CHECK(specs[i].TransClass() == 3);
        CHECK(specs[i].EngineClass() == 1);
        CHECK(specs[i].ManifoldClass() == 4);
        CHECK(specs[i].CarbClass() == 2);
        CHECK(specs[i].BodyAsset() == 40 + i);
    }
}

TEST_CASE("brand mask maps to the same values the Amiga path produces") {
    std::vector<uint8_t> buf = MakeSyntheticHotData();
    DataParser parser;
    struct { uint8_t mask; int expected; } cases[] = {{1, 0}, {2, 1}, {4, 2}};
    for (const auto& c : cases) {
        buf[HOT_DATA_CARSPEC_START + 4] = c.mask;
        CHECK(parser.ParseCarSpecTableDOS(buf)[0].Brand() == c.expected);
    }
}

TEST_CASE("sprite refs drop the +1000 base and report missing variants as -1") {
    const std::vector<uint8_t> buf = MakeSyntheticHotData();
    DataParser parser;
    const auto sprites = parser.ParseCarSpritesDOS(buf);
    REQUIRE(sprites.size() == HOT_DATA_CAR_COUNT);

    CHECK(sprites[0].body == 40);
    CHECK(sprites[0].choppedRoof == 200);
    CHECK(sprites[0].airScoop == 182);
    CHECK(sprites[0].rearBumperStripped == 100);
    CHECK(sprites[0].frontBumperStripped == 150);

    const DosCarSprites& king = sprites[HOT_DATA_CAR_COUNT - 1];
    CHECK(king.body == 40 + HOT_DATA_CAR_COUNT - 1);
    CHECK(king.choppedRoof == -1);
    CHECK(king.rearBumperStripped == -1);
    CHECK(king.frontBumperStripped == -1);
}

TEST_CASE("metadata reader fills offsets, click boxes and classifieds") {
    const std::vector<uint8_t> buf = MakeSyntheticHotData();
    DataParser parser;
    int count = 0;
    CarMetadata_t* meta = parser.SrLibDataReadDataFromBufferDOS(buf.data(), buf.size(), &count);
    REQUIRE(meta != nullptr);
    REQUIRE(count == CAR_PLAYER_MODEL_COUNT);

    CHECK(meta[3].offsets.offsetXRearTire == 3);
    CHECK(meta[3].offsets.offsetYDecal == 3 + 11);
    // Gas cap comes from the gas station table, not the 12-byte offset record.
    CHECK(meta[3].offsets.offsetXGasCap == 3 * 3);
    CHECK(meta[3].offsets.offsetYGasCap == 3 * 3 + 1);

    // Rectangle order is rear bumper, chopped roof, transmission, engine, front bumper.
    CHECK(meta[2].clickBoxes.clickBoxRearBumper.x == 4);
    CHECK(meta[2].clickBoxes.clickBoxChoppedRoof.x == 8);
    CHECK(meta[2].clickBoxes.clickBoxFrontBumper.x == 20);
    CHECK(meta[2].clickBoxes.clickBoxGasCap.width == 12);
    CHECK(meta[2].clickBoxes.clickBoxRearWindow.width == 24);

    CHECK(std::string(meta[0].classified.carName) == "1940 Testmobile.");
    CHECK(meta[0].classified.year == 1940);
    CHECK(meta[0].classified.price == 100);
    CHECK(std::string(meta[4].classified.carName) == "1944 Testmobile.");
    CHECK(meta[4].classified.price == 104);

    for (int i = 0; i < count; i++) SrLibDataFreeClassified(&meta[i].classified);
    free(meta);
}

TEST_CASE("a truncated file is rejected rather than read out of bounds") {
    std::vector<uint8_t> buf = MakeSyntheticHotData();
    buf.resize(HOT_DATA_TEXT_START - 1);
    DataParser parser;
    int count = -1;
    CHECK(parser.ParseCarSpecTableDOS(buf).empty());
    CHECK(parser.ParseCarSpritesDOS(buf).empty());
    CHECK(parser.SrLibDataReadDataFromBufferDOS(buf.data(), buf.size(), &count) == nullptr);
    CHECK(count == 0);
}

// ---------------------------------------------------------------------------
// Against the shipped DOS files
// ---------------------------------------------------------------------------

TEST_CASE("stock DOS HOT_DATA matches the known car list") {
    const std::vector<uint8_t> data = LoadHotData("RodLegacyBins/dos/SR1/HOT_DATA");
    if (data.empty()) {
        MESSAGE("shipped DOS SR1 HOT_DATA not found next to the build; skipping");
        return;
    }
    DataParser parser;
    const auto specs = parser.ParseCarSpecTableDOS(data);
    const auto sprites = parser.ParseCarSpritesDOS(data);
    REQUIRE(specs.size() == HOT_DATA_CAR_COUNT);
    REQUIRE(sprites.size() == HOT_DATA_CAR_COUNT);

    CHECK(specs[0].Price() == 400);      // 1940 Chevrolet 2-dr coupe
    CHECK(specs[0].Brand() == 0);        // GM
    CHECK(specs[5].Price() == 3770);     // 1963 Corvette, the fastest buyable
    CHECK(specs[5].car_class == 8);
    CHECK(specs[25].Price() == 0);       // The King is never for sale
    CHECK(!specs[25].ForSale());

    // The three Corvettes are the cars with no chopped roof, and the two 1932
    // Fords are the ones with no bumper variants - same as AmigaLibAsset lists.
    for (int slot : {3, 4, 5}) {
        CHECK(sprites[slot].choppedRoof == -1);
        CHECK(sprites[slot].rearBumperStripped != -1);
    }
    for (int slot : {7, 10}) {
        CHECK(sprites[slot].choppedRoof != -1);
        CHECK(sprites[slot].rearBumperStripped == -1);
        CHECK(sprites[slot].frontBumperStripped == -1);
    }

    int count = 0;
    CarMetadata_t* meta = parser.SrLibDataReadDataFromBufferDOS(data.data(), data.size(), &count);
    REQUIRE(meta != nullptr);
    REQUIRE(count == CAR_PLAYER_MODEL_COUNT);
    CHECK(std::string(meta[0].classified.carName) == "1940 Chevrolet 2-dr coupe.");
    CHECK(meta[0].classified.price == 400);
    // A car with no bumper variants has the matching offset fields zeroed.
    CHECK(meta[7].offsets.offsetYRearBumper == 0);
    CHECK(meta[7].offsets.offsetXFrontBumper == 0);
    for (int i = 0; i < count; i++) SrLibDataFreeClassified(&meta[i].classified);
    free(meta);
}

TEST_CASE("SRSE HOT_DATA gives the replacement cars and their sprites") {
    const std::vector<uint8_t> data = LoadHotData("RodLegacyBins/dos/SRSE/HOT_DATA");
    if (data.empty()) {
        MESSAGE("SRSE HOT_DATA not found next to the build; skipping");
        return;
    }
    DataParser parser;
    const auto specs = parser.ParseCarSpecTableDOS(data);
    const auto sprites = parser.ParseCarSpritesDOS(data);
    REQUIRE(specs.size() == HOT_DATA_CAR_COUNT);

    // Same 25 body sprites as stock DOS, different art behind them.
    const int expectedBodies[] = {39, 40, 47, 48, 53, 54, 60, 61, 112, 113, 118, 119, 126,
                                  127, 131, 135, 145, 149, 156, 157, 164, 165, 171, 172, 179};
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        CHECK(sprites[i].body == expectedBodies[i]);
    }

    // Unlike stock, every SRSE car has all three variants.
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        CHECK(sprites[i].choppedRoof != -1);
        CHECK(sprites[i].rearBumperStripped != -1);
        CHECK(sprites[i].frontBumperStripped != -1);
    }
    CHECK(sprites[0].choppedRoof == 38);
    CHECK(sprites[0].rearBumperStripped == 37);
    CHECK(sprites[0].frontBumperStripped == 33);
    // Slot 21's roof sits outside the car blocks, and slots 4/9/18/19 share one
    // rear bumper patch.
    CHECK(sprites[21].choppedRoof == 235);
    for (int slot : {4, 9, 18, 19}) CHECK(sprites[slot].rearBumperStripped == 153);

    // SRSE cars are not just restyled - the stats differ from the stock slot.
    CHECK(specs[2].Price() == 2300);
    CHECK(specs[2].car_class == 7);
    CHECK(specs[2].TireClass() == 2);

    int count = 0;
    CarMetadata_t* meta = parser.SrLibDataReadDataFromBufferDOS(data.data(), data.size(), &count);
    REQUIRE(meta != nullptr);
    REQUIRE(count == CAR_PLAYER_MODEL_COUNT);
    CHECK(std::string(meta[0].classified.carName) == "1939 Pontiac 2-dr coupe.");
    CHECK(std::string(meta[12].classified.carName) == "1934 Ford Woodie.");
    CHECK(meta[12].classified.price == 3100);
    // The data disk ships its own pixel offsets, so nothing needs hand
    // measuring. A zero here is a real coordinate, not an absent variant -
    // the Woodie's chopped roof sits flush at x=0 and it does have a roof
    // sprite; only the sprite table says whether a variant exists.
    CHECK(meta[12].offsets.offsetXChoppedRoof == 0);
    CHECK(sprites[12].choppedRoof == 125);
    CHECK(meta[12].offsets.offsetXFrontTire != 0);
    for (int i = 0; i < count; i++) SrLibDataFreeClassified(&meta[i].classified);
    free(meta);
}

TEST_CASE("shifted SRSE car keys cannot collide with lib.p car assets") {
    const std::vector<uint8_t> data = LoadHotData("RodLegacyBins/dos/SRSE/HOT_DATA");
    if (data.empty()) {
        MESSAGE("SRSE HOT_DATA not found next to the build; skipping");
        return;
    }
    DataParser parser;
    const auto sprites = parser.ParseCarSpritesDOS(data);
    REQUIRE(sprites.size() == HOT_DATA_CAR_COUNT);

    // CarModel::index is the pool's key. Raw SRSE body numbers run 39..179 and
    // would collide with lib.p's car assets (179..272) - slot 24's body really
    // is 179, the same number as CHEVROLET_2DR_COUPE_1940 - so CarData shifts
    // them by HOT_DATA_SPRITE_BASE. Check nothing lands back in lib.p's range.
    bool sawRawCollision = false;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        for (int stock = 0; stock < CAR_INDEX_COUNT; ++stock) {
            if (sprites[i].body == kCarIndex[stock]) sawRawCollision = true;
        }
        const int key = HOT_DATA_SPRITE_BASE + sprites[i].body;
        CHECK(key > THE_KING_CHEVROLET_CORVETTE_1963_BODY);
        for (int stock = 0; stock < CAR_INDEX_COUNT; ++stock) {
            CHECK(key != kCarIndex[stock]);
        }
    }
    // If this ever stops being true the shift is no longer load-bearing, but
    // it is true today and is the reason the shift exists.
    CHECK(sawRawCollision);
}

} // TEST_SUITE
