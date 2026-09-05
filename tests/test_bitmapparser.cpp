#include <doctest/doctest.h>
#include "parsers/libp.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a minimal Amiga lib binary: 2-byte BE count + N * sizeof(BitmapEntry_t) headers
static std::vector<uint8_t> MakeAmigaLibHeader(const std::vector<BitmapEntry_t>& entries) {
    std::vector<uint8_t> out;
    uint16_t count = (uint16_t)entries.size();
    out.push_back(count >> 8);
    out.push_back(count & 0xFF);
    for (const auto& e : entries) {
        // Fields in wire order (BE): width(2), height(2), depth(1), unknown1(1), rleDict(16), offset(4), sizePacked(2)
        out.push_back(e.width >> 8);   out.push_back(e.width & 0xFF);
        out.push_back(e.height >> 8);  out.push_back(e.height & 0xFF);
        out.push_back(e.depth);
        out.push_back(e.unknown1);
        for (int i = 0; i < 16; i++) out.push_back(e.rleDictionary[i]);
        out.push_back((e.offset >> 24) & 0xFF);
        out.push_back((e.offset >> 16) & 0xFF);
        out.push_back((e.offset >>  8) & 0xFF);
        out.push_back( e.offset        & 0xFF);
        out.push_back(e.sizePacked >> 8);
        out.push_back(e.sizePacked & 0xFF);
    }
    return out;
}

// ---------------------------------------------------------------------------
// GetRGBFromRGB4
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetRGBFromRGB4") {

TEST_CASE("black (0x000)") {
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0x000);
    CHECK(c.r == 0);
    CHECK(c.g == 0);
    CHECK(c.b == 0);
    CHECK(c.a == 255);
}

TEST_CASE("white (0xFFF) gives 0xF0 per channel") {
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0xFFF);
    CHECK(c.r == 0xF0);
    CHECK(c.g == 0xF0);
    CHECK(c.b == 0xF0);
    CHECK(c.a == 255);
}

TEST_CASE("pure red (0xF00)") {
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0xF00);
    CHECK(c.r == 0xF0);
    CHECK(c.g == 0);
    CHECK(c.b == 0);
}

TEST_CASE("pure green (0x0F0)") {
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0x0F0);
    CHECK(c.r == 0);
    CHECK(c.g == 0xF0);
    CHECK(c.b == 0);
}

TEST_CASE("pure blue (0x00F)") {
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0x00F);
    CHECK(c.r == 0);
    CHECK(c.g == 0);
    CHECK(c.b == 0xF0);
}

TEST_CASE("nibble extraction is independent per channel") {
    // 0x810: R=0x8 → 0x80, G=0x1 → 0x10, B=0x0 → 0x00
    SrLibColor c = BitmapParser::GetRGBFromRGB4(0x810);
    CHECK(c.r == 0x80);
    CHECK(c.g == 0x10);
    CHECK(c.b == 0x00);
}

TEST_CASE("alpha is always 255") {
    for (uint16_t v : {(uint16_t)0x000, (uint16_t)0x888, (uint16_t)0xFFF}) {
        CHECK(BitmapParser::GetRGBFromRGB4(v).a == 255);
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// PreparePalette
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::PreparePalette") {

TEST_CASE("without EHB: output size equals input size") {
    BitmapParser p;
    std::vector<uint16_t> pal = {0x000, 0xFFF, 0xF00, 0x0F0};
    auto result = p.PreparePalette(pal, false);
    CHECK(result.size() == pal.size());
}

TEST_CASE("without EHB: colours are converted correctly") {
    BitmapParser p;
    std::vector<uint16_t> pal = {0xF00, 0x0F0};
    auto result = p.PreparePalette(pal, false);
    CHECK(result[0].r == 0xF0);
    CHECK(result[0].g == 0x00);
    CHECK(result[1].r == 0x00);
    CHECK(result[1].g == 0xF0);
}

TEST_CASE("with EHB: output size is doubled") {
    BitmapParser p;
    std::vector<uint16_t> pal(32, 0x000);
    auto result = p.PreparePalette(pal, true);
    CHECK(result.size() == 64);
}

TEST_CASE("with EHB: first half matches non-EHB version") {
    BitmapParser p;
    std::vector<uint16_t> pal = {0xF00, 0x0F0, 0x00F};
    auto normal = p.PreparePalette(pal, false);
    auto ehb    = p.PreparePalette(pal, true);
    for (size_t i = 0; i < pal.size(); i++) {
        CHECK(ehb[i].r == normal[i].r);
        CHECK(ehb[i].g == normal[i].g);
        CHECK(ehb[i].b == normal[i].b);
    }
}

TEST_CASE("with EHB: second half is half-brightness") {
    BitmapParser p;
    std::vector<uint16_t> pal = {0xF00}; // r=0xF0, g=0, b=0
    auto result = p.PreparePalette(pal, true);
    // EHB: (0xF0 / 2) & 0xF0 = 0x78 & 0xF0 = 0x70
    CHECK(result[1].r == (uint8_t)((0xF0 / 2) & 0xF0));
    CHECK(result[1].g == 0);
    CHECK(result[1].b == 0);
    CHECK(result[1].a == 255);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// PrepareCarPalette
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::PrepareCarPalette") {

TEST_CASE("output has 32 normal + palette.size() EHB entries") {
    BitmapParser p;
    // The function adds 32 normal entries then palette.size() EHB entries
    std::vector<uint16_t> pal(32, 0xF00); // 32-entry palette → 32+32=64
    auto result = p.PrepareCarPalette(pal);
    CHECK(result.size() == 64);
}

TEST_CASE("index 0 is black") {
    BitmapParser p;
    std::vector<uint16_t> pal(9, 0xFFF);
    auto result = p.PrepareCarPalette(pal);
    CHECK(result[0].r == 0);
    CHECK(result[0].g == 0);
    CHECK(result[0].b == 0);
}

TEST_CASE("indices 1-9 carry palette colours") {
    BitmapParser p;
    // Pad to 32 entries so the EHB loop doesn't crash for colours at indices 10+
    std::vector<uint16_t> pal(32, 0x000);
    pal[0] = 0xF00; pal[1] = 0x0F0; pal[2] = 0x00F;
    pal[3] = 0x888; pal[4] = 0xFFF; pal[5] = 0x123;
    pal[6] = 0x456; pal[7] = 0x789; pal[8] = 0xABC;
    auto result = p.PrepareCarPalette(pal);
    for (int i = 0; i < 9; i++) {
        SrLibColor expected = BitmapParser::GetRGBFromRGB4(pal[i]);
        CHECK(result[i + 1].r == expected.r);
        CHECK(result[i + 1].g == expected.g);
        CHECK(result[i + 1].b == expected.b);
    }
}

TEST_CASE("indices 10-31 are black") {
    BitmapParser p;
    std::vector<uint16_t> pal(9, 0xFFF);
    auto result = p.PrepareCarPalette(pal);
    for (int i = 10; i < 32; i++) {
        CHECK(result[i].r == 0);
        CHECK(result[i].g == 0);
        CHECK(result[i].b == 0);
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetDOSPalette
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetDOSPalette") {

TEST_CASE("type 0: 16 entries") {
    BitmapParser p;
    auto pal = p.GetDOSPalette(0);
    CHECK(pal.size() == 16);
}

TEST_CASE("type 0: index 0 is black") {
    BitmapParser p;
    auto pal = p.GetDOSPalette(0);
    CHECK(pal[0].r == 0x00);
    CHECK(pal[0].g == 0x00);
    CHECK(pal[0].b == 0x00);
}

TEST_CASE("type 1: index 0 is red (transparency marker)") {
    BitmapParser p;
    auto pal = p.GetDOSPalette(1);
    CHECK(pal[0].r == 0xFF);
    CHECK(pal[0].g == 0x00);
    CHECK(pal[0].b == 0x00);
}

TEST_CASE("type 2: index 0 is white (mask)") {
    BitmapParser p;
    auto pal = p.GetDOSPalette(2);
    CHECK(pal[0].r == 0xFF);
    CHECK(pal[0].g == 0xFF);
    CHECK(pal[0].b == 0xFF);
}

TEST_CASE("type 1 and 2 only differ at index 0") {
    BitmapParser p;
    auto pal1 = p.GetDOSPalette(1);
    auto pal0 = p.GetDOSPalette(0);
    for (int i = 1; i < 16; i++) {
        CHECK(pal1[i].r == pal0[i].r);
        CHECK(pal1[i].g == pal0[i].g);
        CHECK(pal1[i].b == pal0[i].b);
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetStockPalette
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetStockPalette") {

TEST_CASE("each enum returns a non-empty vector of 32 entries") {
    for (auto p : {StockPaletteAmigaDiner, StockPaletteAmigaGasStation, StockPaletteAmigaDriving}) {
        CHECK(BitmapParser::GetStockPalette(p).size() == 32);
    }
}

TEST_CASE("Diner and GasStation palettes differ") {
    auto d = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    auto g = BitmapParser::GetStockPalette(StockPaletteAmigaGasStation);
    bool any_diff = false;
    for (size_t i = 0; i < d.size(); i++) any_diff = any_diff || (d[i] != g[i]);
    CHECK(any_diff);
}

TEST_CASE("unknown enum falls back to Diner palette") {
    auto fallback = BitmapParser::GetStockPalette((StockPalette)999);
    auto diner    = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    CHECK(fallback == diner);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetAssetType
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetAssetType") {

TEST_CASE("tire assets are spritesheets") {
    BitmapParser p;
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, CAR_TIRES_BRAND)  == LibAssetSpritesheet);
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, CAR_TIRES_RACING) == LibAssetSpritesheet);
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, CAR_TIRES_STOCK)  == LibAssetSpritesheet);
}

TEST_CASE("mask assets are masks") {
    BitmapParser p;
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, GUS_GAS_MASK)    == LibAssetMask);
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, DINER_SCREEN_MASK) == LibAssetMask);
}

TEST_CASE("general assets are regular") {
    BitmapParser p;
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, GARAGE_SCREEN) == LibAssetRegular);
    CHECK(p.GetAssetType(SrLibFileAmigaLibP, DINER_SCREEN)  == LibAssetRegular);
}

TEST_CASE("non-libp files default to regular") {
    BitmapParser p;
    CHECK(p.GetAssetType(SrLibFileAmigaLibStart, 0) == LibAssetRegular);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// LoadsOpaque
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::LoadsOpaque") {

TEST_CASE("mask assets keep their black") {
    BitmapParser p;
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, GUS_GAS_MASK)      == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, DINER_SCREEN_MASK) == true);
}

// Each frame has to paint over the one before it, so its black background is
// part of the picture rather than a colour key.
TEST_CASE("engine tuning animations keep their black") {
    BitmapParser p;
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_BELT_ANIMATION)         == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_PISTON_LEFT_ANIMATION)  == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_PISTON_RIGHT_ANIMATION) == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_CRANK_ANIMATION)        == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_FLYWHEEL_ANIMATION)     == true);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, ENGINE_CAM_ANIMATION)          == true);
}

TEST_CASE("ordinary art keys its black out") {
    BitmapParser p;
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, GARAGE_SCREEN) == false);
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibP, GUS_GAS_SCREEN) == false);
}

TEST_CASE("non-libp files key their black out") {
    BitmapParser p;
    CHECK(p.LoadsOpaque(SrLibFileAmigaLibStart, 0) == false);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetSpritesheetInfo
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetSpritesheetInfo") {

TEST_CASE("tire assets: 1 row, 4 columns") {
    BitmapParser p;
    auto [rows, cols] = p.GetSpritesheetInfo(SrLibFileAmigaLibP, CAR_TIRES_BRAND);
    CHECK(rows == 1);
    CHECK(cols == 4);
}

TEST_CASE("phonebook guy: 2 rows, 5 columns") {
    BitmapParser p;
    auto [rows, cols] = p.GetSpritesheetInfo(SrLibFileAmigaLibP, PHONEBOOK_GUY);
    CHECK(rows == 2);
    CHECK(cols == 5);
}

TEST_CASE("decals: 2 rows, 4 columns") {
    BitmapParser p;
    auto [rows, cols] = p.GetSpritesheetInfo(SrLibFileAmigaLibP, DECALS);
    CHECK(rows == 2);
    CHECK(cols == 4);
}

TEST_CASE("unknown asset returns (0, 0)") {
    BitmapParser p;
    auto [rows, cols] = p.GetSpritesheetInfo(SrLibFileAmigaLibP, GARAGE_SCREEN);
    CHECK(rows == 0);
    CHECK(cols == 0);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetPalette
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetPalette") {

TEST_CASE("index < 45 returns driving low") {
    BitmapParser p;
    auto result = p.GetPalette(SrLibFileAmigaLibP, SPEECH_BUBBLE_ARROW_RIGHT); // index 0
    auto expected = BitmapParser::GetStockPalette(StockPaletteAmigaDrivingLow);
    CHECK(result == expected);
}

TEST_CASE("index 45-80 returns driving-low palette (differs from PAL1)") {
    BitmapParser p;
    auto result  = p.GetPalette(SrLibFileAmigaLibP, 50); // middle of 45-80 range
    auto pal1    = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    CHECK(result != pal1);
}

TEST_CASE("gas station assets return gas palette") {
    BitmapParser p;
    auto gas   = p.GetPalette(SrLibFileAmigaLibP, GUS_GAS_SCREEN);
    auto pal1  = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    auto gasEx = BitmapParser::GetStockPalette(StockPaletteAmigaGasStation);
    CHECK(gas == gasEx);
    CHECK(gas != pal1);
}

TEST_CASE("high index (>176) returns PAL1") {
    BitmapParser p;
    auto hi  = p.GetPalette(SrLibFileAmigaLibP, GARAGE_SCREEN); // 365, high index
    auto pal = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    CHECK(hi == pal);
}

TEST_CASE("newspaper assets (5-10) return PAL1, not driving-low") {
    // Regression test: newspaper indices sit inside the <45 driving-backdrop
    // bucket and were previously dragged onto PAL_DRIVING_LOW's olive tones
    // by that bucket, corrupting the newspaper scene's colors.
    BitmapParser p;
    auto pal1 = BitmapParser::GetStockPalette(StockPaletteAmigaDiner);
    for (int i = NEWSPAPER_MENU; i <= NEWSPAPER_NO_SOLDIERS_IN_VIETNAM; i++) {
        auto result = p.GetPalette(SrLibFileAmigaLibP, i);
        CHECK(result == pal1);
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// ReadEntries (vector overload) with synthetic data
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::ReadEntries") {

TEST_CASE("reads correct entry count") {
    BitmapEntry_t e = {};
    e.width = 320; e.height = 200; e.depth = 5;
    auto data = MakeAmigaLibHeader({e});
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    CHECK(count == 1);
    CHECK(entries.size() == 1);
}

TEST_CASE("parses width, height, depth correctly") {
    BitmapEntry_t e = {};
    e.width = 160; e.height = 100; e.depth = 3;
    auto data = MakeAmigaLibHeader({e});
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    CHECK(entries[0].width  == 160);
    CHECK(entries[0].height == 100);
    CHECK(entries[0].depth  == 3);
}

TEST_CASE("parses offset and sizePacked correctly") {
    BitmapEntry_t e = {};
    e.width = 8; e.height = 8; e.depth = 1;
    e.offset     = 0x00001234;
    e.sizePacked = 0x0056;
    auto data = MakeAmigaLibHeader({e});
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    CHECK(entries[0].offset     == 0x00001234u);
    CHECK(entries[0].sizePacked == 0x0056);
}

TEST_CASE("parses rleDictionary bytes") {
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;
    for (int i = 0; i < 16; i++) e.rleDictionary[i] = (uint8_t)(i + 1);
    auto data = MakeAmigaLibHeader({e});
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    for (int i = 0; i < 16; i++) {
        CHECK(entries[0].rleDictionary[i] == (uint8_t)(i + 1));
    }
}

TEST_CASE("zero entries is valid") {
    std::vector<uint8_t> data = {0x00, 0x00}; // count = 0
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    CHECK(count == 0);
    CHECK(entries.empty());
}

TEST_CASE("multiple entries are all parsed") {
    BitmapEntry_t e1 = {}, e2 = {};
    e1.width = 64;  e1.height = 32; e1.depth = 4;
    e2.width = 128; e2.height = 64; e2.depth = 5;
    auto data = MakeAmigaLibHeader({e1, e2});
    BitmapParser p;
    auto [count, entries] = p.ReadEntries(data);
    REQUIRE(count == 2);
    CHECK(entries[0].width == 64);
    CHECK(entries[1].width == 128);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetPackedData (vector overload)
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::GetPackedData") {

TEST_CASE("extracts correct bytes from data buffer") {
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;
    e.offset = 0;
    e.sizePacked = 3;
    int totalEntries = 1;

    // header = 2 + 1*sizeof(BitmapEntry_t) bytes, then the packed payload
    auto header = MakeAmigaLibHeader({e});
    header.push_back(0xAA);
    header.push_back(0xBB);
    header.push_back(0xCC);

    BitmapParser p;
    auto packed = p.GetPackedData(header, e, totalEntries);
    REQUIRE(packed.size() == 3);
    CHECK(packed[0] == 0xAA);
    CHECK(packed[1] == 0xBB);
    CHECK(packed[2] == 0xCC);
}

TEST_CASE("throws when offset + size exceeds data") {
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;
    e.offset = 0;
    e.sizePacked = 100;
    auto header = MakeAmigaLibHeader({e});
    // Only 2 extra bytes, but sizePacked=100
    header.push_back(0x00);
    header.push_back(0x01);

    BitmapParser p;
    CHECK_THROWS_AS(p.GetPackedData(header, e, 1), std::runtime_error);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// UnpackData
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::UnpackData") {

// Helper: make a minimal BitmapEntry_t for unpacking
static BitmapEntry_t MakeEntry(int width, int height, int depth, uint8_t dict0 = 0) {
    BitmapEntry_t e = {};
    e.width  = (uint16_t)width;
    e.height = (uint16_t)height;
    e.depth  = (uint8_t)depth;
    e.rleDictionary[0] = dict0;
    return e;
}

const std::vector<std::pair<int, uint8_t>> RLE_TABLE = RLE;

TEST_CASE("literal bytes pass through unchanged") {
    // 8px wide, 1 row, 1 plane → expects 1 byte output
    auto e = MakeEntry(8, 1, 1);
    std::vector<uint8_t> data = {0x42};
    BitmapParser p;
    auto result = p.UnpackData(RLE_TABLE, e, data);
    REQUIRE(result.size() == 1);
    CHECK(result[0] == 0x42);
}

TEST_CASE("0x00 run-length escape expands correctly") {
    // 16px, 1 row, 1 plane → 2 bytes output
    // [0x00, 0x01] → repeat 0x00 for (1+1)=2 times
    auto e = MakeEntry(16, 1, 1);
    std::vector<uint8_t> data = {0x00, 0x01};
    BitmapParser p;
    auto result = p.UnpackData(RLE_TABLE, e, data);
    REQUIRE(result.size() == 2);
    CHECK(result[0] == 0x00);
    CHECK(result[1] == 0x00);
}

TEST_CASE("0xFF run-length escape expands correctly") {
    // 24px, 1 row, 1 plane → 3 bytes output
    // [0xFF, 0x02] → repeat 0xFF for (2+1)=3 times
    auto e = MakeEntry(24, 1, 1);
    std::vector<uint8_t> data = {0xFF, 0x02};
    BitmapParser p;
    auto result = p.UnpackData(RLE_TABLE, e, data);
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 0xFF);
    CHECK(result[1] == 0xFF);
    CHECK(result[2] == 0xFF);
}

TEST_CASE("dictionary match emits rle count/value") {
    // rle[0] = {1, 0x00}: dict byte → 1 copy of 0x00
    // Set rleDictionary[0] to 0xAA → 0xAA becomes 1 copy of 0x00
    auto e = MakeEntry(8, 1, 1, /*dict0=*/0xAA);
    std::vector<uint8_t> data = {0xAA};
    BitmapParser p;
    auto result = p.UnpackData(RLE_TABLE, e, data);
    REQUIRE(result.size() == 1);
    CHECK(result[0] == 0x00); // rle[0].second
}

TEST_CASE("two literal bytes for 16-pixel wide bitmap") {
    // width=16, height=1, depth=1 → expects 2 bytes of planar data
    auto e = MakeEntry(16, 1, 1);
    std::vector<uint8_t> data = {0x42, 0x55};
    BitmapParser p;
    auto result = p.UnpackData(RLE_TABLE, e, data);
    REQUIRE(result.size() == 2);
    CHECK(result[0] == 0x42);
    CHECK(result[1] == 0x55);
}

TEST_CASE("empty rle table returns empty output") {
    auto e = MakeEntry(8, 1, 1);
    std::vector<uint8_t> data = {0x42};
    std::vector<std::pair<int, uint8_t>> empty_rle; // size != 16
    BitmapParser p;
    auto result = p.UnpackData(empty_rle, e, data);
    CHECK(result.empty());
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// PlanarToChunky (Amiga variant)
// ---------------------------------------------------------------------------

TEST_SUITE("BitmapParser::PlanarToChunky (Amiga)") {

TEST_CASE("single plane all-ones: all pixels get palette index 1") {
    BitmapParser p;
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;

    std::vector<uint8_t> planar = {0xFF}; // all 8 bits set → index 1 for every pixel

    Palette pal = {
        {0, 0, 0, 255},       // index 0: black
        {255, 0, 0, 255},     // index 1: red
    };

    auto pixels = p.PlanarToChunky(planar, e, pal);
    REQUIRE(pixels.size() == 8);
    for (auto& px : pixels) {
        CHECK(px.r == 255);
        CHECK(px.g == 0);
        CHECK(px.b == 0);
    }
}

TEST_CASE("single plane all-zeros: all pixels get palette index 0") {
    BitmapParser p;
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;

    std::vector<uint8_t> planar = {0x00};

    Palette pal = {
        {10, 20, 30, 255},  // index 0
        {255, 0, 0, 255},   // index 1
    };

    auto pixels = p.PlanarToChunky(planar, e, pal);
    REQUIRE(pixels.size() == 8);
    for (auto& px : pixels) {
        CHECK(px.r == 10);
        CHECK(px.g == 20);
        CHECK(px.b == 30);
    }
}

TEST_CASE("two planes combine bits correctly") {
    // depth=2, width=8, height=1 → planeSize=1
    // plane 0 = 0xFF (all bits set → bit 0 = 1 for all pixels)
    // plane 1 = 0xFF (all bits set → bit 1 = 1 for all pixels)
    // → every pixel has value = 0b11 = 3
    BitmapParser p;
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 2;

    std::vector<uint8_t> planar = {0xFF, 0xFF};

    Palette pal = {
        {0,0,0,255}, {255,0,0,255}, {0,255,0,255}, {0,0,255,255}
    };

    auto pixels = p.PlanarToChunky(planar, e, pal);
    REQUIRE(pixels.size() == 8);
    for (auto& px : pixels) {
        CHECK(px.b == 255); // palette[3] = blue
    }
}

TEST_CASE("MSB of plane byte becomes leftmost pixel") {
    BitmapParser p;
    BitmapEntry_t e = {};
    e.width = 8; e.height = 1; e.depth = 1;

    std::vector<uint8_t> planar = {0x80}; // only bit 7 set → pixel 0 = index 1

    Palette pal = {
        {0,0,0,255},
        {255,0,0,255},
    };

    auto pixels = p.PlanarToChunky(planar, e, pal);
    REQUIRE(pixels.size() == 8);
    CHECK(pixels[0].r == 255); // pixel 0 = index 1 = red
    for (int i = 1; i < 8; i++) {
        CHECK(pixels[i].r == 0); // pixels 1-7 = index 0 = black
    }
}

} // TEST_SUITE
