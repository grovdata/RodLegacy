#include "car/sr2_engine_bay_layout.h"

namespace {

// 0x262580: manifold y from the engine's, per manifold class.
const int kManifoldDy[9] = {7, 8, 7, 8, 8, 3, -5, -6, 4};
// 0x2624ce / 0x262504: carb offset from the manifold, [manifold class][carb class]; 1000 = no such pairing.
const int kCarbDx[9][3] = {{-24, 1000, 1000}, {1000, -22, -22}, {13, 1000, 1000}, {1000, 13, 13}, {1000, 13, 13},
                           {1000, -22, -22}, {1000, 13, 13},     {1000, 1000, 1000}, {1000, 1000, 1000}};
const int kCarbDy[9][3] = {{-14, 1000, 1000}, {1000, -10, -10}, {-16, 1000, 1000}, {1000, -5, -5}, {1000, -7, -7},
                           {1000, -4, -4},     {1000, -7, -7},   {1000, 1000, 1000}, {1000, 1000, 1000}};
// 0x26253a / 0x262543: extra x for the second carb, extra y for carbs after the first.
const int kSecondCarbDx[9] = {0, 0, 0, -35, -35, 0, -35, 0, 0};
const int kLaterCarbDy[9] = {0, 0, 15, 0, -2, 0, 12, 0, 0};
// 0x262592: air filter y from the manifold, [manifold class][carb index].
const int kFilterDy[7][2] = {{-10, 0}, {-6, 0}, {-19, 0}, {-17, -1}, {-19, -5}, {-14, 0}, {-19, -5}};
// 0x262558/0x262560 (four screws) and 0x262568/0x262574 (supercharger's six).
const int kScrewX4[4] = {11, 58, 6, 62};
const int kScrewY4[4] = {14, 14, 28, 28};
const int kScrewX6[6] = {15, 56, 12, 60, 7, 64};
const int kScrewY6[6] = {6, 6, 20, 20, 45, 45};
// 0x2624c2 / 0x2624c8: a carburettor's two screws, by mount kind.
const int kCarbScrewX[3] = {9, 43, 42};
const int kCarbScrewY[3] = {23, 40, 26};
const int kCarbScrewPitch = 0x1b;
const int kScrewCropH[4] = {2, 4, 6, 4};

// Added to the screw y by manifold class (FUN_0023638c).
int BodyScrewYBias(int manifoldClass) {
    switch (manifoldClass) {
        case 0: case 2: return 0;
        case 1: case 3: case 4: return 10;
        case 7: return 24;
        case 6: return 23;
        default: return 15;
    }
}

// Which of the carb screw columns a carburettor uses (FUN_00236074).
int CarbScrewKind(int manifoldClass, int idx) {
    if (idx == 0) return manifoldClass == 5 ? 2 : (manifoldClass == 0 || manifoldClass == 1) ? 1 : 0;
    if (manifoldClass == 6) return 2;
    if (idx == 1 && manifoldClass == 2) return 0;
    return 1;
}

// The carburettor sprite changes with the mount (FUN_00236192's switch); lib.p index.
int CarbAssetFor(int manifoldClass, int carbClass, int idx) {
    const bool carb4 = carbClass == 1;
    switch (manifoldClass) {
        case 2: return idx == 2 ? 23 : 13;
        case 3: case 4:
            if (idx == 0) return carb4 ? 17 : 26;
            break;
        case 5: case 6:
            if (idx == 1 || manifoldClass == 5) return carb4 ? 25 : 28;
            return carb4 ? 17 : 26;
        default: break;
    }
    return Sr2CarbAsset(carbClass);
}

}  // namespace

int Sr2EngineAsset(int engineClass) {
    return engineClass == 0 ? 8 : engineClass == 1 ? 9 : 21;
}

int Sr2ManifoldAsset(int manifoldClass) {
    static const int kAsset[9] = {12, 16, 20, 19, 22, 129, 29, 30, 31};
    return kAsset[manifoldClass < 0 ? 0 : manifoldClass > 8 ? 8 : manifoldClass];
}

int Sr2CarbAsset(int carbClass) {
    return carbClass == 0 ? 14 : carbClass == 1 ? 18 : 27;
}

int Sr2ScrewCropHeight(int state) {
    return kScrewCropH[state < 0 ? 0 : state > 3 ? 3 : state];
}

Sr2BayLayout ComputeSr2BayLayout(int engineClass, int manifoldClass, int carbClass, int carbCount) {
    Sr2BayLayout out;

    out.engine.present = true;
    out.engine.asset = Sr2EngineAsset(engineClass);
    out.engine.at = {kSr2BayOrigin.x + 0x47, kSr2BayOrigin.y + (engineClass == 0 ? 0x37 : 0x2d)};

    out.manifold.present = true;
    out.manifold.asset = Sr2ManifoldAsset(manifoldClass);
    out.manifold.at = {out.engine.at.x + 0x2d, out.engine.at.y + kManifoldDy[manifoldClass]};

    const bool super = manifoldClass == 8;
    const int bodyScrews = super ? 6 : 4;
    const int bias = super ? 0 : BodyScrewYBias(manifoldClass);
    for (int i = 0; i < bodyScrews; ++i) {
        const int dx = super ? kScrewX6[i] : kScrewX4[i];
        const int dy = (super ? kScrewY6[i] : kScrewY4[i]) + bias;
        out.bodyScrews.push_back({{out.manifold.at.x + dx, out.manifold.at.y + dy}});
    }

    const int mClass = manifoldClass < 0 ? 0 : manifoldClass > 8 ? 8 : manifoldClass;
    for (int idx = 0; idx < carbCount && idx < 3; ++idx) {
        const int cc = carbClass < 0 ? 0 : carbClass > 2 ? 2 : carbClass;
        int dx = idx == 2 ? -20 : kCarbDx[mClass][cc];
        const int dy = kCarbDy[mClass][cc];
        if (dx == 1000 || dy == 1000) break;
        int x = out.manifold.at.x + dx;
        int y = out.manifold.at.y + dy;
        if (idx == 1) x += kSecondCarbDx[mClass];
        if (idx >= 1) y += kLaterCarbDy[mClass];
        if (idx == 2) y -= 2;
        out.carbs[idx] = {CarbAssetFor(mClass, cc, idx), {x, y}, true};

        const int kind = CarbScrewKind(mClass, idx);
        for (int k = 0; k < 2; ++k) {
            out.carbScrews[idx].push_back({{x + kCarbScrewX[kind] + k * kCarbScrewPitch, y + kCarbScrewY[kind]}});
        }

        // The air filter: one per carburettor, but a 3-2brl manifold carries a single one on its third.
        if (mClass == 2 && idx != 2) continue;
        if (mClass > 6) continue;
        const int param = mClass == 2 ? 0 : idx;
        const int fx = out.manifold.at.x + (mClass == 2 ? 0xf : 0x14);
        const int fy = out.manifold.at.y + kFilterDy[mClass][param];
        out.airFilters[idx] = {15, {fx, fy}, true};
    }
    return out;
}
