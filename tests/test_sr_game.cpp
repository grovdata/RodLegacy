#include <doctest/doctest.h>
#include "audio/adlib_engine.h"
#include "parsers/libp.h"
#include "car/car_set.h"
#include "car/part.h"
#include <cstring>
#include <vector>
#include <string>

// The game selection is what lets one asset path serve both builds.

TEST_CASE("SR1 selects the Street Rod disks") {
    const SrGameFiles& g = SrGameGetFiles(SrGame::SR1);
    CHECK(g.disk1 == SrLibArchiveSR1AmigaDisk1);
    CHECK(g.disk2 == SrLibArchiveSR1AmigaDisk2);
    CHECK(std::strcmp(g.zipName, "sr1amiga.zip") == 0);
    CHECK(std::strcmp(g.dataFile, "SR_Data") == 0);
}

TEST_CASE("SR2 selects the Street Rod 2 disks") {
    const SrGameFiles& g = SrGameGetFiles(SrGame::SR2);
    CHECK(g.disk1 == SrLibArchiveSR2AmigaDisk1);
    CHECK(g.disk2 == SrLibArchiveSR2AmigaDisk2);
    CHECK(std::strcmp(g.zipName, "sr2amiga.zip") == 0);
    // SR2's disk 2 carries SR2_data where SR1 carries SR_Data; every other
    // filename is shared, which is why only this one is in the table.
    CHECK(std::strcmp(g.dataFile, "SR2_data") == 0);
}

TEST_CASE("the two games never share a disk or a data file") {
    const SrGameFiles& a = SrGameGetFiles(SrGame::SR1);
    const SrGameFiles& b = SrGameGetFiles(SrGame::SR2);
    CHECK(a.disk1 != b.disk1);
    CHECK(a.disk2 != b.disk2);
    CHECK(std::strcmp(a.zipName, b.zipName) != 0);
    CHECK(std::strcmp(a.dataFile, b.dataFile) != 0);
}

// A minimal SR2_data: 22 driver records, then 22 car specs, then the text.
// Only the fields the parser reads are filled in.
static std::vector<uint8_t> MakeSr2Data(int carIndex, int price, int brandMask,
                                        int cfg, int body, int carClass) {
    const size_t driverBlock = SR2_DATA_DRIVER_ENTRY_COUNT * 10;
    const size_t specBlock = SR2_DATA_CARSPEC_ENTRY_COUNT * SR2_DATA_CARSPEC_RECORD_SIZE;
    std::vector<uint8_t> d(SR2_DATA_TEXT_BLOB_OFFSET + 64, 0);

    auto be16 = [&](size_t at, int v) {
        d[at] = static_cast<uint8_t>((v >> 8) & 0xFF);
        d[at + 1] = static_cast<uint8_t>(v & 0xFF);
    };
    auto be32 = [&](size_t at, uint32_t v) {
        d[at] = static_cast<uint8_t>(v >> 24);  d[at + 1] = static_cast<uint8_t>(v >> 16);
        d[at + 2] = static_cast<uint8_t>(v >> 8); d[at + 3] = static_cast<uint8_t>(v);
    };

    // Pointers are absolute into the loaded image; pick any base. Car 0's ad
    // pointer is what the parser uses to recover it.
    const uint32_t base = 0x00214CB4;
    for (int i = 0; i < SR2_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        const size_t at = driverBlock + static_cast<size_t>(i) * SR2_DATA_CARSPEC_RECORD_SIZE;
        be32(at + 10, base + SR2_DATA_TEXT_BLOB_OFFSET);
    }
    const size_t rec = driverBlock + static_cast<size_t>(carIndex) * SR2_DATA_CARSPEC_RECORD_SIZE;
    be16(rec + 0, price);
    d[rec + 4] = static_cast<uint8_t>(carClass);
    be16(rec + 6, brandMask);
    be16(rec + 8, cfg);
    d[rec + 14] = 2;
    be16(rec + 16, body);

    // Driver 3's name, reached through its absolute pointer.
    be32(3 * 10 + 6, base + SR2_DATA_TEXT_BLOB_OFFSET + 8);
    const char* name = "Patty";
    memcpy(d.data() + SR2_DATA_TEXT_BLOB_OFFSET + 8, name, strlen(name));
    (void)specBlock;
    return d;
}

TEST_CASE("SR2 car specs unpack into the shared entry") {
    // The King: config 0xbb92, which is SR2's fully loaded car, and brand Ford.
    std::vector<uint8_t> d = MakeSr2Data(21, 0, 2, 0xbb92, 264, 8);
    DataParser dp;
    std::vector<SrCarSpecEntry> specs = dp.ParseCarSpecTableSR2(d);
    REQUIRE(specs.size() == SR2_DATA_CARSPEC_ENTRY_COUNT);

    const SrCarSpecEntry& king = specs[21];
    CHECK(king.Price() == 0);
    CHECK_FALSE(king.ForSale());          // never for sale, in either game
    CHECK(king.car_class == 8);
    CHECK(king.BodyAsset() == 263);       // stored one-based

    // SR2 packs these one bit lower than SR1 and keeps the brand separately;
    // the parser repacks so these accessors mean the same for both games.
    CHECK(king.EngineClass() == 2);       // big V8
    CHECK(king.CarbClass() == 2);         // racing 4-barrel
    CHECK(king.TransClass() == 3);        // racing 4-speed
    CHECK(king.TireClass() == 2);         // slicks
    CHECK(king.Brand() == 1);             // mask 2 collapses to Ford

    // Bits 12..15 are the three parts SR2 added.
    CHECK(king.exhaust == SR_EXHAUST_HEADERS);
    CHECK(king.muffler == SR_MUFFLER_GLASSPACK);
    CHECK(king.differential == SR_DIFF_4_11);
}

TEST_CASE("SR2's three extra slots decode independently") {
    DataParser dp;
    struct { int cfg; int exhaust; int muffler; int diff; } cases[] = {
        {0x0000, SR_EXHAUST_STOCK,   SR_MUFFLER_STOCK,     SR_DIFF_2_78},
        {0x1000, SR_EXHAUST_HEADERS, SR_MUFFLER_STOCK,     SR_DIFF_2_78},
        {0x2000, SR_EXHAUST_STOCK,   SR_MUFFLER_GLASSPACK, SR_DIFF_2_78},
        {0x4000, SR_EXHAUST_STOCK,   SR_MUFFLER_STOCK,     SR_DIFF_3_73},
        {0x8000, SR_EXHAUST_STOCK,   SR_MUFFLER_STOCK,     SR_DIFF_4_11},
    };
    for (const auto& c : cases) {
        std::vector<uint8_t> d = MakeSr2Data(0, 1050, 2, c.cfg, 182, 1);
        std::vector<SrCarSpecEntry> specs = dp.ParseCarSpecTableSR2(d);
        REQUIRE(!specs.empty());
        CHECK(specs[0].exhaust == c.exhaust);
        CHECK(specs[0].muffler == c.muffler);
        CHECK(specs[0].differential == c.diff);
        // The low bits have to survive the same repack.
        CHECK(specs[0].Price() == 1050);
        CHECK(specs[0].Brand() == 1);
    }
}

TEST_CASE("a Street Rod car carries the neutral setting in SR2's slots") {
    // Street Rod and SE cars have no exhaust, muffler or differential of their own.
    DataParser dp;
    std::vector<uint8_t> sr1(SR_DATA_DRIVER_ENTRY_COUNT * 10 +
                             SR_DATA_CARSPEC_ENTRY_COUNT * SR_DATA_CARSPEC_RECORD_SIZE, 0);
    std::vector<SrCarSpecEntry> specs = dp.ParseCarSpecTable(sr1);
    REQUIRE(specs.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    CHECK(specs[0].exhaust == SR_EXHAUST_STOCK);
    CHECK(specs[0].muffler == SR_MUFFLER_STOCK);
    CHECK(specs[0].differential == SR_DIFF_3_73);

    // A default-built entry means the same thing, since that is what every
    // car the port creates outside a data file starts from.
    SrCarSpecEntry fresh;
    CHECK(fresh.differential == SR_DIFF_3_73);
}

TEST_CASE("SR2 brand mask collapses the same way SR1's does") {
    DataParser dp;
    struct { int mask; int expected; } cases[] = {{1, 0}, {2, 1}, {4, 2}};
    for (const auto& c : cases) {
        std::vector<uint8_t> d = MakeSr2Data(0, 1050, c.mask, 0x0400, 182, 1);
        std::vector<SrCarSpecEntry> specs = dp.ParseCarSpecTableSR2(d);
        REQUIRE(specs.size() == SR2_DATA_CARSPEC_ENTRY_COUNT);
        CHECK(specs[0].Brand() == c.expected);
        CHECK(specs[0].Price() == 1050);
        CHECK(specs[0].TireClass() == 1);
    }
}

TEST_CASE("SR2 driver names resolve through the absolute pointers") {
    std::vector<uint8_t> d = MakeSr2Data(0, 1050, 2, 0x0400, 182, 1);
    DataParser dp;
    std::vector<std::string> names = dp.ParseDriverNamesSR2(d);
    REQUIRE(names.size() == SR2_DATA_DRIVER_ENTRY_COUNT);
    CHECK(names[3] == "Patty");
}

TEST_CASE("a short SR2_data yields nothing rather than reading past the end") {
    DataParser dp;
    std::vector<uint8_t> tiny(32, 0);
    CHECK(dp.ParseCarSpecTableSR2(tiny).empty());
    CHECK(dp.ParseDriverNamesSR2(tiny).empty());
}

// ---------------------------------------------------------------------------
// Sets. Cars and drivers share one registry, so a set's index means the same
// thing for both. A "driver set" is just a set with providesDrivers.
// ---------------------------------------------------------------------------

TEST_CASE("each game brings its own drivers, the data disk brings none") {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();

    auto find = [&](const char* id) -> const CarSetDef* {
        for (const auto& d : defs) if (d.id == id) return &d;
        return nullptr;
    };

    const CarSetDef* sr1 = find("amiga");
    const CarSetDef* srse = find("srse");
    const CarSetDef* sr2 = find("sr2amiga");
    REQUIRE(sr1);
    REQUIRE(srse);
    REQUIRE(sr2);

    // Street Rod and Street Rod 2 each ship a driver block with its own
    // portraits, so both can supply opponents.
    CHECK(sr1->providesDrivers);
    CHECK(sr2->providesDrivers);
    // The SE data disk adds cars only. Switching it on must never change who
    // you race against.
    CHECK_FALSE(srse->providesDrivers);
    CHECK(srse->providesCars);
}

TEST_CASE("a game maps to the set that reads its own files") {
    CHECK(CarSetForGame(SrGame::SR1) == kAmigaCarSet);
    CHECK(GetCarSetDefs()[CarSetForGame(SrGame::SR1)].id == "amiga");
    CHECK(GetCarSetDefs()[CarSetForGame(SrGame::SR2)].id == "sr2amiga");
    // They must be different sets, or SR2 would read Street Rod's lib.
    CHECK(CarSetForGame(SrGame::SR1) != CarSetForGame(SrGame::SR2));
}

TEST_CASE("registry order is stable and ids are unique") {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    // Index 0 must stay the Street Rod set: original .SAV files store car
    // indices in its range, and the merged table is built in registry order.
    REQUIRE(defs.size() >= 3);
    CHECK(defs[kAmigaCarSet].id == "amiga");
    // Which set cannot be switched off follows the game being played, not the
    // registry: Street Rod 2 needs its own set the way Street Rod needs this.
    CHECK(CarSetIsOwnGame(defs[kAmigaCarSet], SrGame::SR1));
    CHECK_FALSE(CarSetIsOwnGame(defs[kAmigaCarSet], SrGame::SR2));
    CHECK(CarSetIsOwnGame(defs[CarSetForGame(SrGame::SR2)], SrGame::SR2));

    for (size_t i = 0; i < defs.size(); i++) {
        for (size_t j = i + 1; j < defs.size(); j++) {
            CHECK(defs[i].id != defs[j].id);
        }
    }
}

TEST_CASE("sets with no files of their own name the game they read") {
    for (const auto& d : GetCarSetDefs()) {
        if (!d.zipName.empty()) continue;        // brings its own files
        // Anything reading a game's own lib.p must say which game, or two
        // Amiga sets would both resolve to the same assets.
        CHECK((d.game == SrGame::SR1 || d.game == SrGame::SR2));
    }
}

TEST_CASE("archive names and checksums line up with the disk indices") {
    // The names array is indexed by SrLibArchive, so a reordering of the enum
    // would silently point a game at the wrong ADF.
    CHECK(std::strcmp(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk1], "SR1AMIGA_DISK1.adf") == 0);
    CHECK(std::strcmp(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk2], "SR1AMIGA_DISK2.adf") == 0);
    CHECK(std::strcmp(SrLibArchiveNames[SrLibArchiveSR2AmigaDisk1], "SR2AMIGA_DISK1.adf") == 0);
    CHECK(std::strcmp(SrLibArchiveNames[SrLibArchiveSR2AmigaDisk2], "SR2AMIGA_DISK2.adf") == 0);

    // All four checksums differ; a duplicate would mean a copy-paste slip.
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            CHECK(SrLibArchiveChecksums[i] != SrLibArchiveChecksums[j]);
        }
    }
}

// ---------------------------------------------------------------------------
// The blocks SR2_data shares with SR_data, read at SR2's offsets. Layouts are
// in RodLegacyResearch/docs/formats/sr_data.md, "SR2_data".
// ---------------------------------------------------------------------------

namespace {

// A fuller synthetic SR2_data: sprite refs, sprite offsets, click boxes, the
// string offset table and one car's ad, on top of the spec table above.
struct Sr2Builder {
    std::vector<uint8_t> d;
    Sr2Builder() : d(SR2_DATA_TEXT_BLOB_OFFSET + 256, 0) {}
    void be16(size_t at, int v) {
        d[at] = uint8_t((v >> 8) & 0xFF);
        d[at + 1] = uint8_t(v & 0xFF);
    }
    size_t spec(int car) const {
        return SR2_DATA_DRIVER_ENTRY_COUNT * 10 +
               size_t(car) * SR2_DATA_CARSPEC_RECORD_SIZE;
    }
    size_t refs(int car) const { return SR2_DATA_CAR_SPRITES_START + size_t(car) * 10; }
};

} // namespace

TEST_CASE("SR2 sprite refs name a car's variants, one-based") {
    Sr2Builder b;
    // Car 3: body 193, chopped roof 195, scoop 127, bumpers 196 and 197 - real
    // values off the shipped file, all stored one higher than the asset.
    b.be16(b.spec(3) + 16, 193 + 1);
    b.be16(b.refs(3) + 2, 195 + 1);
    b.be16(b.refs(3) + 4, 127 + 1);
    b.be16(b.refs(3) + 6, 196 + 1);
    b.be16(b.refs(3) + 8, 197 + 1);
    // Car 4 has no chopped roof and no stripped front bumper: 0 is "no such
    // variant", not asset 0.
    b.be16(b.spec(4) + 16, 221 + 1);

    DataParser dp;
    std::vector<CarSpriteRefs> refs = dp.ParseCarSpritesAmiga(b.d, SrGame::SR2);
    REQUIRE(refs.size() == SR2_DATA_CARSPEC_ENTRY_COUNT);

    CHECK(refs[3].body == 193);
    CHECK(refs[3].choppedRoof == 195);
    CHECK(refs[3].airScoop == 127);
    CHECK(refs[3].rearBumperStripped == 196);
    CHECK(refs[3].frontBumperStripped == 197);

    CHECK(refs[4].body == 221);
    CHECK(refs[4].choppedRoof == -1);
    CHECK(refs[4].frontBumperStripped == -1);
}

TEST_CASE("an SR2 car's dashboard is the second byte of its sprite ref record") {
    Sr2Builder b;
    b.d[b.refs(5)] = 2;       // first byte: not the dashboard
    b.d[b.refs(5) + 1] = 2;   // 1969 Shelby era: the third dashboard
    b.d[b.refs(0) + 1] = 1;

    DataParser dp;
    std::vector<CarSpriteRefs> refs = dp.ParseCarSpritesAmiga(b.d, SrGame::SR2);
    REQUIRE(refs.size() == SR2_DATA_CARSPEC_ENTRY_COUNT);
    CHECK(refs[5].dashboard == 2);
    CHECK(refs[5].bodyStyle == 2);
    CHECK(refs[1].bodyStyle == 0);
    CHECK(refs[0].dashboard == 1);
    CHECK(refs[1].dashboard == 0);
}

TEST_CASE("the Amiga sprite ref reader is the same for both games") {
    // Street Rod's block is at its own offset with 26 cars; getting the game
    // wrong would silently read the wrong block.
    DataParser dp;
    std::vector<uint8_t> sr1(0x854, 0);
    CHECK(dp.ParseCarSpritesAmiga(sr1, SrGame::SR1).size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    CHECK(dp.ParseCarSpritesAmiga(sr1, SrGame::SR2).size() == SR2_DATA_CARSPEC_ENTRY_COUNT);
    // Too short for either: nothing rather than a read past the end.
    std::vector<uint8_t> tiny(64, 0);
    CHECK(dp.ParseCarSpritesAmiga(tiny, SrGame::SR1).empty());
    CHECK(dp.ParseCarSpritesAmiga(tiny, SrGame::SR2).empty());
}

TEST_CASE("SR2 car metadata comes out of the blocks at SR2's offsets") {
    Sr2Builder b;
    const int car = 2;
    b.be16(b.spec(car) + 0, 975);          // priced, so it carries an ad
    b.d[b.spec(car) + 14] = 2;             // two ad lines after the name
    b.be16(b.refs(car) + 4, 128 + 1);      // hood scoop

    uint8_t* off = b.d.data() + SR2_DATA_OFFSETS_START + car * 12;
    for (int i = 0; i < 12; i++) off[i] = uint8_t(10 + i);

    uint8_t* box = b.d.data() + SR2_DATA_CLICKBOX_START + car * 20;
    for (int i = 0; i < 20; i++) box[i] = uint8_t(40 + i);

    const int adAt = 32;
    b.be16(SR2_DATA_STRING_TABLE_START + car * 2, adAt);
    const char* ad = "1966 Ford Galaxie.\0+ V8, Good work\0car, very reliable $975";
    memcpy(b.d.data() + SR2_DATA_TEXT_BLOB_OFFSET + adAt, ad, 58);

    DataParser dp;
    int count = 0;
    CarMetadata_t* md = dp.SrLibDataReadDataFromBufferSR2(b.d.data(), b.d.size(), &count);
    REQUIRE(md != nullptr);
    CHECK(count == SR2_DATA_CARSPEC_ENTRY_COUNT);

    CHECK(md[car].offsets.offsetXRearTire == 10);
    CHECK(md[car].offsets.offsetYDecal == 21);
    // SR2_data has no gas station block and no stickers: the car draw routine takes the gas cap position from the
    // sprite offset record's two decal bytes, and its click box sits there.
    CHECK(md[car].offsets.offsetXGasCap == md[car].offsets.offsetXDecal);
    CHECK(md[car].offsets.offsetYGasCap == 21);
    CHECK(md[car].clickBoxes.clickBoxGasCap.x == md[car].offsets.offsetXDecal);
    CHECK(md[car].clickBoxes.clickBoxGasCap.y == 21);

    CHECK(md[car].airScoop == 128);
    CHECK(md[car].clickBoxes.clickBoxRearBumper.x == 40);
    CHECK(md[car].clickBoxes.clickBoxFrontBumper.height == 59);

    REQUIRE(md[car].classified.carName != nullptr);
    CHECK(std::string(md[car].classified.carName) == "1966 Ford Galaxie.");
    CHECK(md[car].classified.year == 1966);
    CHECK(md[car].classified.price == 975);

    // The King's slot is never advertised. Its line count points into the
    // driver names that follow the ads, so reading it would print a name.
    CHECK(md[21].classified.carName == nullptr);
    free(md);
}

TEST_CASE("SR2 palettes are read from the executable, or refused") {
    // Nine 32-word tables at a known offset. Anything with a bit set above
    // 0x0FFF is not an Amiga palette, so none of it is taken.
    const size_t size = SR2_EXE_PALETTE_OFFSET + SR2_PALETTE_COUNT * SR2_PALETTE_SIZE * 2;
    std::vector<uint8_t> exe(size, 0);
    for (size_t i = SR2_EXE_PALETTE_OFFSET; i + 1 < size; i += 2) {
        exe[i] = 0x0F;
        exe[i + 1] = 0xFF;
    }
    std::vector<std::vector<uint16_t>> pals = SrLibReadSr2Palettes(exe);
    REQUIRE(pals.size() == SR2_PALETTE_COUNT);
    CHECK(pals[0].size() == SR2_PALETTE_SIZE);
    CHECK(pals[0][0] == 0x0FFF);

    exe[SR2_EXE_PALETTE_OFFSET] = 0x10;
    CHECK(SrLibReadSr2Palettes(exe).empty());

    std::vector<uint8_t> truncated(SR2_EXE_PALETTE_OFFSET, 0);
    CHECK(SrLibReadSr2Palettes(truncated).empty());
}

TEST_CASE("The King's car spec is the last one in whichever game is played") {
    // Both tables end on him, but they are not the same length, so a fixed 25
    // would read a buyable car in Street Rod 2.
    CHECK(SrKingCarSpecIndex(SrGame::SR1) == SR_DATA_KING_CARSPEC_INDEX);
    CHECK(SrKingCarSpecIndex(SrGame::SR1) == SR_DATA_CARSPEC_ENTRY_COUNT - 1);
    CHECK(SrKingCarSpecIndex(SrGame::SR2) == SR2_DATA_CARSPEC_ENTRY_COUNT - 1);
    CHECK(SrKingCarSpecIndex(SrGame::SR1) != SrKingCarSpecIndex(SrGame::SR2));
    // His driver record is index 21 in both, since both ship 22 drivers.
    CHECK(SR_DATA_DRIVER_ENTRY_COUNT == SR2_DATA_DRIVER_ENTRY_COUNT);
    CHECK(SR_DATA_KING_DRIVER_INDEX == SR_DATA_DRIVER_ENTRY_COUNT - 1);
}

TEST_CASE("car sets mix forward only") {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    auto find = [&](const char* id) -> const CarSetDef& {
        for (const auto& d : defs) if (d.id == id) return d;
        FAIL("no such set: ", id);
        return defs[0];
    };

    // Street Rod 2's cars carry exhaust manifold, muffler and differential
    // slots Street Rod has no model for, so they must never be offered there.
    CHECK_FALSE(CarSetFitsGame(find("sr2amiga"), SrGame::SR1));
    CHECK(CarSetFitsGame(find("sr2amiga"), SrGame::SR2));

    // Street Rod and SE cars predate those slots, so they drop into either.
    CHECK(CarSetFitsGame(find("amiga"), SrGame::SR1));
    CHECK(CarSetFitsGame(find("amiga"), SrGame::SR2));
    CHECK(CarSetFitsGame(find("srse"), SrGame::SR1));
    CHECK(CarSetFitsGame(find("srse"), SrGame::SR2));

    // The rule is a compare on SrGame, so the enum must stay in release order.
    CHECK(static_cast<int>(SrGame::SR1) < static_cast<int>(SrGame::SR2));

    // A game's own set always fits the game it reads.
    for (const auto& d : defs) {
        if (!d.zipName.empty()) continue;
        CHECK(CarSetFitsGame(d, d.game));
    }
}

// ---------------------------------------------------------------------------
// Tables Street Rod 2 keeps in its executable.
// ---------------------------------------------------------------------------

namespace {

// A stand-in executable. The mech tables sit past the parts catalogue, so it
// is sized on the highest address either reader touches, not on the catalogue.
struct Sr2Exe {
    std::vector<uint8_t> d;
    Sr2Exe() : d(Sr2ExeOffset(0x00264000), 0) {}

    void W16(size_t at, int v) {
        REQUIRE(at + 2 <= d.size());
        d[at] = uint8_t((v >> 8) & 0xFF);
        d[at + 1] = uint8_t(v & 0xFF);
    }
    void W32(size_t at, uint32_t v) {
        REQUIRE(at + 4 <= d.size());
        d[at] = uint8_t(v >> 24); d[at + 1] = uint8_t(v >> 16);
        d[at + 2] = uint8_t(v >> 8); d[at + 3] = uint8_t(v);
    }
    void Row(uint32_t addr, std::initializer_list<int> vals) {
        size_t at = Sr2ExeOffset(addr);
        for (int v : vals) { W16(at, v); at += 2; }
    }
    // The catalogue's documented group sizes, in group order.
    void FillCatalog() {
        const int perGroup[Sr2PartGroupCount] = {9, 12, 3, 26, 3, 6, 2, 3};
        int rec = 0;
        for (int g = 0; g < Sr2PartGroupCount; g++) {
            for (int n = 0; n < perGroup[g]; n++, rec++) {
                const size_t at = Sr2ExeOffset(SR2_PARTS_CATALOG_ADDR) + size_t(rec) * 16;
                W16(at + 0, 5 + rec);       // price, never zero
                W16(at + 2, 0);
                W16(at + 4, 0x100 + rec);
                W16(at + 6, 7);
                W16(at + 8, g);
                W32(at + 10, 0);            // ad text at the hunk base
                d[at + 14] = 1;
            }
        }
    }
    void FillMechTables() {
        Row(0x263AF8, {500, 600, 300, 100, 70});
        Row(0x263B02, {600, 700, 350, 120, 90});
        Row(0x263B0C, {600, 760, 380, 140, 90});
        Row(0x263AD0, {0, 25, 50, 100, 0});
        Row(0x263ADA, {0, 25, 50, 100, 0});
        Row(0x263AE4, {0, 25, 50, 75, 100});
        Row(0x263AEE, {0, 30, 50, 80, 100});
        Row(0x263B16, {2900, 4500, 2900, 5000, 2500, 5200});
        Row(0x263B22, {0, 0, 80, 0, 0, 0, 0, 90, 100});
        Row(0x263B34, {0, 40, 0, 70, 60, 50, 60, 90, 100});
        Row(0x263B46, {0, 0, 0, 40, 80, 70, 85, 90, 100});
        Row(0x263B58, {0, 50, 100});
        Row(0x263B5E, {0, 33, 66, 100});
        Row(0x263B66, {4500, 5000, 5200});
        Row(0x263B6C, {3, 3, 4, 4});
        Row(0x263B74, {9, 8, 5});
        Row(0x263B7A, {46, 50, 55, 68, 73, 78, 91, 96, 100});
    }
};

} // namespace

TEST_CASE("the SR2 parts catalogue reads as 64 records in eight groups") {
    Sr2Exe exe;
    exe.FillCatalog();
    const char* ad = "Glass Pack Muffler. Fits all.";
    memcpy(exe.d.data() + SR2_EXE_PART_TEXT_BASE, ad, strlen(ad));

    std::vector<Sr2PartEntry> parts = SrLibReadSr2Parts(exe.d);
    REQUIRE(parts.size() == SR2_PARTS_CATALOG_COUNT);

    int perGroup[Sr2PartGroupCount] = {};
    for (const auto& p : parts) perGroup[p.group]++;
    // Street Rod's five slots, then the three Street Rod 2 adds.
    CHECK(perGroup[Sr2PartGroupEngine] == 9);
    CHECK(perGroup[Sr2PartGroupTransmission] == 12);
    CHECK(perGroup[Sr2PartGroupCarburettor] == 3);
    CHECK(perGroup[Sr2PartGroupIntakeManifold] == 26);
    CHECK(perGroup[Sr2PartGroupTires] == 3);
    CHECK(perGroup[Sr2PartGroupExhaustManifold] == 6);
    CHECK(perGroup[Sr2PartGroupMuffler] == 2);
    CHECK(perGroup[Sr2PartGroupDifferential] == 3);

    CHECK(parts[0].price == 5);
    CHECK(parts[0].fitMask == 7);
    CHECK(parts[0].ad == ad);
}

TEST_CASE("a file that does not hold the catalogue yields nothing") {
    // Every record names a slot and costs something, so one that does not is
    // read as "this is not the catalogue" rather than as a free part.
    Sr2Exe exe;
    exe.FillCatalog();
    exe.W16(Sr2ExeOffset(SR2_PARTS_CATALOG_ADDR) + 3 * 16 + 0, 0);   // price
    CHECK(SrLibReadSr2Parts(exe.d).empty());

    Sr2Exe badGroup;
    badGroup.FillCatalog();
    badGroup.W16(Sr2ExeOffset(SR2_PARTS_CATALOG_ADDR) + 8, Sr2PartGroupCount);
    CHECK(SrLibReadSr2Parts(badGroup.d).empty());

    std::vector<uint8_t> truncated(Sr2ExeOffset(SR2_PARTS_CATALOG_ADDR), 0);
    CHECK(SrLibReadSr2Parts(truncated).empty());
}

TEST_CASE("the SR2 mech tables read at the documented addresses") {
    Sr2Exe exe;
    exe.FillMechTables();

    Sr2MechTables t = SrLibReadSr2MechTables(exe.d);
    REQUIRE(t.valid);

    // Acceleration comes off rpmGain, top speed off the score tables; a row
    // read at the wrong stride would still look plausible, so pin the corners.
    CHECK(t.rpmGain[0][0] == 500);
    CHECK(t.rpmGain[2][4] == 90);
    CHECK(t.gearRatio[3][1] == 30);      // racing 4-speed is the odd row
    CHECK(t.gearRatio[0][4] == 0);       // an automatic has no fifth step
    CHECK(t.powerBand[2][0] == 2500);
    CHECK(t.redline[2] == 5200);
    CHECK(t.gearCount[0] == 3);
    CHECK(t.tireGrip[2] == 5);           // slicks grip hardest, lowest number
    CHECK(t.tierScore[8] == 100);        // The King's tier
    CHECK(t.engineScore[2] == 100);
    CHECK(t.transScore[3] == 100);
    CHECK(t.manifoldScore[1][1] == 40);  // only the 4-barrel row scores here
    CHECK(t.manifoldScore[0][1] == 0);
}

TEST_CASE("mech tables that do not hold together are refused") {
    // Each engine's redline is the top of its power band.
    Sr2Exe exe;
    exe.FillMechTables();
    exe.Row(0x263B66, {4500, 5000, 1234});
    CHECK_FALSE(SrLibReadSr2MechTables(exe.d).valid);

    Sr2Exe unsorted;
    unsorted.FillMechTables();
    unsorted.Row(0x263B7A, {46, 50, 55, 68, 73, 78, 91, 96, 10});
    CHECK_FALSE(SrLibReadSr2MechTables(unsorted.d).valid);

    std::vector<uint8_t> truncated(1024, 0);
    CHECK_FALSE(SrLibReadSr2MechTables(truncated).valid);
}

TEST_CASE("SR2's three parts sell at a flat 70% of catalogue price") {
    // lmain.c FUN_00257166 switches on the catalogue group and its jump table sends groups 5, 6 and 7 to one
    // arm: value * 7 / 10, with no wear term and no haggle roll.
    const int roll = 0;  // unused by these categories
    for (PartCategory c : {PartCategory::ExhaustManifold,
                           PartCategory::Muffler,
                           PartCategory::Differential}) {
        CHECK(PartSalePrice(c, 150, 0, roll) == 105);       // 4.11:1 differential
        CHECK(PartSalePrice(c, 60, 0, roll) == 40);         // GM tube headers, 42 -> 40
        CHECK(PartSalePrice(c, 15, 0, roll) == 10);         // glass pack, 10.5 -> 10

        // Wear does not enter, unlike engines and transmissions.
        CHECK(PartSalePrice(c, 150, WEAR_MAX, roll) == PartSalePrice(c, 150, 0, roll));
        // Nor does the roll, unlike carburettors and manifolds.
        CHECK(PartSalePrice(c, 150, 0, 19) == PartSalePrice(c, 150, 0, 0));
    }

    // Street Rod 2 left the other four alone, so the categories it shares with
    // Street Rod must still price the old way.
    CHECK(PartSalePrice(PartCategory::Tire, 150, 0, roll) == 0);
    CHECK(PartSalePrice(PartCategory::Engine, 500, 0, roll) == 450);        // 500*100*9/1000
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 0, 0) == 70);        // 100*(70+0)/100
}

TEST_CASE("the SR2 message table reads hunk relative string pointers") {
    // A synthetic executable: the table where the real one sits.
    std::vector<uint8_t> exe(SR2_MESSAGE_TABLE_OFFSET +
                             SR2_MESSAGE_COUNT * SR2_MESSAGE_RECORD_SIZE + 0x200, 0);

    auto putU16 = [&](size_t at, uint16_t v) {
        exe[at] = static_cast<uint8_t>(v >> 8);
        exe[at + 1] = static_cast<uint8_t>(v & 0xff);
    };
    auto putU32 = [&](size_t at, uint32_t v) {
        exe[at] = static_cast<uint8_t>(v >> 24);
        exe[at + 1] = static_cast<uint8_t>((v >> 16) & 0xff);
        exe[at + 2] = static_cast<uint8_t>((v >> 8) & 0xff);
        exe[at + 3] = static_cast<uint8_t>(v & 0xff);
    };
    // Two strings laid down past the table, and their hunk relative offsets.
    const size_t tableEnd = SR2_MESSAGE_TABLE_OFFSET +
                            SR2_MESSAGE_COUNT * SR2_MESSAGE_RECORD_SIZE;
    const char* lineA = "You crossed the center line.";
    const char* lineB = "Out of the running.";
    const size_t atA = tableEnd + 8;
    const size_t atB = atA + std::strlen(lineA) + 1;
    std::memcpy(exe.data() + atA, lineA, std::strlen(lineA));
    std::memcpy(exe.data() + atB, lineB, std::strlen(lineB));

    const size_t rec = SR2_MESSAGE_TABLE_OFFSET +
                       SR2_MSG_GRUDGE_CROSSED_LINE * SR2_MESSAGE_RECORD_SIZE;
    putU16(rec, SR2_MESSAGE_TWO_LINE_STYLE);
    putU32(rec + 2, static_cast<uint32_t>(atA - SR2_DATA67_FILE_OFFSET));
    putU32(rec + 6, static_cast<uint32_t>(atB - SR2_DATA67_FILE_OFFSET));

    // A one line style carries a second pointer anyway, and it has to be left
    // alone; a record with no string at all is the null pointer.
    const size_t recOne = SR2_MESSAGE_TABLE_OFFSET +
                          SR2_MSG_GRUDGE_LOST * SR2_MESSAGE_RECORD_SIZE;
    putU16(recOne, 1);
    putU32(recOne + 2, static_cast<uint32_t>(atB - SR2_DATA67_FILE_OFFSET));
    putU32(recOne + 6, static_cast<uint32_t>(atA - SR2_DATA67_FILE_OFFSET));

    auto msgs = SrLibReadSr2Messages(exe);
    REQUIRE(msgs.size() == static_cast<size_t>(SR2_MESSAGE_COUNT));
    REQUIRE(msgs[SR2_MSG_GRUDGE_CROSSED_LINE].size() == 2);
    CHECK(msgs[SR2_MSG_GRUDGE_CROSSED_LINE][0] == lineA);
    CHECK(msgs[SR2_MSG_GRUDGE_CROSSED_LINE][1] == lineB);
    REQUIRE(msgs[SR2_MSG_GRUDGE_LOST].size() == 1);
    CHECK(msgs[SR2_MSG_GRUDGE_LOST][0] == lineB);
    CHECK(msgs[SR2_MSG_GRUDGE_BROKE_OUT].empty());

    // A short file gives nothing rather than reading off the end.
    CHECK(SrLibReadSr2Messages(std::vector<uint8_t>(64, 0)).empty());

    // A style the box table does not have reads as one line, not as a count.
    putU16(rec, 99);
    msgs = SrLibReadSr2Messages(exe);
    CHECK(msgs[SR2_MSG_GRUDGE_CROSSED_LINE].size() == 1);
}

TEST_CASE("the AdLib engine's pitch follows the revs the way the capture does") {
    // Measured off captures/sr_001.dro: voice 3 sits near 23 Hz at idle and climbs to about 73 flat out, key
    // held down the whole way.
    CHECK(AdlibEngine::HzForRpm(AdlibEngine::kRpmMin) == doctest::Approx(23.0));
    CHECK(AdlibEngine::HzForRpm(AdlibEngine::kRpmMax) == doctest::Approx(41.0));
    // Below a tone at both ends, which is what makes it a growl. If the top end
    // creeps up it starts to whine instead.
    CHECK(AdlibEngine::kHzAtRedline < 50.0);
    // Clamped, not extrapolated - a stalled or over-revving engine must not
    // wander off the end of the fnum range.
    CHECK(AdlibEngine::HzForRpm(0) == doctest::Approx(23.0));
    CHECK(AdlibEngine::HzForRpm(99000) == doctest::Approx(41.0));
    // Monotonic across the range.
    double prev = 0.0;
    for (int rpm = AdlibEngine::kRpmMin; rpm <= AdlibEngine::kRpmMax; rpm += 100) {
        const double hz = AdlibEngine::HzForRpm(rpm);
        CHECK(hz >= prev);
        prev = hz;
    }

    // The OPL encoding: fnum * 49716 / 2^20, shifted by the block. Round trip
    // every rev step and it has to come back within a fraction of a Hz.
    for (int rpm = AdlibEngine::kRpmMin; rpm <= AdlibEngine::kRpmMax; rpm += 50) {
        const double hz = AdlibEngine::HzForRpm(rpm);
        const auto f = AdlibEngine::FnumForHz(hz);
        CHECK(f.fnum <= 1023);
        CHECK(f.block <= 7);
        const double back = static_cast<double>(f.fnum) * 49716.0 *
                            static_cast<double>(1u << f.block) / 1048576.0;
        CHECK(back == doctest::Approx(hz).epsilon(0.02));
    }

    // The capture's idle fnum was 486 in block 0; the same frequency has to
    // encode the same way.
    const auto idle = AdlibEngine::FnumForHz(AdlibEngine::kHzAtIdle);
    CHECK(idle.block == 0);
    CHECK(idle.fnum == 485);   // 23.0 Hz exactly; the capture's 486 is 23.05

    // The top of the range encodes back to the same frequency.
    const auto top = AdlibEngine::FnumForHz(AdlibEngine::kHzAtRedline);
    CHECK(top.fnum <= 1023);
    const double back = static_cast<double>(top.fnum) * 49716.0 *
                        static_cast<double>(1u << top.block) / 1048576.0;
    CHECK(back == doctest::Approx(AdlibEngine::kHzAtRedline).epsilon(0.02));
}

TEST_CASE("the AdLib engine's loudness follows the volume, and the muffler") {
    // The original hands SetVoiceLevel a 0..127 volume, not a register value.
    CHECK(AdlibEngine::LevelForVolume(AdlibEngine::kVolumeMuffledIdle) == 0x17);
    CHECK(AdlibEngine::LevelForVolume(AdlibEngine::kVolumeMax) == 0x00);
    // Clamped both ways, and never outside the register's six bits.
    CHECK(AdlibEngine::LevelForVolume(-50) == 0x3f);
    CHECK(AdlibEngine::LevelForVolume(9999) == 0x00);
    for (int v = 0; v <= 127; ++v) {
        CHECK(AdlibEngine::LevelForVolume(v) <= 0x3f);
    }

    // Muffled: quiet at idle, up to full by the redline. 0 is loudest in OPL
    // terms, so the level counts down.
    CHECK(AdlibEngine::LevelForRpm(AdlibEngine::kRpmMin, false) == 0x17);
    CHECK(AdlibEngine::LevelForRpm(AdlibEngine::kRpmMax, false) == 0x00);
    int prev = 0xff;
    for (int rpm = AdlibEngine::kRpmMin; rpm <= AdlibEngine::kRpmMax; rpm += 100) {
        const int lvl = AdlibEngine::LevelForRpm(rpm, false);
        CHECK(lvl <= prev);          // monotonically louder
        prev = lvl;
    }

    // Muffler off: full volume from idle.
    for (int rpm = AdlibEngine::kRpmMin; rpm <= AdlibEngine::kRpmMax; rpm += 100) {
        CHECK(AdlibEngine::VolumeForRpm(rpm, true) == AdlibEngine::kVolumeMax);
        CHECK(AdlibEngine::LevelForRpm(rpm, true) == 0x00);
    }
    // And never quieter than the muffled car at the same revs.
    for (int rpm = AdlibEngine::kRpmMin; rpm <= AdlibEngine::kRpmMax; rpm += 100) {
        CHECK(AdlibEngine::LevelForRpm(rpm, true) <= AdlibEngine::LevelForRpm(rpm, false));
    }
}

