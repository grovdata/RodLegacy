#include "race/racing.h"

#include "scenes/grudge_night.h"
#include "debug/debug_ui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "car/car.h"
#include "car/car_set.h"
#include "car/car_system.h"
#include "car/engine_part.h"
#include "core/gamepad.h"
#include "core/global.h"
#include "core/log.h"
#include "car/part.h"
#include "race/track_gen.h"
#include "core/state.h"
#include "race/racing_internal.h"

using namespace racing_internal;

namespace {

// The rpm the drive cutscene shifts at and then holds in top gear.
constexpr int kCruiseRpm = 4500;
// How far ahead the arriving car turns in. One segment shows no turn, twelve drifts across the road; tune here.
constexpr int kCruiseArrivalSegs = 3;
// Sim ticks a one-shot sample takes to play, rounded up.
int SampleTicks(uint32_t length, uint32_t rate) {
    return static_cast<int>((length * static_cast<uint32_t>(srrace::SimHz()) + rate - 1) / rate);
}

// One needle table shared by speedo and tacho: 82 entries of {x0, y0, x1, y1} signed bytes, inner end first,
// relative to the dial origin. The needle is drawn from these lines, not swept with trig.
constexpr size_t kNeedleTableOffset = 0x12430;
constexpr size_t kNeedleTableEntries = 82;

std::vector<NeedleLine> LoadNeedleTable(const byte_vector& exe) {
    const size_t needed = kNeedleTableOffset + kNeedleTableEntries * 4;
    if (exe.size() < needed) {
        LOG_ERROR("LoadNeedleTable: Street_Rod exe too small (%zu < %zu)", exe.size(), needed);
        return {};
    }
    std::vector<NeedleLine> table(kNeedleTableEntries);
    for (size_t i = 0; i < kNeedleTableEntries; ++i) {
        const uint8_t* e = exe.data() + kNeedleTableOffset + i * 4;
        table[i] = {static_cast<int8_t>(e[0]), static_cast<int8_t>(e[1]),
                    static_cast<int8_t>(e[2]), static_cast<int8_t>(e[3])};
    }
    // Guards against the wrong build: entry 0 and 81 are mirror stops, and no offset exceeds 24px.
    const NeedleLine& lo = table.front();
    const NeedleLine& hi = table.back();
    if (lo.x0 != -2 || lo.y0 != 4 || lo.x1 != -10 || lo.y1 != 13 ||
        hi.x0 != 3 || hi.y0 != 4 || hi.x1 != 11 || hi.y1 != 13) {
        LOG_ERROR("LoadNeedleTable: unexpected needle data at 0x%zx - gauges disabled",
                  kNeedleTableOffset);
        return {};
    }
    return table;
}

constexpr size_t kSpriteScaleTableOffset = 0x28024;
constexpr size_t kSpriteScaleRecordSize = 0x36;

// One roadside_sprite_table record: a lib.p load handle, two zero pointers, then eleven {srcX, srcY, w, h} sub-rects.
struct RoadsideTable {
    int slots = 0;
    std::array<int, kRoadsideSlots> assetId{};
    std::array<std::array<SpriteRect, 11>, kRoadsideSlots> frames{};
};

RoadsideTable LoadRoadsideTable(const byte_vector& exe, size_t offset, int slots) {
    RoadsideTable table;
    slots = std::min(slots, kRoadsideSlots);
    const size_t needed = offset + static_cast<size_t>(slots) * kSpriteScaleRecordSize;
    if (exe.size() < needed) {
        LOG_ERROR("LoadRoadsideTable: executable too small (%zu < %zu)", exe.size(), needed);
        return table;
    }
    for (int slot = 0; slot < slots; ++slot) {
        const uint8_t* rec = exe.data() + offset + slot * kSpriteScaleRecordSize;
        table.assetId[slot] = ((rec[0] << 8) | rec[1]) - 1;
        for (int r = 0; r < kSpriteScaleFrameCount; ++r) {
            const uint8_t* frame = rec + 10 + static_cast<size_t>(r) * 4;
            table.frames[slot][r] = {frame[0], frame[1], frame[2], frame[3]};
        }
    }
    table.slots = slots;
    return table;
}

// Street Rod 2 replaced the road race with Mulholland, on a shorter road and in a different window.
srrace::RaceKind ToRaceKind(SrTrackType trackType, SrGame game) {
    if (trackType == SR_TRACK_DRAG_RACE) return srrace::RaceKind::Drag;
    return game == SrGame::SR2 ? srrace::RaceKind::Mulholland : srrace::RaceKind::Road;
}

}

Racing::Racing() : rng(static_cast<uint32_t>(SDL_GetTicks())) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    const Uint32 rmask = 0xff000000;
    const Uint32 gmask = 0x00ff0000;
    const Uint32 bmask = 0x0000ff00;
    const Uint32 amask = 0x000000ff;
#else
    const Uint32 rmask = 0x000000ff;
    const Uint32 gmask = 0x0000ff00;
    const Uint32 bmask = 0x00ff0000;
    const Uint32 amask = 0xff000000;
#endif
    pixels = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_RACING_HEIGHT, 32, rmask, gmask, bmask, amask);
    hud = new HUD(StockPaletteAmigaDriving, 4, 24);
    hud->SetVisible(true);
    LoadSpriteBitmaps();
    ResetForNewRace();
}

Racing::Racing(AudioManager* audioMgr) : Racing() {
    audioManager = audioMgr;
}

Racing::~Racing() {
    if (audioManager) {
        audioManager->StopLoop(0);
    }
    if (hud) {
        delete hud;
        hud = nullptr;
    }
    if (pixels) {
        SDL_FreeSurface(pixels);
        pixels = nullptr;
    }
}

// A Street Rod 2 race started from a Street Rod session needs that game's own roadside table and art.
void Racing::LoadRoadsideForGame(SrGame game) {
    if (roadsideLoadedFor == game && roadsideSlotCount > 0) {
        return;
    }
    const bool sr2 = game == SrGame::SR2;
    const RoadsideTable table = LoadRoadsideTable(
        state.GetAssets().GetSrExeData(game),
        sr2 ? srrace::HAZARD_SHAPE_TABLE_OFFSET : kSpriteScaleTableOffset,
        sr2 ? srrace::ROADSIDE_RECORDS_SR2 : srrace::ROADSIDE_RECORDS_SR1);
    if (table.slots == 0) {
        return;
    }
    roadsideFrames = table.frames;
    roadsideAssetId = table.assetId;
    roadsideSlotCount = table.slots;
    roadsideSprites = {};
    roadsideAnim = {};
    for (int slot = 0; slot < roadsideSlotCount; ++slot) {
        const int assetId = roadsideAssetId[slot];
        if (assetId < 0) {
            continue;
        }
        Bitmap* asset = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(assetId), game});
        if (asset) {
            roadsideSprites[slot] = std::make_unique<Bitmap>(*asset);
        }
        // Street Rod's hand-authored frames are preferred; Street Rod 2 only has the executable's table.
        roadsideAnim[slot] = sr2 ? nullptr : GetSpriteAnimationInfo(static_cast<uint16_t>(assetId));
    }
    roadsideLoadedFor = game;
}

void Racing::LoadSpriteBitmaps() {
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        if (Car* car = state.GetCurrentCar()) {
            sr2DashboardVariant = std::clamp(car->GetModel().sprites.dashboard, 0, 2);
        }
        if (!sr2Gauges.Load(state.GetAssets().GetSrExeData())) {
            LOG_ERROR("Racing: SR2 gauge tables not found in the executable - gauges disabled");
        }
    } else {
        needleLines = LoadNeedleTable(state.GetAssets().GetSrExeData());
    }

    LoadRoadsideForGame(state.GetAssets().GetGame());

    if (state.GetAssets().IsGameLoaded(SrGame::SR2)) {
        if (!srrace::LoadHazardShapes(state.GetAssets().GetSrExeData())) {
            LOG_WARN("Hazard shape table not found in the executable, using built-in copy");
        }
        for (int kind = 0; kind < srrace::HazardKindCount; ++kind) {
            Bitmap* hz = state.GetAssets().GetBitmapAsset(
                {LibAssetRegular, SrLibFileAmigaLibP, srrace::HazardAsset(kind), SrGame::SR2});
            if (hz) {
                hazardSprites[kind] = std::make_unique<Bitmap>(*hz);
            }
        }
    }

    // The two games' lib.p hold the same backdrop and overhead-sign art under different ids.
    const SrGame game = state.GetAssets().GetGame();
    const bool sr2 = game == SrGame::SR2;
    auto libAsset = [&](int sr1Id, int sr2Id) {
        return state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(sr2 ? sr2Id : sr1Id), game});
    };
    if (auto* a = libAsset(RACING_MOUNTAINS, SR2_RACING_MOUNTAINS)) {
        mountainSprite = std::make_unique<Bitmap>(*a);
    }
    if (auto* a = libAsset(RACING_CLOUDS, SR2_RACING_CLOUDS)) {
        cloudSprite = std::make_unique<Bitmap>(*a);
    }
    if (auto* a = libAsset(RACING_HORIZON_REARVIEWMIRROR, SR2_RACING_HORIZON_REARVIEWMIRROR)) {
        rearviewMirrorSprite = std::make_unique<Bitmap>(*a);
    }
    if (auto* a = libAsset(RACING_SIGN_COUNTY_LINE, SR2_RACING_SIGN_COUNTY_LINE)) {
        signCountyLineSprite = std::make_unique<Bitmap>(*a);
    }
    if (auto* a = libAsset(RACING_SIGN_ROUTE_69, SR2_RACING_SIGN_ROUTE_69)) {
        signRoute69Sprite = std::make_unique<Bitmap>(*a);
    }
    if (auto* a = libAsset(RACING_SIGN_EXIT_48, SR2_RACING_SIGN_EXIT_48)) {
        signExit48Sprite = std::make_unique<Bitmap>(*a);
    }
    // Street Rod 2 draws its own start lights.
    auto* lightAsset = sr2 ? nullptr
                           : state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RED_LIGHTS});
    if (lightAsset) {
        startLightSprite = std::make_unique<Bitmap>(*lightAsset);
    }
}

// SR2's light art, fetched at draw time rather than in LoadSpriteBitmaps.
Bitmap* Racing::Sr2Asset(int id) const {
    return state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(id), SrGame::SR2});
}

std::unique_ptr<Bitmap> Racing::LoadOpponentSprite(uint16_t spriteId) {
    // Street Rod's art, also when an SR2 race borrows it for a car from SR1's set.
    byte_vector& libFile = state.GetAssets().GetLibPData(SrGame::SR1);
    BitmapParser bp;
    auto imageEntries = state.GetAssets().GetImageEntries(SrGame::SR1);
    BitmapEntry_t entry = imageEntries.second[spriteId];
    std::vector<uint8_t> data = bp.ReadImage(libFile, entry, spriteId, imageEntries.first);

    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, spriteId);
    // The race colour is the original's one-colour-per-paint table, not a ramp shade. Some sprites carry the magenta
    // placeholder at other palette indices too, so every occurrence is replaced.
    const uint16_t carColor = GetCarRaceColor(opponentColor);
    palData[7] = carColor;
    for (uint16_t& c : palData) {
        if (c == 0x0F0C) {
            c = carColor;
        }
    }

    Palette pal = bp.PreparePalette(palData, true);
    Palette emptyCarPalette;
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, entry, pal, emptyCarPalette, spriteId);
    auto bitmap = std::make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, entry.width, entry.height);
    bitmap->SetDimensions(entry.width, entry.height);
    return bitmap;
}

namespace {

// Street Rod 2 first block of each body style's 16 sprites (lib.p), style 0 being the King's.
constexpr int kSr2StyleBlock[5] = {SR2_RACING_OPPONENT_KING_FRONT_SIDE, SR2_RACING_OPPONENT_STYLE1_FRONT_SIDE,
                                   SR2_RACING_OPPONENT_STYLE2_FRONT_SIDE, SR2_RACING_OPPONENT_STYLE3_FRONT_SIDE,
                                   SR2_RACING_OPPONENT_STYLE4_FRONT_SIDE};
// Scale masks, SR2_data block 9 (offset 1732), one 3-byte row per tilt, tilt 3 the level one: rows and columns are
// kept where the mask bit (7 - n % 8) is set. Pieces 3..5 come from sprite 2, 6..8 from piece 5, 10..12 from sprite 9.
constexpr uint8_t kSr2Masks[5][3][srrace::AQU_OPP_TILTS * 3] = {
    {{0xdb, 0xaa, 0x49, 0xf6, 0x55, 0x94, 0xf6, 0x9a, 0x92, 0x6f, 0x66, 0x29, 0x7b, 0x55, 0x94, 0xee, 0xaa, 0x29, 0xee, 0xaa, 0x4a},
     {0xdd, 0x2b, 0x25, 0xf3, 0x4d, 0x91, 0xde, 0xb4, 0x4a, 0xbd, 0xac, 0xa2, 0xdb, 0x53, 0x4c, 0xee, 0xa6, 0x4c, 0xde, 0x55, 0x4a},
     {0x5b, 0x49, 0x42, 0x5b, 0x49, 0x12, 0x6b, 0x19, 0x24, 0x57, 0x4a, 0x44, 0x5b, 0x51, 0x42, 0x5b, 0x4a, 0x42, 0x3d, 0x1a, 0x12}},
    {{0xee, 0xaa, 0xc4, 0xdd, 0xaa, 0x94, 0xf6, 0xaa, 0x52, 0xdb, 0x6a, 0x29, 0x6f, 0x55, 0xa4, 0x77, 0x55, 0x52, 0xdd, 0x55, 0x49},
     {0xdb, 0x4b, 0x49, 0xdb, 0xaa, 0x29, 0x7d, 0x69, 0x07, 0xf6, 0x95, 0x94, 0x77, 0xb4, 0x52, 0xbd, 0xaa, 0xa8, 0xaf, 0xaa, 0x58},
     {0xb6, 0x91, 0x11, 0xab, 0x31, 0x22, 0x67, 0x2c, 0x21, 0xcd, 0x45, 0x48, 0xb6, 0x4a, 0x50, 0xad, 0x2a, 0x22, 0x7a, 0x25, 0x22}},
    {{0xbb, 0xaa, 0x94, 0xb7, 0x55, 0xa4, 0xed, 0x55, 0x52, 0xfc, 0x6c, 0x31, 0xf6, 0x55, 0x52, 0x77, 0x55, 0x52, 0x77, 0xaa, 0x29},
     {0x7d, 0xaa, 0x2a, 0xde, 0xaa, 0xa4, 0x7d, 0x55, 0x49, 0xfc, 0xaa, 0xa2, 0x7b, 0xac, 0x31, 0xb7, 0x55, 0x49, 0xbd, 0x55, 0x52},
     {0x37, 0x13, 0x11, 0x5b, 0x23, 0x22, 0x5b, 0x19, 0x05, 0x5b, 0xa8, 0x42, 0x5b, 0x15, 0x09, 0x5b, 0x4a, 0x24, 0x5b, 0x54, 0x44}},
    {{0x77, 0xaa, 0x29, 0x77, 0x55, 0x4a, 0xed, 0xaa, 0x49, 0xdb, 0xca, 0x52, 0x7b, 0x55, 0x29, 0xf6, 0x55, 0x29, 0xee, 0x55, 0x52},
     {0xbb, 0x55, 0x25, 0xbb, 0x4b, 0x25, 0xf9, 0xd1, 0x29, 0xaf, 0xa6, 0x91, 0xfc, 0xb1, 0x68, 0xde, 0xb2, 0x29, 0xde, 0xb2, 0x25},
     {0x5b, 0x2a, 0x12, 0x5b, 0x26, 0x12, 0x5b, 0x23, 0x05, 0xa7, 0x4a, 0x42, 0x5b, 0x49, 0x11, 0xd5, 0x2a, 0x12, 0x7a, 0x51, 0x11}},
    {{0xb7, 0xaa, 0x29, 0xbb, 0x55, 0x49, 0xed, 0x55, 0xa4, 0x7e, 0x72, 0x4a, 0x77, 0x55, 0x29, 0xed, 0x55, 0xa4, 0xee, 0xaa, 0xa4},
     {0x6f, 0x5a, 0x4a, 0xbb, 0xb2, 0x0b, 0xf6, 0x56, 0x16, 0xeb, 0x99, 0xc4, 0xf6, 0xa9, 0xa8, 0xb7, 0x35, 0x49, 0xed, 0xe4, 0x15},
     {0x5d, 0x25, 0x09, 0x5b, 0x31, 0x0a, 0x5b, 0x45, 0x42, 0xcd, 0x45, 0x42, 0x5b, 0x91, 0x42, 0x5b, 0x4a, 0x88, 0x5b, 0x49, 0x09}},
};

int PopCount(uint8_t b) {
    int n = 0;
    for (; b; b &= b - 1) ++n;
    return n;
}

struct Chunky {
    std::vector<SrLibColor> px;
    int w = 0;
    int h = 0;
};

Chunky ScaleByMask(const Chunky& src, uint8_t mask) {
    std::vector<int> cols;
    std::vector<int> rows;
    for (int x = 0; x < src.w; ++x) if (mask & (0x80 >> (x % 8))) cols.push_back(x);
    for (int y = 0; y < src.h; ++y) if (mask & (0x80 >> (y % 8))) rows.push_back(y);
    Chunky out;
    out.w = static_cast<int>(cols.size());
    out.h = static_cast<int>(rows.size());
    out.px.reserve(static_cast<size_t>(out.w) * out.h);
    for (int y : rows) for (int x : cols) out.px.push_back(src.px[static_cast<size_t>(y) * src.w + x]);
    return out;
}

}  // namespace

void Racing::LoadSr2OpponentSprites() {
    auto& assets = state.GetAssets();
    byte_vector& libFile = assets.GetLibPData(SrGame::SR2);
    const auto& entries = assets.GetImageEntries(SrGame::SR2);
    static const std::vector<std::vector<uint16_t>> palettes = SrLibReadSr2Palettes(assets.GetSrExeData(SrGame::SR2));
    BitmapParser bp;

    auto load = [&](int id) {
        Chunky c;
        if (id < 0 || id >= static_cast<int>(entries.second.size()) || palettes.size() < 2) return c;
        const BitmapEntry_t& entry = entries.second[id];
        std::vector<uint8_t> data = bp.ReadImage(libFile, entry, id, entries.first);
        std::vector<uint16_t> palData = palettes[1];
        // The magenta placeholders take the race colour.
        const uint16_t carColor = GetCarRaceColor(opponentColor);
        for (uint16_t& col : palData) {
            if (col == 0x0F0C || col == 0x0B0A) col = carColor;
        }
        Palette pal = bp.PreparePalette(palData, true);
        c.px = bp.PlanarToChunky(data, entry, pal, Palette(), id, SrGame::SR2);
        c.w = entry.width;
        c.h = entry.height;
        return c;
    };
    auto make = [](const Chunky& c) {
        auto bmp = std::make_unique<Bitmap>();
        bmp->LoadBitmapFromChunky(c.px, c.w, c.h);
        bmp->SetDimensions(c.w, c.h);
        return bmp;
    };

    if (opponentCarType == 2) {
        for (int i = 0; i < 13; ++i) sr2Cop[i] = make(load(SR2_RACING_OPPONENT_CAR_COP_FRONT_SIDE + i));
        return;
    }

    // Every state at every tilt, as the Aqueduct init FUN_0024214c and FUN_0023d290 build them. Only states 0 and 1
    // have no tilted frames.
    constexpr int kTilts = srrace::AQU_OPP_TILTS;
    constexpr int kLevel = srrace::AQU_OPP_LEVEL_TILT;
    const int style = std::clamp(sr2OppStyle, 0, 4);
    const int block = kSr2StyleBlock[style];
    const auto& masks = kSr2Masks[style];
    Chunky piece[13][kTilts];
    piece[0][kLevel] = load(block);
    piece[1][kLevel] = load(block + 1);
    for (int t = 0; t < kTilts; ++t) {
        piece[2][t] = load(block + 2 + t);   // REAR_1..7
        piece[9][t] = load(block + 9 + t);   // FAR_1..7
        piece[5][t] = ScaleByMask(piece[2][t], masks[0][t * 3 + 2]);
        for (int i = 0; i < 2; ++i) piece[3 + i][t] = ScaleByMask(piece[2][t], masks[0][t * 3 + i]);
        for (int i = 0; i < 3; ++i) {
            piece[6 + i][t] = ScaleByMask(piece[5][t], masks[1][t * 3 + i]);
            piece[10 + i][t] = ScaleByMask(piece[9][t], masks[2][t * 3 + i]);
        }
    }
    for (int s = 0; s < 13; ++s) {
        for (int t = 0; t < kTilts; ++t) {
            sr2Opp[s][t] = (s < 2 && t != kLevel) ? nullptr : make(piece[s][t]);
        }
    }

    // The anchors. The three base states are measured; the scaled ones take their base's, shrunk by how many
    // pixels in 8 the mask keeps - the tilt 0 mask row whatever the tilt (FUN_002420a2).
    std::memset(sr2OppXOff, 0, sizeof(sr2OppXOff));
    std::memset(sr2OppYOff, 0, sizeof(sr2OppYOff));
    struct Derived { int base; int count; int mask; };
    for (const Derived& d : {Derived{2, 2, 0}, Derived{5, 3, 1}, Derived{9, 3, 2}}) {
        // ponytail: half the declared width; FUN_002233f4 also drops right-hand padding set in plane 0, which the
        // chunky conversion has already cut off.
        const int half = piece[d.base][kLevel].w / 2;
        sr2OppXOff[d.base][kLevel] = static_cast<int8_t>(half);
        for (int t = 0; t < kTilts; ++t) {
            if (t == kLevel) continue;
            int xo = 0, yo = 0;
            srrace::AqueductTiltAnchor(piece[d.base][t].w, piece[d.base][t].h, srrace::AQU_OPP_TILT_ANGLE[t], half,
                                       xo, yo);
            sr2OppXOff[d.base][t] = static_cast<int8_t>(xo);
            sr2OppYOff[d.base][t] = static_cast<int8_t>(yo);
        }
        for (int k = 1; k <= d.count; ++k) {
            const int n = PopCount(masks[d.mask][k - 1]);
            for (int t = 0; t < kTilts; ++t) {
                sr2OppXOff[d.base + k][t] = static_cast<int8_t>((sr2OppXOff[d.base][t] * n) >> 3);
                sr2OppYOff[d.base + k][t] = static_cast<int8_t>((sr2OppYOff[d.base][t] * n) >> 3);
            }
        }
    }
}

void Racing::LoadOpponentCarSprites() {
    if (state.GetAssets().GetGame() == SrGame::SR2 && !sr1BodyInSr2) {
        LoadSr2OpponentSprites();
        return;
    }
    switch (opponentCarType) {
        case 0:
            opponentChevyFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_FRONT_SIDE);
            opponentChevyRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChevyRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChevyBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_BEHIND_1 + i);
            }
            break;
        case 1:
            opponentChryslerFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_FRONT_SIDE);
            opponentChryslerRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChryslerRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChryslerBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_BEHIND_1 + i);
            }
            break;
        case 2:
            opponentCopFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_FRONT_SIDE);
            opponentCopRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentCopRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentCopBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_BEHIND_1 + i);
            }
            break;
        case 3:
            opponentOldsmobileFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDSMOBILE_FRONT_SIDE);
            opponentOldsmobileRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentOldsmobileRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentOldsmobileBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_BEHIND_1 + i);
            }
            break;
        case 4:
            opponentChevy2FrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_FRONT_SIDE);
            opponentChevy2RearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChevy2Rear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChevy2Behind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_BEHIND_1 + i);
            }
            break;
        case 5:
            opponentCorvette63FrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_FRONT_SIDE);
            opponentCorvette63RearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentCorvette63Rear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentCorvette63Behind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_BEHIND_1 + i);
            }
            break;
        case 6:
            opponentFordFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_FRONT_SIDE);
            opponentFordRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentFordRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentFordBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_BEHIND_1 + i);
            }
            break;
        default:
            opponentCarType = 0;
            LoadOpponentCarSprites();
            return;
    }
    if (sr1BodyInSr2) {
        BuildSr1TiltFrames();
    }
}

// Street Rod's bodies were never drawn tilted, so the Aqueduct's seven tilts are made by turning each level frame.
// They then go in the same tables as SR2's own: anchor on the bottom centre, turned with the frame.
void Racing::BuildSr1TiltFrames() {
    constexpr int kLevel = srrace::AQU_OPP_LEVEL_TILT;
    std::memset(sr2OppXOff, 0, sizeof(sr2OppXOff));
    std::memset(sr2OppYOff, 0, sizeof(sr2OppYOff));
    for (int s = 0; s < 13; ++s) {
        for (auto& b : sr2Opp[s]) b.reset();
        Bitmap* level = OpponentSpriteForState(s);
        SDL_Surface* surf = level ? level->GetSurface() : nullptr;
        if (!surf || surf->w <= 0 || surf->h <= 0) continue;

        const int w = surf->w, h = surf->h;
        std::vector<SrLibColor> px(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                Uint32 v = 0;
                std::memcpy(&v, static_cast<const Uint8*>(surf->pixels) + y * surf->pitch +
                                    x * surf->format->BytesPerPixel, surf->format->BytesPerPixel);
                SrLibColor& c = px[static_cast<size_t>(y) * w + x];
                SDL_GetRGBA(v, surf->format, &c.r, &c.g, &c.b, &c.a);
            }
        }
        auto make = [](const std::vector<SrLibColor>& p, int pw, int ph) {
            auto bmp = std::make_unique<Bitmap>();
            bmp->LoadBitmapFromChunky(p, pw, ph);
            bmp->SetDimensions(pw, ph);
            return bmp;
        };
        sr2Opp[s][kLevel] = make(px, w, h);
        // The two side views are never drawn tilted.
        if (s < 2) continue;

        const int half = w / 2;
        sr2OppXOff[s][kLevel] = static_cast<int8_t>(half);
        for (int t = 0; t < srrace::AQU_OPP_TILTS; ++t) {
            if (t == kLevel) continue;
            int rw = 0, rh = 0;
            const std::vector<SrLibColor> turned =
                srrace::AqueductRotateSprite(px, w, h, srrace::AQU_OPP_TILT_ANGLE[t], SrLibColor{0, 0, 0, 0}, rw, rh);
            sr2Opp[s][t] = make(turned, rw, rh);
            int xo = 0, yo = 0;
            srrace::AqueductTiltAnchor(w, h, srrace::AQU_OPP_TILT_ANGLE[t], half, xo, yo);
            sr2OppXOff[s][t] = static_cast<int8_t>(xo);
            sr2OppYOff[s][t] = static_cast<int8_t>(yo);
        }
    }
}

srrace::CarSetup Racing::BuildCarSetupFromCar(const Car* car, bool isPlayer) const {
    srrace::CarSetup setup;
    if (car) {
        // The car's own gearbox: automaticOverride is set once from the startup car.
        setup = CarSetupFromCar(*car, false);
    } else {
        setup.transClass = automaticOverride ? 0 : 2;
    }
    setup.driverSkill = isPlayer ? 130 : 100;
    return setup;
}

void Racing::InitializeRaceSimulation() {
    // The template belongs to the profile, not the race.
    trackVariant = state.GetTrackVariant();
    const srrace::TrackGenSource genSource = state.GetAssets().GetGame() == SrGame::SR2
                                                ? srrace::TrackGenSource::StreetRod2
                                                : srrace::TrackGenSource::StreetRod;
    // The road belongs to the session, not to the race.
    srrace::RaceRng genRng(state.GetScatterSeed());
    srrace::RoadLayoutInit(track, genRng, 1, genSource);
    srrace::GenerateTrackShapes(track, trackVariant, genSource);
    srrace::ScatterHazards(track, genRng, genSource);
    // The Aqueduct is its own track: straight, level and walled.
    // A cruise is a town drive, whatever race was set up last.
    const bool grudgeRace = !cruiseStarting && state.GetPendingRaceType() == RaceType::Grudge;
    if (grudgeRace) {
        srrace::BuildGrudgeStrip(track);
    }
    if (aqueductPreview) {
        srrace::BuildAqueductRoad(track);
        srrace::BuildAqueductTrack(aqueductTrack);
    }
    // Both of these draw Street Rod 2 art.
    if (aqueductPreview || grudgeRace) {
        if (!state.GetAssets().IsGameLoaded(SrGame::SR2)) {
            std::string err;
            if (!state.GetAssets().LoadGame(SrGame::SR2, &err)) {
                LOG_WARN("This race needs Street Rod 2's lib.p: %s", err.c_str());
            }
        }
        // ...and its roadside, or the strip is lined with Street Rod's scenery.
        LoadRoadsideForGame(SrGame::SR2);
    } else {
        LoadRoadsideForGame(state.GetAssets().GetGame());
    }

    const Car* playerCar = state.GetCurrentCar();
    srrace::CarSetup playerSetup = BuildCarSetupFromCar(playerCar, true);
    // Keep the carless fallback and IsAutomatic()'s pre-init answer in step with the raced car.
    if (playerCar) {
        automaticOverride = (playerSetup.transClass == 0);
    }

    srrace::CarSetup npcSetup;
    Opponent* opponent = state.GetCurrentOpponent();
    if (opponent && opponent->car) {
        npcSetup = BuildCarSetupFromCar(opponent->car.get(), false);
    } else {
        npcSetup = playerSetup;
        npcSetup.driverSkill = 100;
    }
    // The side-swipe roll is rand(aggression/4 + 2).
    if (const SrDriverData* driverData = state.GetCurrentDriverData()) {
        npcSetup.aggression = driverData->aggression;
        // The opponent's upshift point.
        npcSetup.reaction = driverData->reaction;
    }

    // A previous race may have left the cop's sprites in the opponent slot.
    if (opponentCarTypeBeforeCop >= 0) {
        opponentCarType = opponentCarTypeBeforeCop;
        sr1BodyInSr2 = sr1BodyBeforeCop;
        opponentCarTypeBeforeCop = -1;
        LoadOpponentCarSprites();
    }

    // Suppresses the cop spawn and picks the King's engine branches.
    const bool kingRace = state.IsKingOpponent(opponent);
    const srrace::RaceKind kind =
        aqueductPreview ? srrace::RaceKind::Aqueduct
      : grudgeRace ? srrace::RaceKind::Grudge
                        : ToRaceKind(currentTrackType, state.GetAssets().GetGame());
    sim.Init(&track, kind,
             playerSetup, npcSetup,
             static_cast<uint32_t>(SDL_GetTicks()), kingRace);
    sim.SetAqueductTrack(aqueductPreview ? &aqueductTrack : nullptr);
    // Grudge Night stages on a fixed 110 frames rather than the random launch time.
    if (kind == srrace::RaceKind::Grudge) {
        sim.SetLaunchHandicap(srrace::GRUDGE_STAGE_FRAMES, srrace::GRUDGE_STAGE_FRAMES);
    }
    simInitialized = true;
    aqueductPreviewApplied = aqueductPreview;
    raceBetAmount = state.GetPendingBetAmount();
    raceIsPinkSlip = state.GetPendingIsPinkSlip();
    fixedAccumulator = 0.0f;
    havePrevStep = false;
    prevSpaceDown = false;
    pendingShift = false;
    firstPlayerFinishTick = -1;
    firstNpcFinishTick = -1;
    simTickCounter = 0;
    speedSumMph = 0;
    // StartCruise sets these again afterwards.
    cruiseMode = false;
    cruiseSignSprite.reset();
    cruiseSignAnim = nullptr;
    raceOutcome = RaceOutcome::Ongoing;
    bShowCrashOverlay = false;
    crashHandled = false;
    fallStep = 0;
    startLightHeight = 32;  // full; retracts once the car launches
    sr2LightSlide = 0;
    sr2StarterSeg = -1;
    // The scene outlives a race, so per-race timers are cleared here.
    tireScreechCooldownTicks = 0;
    copSirenCooldownTicks = 0;
    lastVisibleRow = -1;
}

// The ordinary race loop with the race flag cleared, started at segment 10 / 65 / 115. The finish is the drag
// finish at 380, the arch a race starts under.
void Racing::StartCruise(int startSeg, int signAsset) {
    SetTrackType(SR_TRACK_DRAG_RACE);
    // The last race's type is still pending: neither the channel nor the grudge strip.
    aqueductPreview = false;
    cruiseStarting = true;
    // After InitializeRaceSimulation, which clears cruiseMode.
    InitializeRaceSimulation();
    cruiseStarting = false;
    cruiseMode = true;
    if (signAsset > 0) {
        Bitmap* asset = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(signAsset)});
        if (asset) {
            cruiseSignSprite = std::make_unique<Bitmap>(*asset);
        }
        cruiseSignAnim = GetSpriteAnimationInfo(static_cast<uint16_t>(signAsset));
    }
    sim.BeginCruiseAt(ClampInt(startSeg, 0, kTrackSegments - 1));
    startLightHeight = 0;    // no start light on a cruise
}

// A cruise ends at the next segment with flag bit 0 on either side, i.e. the next building, so each destination
// has its own start segment.
bool Racing::CruiseFinished() const {
    if (!cruiseMode) {
        return false;
    }
    const int seg = ClampInt(static_cast<int>(sim.Player().segPos) + 1, 0, kTrackSegments - 1);
    return ((track.left[seg] | track.right[seg]) & 0x0001) != 0;
}

// -1 left, +1 right, 0 not in range yet. The buildings are at segment 44 (right), 108 (left) and 156 (left).
int Racing::CruiseArrivalSide() const {
    if (!cruiseMode) {
        return 0;
    }
    const int from = static_cast<int>(sim.Player().segPos);
    for (int k = 1; k <= kCruiseArrivalSegs; ++k) {
        const int seg = ClampInt(from + k, 0, kTrackSegments - 1);
        if (track.right[seg] & 0x0001) {
            return +1;
        }
        if (track.left[seg] & 0x0001) {
            return -1;
        }
    }
    return 0;
}

void Racing::SetTrackType(SrTrackType trackType) {
    currentTrackType = trackType;
}

SrTrackType Racing::GetTrackType() const {
    return currentTrackType;
}

void Racing::LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType) {
    (void)binaryData;
    SetTrackType(trackType);
    InitializeRaceSimulation();
}

void Racing::StopEngineSound() {
    if (engineSoundStarted && audioManager) {
        audioManager->StopLoop(0);
    }
    engineSoundStarted = false;
}

void Racing::ResetForNewRace() {
    // Driving and this Racing are built once and Init() never runs again, so per-race state is cleared here.

    // A loop left running from an unfinished race would stack with the new one.
    StopEngineSound();

    // A race always starts in the cockpit.
    flybyMode = false;
    flybyNpcVisible = false;
    flybySeg = srrace::RACE_START_SEG;
    flybyScrollAcc = 0;
    flybyHeading = 0;
    flybyLateral = 2560;
    opponentActiveBeforeFlyby = false;

    InitializeRaceSimulation();
}

// Body asset -> race-view NPC sprite set: 0 chevy, 1 chrysler, 3 oldsmobile, 4 chevy2/corvette-56-61,
// 5 corvette-63, 6 ford; 2 is the cop, never selected by body. From the game's per-car asset table.
static int NpcCarTypeForBodyAsset(int bodyAsset) {
    switch (bodyAsset) {
        // chevy set (selector 1)
        case DODGE_POLARA_1962_BODY:
        case PLYMOUTH_SAVOY_2DR_1962_BODY:
        case FORD_FAIRLANE_VICTORIA_1955_BODY:
        case FORD_FAIRLANE_500_1956_BODY:
        case CHEVROLET_IMPALA_1958_BODY:
        case VALIANT_V200_1961_BODY:
        case VALIANT_V100_2DR_1963_BODY:
            return 0;
        // chrysler set (selector 2)
        case DODGE_CUSTOM_ROYAL_LANCER_1955_BODY:
        case MERCURY_MONTEREY_1954_BODY:
        case MERCURY_CUSTOM_2DR_1956_BODY:
        case CHEVROLET_BELAIR_SPORTS_COUPE_BODY:
            return 1;
        // oldsmobile set (selector 3) - the prewar bodies
        case CHEVROLET_2DR_COUPE_1940_BODY:
        case CHEVROLET_ROADSTER_1940_BODY:
        case CHEVROLET_MASTER_DELUXE_2DR_1938_BODY:
        case FORD_DEUCE_COUPE_1932_BODY:
        case FORD_DELUXE_2DR_1940_BODY:
            return 3;
        // chevy2 set (selector 4)
        case CHEVROLET_CORVETTE_1956_BODY:
        case CHEVROLET_CORVETTE_1961_BODY:
        case CHEVROLET_CORVETTE_1963_BODY:
        case CHEVROLET_STYLELINE_2DR_1949_BODY:
        case OLDSMOBILE_88_2DR_CUSTOM_1949_BODY:
        case OLDSMOBILE_88_CUSTOM_1952_BODY:
        case PONTIAC_SILVER_STREAK_1950_BODY:
        case FORD_THUNDERBIRD_1957_BODY:
            return 4;
        // corvette-63 set (selector 0) - The King only
        case THE_KING_CHEVROLET_CORVETTE_1963_BODY:
            return 5;
        // ford set (selector 5)
        case FORD_SEDAN_1932_BODY:
            return 6;
        default:
            return -1;
    }
}

void Racing::InitializeOpponent() {
    Opponent* opponent = state.GetCurrentOpponent();
    if (!opponent || !opponent->car) {
        opponentActive = false;
        return;
    }

    opponentActive = true;
    opponentColor = opponent->car->GetColor();
    const CarModel& model = opponent->car->GetModel();
    sr1BodyInSr2 = false;
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        // A car from another set has no SR2 body style (0 would be the King's own block), so it races with
        // Street Rod's opponent sprites, picked as a Street Rod game picks them.
        const bool sr2Car = GetCarSetDefs()[model.carSet].game == SrGame::SR2;
        if (sr2Car || state.IsKingOpponent(opponent) ||
            (!state.GetAssets().IsGameLoaded(SrGame::SR1) && !state.GetAssets().LoadGame(SrGame::SR1))) {
            // ponytail: without Street Rod's files a foreign car takes an SR2 style off its model index.
            int style = model.sprites.bodyStyle;
            if (!state.IsKingOpponent(opponent) && (!sr2Car || style < 1 || style > 4)) {
                style = 1 + model.index % 4;
            }
            sr2OppStyle = style;
            opponentCarType = 0;
            LoadOpponentCarSprites();
            return;
        }
        sr1BodyInSr2 = true;
    }
    // sprites.body, not index: The King borrows the plain '63 Corvette's row and only overrides the body art.
    // Other car sets go by index and fall through to the vendor guess.
    const int bodyAsset = (model.carSet == kAmigaCarSet) ? model.sprites.body : model.index;
    const int mappedType = NpcCarTypeForBodyAsset(bodyAsset);
    if (mappedType >= 0) {
        opponentCarType = mappedType;
        LoadOpponentCarSprites();
        return;
    }

    // Only a non-Amiga car set reaches here.
    switch (model.vendor) {
        case CarVendorGM:
            opponentCarType = 0;
            break;
        case CarVendorChrysler:
            opponentCarType = 1;
            break;
        case CarVendorFord:
        case CarVendorCustom:
        case CarVendorUnknown:
        default:
            opponentCarType = 0;
            break;
    }
    LoadOpponentCarSprites();
}

float Racing::GetSpeed() const {
    return static_cast<float>(sim.Player().speed);
}

int16_t Racing::GetCurrentGear() const {
    return sim.Player().gear;
}

int16_t Racing::GetSteerDisplay() const {
    return sim.Player().slideVel;
}

// Delegates to the SDL-free srrace::BuildRaceEndReason.
srrace::RaceEndContext Racing::BuildRaceEndContext() const {
    srrace::RaceEndContext ctx;
    ctx.endFlags = sim.Player().endFlags;
    ctx.playerWon = (raceOutcome == RaceOutcome::PlayerWon);
    ctx.playerLost = (raceOutcome == RaceOutcome::PlayerLost);
    ctx.isPinkSlip = raceIsPinkSlip;
    ctx.betAmount = raceBetAmount;
    ctx.isDragRace = (currentTrackType == SR_TRACK_DRAG_RACE);
    ctx.isSr2 = state.GetAssets().GetGame() == SrGame::SR2;
    ctx.isAqueduct = aqueductPreview;
    ctx.isGrudge = (sim.Kind() == srrace::RaceKind::Grudge);
    ctx.tiresBlown = sim.PlayerTiresBlown();
    ctx.speedingFine = sim.SpeedingFine();
    ctx.bankroll = state.GetBankroll();
    // The car freezes on the failure bit, so this is still the impact speed.
    ctx.crashSpeed = sim.Player().speed;
    ctx.carValue = state.GetCarValue(state.GetCurrentCar());
    ctx.carStrippedValue = state.GetCarStrippedValue(state.GetCurrentCar());
    return ctx;
}

// In the original's time scale, so a race reads the same at any race speed.
int Racing::RaceTimeTenths() const {
    return simTickCounter * 10 / srrace::SIM_HZ_ORIGINAL;
}

// How far behind, in tenths.
int Racing::LossMarginTenths() const {
    const int pct = ClampInt(DistancePercent(), 0, 100);
    return std::min(999, RaceTimeTenths() * (100 - pct) / 100);
}

int Racing::AverageSpeedMph() const {
    return simTickCounter > 0 ? speedSumMph / simTickCounter : 0;
}

// (player - start) * 100 / (opponent - start), capped at 99 so a loss never reads 100%.
int Racing::DistancePercent() const {
    const int start = sim.StartSegment();
    const int player = static_cast<int>(sim.Player().segPos) - start;
    const int npc = static_cast<int>(sim.Npc().segPos) - start;
    if (npc <= 0) {
        return 0;
    }
    return std::min(99, player * 100 / npc);
}

bool Racing::IsAutomatic() const {
    if (!simInitialized) {
        return automaticOverride;
    }
    return sim.PlayerTables().automatic;
}

int Racing::GetNumGears() const {
    return sim.PlayerTables().numGears;
}

bool Racing::IsAutoDriveEngaged() const {
    return sim.Player().autoDrive;
}

bool Racing::ShouldShowCrashOverlay() const {
    return bShowCrashOverlay;
}

// INFERRED, not verified: the draw is not statically findable. Blood follows the crash tiers at 31, 60 and 91 mph.
// Tuning knob until the real condition is known.
bool Racing::ShouldShowCrashBlood() const {
    return bShowCrashOverlay && sim.PlayerWrecked() && sim.Player().speed >= 60;
}

RaceOutcome Racing::GetRaceOutcome() const {
    return raceOutcome;
}

void Racing::ResetRaceOutcome() {
    raceOutcome = RaceOutcome::Ongoing;
}

void Racing::HandleOutcomeAfterTicks() {
    if (raceOutcome != RaceOutcome::Ongoing || cruiseMode) {
        return;   // a cruise has no outcome; Driving watches CruiseFinished()
    }

    // Any failure ends the race as a loss, but only a wall hit wrecks the car.
    if (sim.PlayerFailed()) {
        // Over the edge the view drops for eight frames before the crash lands.
        if (sim.FallDirection() != 0 && fallStep < 8) {
            return;
        }
        if (!crashHandled) {
            crashHandled = true;
            if (sim.PlayerWrecked()) {
                bShowCrashOverlay = true;
                // The bash sample at full length, one low voice.
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CRASH);
                }
            }
            // Driving applies the drivetrain damage with the rest of the outcome.
        }
        raceOutcome = RaceOutcome::PlayerLost;
        return;
    }

    // The sim reports the race over once the car has stopped past the line; the verdict was latched at the line.
    if (sim.RaceOver()) {
        raceOutcome = sim.PlayerWonAtLine() ? RaceOutcome::PlayerWon
                                            : RaceOutcome::PlayerLost;
    }
}

void Racing::Draw(float fElapsedTime, SDL_Event* event) {
    (void)event;
    if (!simInitialized) {
        InitializeRaceSimulation();
    }
    // Toggling the preview swaps the track under the car, so lay the race out again.
    if (aqueductPreview != aqueductPreviewApplied) {
        InitializeRaceSimulation();
    }
    if (!engineSoundStarted && audioManager) {
        // A stripped exhaust is louder, which the AdLib engine models by volume.
        const Car* car = state.GetCurrentCar();
        audioManager->SetMufflerRemoved(car != nullptr && car->IsMufflerRemoved());
        audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
        engineSoundStarted = true;
    }

    sim.SetInvulnerable(debugInvulnerable);

    if (flybyMode) {
        UpdateFlybyMovement(fElapsedTime);
    } else {
        fixedAccumulator += std::max(0.0f, fElapsedTime);
        fixedAccumulator = std::min(fixedAccumulator, StepSeconds() * 2.0f);

        // Consult the debug UI before reading raw key state.
        const Uint8* keyboard = DebugUiWantsKeyboard() ? nullptr : SDL_GetKeyboardState(nullptr);
        uint16_t baseInput = 0;

        // Pad and keyboard drive the same four sim inputs.
        const float padSteer = gamepad.Axis(SDL_CONTROLLER_AXIS_LEFTX);
        const bool padThrottle = gamepad.TriggerDown(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        const bool padBrake = gamepad.TriggerDown(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        const bool padShift = gamepad.Pressed(SDL_CONTROLLER_BUTTON_A) ||
                              gamepad.Pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

        const bool up = (keyboard && keyboard[SDL_SCANCODE_UP]) || padThrottle;
        const bool down = (keyboard && keyboard[SDL_SCANCODE_DOWN]) || padBrake;
        const bool left = (keyboard && keyboard[SDL_SCANCODE_LEFT]) ||
                          padSteer < 0.0f ||
                          gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        const bool right = (keyboard && keyboard[SDL_SCANCODE_RIGHT]) ||
                           padSteer > 0.0f ||
                           gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        const bool space = keyboard && keyboard[SDL_SCANCODE_SPACE];

        if (up) baseInput |= srrace::INPUT_THROTTLE;
        if (down && !up) baseInput |= srrace::INPUT_BRAKE;
        if (left) baseInput |= srrace::INPUT_STEER_LEFT;
        if (right) baseInput |= srrace::INPUT_STEER_RIGHT;
        if ((space && !prevSpaceDown) || padShift) {
            pendingShift = true;
        }
        prevSpaceDown = space;

        // 1.0 unless analog_steering is on and the stick is steering; keyboard and D-pad stay original.
        float steerAmount = 1.0f;
        if (state.GetAssets().GetAnalogSteeringRef()) {
            const bool digitalSteer =
                (keyboard && (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_RIGHT])) ||
                gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
                gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            if (!digitalSteer) {
                steerAmount = std::fabs(padSteer);
            }
        }

        if (cruiseMode) {
            // The original's cutscene winds rpm up, shifts at 4500 and holds top gear. With real physics, hold the throttle
            // to that point and feather it. Driving turns any key into "skip".
            const srrace::CarState& pc = sim.Player();
            const bool topGear = pc.gear >= sim.PlayerTables().numGears;
            baseInput = (topGear && pc.rpm >= kCruiseRpm) ? 0 : srrace::INPUT_THROTTLE;
            pendingShift = !topGear && pc.rpm > kCruiseRpm;

            // Turn towards the building only in the last segment; the original never slows on a cruise.
            const int side = CruiseArrivalSide();
            if (side != 0) {
                baseInput |= (side < 0) ? srrace::INPUT_STEER_LEFT : srrace::INPUT_STEER_RIGHT;
            }
            // A stick resting off centre must not soften the turn-in.
            steerAmount = 1.0f;
        }

        if (fixedAccumulator >= StepSeconds()) {
            uint16_t inputBits = baseInput;
            if (pendingShift) {
                inputBits |= srrace::INPUT_SHIFT;
                pendingShift = false;
            }
            prevPlayer = sim.Player();
            prevNpc = sim.Npc();
            havePrevStep = true;
            sim.Tick(inputBits, steerAmount);
            ++simTickCounter;
            if (sim.PlayerFailed() && sim.FallDirection() != 0 && fallStep < 8) {
                ++fallStep;
            }
            // For the post-race stats panel's average speed.
            speedSumMph += sim.Player().speed;

            // The impact sound as the player is shoved clear.
            if (sim.ConsumeNpcBump() && audioManager) {
                // `random % 40 + 310` per shot.
                const int period = SRL0_CAR_BASH_PERIOD_BASE +
                                   (std::rand() % SRL0_CAR_BASH_PERIOD_SPREAD);
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f, 3579545 / period);
            }

            // The renderer switches on opponentCarType.
            if (sim.ConsumeCopSpawn()) {
                if (opponentCarTypeBeforeCop < 0) {
                    opponentCarTypeBeforeCop = opponentCarType;
                    sr1BodyBeforeCop = sr1BodyInSr2;
                }
                opponentCarType = 2;
                sr1BodyInSr2 = false;   // the game's own cop
                LoadOpponentCarSprites();
            }

            // Retriggered on a cooldown: our one-shots queue rather than restart a channel.
            if (copSirenCooldownTicks > 0) {
                --copSirenCooldownTicks;
            }
            if (sim.CopSirenOn() && audioManager) {
                if (copSirenCooldownTicks == 0) {
                    audioManager->PlaySound(AudioSampleType::COP_SIREN);
                    copSirenCooldownTicks = SampleTicks(SRL0_COP_SIREN_LENGTH, SRL0_COP_SIREN_RATE);
                }
            } else {
                copSirenCooldownTicks = 0;
            }

            // See PlayerSkidding().
            if (tireScreechCooldownTicks > 0) {
                --tireScreechCooldownTicks;
            }
            if (sim.PlayerSkidding() && audioManager) {
                if (tireScreechCooldownTicks == 0) {
                    const int rate = static_cast<int>(
                        3579545u / (SRL0_TIRE_SCREECH_PERIOD_BASE + std::rand() % SRL0_TIRE_SCREECH_PERIOD_SPREAD));
                    audioManager->PlaySound(AudioSampleType::TIRE_SCREECH, SRL0_TIRE_SCREECH_VOLUME / 64.0f, rate);
                    tireScreechCooldownTicks = SampleTicks(SRL0_TIRE_SCREECH_LENGTH, rate);
                }
            } else {
                tireScreechCooldownTicks = 0;
            }

            if (sim.Phase() != srrace::RacePhase::Countdown && sim.Player().speed > 0) {
                startLightHeight = std::max(0, startLightHeight - 4);
            }

            fixedAccumulator -= StepSeconds();
        }

        HandleOutcomeAfterTicks();

        // Or it keeps running under the race-end bubble.
        if (raceOutcome != RaceOutcome::Ongoing && engineSoundStarted && audioManager) {
            audioManager->StopLoop(0);
            engineSoundStarted = false;
        }
    }

    if (audioManager) {
        audioManager->UpdateEnginePitch(sim.Player().rpm);
    }

    if (SDL_LockSurface(pixels) != 0) {
        return;
    }

    UpdateRenderView();
    RenderSky();
    lastVisibleRow = BuildRowTableAndGetLastVisible();
    if (aqueductPreview) {
        // The channel replaces the road and everything standing beside it;
        // the Aqueduct has no roadside at all.
        RenderAqueduct();
    } else {
        RenderRoad(lastVisibleRow);
        if (!IsMulholland()) {
            RenderMountains(rows[lastVisibleRow].y);
        }
        DrawTownWalls(lastVisibleRow);
        DrawRoadsideAndArchSprites(lastVisibleRow);
    }
    // Grudge Night simulates the opponent but does not draw it.
    if (sim.Kind() != srrace::RaceKind::Grudge) {
        DrawNpcCar(lastVisibleRow);
    }
    DrawStartLight();
    DrawRearviewMirror();
    DrawSideMirror();

    SDL_UnlockSurface(pixels);
    if (flybyMode) {
        DrawFlybyHud(lastVisibleRow);
    } else {
        DrawHud();
    }
}

SDL_Surface* Racing::GetSurface() {
    return pixels;
}
