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
    put32(table + 20 + 2, 0x10);
    put32(table + 20 + 6, 0x30);
    GameText::Load(exe);
    CHECK(GameText::Msg(GameText::Message(0)).empty());
    CHECK(GameText::Msg(GameText::Message(1)) == "One $12345 more.");
    CHECK(GameText::Msg(GameText::Message(2)) == "One $12345 more.\nTwo");
    CHECK(GameText::Msg(GameText::Message(1), 7) == "One $7 more.");
    CHECK(GameText::Msg(GameText::Message(200)).empty());
}

TEST_CASE("a short executable loads nothing") {
    GameText::Load(std::vector<uint8_t>(16, 0));
    CHECK(GameText::Msg(GameText::Message(1)).empty());
}

TEST_CASE("ReplaceNumber leaves digitless text alone") {
    CHECK(GameText::ReplaceNumber("No digits", 5) == "No digits");
}

}
