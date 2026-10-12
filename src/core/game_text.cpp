#include "core/game_text.h"

#include <cctype>

namespace GameText {

namespace {
struct Layout {
    size_t table;
    size_t dataHunk;   // the hunk the table's pointers are relative to
    int count;
    uint32_t loose[LOOSE_COUNT];
};
const Layout kSr1 = {0x29054, 0x28468, 96,
                     {0x2cad6, 0x2cb0a, 0x2cb1c, 0x2cb40, 0x2cb6e, 0x2cb98, 0x7fbc, 0x29f50, 0x29f88}};
const Layout kSr2 = {0x3b4fa, 0x3a3a4, 122,
                     {0x410e2, 0x41116, 0x41128, 0x4114c, 0x411d4, 0x411fe, 0x9dc1, 0x3cde8, 0x3ce20}};

// Each message's number in Street Rod's table and in Street Rod 2's, 0 where that game lacks it.
struct Key {
    Message id;
    int sr1;
    int sr2;
};
const Key kKeys[MESSAGE_COUNT] = {
    {NO_MESSAGE,               0,     0},
    {CANT_AFFORD_SHORT,        1,     1},
    {ITS_YOURS,                2,     2},
    {NO_DRINKING,              3,     0},
    {ALL_PARTS_GONE,           4,     4},
    {NO_SPARE_PARTS,           5,     5},
    {BUYER_OFFER,              6,     6},
    {BUYER_FOR_NOTHING,        7,     7},
    {ALL_CARS_GONE,            8,     8},
    {NO_CAR,                   9,     9},
    {HAGGLE_KIDDING,           10,   10},
    {HAGGLE_NO_WAY,            11,   11},
    {HAGGLE_GET_LOST,          12,   12},
    {HAGGLE_NO_THANKS,         13,   13},
    {HAGGLE_COUNTER,           14,   14},
    {HAGGLE_CRAZY_TAKE_IT,     15,   15},
    {HAGGLE_TAKE_IT,           16,   16},
    {HERE_IT_IS,               17,   17},
    {GARAGE_FULL_CARS,         18,   18},
    {GARAGE_FULL_PART,         19,   19},
    {GARAGE_FULL_PARTS,        20,   20},
    {OUT_OF_DOUGH,             21,   21},
    {STICKERS_NOT_FREE,        22,    0},
    {NO_ROOM_FOR_PART,         23,   22},
    {NO_SPARE_TRANSMISSION,    24,   23},
    {PART_WONT_FIT,            25,   24},
    {PAINT_NOT_FREE,           26,   25},
    {CANT_TUNE,                27,   26},
    {NO_SPARE_ENGINE,          28,   27},
    {NO_SPARE_MANIFOLD,        29,   28},
    {NO_SPARE_CARB,            30,   29},
    {NO_SPARE_TIRES,           31,   30},
    {CANT_CHOP_ROOF,           32,   31},
    {BUMPER_GONE,              33,   32},
    {CUSTOMIZATION_NOT_FREE,   34,   33},
    {SELL_CARS_FIRST,          35,   34},
    {SELL_PARTS_FIRST,         36,   35},
    {NOT_ENOUGH_GAS,           37,   36},
    {TANK_FULL,                38,   37},
    {GAS_NOT_FREE,             39,   38},
    {CAR_NOT_RUNNING,          40,   39},
    {PICK_A_CAR,               41,   40},
    {WHATS_HIS_NAME,           42,   41},
    {INSULT_1,                 43,   42},
    {INSULT_2,                 44,   43},
    {INSULT_3,                 45,   44},
    {INSULT_4,                 46,   45},
    {INSULT_5,                 47,   46},
    {ACCEPT_1,                 48,   47},
    {ACCEPT_2,                 49,   48},
    {ACCEPT_3,                 50,   49},
    {DRAG_REFUSE_1,            51,   50},
    {DRAG_REFUSE_2,            52,   51},
    {DRAG_REFUSE_3,            53,   52},
    {DRAG_REFUSE_4,            54,   53},
    {DRAG_ACCEPT_1,            55,   54},
    {DRAG_ACCEPT_2,            56,   55},
    {DRAG_ACCEPT_3,            57,   56},
    {KING_AMATEUR,             58,   57},
    {KING_WIN_MORE,            59,   58},
    {KING_MORE_DRAG,           60,    0},
    {KING_MORE_ROAD,           61,    0},
    {KING_DRAG_ON,             62,   62},
    {KING_PINK_SLIP_ON,        63,   64},
    {KING_NEED_GAS,            64,   65},
    {BET_TOO_BIG,              65,   66},
    {CHICKEN,                  66,   67},
    {HERO,                     67,   68},
    {PUMP_FULL_SERVICE,        68,   69},
    {PUMP_NOT_FULL,            69,   70},
    {PUMP_PAY_ATTENTION,       70,   71},
    {PUMP_GET_OUT,             71,    0},
    {WON_NO_BET,               72,   73},
    {WON_10,                   73,   74},
    {WON_25,                   74,   75},
    {WON_50,                   75,   76},
    {WON_100,                  76,    0},
    {WON_CAR,                  77,   78},
    {BEAT_KING_DRAG,           78,   79},
    {BEAT_KING_ROAD,           79,   80},
    {JUMPED_LIGHT,             80,   81},
    {JAILED_FOR_DEBTS,         81,   82},
    {LOST_PINK_SLIP,           82,   83},
    {LOST,                     83,   84},
    {LOST_TO_KING_DRAG,        84,   85},
    {LOST_TO_KING_ROAD,        85,   86},
    {TRANS_DROPPED,            86,   87},
    {ENGINE_BLOWN,             87,   88},
    {TRANS_DROPPED_PINK,       88,   89},
    {ENGINE_BLOWN_PINK,        89,   90},
    {WRECK_PINK,               90,   91},
    {TIRES_BLOWN_PINK,         91,   92},
    {TOTALED,                  92,   93},
    {SLOW_CRASH,               93,   94},
    {SLOW_CRASH_TIRES,         94,   95},
    {OUT_OF_TIME,              95,   96},
    {NOT_WEDNESDAY,            0,     3},
    {KING_NEED_GRUDGE,         0,    59},
    {KING_MORE_MULHOLLAND,     0,    60},
    {KING_MORE_AQUEDUCT,       0,    61},
    {KING_MULHOLLAND_ON,       0,    63},
    {CHEATED,                  0,    72},
    {WON_150,                  0,    77},
    {NEED_BETTER_GRAPHICS,     0,    97},
    {USE_ANOTHER_PUMP,         0,    98},
    {GET_GAS_FIRST,            0,    99},
    {TAKE_A_PUMP,              0,   100},
    {GRUDGE_GET_BREAKOUT,      0,   101},
    {GRUDGE_BREAKOUT,          0,   102},
    {GRUDGE_WON_ROUND_ADVANCE, 0,   103},
    {GRUDGE_WON_ROUND,         0,   104},
    {GRUDGE_LOST_BY,           0,   105},
    {GRUDGE_LOST_BY_OUT,       0,   106},
    {GRUDGE_PLACED,            0,   107},
    {CROSSED_CENTER,           0,   108},
    {CROSSED_CENTER_OUT,       0,   109},
    {BROKE_OUT,                0,   110},
    {BROKE_OUT_OUT,            0,   111},
    {GRUDGE_STAGE,             0,   112},
    {BREAKOUT_TOO_WEAK,        0,   113},
    {NO_GRUDGE_STAKE,          0,   114},
    {ENGINE_FIRST,             0,   115},
    {NO_SPARE_EXHAUST,         0,   116},
    {NO_SPARE_MUFFLER,         0,   117},
    {NO_SPARE_DIFF,            0,   118},
    {SOLD,                     0,   119},
    {BUMPED_STARTER,           0,   120},
    {OPPONENT_BROKE_OUT,       0,   121},
};

bool isSr2 = false;
std::vector<std::string> messages;
std::vector<std::string> loose(LOOSE_COUNT);

std::string CString(const std::vector<uint8_t>& d, size_t at) {
    if (at >= d.size()) return {};
    size_t end = at;
    while (end < d.size() && d[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(d.data() + at), end - at);
}

uint32_t BeU32(const std::vector<uint8_t>& d, size_t at) {
    return (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) | (uint32_t(d[at + 2]) << 8) | d[at + 3];
}
}

void Load(const std::vector<uint8_t>& exe, bool sr2) {
    const Layout& lay = sr2 ? kSr2 : kSr1;
    isSr2 = sr2;
    messages.assign(size_t(lay.count), {});
    loose.assign(LOOSE_COUNT, {});
    if (exe.size() < lay.table + size_t(lay.count) * 10) return;
    for (int i = 0; i < LOOSE_COUNT; ++i) loose[size_t(i)] = CString(exe, lay.loose[i]);
    for (int i = 0; i < lay.count; ++i) {
        const size_t rec = lay.table + size_t(i) * 10;
        const int lines = (exe[rec] << 8 | exe[rec + 1]) == 2 ? 2 : 1;
        std::string text;
        for (int l = 0; l < lines; ++l) {
            const uint32_t ptr = BeU32(exe, rec + 2 + size_t(l) * 4);
            if (ptr == 0) break;
            if (l) text += '\n';
            text += CString(exe, lay.dataHunk + ptr);
        }
        messages[size_t(i)] = text;
    }
}

int TableIndex(Message id, bool sr2) {
    if (id < 0 || id >= MESSAGE_COUNT) return 0;
    return sr2 ? kKeys[id].sr2 : kKeys[id].sr1;
}

Message KeyId(int i) { return kKeys[i].id; }

std::string Msg(Message id) {
    const int i = TableIndex(id, isSr2);
    return i > 0 && i < int(messages.size()) ? messages[size_t(i)] : std::string();
}

std::string ReplaceNumber(const std::string& s, int number) {
    size_t a = 0;
    while (a < s.size() && !std::isdigit(static_cast<unsigned char>(s[a]))) ++a;
    if (a == s.size()) return s;
    size_t b = a;
    while (b < s.size() && std::isdigit(static_cast<unsigned char>(s[b]))) ++b;
    return s.substr(0, a) + std::to_string(number) + s.substr(b);
}

std::string Msg(Message id, int number) { return ReplaceNumber(Msg(id), number); }

std::string Get(Loose at) { return loose[size_t(at)]; }

std::string Get(Loose at, int number) { return ReplaceNumber(Get(at), number); }

}
