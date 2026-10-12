#include <doctest/doctest.h>
#include "core/game_text.h"

#include <cstring>

TEST_SUITE("GameText") {

TEST_CASE("message table records join their two lines") {
    std::vector<uint8_t> exe(0x2a000, 0);
    const size_t table = 0x29054, hunk = 0x28468;
    auto put32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) exe[at + i] = uint8_t(v >> (24 - 8 * i));
    };
    std::memcpy(&exe[hunk + 0x10], "One $12345 more.", 17);
    std::memcpy(&exe[hunk + 0x30], "Two", 4);
    put32(table + 10 + 2, 0x10);
    exe[table + 21] = 2;
    put32(table + 20 + 2, 0x10);
    put32(table + 20 + 6, 0x30);
    GameText::Load(exe, false);
    CHECK(GameText::Msg(GameText::NO_MESSAGE).empty());
    CHECK(GameText::Msg(GameText::CANT_AFFORD_SHORT) == "One $12345 more.");
    CHECK(GameText::Msg(GameText::ITS_YOURS) == "One $12345 more.\nTwo");
    CHECK(GameText::Msg(GameText::CANT_AFFORD_SHORT, 7) == "One $7 more.");
    CHECK(GameText::Msg(GameText::MESSAGE_COUNT).empty());
}

TEST_CASE("a short executable loads nothing") {
    GameText::Load(std::vector<uint8_t>(16, 0), false);
    CHECK(GameText::Msg(GameText::CANT_AFFORD_SHORT).empty());
}

TEST_CASE("ReplaceNumber leaves digitless text alone") {
    CHECK(GameText::ReplaceNumber("No digits", 5) == "No digits");
}

TEST_CASE("the key table is in enum order") {
    for (int i = 0; i < GameText::MESSAGE_COUNT; ++i) CHECK(GameText::KeyId(i) == i);
}

TEST_CASE("Street Rod 2 numbers its table differently") {
    CHECK(GameText::TableIndex(GameText::NO_CAR, true) == 9);
    CHECK(GameText::TableIndex(GameText::OUT_OF_DOUGH, true) == 21);
    CHECK(GameText::TableIndex(GameText::NO_ROOM_FOR_PART, true) == 22);
    CHECK(GameText::TableIndex(GameText::CAR_NOT_RUNNING, true) == 39);
    CHECK(GameText::TableIndex(GameText::KING_WIN_MORE, true) == 58);
    CHECK(GameText::TableIndex(GameText::KING_DRAG_ON, true) == 62);
    CHECK(GameText::TableIndex(GameText::KING_PINK_SLIP_ON, true) == 64);
    CHECK(GameText::TableIndex(GameText::CHICKEN, true) == 67);
    CHECK(GameText::TableIndex(GameText::PUMP_PAY_ATTENTION, true) == 71);
    CHECK(GameText::TableIndex(GameText::WON_NO_BET, true) == 73);
    CHECK(GameText::TableIndex(GameText::WON_50, true) == 76);
    CHECK(GameText::TableIndex(GameText::WON_CAR, true) == 78);
    CHECK(GameText::TableIndex(GameText::OUT_OF_TIME, true) == 96);
    CHECK(GameText::TableIndex(GameText::WON_150, true) == 77);
    CHECK(GameText::TableIndex(GameText::NOT_WEDNESDAY, true) == 3);
    CHECK(GameText::TableIndex(GameText::NO_DRINKING, true) == 0);
    CHECK(GameText::TableIndex(GameText::WON_100, true) == 0);
    CHECK(GameText::TableIndex(GameText::WON_150, false) == 0);
}

}
