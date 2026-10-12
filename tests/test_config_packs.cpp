#include <doctest/doctest.h>
#include "scenes/config_screen.h"

// The setup screen lists a pack per game and lets either one start the game.

static const ZipPack* FindPack(const char* filename) {
    for (const ZipPack& p : GetZipPacks()) {
        if (p.filename == filename) return &p;
    }
    return nullptr;
}

TEST_CASE("each game has a mandatory pack naming its own disks") {
    for (SrGame g : {SrGame::SR1, SrGame::SR2}) {
        const SrGameFiles& gf = SrGameGetFiles(g);
        const ZipPack* p = FindPack(gf.zipName);
        REQUIRE(p != nullptr);
        CHECK(p->mandatory);
        REQUIRE(p->adfContents.size() == 2);
        CHECK(p->adfContents[0] == SrLibArchiveNames[gf.disk1]);
        CHECK(p->adfContents[1] == SrLibArchiveNames[gf.disk2]);
    }
}

TEST_CASE("the data disk is optional and brings no ADFs of its own") {
    const ZipPack* p = FindPack("srsepc.zip");
    REQUIRE(p != nullptr);
    CHECK_FALSE(p->mandatory);
    CHECK(p->adfContents.empty());
}

TEST_CASE("one mandatory pack is enough to start") {
    std::vector<ZipPackStatus> status;
    for (const ZipPack& p : GetZipPacks()) status.push_back({p, false, false});

    CHECK_FALSE(AnyMandatoryValid(status));

    // Street Rod 2 alone starts, the same as Street Rod alone.
    for (ZipPackStatus& s : status) {
        s.zipFound = s.zipValid = (s.pack.filename == SrGameGetFiles(SrGame::SR2).zipName);
    }
    CHECK(AnyMandatoryValid(status));

    for (ZipPackStatus& s : status) {
        s.zipFound = s.zipValid = (s.pack.filename == SrGameGetFiles(SrGame::SR1).zipName);
    }
    CHECK(AnyMandatoryValid(status));

    // The data disk on its own is not a game.
    for (ZipPackStatus& s : status) {
        s.zipFound = s.zipValid = (s.pack.filename == "srsepc.zip");
    }
    CHECK_FALSE(AnyMandatoryValid(status));
}
