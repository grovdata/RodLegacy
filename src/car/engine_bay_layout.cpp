#include "car/engine_bay_layout.h"

namespace {

// The game composites the parts into a 176x167 offscreen buffer that lands
// on screen at (38,4); its tables are relative to a point 14px left of that,
// so screen x = tableX + 24 and screen y = tableY - assetHeight.
constexpr int kOriginX = 24;

// Screws blit a fixed 13-row box, bottom-aligned on the table position.
constexpr int kScrewBoxH = 13;

// Every engine table is indexed by body shape, not by CarEngine_t.
int EngineTier(CarEngine_t engine) {
    switch (engine) {
        case CarEngineChrysler_V6:
        case CarEngineGM_V6:
        case CarEngineFord_V6:
            return 0;
        case CarEngineChrysler_V8_285:
        case CarEngineGM_V8_282:
        case CarEngineFord_V8_292:
            return 1;
        default:
            return 2;
    }
}

const SDL_Point kEngineBody[3] = {{24, 156}, {24, 156}, {14, 165}};    // A4-0x4432
const SDL_Point kEngineScrew[3] = {{147, 129}, {166, 129}, {174, 134}};  // A4-0x4426
const SDL_Point kEngineWire[3] = {{150, 160}, {168, 161}, {174, 169}};   // A4-0x441a
const SDL_Point kManifoldBody = {56, 76};

// Carb position slots 4..13 (A4-0x4402), and the first slot each manifold
// class draws into (A4-0x3b7a).
const SDL_Point kCarbSlot[10] = {
    {80, 56}, {87, 51}, {80, 51}, {56, 56}, {80, 56},
    {104, 56}, {64, 51}, {104, 51}, {64, 52}, {104, 52}
};
const int kClassFirstSlot[5] = {5, 6, 7, 10, 12};

// Added to the carb's own slot position to get its screw (A4-0x43da).
const SDL_Point kCarbScrewOffset[3] = {{18, -4}, {30, -2}, {30, -2}};

// A class with more than one carb uses consecutive slots.
SDL_Point CarbSlot(ManifoldType manifold, CarEngine_t engine, int carbIndex) {
    int cls = ManifoldClass(manifold);
    int slot = kClassFirstSlot[cls];
    // The single-carb 2-barrel manifold sits lower on a V6 block.
    if (cls == 0 && EngineTier(engine) == 0) slot = 4;
    slot += carbIndex;
    if (slot < 4) slot = 4;
    if (slot > 13) slot = 13;
    return kCarbSlot[slot - 4];
}

SDL_Rect BottomAnchoredRect(SDL_Rect asset, SDL_Point table) {
    return {table.x + kOriginX, table.y - asset.h, asset.w, asset.h};
}

}  // namespace

SDL_Rect EngineBodyRect(SDL_Rect asset, CarEngine_t engine) {
    return BottomAnchoredRect(asset, kEngineBody[EngineTier(engine)]);
}

SDL_Rect ManifoldBodyRect(SDL_Rect asset, ManifoldType manifold) {
    (void)manifold;  // one position for every manifold body
    return BottomAnchoredRect(asset, kManifoldBody);
}

// Like CarbSlot's V6 case, this reads the engine the car carries rather than
// whether one is bolted in right now; the original tests the engine part
// pointer, so a bay with the engine pulled out reverts to the wide body.
bool ManifoldBodyIsNarrow(ManifoldType manifold, CarEngine_t engine) {
    const int cls = ManifoldClass(manifold);
    return cls == 2 || (cls == 0 && EngineTier(engine) == 0);
}

SDL_Rect CarbBodyRect(SDL_Rect asset, ManifoldType manifold, CarEngine_t engine, int carbIndex) {
    return BottomAnchoredRect(asset, CarbSlot(manifold, engine, carbIndex));
}

// The wire is the one part that isn't bottom-anchored: its table entry is an
// offset marker, and the art goes at (x-12, y-22).
SDL_Rect EngineWireRect(SDL_Rect asset, CarEngine_t engine) {
    const SDL_Point& t = kEngineWire[EngineTier(engine)];
    return {t.x - 12, t.y - 22, asset.w, asset.h};
}

SDL_Point EngineBayScrewAnchor(int screwIndex, CarEngine_t engine, ManifoldType manifold,
                               CarburetorType carb) {
    SDL_Point t;
    switch (screwIndex) {
        case 0:
            t = kEngineScrew[EngineTier(engine)];
            break;
        case 1:
            t = {74, 75};
            break;
        case 2:
            // An 88px-wide body pulls its right screw in.
            t = {ManifoldBodyIsNarrow(manifold, engine) ? 131 : 153, 75};
            break;
        default: {
            SDL_Point slot = CarbSlot(manifold, engine, screwIndex - 3);
            int carbType = (int)carb;
            if (carbType < 0 || carbType > (int)CARB_4BARREL_RACING) carbType = 0;
            const SDL_Point& off = kCarbScrewOffset[carbType];
            t = {slot.x + off.x, slot.y + off.y};
            break;
        }
    }
    return {t.x + kOriginX, t.y};
}

// The game's own hotbox is 6px wide and starts 2px left of the sprite; widen
// it to also cover the sprite so a click on the visible screw always lands.
SDL_Rect EngineBayScrewZone(SDL_Point anchor) {
    return {anchor.x - 2, anchor.y - kScrewBoxH, 10, 16};
}
