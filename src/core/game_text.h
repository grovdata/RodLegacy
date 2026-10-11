/*
 * RodLegacy - The game's own lines, read from the player's Street_Rod executable instead of shipped with the port.
 *
 * Created: 2026-10-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace GameText {

enum Message : int {
    CANT_AFFORD_SHORT = 1,
    ITS_YOURS = 2,
    NO_DRINKING = 3,
    ALL_PARTS_GONE = 4,
    NO_SPARE_PARTS = 5,
    BUYER_OFFER = 6,
    BUYER_FOR_NOTHING = 7,
    ALL_CARS_GONE = 8,
    NO_CAR = 9,
    HAGGLE_KIDDING = 10,
    HAGGLE_NO_WAY = 11,
    HAGGLE_GET_LOST = 12,
    HAGGLE_NO_THANKS = 13,
    HAGGLE_COUNTER = 14,
    HAGGLE_CRAZY_TAKE_IT = 15,
    HAGGLE_TAKE_IT = 16,
    HERE_IT_IS = 17,
    GARAGE_FULL_CARS = 18,
    GARAGE_FULL_PART = 19,
    GARAGE_FULL_PARTS = 20,
    OUT_OF_DOUGH = 21,
    STICKERS_NOT_FREE = 22,
    NO_ROOM_FOR_PART = 23,
    NO_SPARE_TRANSMISSION = 24,
    PART_WONT_FIT = 25,
    PAINT_NOT_FREE = 26,
    CANT_TUNE = 27,
    NO_SPARE_ENGINE = 28,
    NO_SPARE_MANIFOLD = 29,
    NO_SPARE_CARB = 30,
    NO_SPARE_TIRES = 31,
    CANT_CHOP_ROOF = 32,
    BUMPER_GONE = 33,
    CUSTOMIZATION_NOT_FREE = 34,
    SELL_CARS_FIRST = 35,
    SELL_PARTS_FIRST = 36,
    NOT_ENOUGH_GAS = 37,
    TANK_FULL = 38,
    GAS_NOT_FREE = 39,
    CAR_NOT_RUNNING = 40,
    PICK_A_CAR = 41,
    WHATS_HIS_NAME = 42,
    INSULT_1 = 43,
    INSULT_2 = 44,
    INSULT_3 = 45,
    INSULT_4 = 46,
    INSULT_5 = 47,
    ACCEPT_1 = 48,
    ACCEPT_2 = 49,
    ACCEPT_3 = 50,
    DRAG_REFUSE_1 = 51,
    DRAG_REFUSE_2 = 52,
    DRAG_REFUSE_3 = 53,
    DRAG_REFUSE_4 = 54,
    DRAG_ACCEPT_1 = 55,
    DRAG_ACCEPT_2 = 56,
    DRAG_ACCEPT_3 = 57,
    KING_AMATEUR = 58,
    KING_WIN_MORE = 59,
    KING_MORE_DRAG = 60,
    KING_MORE_ROAD = 61,
    KING_DRAG_ON = 62,
    KING_PINK_SLIP_ON = 63,
    KING_NEED_GAS = 64,
    BET_TOO_BIG = 65,
    CHICKEN = 66,
    HERO = 67,
    PUMP_FULL_SERVICE = 68,
    PUMP_NOT_FULL = 69,
    PUMP_PAY_ATTENTION = 70,
    PUMP_GET_OUT = 71,
    WON_NO_BET = 72,
    WON_10 = 73,
    WON_25 = 74,
    WON_50 = 75,
    WON_100 = 76,
    WON_CAR = 77,
    BEAT_KING_DRAG = 78,
    BEAT_KING_ROAD = 79,
    JUMPED_LIGHT = 80,
    JAILED_FOR_DEBTS = 81,
    LOST_PINK_SLIP = 82,
    LOST = 83,
    LOST_TO_KING_DRAG = 84,
    LOST_TO_KING_ROAD = 85,
    TRANS_DROPPED = 86,
    ENGINE_BLOWN = 87,
    TRANS_DROPPED_PINK = 88,
    ENGINE_BLOWN_PINK = 89,
    WRECK_PINK = 90,
    TIRES_BLOWN_PINK = 91,
    TOTALED = 92,
    SLOW_CRASH = 93,
    SLOW_CRASH_TIRES = 94,
    OUT_OF_TIME = 95,
};

// Strings outside the message table, by their file offset in the Amiga Street_Rod.
enum Loose : uint32_t {
    WINNING_TIME    = 0x2cad6,  // printf: seconds, tenths, mph
    CONGRATULATIONS = 0x2cb0a,
    COVERED_ONLY    = 0x2cb1c,  // printf: seconds, tenths, percent
    OF_DISTANCE     = 0x2cb40,  // printf: mph
    FINED           = 0x2cb6e,  // printf: dollars
    PLUS_TIRES      = 0x2cb98,
    COULD_BE_WORSE  = 0x7fbc,
    REPAIR_PARTS    = 0x29f50,  // "$123" placeholder: the cost, over 60 mph
    REPAIR_COST     = 0x29f88,  // "$123" placeholder: the cost
};

// Message table at file 0x29054 (A4-0x7414), 96 records of {box, line 1, line 2}; see docs/speech_bubbles.md.
void Load(const std::vector<uint8_t>& exe);

// The message's lines joined by '\n'; empty until Load.
std::string Msg(Message id);
// Replaces the first run of digits, the original's placeholder, with number.
std::string Msg(Message id, int number);
std::string Get(Loose at);
std::string Get(Loose at, int number);

template <typename... Args>
std::string Format(Loose at, Args... args) {
    const std::string f = Get(at);
    if (f.empty()) return {};
    char buf[160];
    snprintf(buf, sizeof(buf), f.c_str(), args...);
    return buf;
}

std::string ReplaceNumber(const std::string& s, int number);

}
