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
    NO_MESSAGE,
    CANT_AFFORD_SHORT,
    ITS_YOURS,
    NO_DRINKING,
    ALL_PARTS_GONE,
    NO_SPARE_PARTS,
    BUYER_OFFER,
    BUYER_FOR_NOTHING,
    ALL_CARS_GONE,
    NO_CAR,
    HAGGLE_KIDDING,
    HAGGLE_NO_WAY,
    HAGGLE_GET_LOST,
    HAGGLE_NO_THANKS,
    HAGGLE_COUNTER,
    HAGGLE_CRAZY_TAKE_IT,
    HAGGLE_TAKE_IT,
    HERE_IT_IS,
    GARAGE_FULL_CARS,
    GARAGE_FULL_PART,
    GARAGE_FULL_PARTS,
    OUT_OF_DOUGH,
    STICKERS_NOT_FREE,
    NO_ROOM_FOR_PART,
    NO_SPARE_TRANSMISSION,
    PART_WONT_FIT,
    PAINT_NOT_FREE,
    CANT_TUNE,
    NO_SPARE_ENGINE,
    NO_SPARE_MANIFOLD,
    NO_SPARE_CARB,
    NO_SPARE_TIRES,
    CANT_CHOP_ROOF,
    BUMPER_GONE,
    CUSTOMIZATION_NOT_FREE,
    SELL_CARS_FIRST,
    SELL_PARTS_FIRST,
    NOT_ENOUGH_GAS,
    TANK_FULL,
    GAS_NOT_FREE,
    CAR_NOT_RUNNING,
    PICK_A_CAR,
    WHATS_HIS_NAME,
    INSULT_1,
    INSULT_2,
    INSULT_3,
    INSULT_4,
    INSULT_5,
    ACCEPT_1,
    ACCEPT_2,
    ACCEPT_3,
    DRAG_REFUSE_1,
    DRAG_REFUSE_2,
    DRAG_REFUSE_3,
    DRAG_REFUSE_4,
    DRAG_ACCEPT_1,
    DRAG_ACCEPT_2,
    DRAG_ACCEPT_3,
    KING_AMATEUR,
    KING_WIN_MORE,
    KING_MORE_DRAG,
    KING_MORE_ROAD,
    KING_DRAG_ON,
    KING_PINK_SLIP_ON,
    KING_NEED_GAS,
    BET_TOO_BIG,
    CHICKEN,
    HERO,
    PUMP_FULL_SERVICE,
    PUMP_NOT_FULL,
    PUMP_PAY_ATTENTION,
    PUMP_GET_OUT,
    WON_NO_BET,
    WON_10,
    WON_25,
    WON_50,
    WON_100,
    WON_CAR,
    BEAT_KING_DRAG,
    BEAT_KING_ROAD,
    JUMPED_LIGHT,
    JAILED_FOR_DEBTS,
    LOST_PINK_SLIP,
    LOST,
    LOST_TO_KING_DRAG,
    LOST_TO_KING_ROAD,
    TRANS_DROPPED,
    ENGINE_BLOWN,
    TRANS_DROPPED_PINK,
    ENGINE_BLOWN_PINK,
    WRECK_PINK,
    TIRES_BLOWN_PINK,
    TOTALED,
    SLOW_CRASH,
    SLOW_CRASH_TIRES,
    OUT_OF_TIME,
    NOT_WEDNESDAY,
    KING_NEED_GRUDGE,
    KING_MORE_MULHOLLAND,
    KING_MORE_AQUEDUCT,
    KING_MULHOLLAND_ON,
    CHEATED,
    WON_150,
    NEED_BETTER_GRAPHICS,
    USE_ANOTHER_PUMP,
    GET_GAS_FIRST,
    TAKE_A_PUMP,
    GRUDGE_GET_BREAKOUT,
    GRUDGE_BREAKOUT,
    GRUDGE_WON_ROUND_ADVANCE,
    GRUDGE_WON_ROUND,
    GRUDGE_LOST_BY,
    GRUDGE_LOST_BY_OUT,
    GRUDGE_PLACED,
    CROSSED_CENTER,
    CROSSED_CENTER_OUT,
    BROKE_OUT,
    BROKE_OUT_OUT,
    GRUDGE_STAGE,
    BREAKOUT_TOO_WEAK,
    NO_GRUDGE_STAKE,
    ENGINE_FIRST,
    NO_SPARE_EXHAUST,
    NO_SPARE_MUFFLER,
    NO_SPARE_DIFF,
    SOLD,
    BUMPED_STARTER,
    OPPONENT_BROKE_OUT,
    MESSAGE_COUNT
};

// Strings outside the message table.
enum Loose {
    WINNING_TIME,     // printf: seconds, tenths, mph
    CONGRATULATIONS,
    COVERED_ONLY,     // printf: seconds, tenths, percent
    OF_DISTANCE,      // printf: mph
    FINED,            // printf: dollars
    PLUS_TIRES,
    COULD_BE_WORSE,
    REPAIR_PARTS,     // "$123" placeholder: the cost, over 60 mph
    REPAIR_COST,      // "$123" placeholder: the cost
    LOOSE_COUNT
};

// The Amiga executable's message table, {box, line 1, line 2} records; see docs/speech_bubbles.md.
void Load(const std::vector<uint8_t>& exe, bool sr2);
// The game's own table number for id, 0 when that game has no such line.
int TableIndex(Message id, bool sr2);
// Exposed for the test that keeps the key table in enum order.
Message KeyId(int i);

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
