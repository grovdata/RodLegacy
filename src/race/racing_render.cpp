#include "race/racing.h"
#include "race/racing_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "core/log.h"
#include "core/state.h"

using namespace racing_internal;

void Racing::PutPixel(SDL_Surface* s, int x, int y, SDL_Color c) {
    if (!s || x < 0 || x >= s->w || y < 0 || y >= s->h) {
        return;
    }
    const int offset = y * s->pitch + x * s->format->BytesPerPixel;
    Uint8* pixel = static_cast<Uint8*>(s->pixels) + offset;
    *(pixel + s->format->Rshift / 8) = c.r;
    *(pixel + s->format->Gshift / 8) = c.g;
    *(pixel + s->format->Bshift / 8) = c.b;
    if (s->format->BytesPerPixel > 3) {
        *(pixel + s->format->Ashift / 8) = c.a;
    }
}

// gaugeValue is the dial's own 0..130 input; index = value * 63 / 100.
void Racing::DrawGaugeNeedle(SDL_Renderer* renderer, int originX, int originY, int gaugeValue) {
    if (!renderer || needleLines.empty()) {
        return;
    }
    const int clamped = std::max(0, std::min(130, gaugeValue));
    const NeedleLine& e = needleLines[static_cast<size_t>(clamped) * 63 / 100];
    SDL_SetRenderDrawColor(renderer, 96, 64, 0, 255);
    SDL_RenderDrawLine(renderer, originX + e.x0, originY + e.y0,
                                 originX + e.x1, originY + e.y1);
}

void Racing::DrawSprite(Bitmap* bitmap, int dstX, int dstY, bool anchorRight, bool anchorBottom,
                        int srcX0, int srcY0, int srcW, int srcH) {
    if (!bitmap) {
        return;
    }
    SDL_Surface* src = bitmap->GetSurface();
    if (!src || src->w <= 0 || src->h <= 0) {
        return;
    }

    if (srcW <= 0) srcW = src->w;
    if (srcH <= 0) srcH = src->h;
    srcX0 = std::max(0, std::min(srcX0, src->w - 1));
    srcY0 = std::max(0, std::min(srcY0, src->h - 1));
    srcW = std::min(srcW, src->w - srcX0);
    srcH = std::min(srcH, src->h - srcY0);
    if (srcW <= 0 || srcH <= 0) {
        return;
    }

    const int drawX = anchorRight ? dstX - srcW : dstX;
    const int drawY = anchorBottom ? dstY - srcH : dstY;

    for (int y = 0; y < srcH; ++y) {
        const int py = drawY + y;
        if (py < 0 || py >= pixels->h) {
            continue;
        }
        for (int x = 0; x < srcW; ++x) {
            const int px = drawX + x;
            if (px < 0 || px >= pixels->w) {
                continue;
            }
            Uint32 srcPixel = 0;
            const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + (srcY0 + y) * src->pitch +
                                  (srcX0 + x) * src->format->BytesPerPixel;
            std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
            if (a == 0) {
                continue;
            }
            PutPixel(pixels, px, py, {r, g, b, a});
        }
    }
}

int Racing::ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const {
    const int adjustedLat = lateral + (roadDelta * d) / 4;
    const int numerator = (d >> 1) + 4 * (player.lateral - adjustedLat);
    return 160 - numerator / d;
}

// Street Rod 2's FUN_0024529a halves the slope term ((slope*d)>>1), so its hills rise half as far as Street Rod's.
// lift is the projected point's height above the 1900 camera: 100 for the road surface.
int Racing::ProjectY(int d, int elevDelta, int lift) const {
    if (state.GetAssets().GetGame() != SrGame::SR2) {
        return kHorizonY + lift * 4 / d + 4 * elevDelta;
    }
    const auto t = static_cast<int16_t>(((static_cast<int16_t>(elevDelta * d) >> 1) + lift + FallDrop()) * 4);
    return kHorizonY + t / d;
}

namespace {
// DATA_67 words from 0x2628b0, read in (x, y) pairs indexed by 15 - row. The far end of a bridge is shaped from
// three windows into it: +0 (0x2628b0), +2 (0x2628b4) and +14 (0x2628cc, x only).
constexpr int16_t kBridgeEndTable[] = {
    30510, 25344, 20, 10, 35, 15, 50, 30, 80, 60, 200, 100, 200, 100, 70, 0, 45, 0, 20, 0,
    -20, 50, -2000, 0, -2000, 0, 8, 6, 5, 4, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
// Girder thickness by row, the words at 0x2628e4 (row 0 nearest).
constexpr int kGirderWidth[15] = {8, 6, 5, 4, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0};
}  // namespace

// jdraw.c FUN_0023b498: lib.p 285 over the whole view, panned by heading and nudged up to 8 rows by the hill.
void Racing::RenderMulhollandBackdrop() {
    for (int y = kClipTop; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }
    Bitmap* bmp = Sr2Asset(SR2_MULHOLLAND_BACKGROUND);
    SDL_Surface* src = bmp ? bmp->GetSurface() : nullptr;
    if (!src) return;
    const srrace::CarState& p = viewPlayer;
    int e = p.elevAtCar - track.elev[ClampInt(static_cast<int>(p.segPos) - 4, 0, kTrackSegments - 1)];
    // Over the edge the -8 floor is lifted, so the picture rides up and pen 0x11 fills under it.
    e = sim.FallDirection() != 0 ? std::min(e, 8) : ClampInt(e, -8, 8);
    const int srcY = std::min(8 - e, src->h);
    const int h = std::min(src->h - srcY, 0x51);
    if (sim.FallDirection() != 0) {
        const SDL_Color cliff = Sr2Pen(0x11, {136, 85, 0, 255});
        for (int y = kClipTop + h; y <= kClipBottom; ++y) {
            for (int x = 0; x < GAME_WIDTH; ++x) PutPixel(pixels, x, y, cliff);
        }
    }
    const int off = (((p.heading * 2) % GAME_WIDTH) + GAME_WIDTH) % GAME_WIDTH;
    for (int y = 0; y < h; ++y) {
        const int py = kClipTop + y;
        if (py >= GAME_RACING_HEIGHT) break;
        for (int x = 0; x < GAME_WIDTH; ++x) {
            const int sx = (x + off) % GAME_WIDTH;
            if (sx >= src->w) continue;
            Uint32 sp = 0;
            const Uint8* ptr = static_cast<const Uint8*>(src->pixels) + (srcY + y) * src->pitch +
                               sx * src->format->BytesPerPixel;
            std::memcpy(&sp, ptr, src->format->BytesPerPixel);
            Uint8 r = 0, g = 0, b = 0, a = 255;
            SDL_GetRGBA(sp, src->format, &r, &g, &b, &a);
            if (a > 0) PutPixel(pixels, x, py, {r, g, b, 255});
        }
    }
}

// jdraw.c FUN_0023f604's canyon sides for row pair (r-1, r), pen 0x11. Right is the left mirrored about the screen.
void Racing::DrawMulhollandWalls(int r, bool farBridgeEnd) {
    const RowProjection& A = rows[r - 1];
    const RowProjection& B = rows[r];
    const RowProjection* C = r >= 2 ? &rows[r - 2] : nullptr;
    if ((track.flags[B.seg] & 0x0080) != 0) return;
    const bool nearBridge = (track.flags[A.seg] & 0x0080) != 0;
    const SDL_Color brown = Sr2Pen(0x11, {136, 85, 0, 255});
    const int bottom = kClipBottom;
    const int bend = viewPlayer.heading - track.curve[ClampInt(static_cast<int>(viewPlayer.segPos), 0,
                                                                kTrackSegments - 1)];
    const int k = 15 - r;
    const int tx = kBridgeEndTable[2 + 2 * k], ty = kBridgeEndTable[3 + 2 * k];
    const int ux = kBridgeEndTable[2 * k], uy = kBridgeEndTable[1 + 2 * k];
    const int vx = kBridgeEndTable[14 + 2 * k];

    for (int side = 0; side < 2; ++side) {
        // s = -1 grows toward the left screen edge, +1 toward the right.
        const int s = side ? 1 : -1;
        const int edge = side ? 0x13f : 0;
        const int aWall = side ? A.wallRY : A.wallLY;
        const int bWall = side ? B.wallRY : B.wallLY;
        const int aVerge = side ? A.vergeRX : A.vergeLX;
        const int bVerge = side ? B.vergeRX : B.vergeLX;
        const int aRoad = side ? A.rightX : A.leftX;
        if (side ? bend >= 100 : bend <= -100) continue;
        if (aWall == A.yRaw) continue;

        if (farBridgeEnd) {
            if (bWall > B.yRaw) continue;
            if (nearBridge && C) {
                const int cRoad = side ? C->rightX : C->leftX;
                const int xs[6] = {cRoad, cRoad, aRoad, aVerge, aVerge + s * tx, side ? 0x13f - vx : vx};
                const int ys[6] = {bottom, C->yRaw, A.yRaw, A.yRaw, A.yRaw, bottom};
                FillPolygon(xs, ys, 6, brown);
                if (bWall < B.yRaw) {
                    const int x3[3] = {aVerge, aVerge + s * (tx / 2), aVerge + s * tx};
                    const int y3[3] = {A.yRaw, A.yRaw - ty, A.yRaw};
                    FillPolygon(x3, y3, 3, brown);
                } else {
                    const int x4[4] = {aVerge, aVerge + s * tx, bVerge + s * tx, bVerge};
                    const int y4[4] = {A.yRaw, A.yRaw, B.yRaw, B.yRaw};
                    FillPolygon(x4, y4, 4, brown);
                }
            }
            if (bWall < B.yRaw) {
                const int x4[4] = {aVerge, aVerge + s * (tx / 2), bVerge + s * (ux / 2), bVerge};
                const int y4[4] = {A.yRaw, A.yRaw - ty, B.yRaw - uy, B.yRaw};
                FillPolygon(x4, y4, 4, brown);
            }
            continue;
        }

        if (r != 1 && nearBridge) {
            const int cRoad = side ? C->rightX : C->leftX;
            const int xs[6] = {edge, aVerge, aRoad, cRoad, cRoad, edge};
            const int ys[6] = {aWall, A.yRaw, A.yRaw, C->yRaw, bottom, bottom};
            FillPolygon(xs, ys, 6, brown);
        } else if (A.vergeLX + 0x1e >= A.vergeRX) {
            continue;
        }
        const int x4[4] = {edge, aVerge, bVerge, side ? 0x140 : 0};
        const int y4[4] = {aWall, A.yRaw, B.yRaw, bWall};
        FillPolygon(x4, y4, 4, brown);
        if (r != 1 && nearBridge) continue;

        // Where the wall ends nearer, close the ground beside the verge.
        const bool onScreen = side ? aVerge < 0x13f : aVerge > 0;
        const int cWall = C ? (side ? C->wallRY : C->wallLY) : 0;
        if (onScreen && ((C && cWall == C->yRaw) || (!C && aWall < A.yRaw))) {
            const int x3[3] = {edge, aVerge, edge};
            const int y3[3] = {aWall, A.yRaw, A.yRaw};
            FillPolygon(x3, y3, 3, brown);
        }
    }
}

// jdraw.c FUN_0023cd14 / FUN_0023cedc: the red girder bridge over a narrow crossing (flags 0x80), pen 0xf.
void Racing::DrawMulhollandBridge(int r) {
    const RowProjection& A = rows[r - 1];
    const RowProjection& B = rows[r];
    if ((track.flags[B.seg] & 0x0080) == 0) return;
    const SDL_Color red = Sr2Pen(0xf, {204, 34, 34, 255});
    const int w = kGirderWidth[r];
    auto rect = [&](int x0, int x1, int y0, int y1) {
        if (x0 > x1) std::swap(x0, x1);
        for (int y = std::max(y0, kClipTop); y <= std::min(y1, kClipBottom); ++y) {
            for (int x = std::max(x0, 0); x <= std::min(x1, GAME_WIDTH - 1); ++x) PutPixel(pixels, x, y, red);
        }
    };
    auto line = [&](int x0, int y0, int x1, int y1) { DrawAqueductLine({x0, y0, x1, y1}, red, kAqueductViewClip); };
    auto portal = [&](int lx, int rx, int top, int bot) {
        rect(lx - w, lx, top, bot);
        rect(rx, rx + w, top, bot);
        rect(lx + 1, rx - 1, top, top + w);
    };
    // The girder top stands 3 * 400/d above the road: y - 4 * (y - y1900).
    const int aTop = A.yRaw - 4 * (A.yRaw - A.yTop);
    const int bTop = B.yRaw - 4 * (B.yRaw - B.yTop);

    if ((track.flags[A.seg] & 0x0080) != 0) {
        portal(A.leftX, A.rightX, aTop, A.yRaw);
        const bool odd = ((viewPlayer.segPos + 14 - r) & 1) != 0;
        for (int k = w + 1; k != 0; --k) {
            if (odd && r < 7) {
                line(A.leftX - k, aTop, B.leftX - k, B.yRaw);
                line(A.rightX + k, aTop, B.rightX + k, B.yRaw);
            } else {
                line(A.leftX - k, A.yRaw, B.leftX - k, bTop);
                line(A.rightX + k, A.yRaw, B.rightX + k, bTop);
            }
            line(A.leftX - k, aTop, B.leftX - k, bTop);
            line(A.rightX + k, aTop, B.rightX + k, bTop);
            line(A.leftX - k, A.yRaw, B.leftX - k, B.yRaw);
            line(A.rightX + k, A.yRaw, B.rightX + k, B.yRaw);
        }
    }
    const int nextSeg = r + 1 < kForwardRows ? rows[r + 1].seg : ClampInt(B.seg + 1, 0, kTrackSegments - 1);
    if ((track.flags[nextSeg] & 0x0080) == 0) {
        portal(B.leftX, B.rightX, bTop, B.yRaw);
    }
}

// jdraw.c FUN_0023f604 0x240b56: where the ground drops away on the right (object bit 0x8000, not a bridge),
// a guard rail at the verge in pen 4: a post on every other segment and a rail band between the rows.
void Racing::DrawMulhollandRail(int r) {
    const RowProjection& A = rows[r - 1];
    const RowProjection& B = rows[r];
    if ((track.right[B.seg] & 0x8000) == 0 || (track.flags[B.seg] & 0x0080) != 0) return;
    const SDL_Color c = Sr2Pen(4, {136, 85, 0, 255});
    if (((railPhaseSeg - B.seg) & 1) != 0) {
        const int w = 30 / (r + 1);
        const int xs[4] = {A.vergeRX - w, A.vergeRX - w, A.vergeRX, A.vergeRX};
        const int ys[4] = {A.yMid - 1, A.yRaw, A.yRaw, A.yMid - 1};
        FillPolygon(xs, ys, 4, c);
    }
    const int xs[4] = {B.vergeRX, B.vergeRX, A.vergeRX, A.vergeRX};
    const int ys[4] = {B.yTop, B.yMid, A.yMid, A.yTop};
    FillPolygon(xs, ys, 4, c);
    DrawAqueductLine({B.vergeRX, B.yTop, A.vergeRX, A.yTop}, c, kAqueductViewClip);
    DrawAqueductLine({B.vergeRX, B.yMid, A.vergeRX, A.yMid}, c, kAqueductViewClip);
}

bool Racing::IsMulholland() const {
    return sim.Kind() == srrace::RaceKind::Mulholland && !aqueductPreview;
}

SDL_Color Racing::Sr2Pen(int pen, SDL_Color fallback) const {
    const std::vector<uint16_t> pal = state.GetAssets().GetAssetPalette(
        {LibAssetRegular, SrLibFileAmigaLibP, SR2_DRIVING_PALETTE_ASSET, SrGame::SR2});
    if (pen < 0 || pen >= static_cast<int>(pal.size())) return fallback;
    const uint16_t v = pal[pen];
    return SDL_Color{static_cast<Uint8>(((v >> 8) & 0xf) * 17), static_cast<Uint8>(((v >> 4) & 0xf) * 17),
                     static_cast<Uint8>((v & 0xf) * 17), 255};
}

// Even-odd scanline fill, clipped to the forward view, as the original's AreaDraw polygons are.
void Racing::FillPolygon(const int* xs, const int* ys, int n, SDL_Color c, const SDL_Rect* clip) {
    const SDL_Rect& r = clip ? *clip : kAqueductViewClip;
    int minY = ys[0], maxY = ys[0];
    for (int i = 1; i < n; ++i) {
        minY = std::min(minY, ys[i]);
        maxY = std::max(maxY, ys[i]);
    }
    minY = std::max(minY, r.y);
    maxY = std::min(maxY, r.y + r.h - 1);
    for (int y = minY; y <= maxY; ++y) {
        int cross[8];
        int nc = 0;
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const int y0 = ys[i], y1 = ys[j];
            if ((y0 <= y) == (y1 <= y)) continue;
            cross[nc++] = xs[i] + static_cast<int>(static_cast<int64_t>(xs[j] - xs[i]) * (y - y0) / (y1 - y0));
        }
        std::sort(cross, cross + nc);
        for (int i = 0; i + 1 < nc; i += 2) {
            for (int x = std::max(r.x, cross[i]); x <= std::min(r.x + r.w - 1, cross[i + 1]); ++x) {
                PutPixel(pixels, x, y, c);
            }
        }
    }
}

void Racing::UpdateRenderView() {
    viewPlayer = sim.Player();
    viewNpc = sim.Npc();
    if (fallStep > 0) {
        // FUN_00240d9a: fall frame k lowers cam elevation and cam_y by k and moves cam_x 100 toward the drop.
        viewPlayer.lateral = static_cast<int16_t>(viewPlayer.lateral + sim.FallDirection() * 100 * fallStep);
        viewPlayer.elevAtCar = static_cast<int16_t>(viewPlayer.elevAtCar - FallDrop());
        return;
    }
    if (flybyMode || !havePrevStep || !state.GetAssets().GetSmoothRacingRef()) return;
    const float t = std::clamp(fixedAccumulator / StepSeconds(), 0.0f, 1.0f);
    srrace::BlendCarView(prevPlayer, viewPlayer, t);
    srrace::BlendCarView(prevNpc, viewNpc, t);
}

int Racing::BuildRowTableAndGetLastVisible() {
    const srrace::CarState& p = viewPlayer;
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);
    const int elevAtCar = p.elevAtCar;
    // Mulholland is a canyon road: the verge is half the width.
    const bool mulholland = sim.Kind() == srrace::RaceKind::Mulholland;
    const bool grudge = sim.Kind() == srrace::RaceKind::Grudge;
    const int verge = mulholland ? 200 : 400;

    for (int k = 0; k < kForwardRows; ++k) {
        int d = 4;
        if (k > 0) {
            d = 12 + 8 * (k - 1) - p.scrollAcc;
            d = std::max(4, d);
        }
        // The horizontal projection reads curve[], the vertical elev[]. Do not swap them.
        const int seg = ClampInt(segBase + k + 1, 0, kTrackSegments - 1);
        // Mulholland clamps the bend of the nearest row only; FUN_00245536's row loop recomputes it unclamped.
        int roadDelta = p.heading - track.curve[seg];
        if (mulholland && k == 0) {
            roadDelta = ClampInt(roadDelta, -0x30, 0x30);
        }
        // A grudge race forces the elevation term to zero.
        const int rowElevDelta = grudge ? 0 : (elevAtCar - track.elev[seg]);
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        if (mulholland) {
            srrace::RoadEdgesSr2(track.flags[seg], true, edgeLeft, edgeRight);
        } else {
            srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);
        }

        RowProjection row;
        row.distance = d;
        row.seg = seg;
        row.roadDelta = roadDelta;
        row.leftX = ProjectX(p, d, roadDelta, edgeLeft);
        row.rightX = ProjectX(p, d, roadDelta, edgeRight);
        row.centerX = ProjectX(p, d, roadDelta, kRoadCenterLat);
        row.shoulderLX = ProjectX(p, d, roadDelta, edgeLeft - verge);
        row.shoulderRX = ProjectX(p, d, roadDelta, edgeRight + verge);
        row.yRaw = ProjectY(d, rowElevDelta);
        row.y = ClampInt(row.yRaw, kClipTop, kClipBottom + 30);
        if (mulholland) {
            row.yTop = ProjectY(d, rowElevDelta, 0);
            row.yMid = ProjectY(d, rowElevDelta, 50);
            row.wallLY = ProjectY(d, rowElevDelta, 100 - track.wallLeft[seg]);
            row.wallRY = ProjectY(d, rowElevDelta, 100 - track.wallRight[seg]);
            // The verge is measured from the full-width edges whatever the road's own width.
            row.vergeLX = ProjectX(p, d, roadDelta, 1500 - verge);
            row.vergeRX = ProjectX(p, d, roadDelta, 3300 + verge);
        }
        //   y - 4*(y - elevated_y), and elevated_y = y - 400/d, so y - 1600/d.
        row.archY = row.y - 1600 / d;
        rows[k] = row;
    }

    int horizonRow = 0;
    int minY = rows[0].y;
    for (int k = 1; k < kForwardRows; ++k) {
        if (rows[k].y < minY) {
            minY = rows[k].y;
            horizonRow = k;
        }
    }
    return horizonRow;
}

void Racing::RenderSky() {
    // The whole surface: near sprites can draw below kClipBottom and would ghost there.
    for (int y = 0; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }
}

// Drawn between the road's sky fill and the mountains, which would otherwise erase it. Clipped to the horizon.
// Half the mountains' pan rate, as in SR.EXE 2645:2982 (sky heading<<1, range heading<<2).
void Racing::RenderClouds(int horizonY) {
    if (!cloudSprite || !cloudSprite->GetSurface()) {
        return;
    }
    SDL_Surface* src = cloudSprite->GetSurface();
    const int baseOffset = ((viewPlayer.heading % src->w) + src->w) % src->w;
    const int yEnd = std::min({src->h, horizonY, static_cast<int>(GAME_RACING_HEIGHT)});
    for (int pass = 0; pass < 2; ++pass) {
        const int drawX = -baseOffset + pass * src->w;
        for (int y = 0; y < yEnd; ++y) {
            for (int x = 0; x < src->w; ++x) {
                const int px = drawX + x;
                if (px < 0 || px >= GAME_WIDTH) {
                    continue;
                }
                Uint32 srcPixel = 0;
                const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 255;
                SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
                if (a > 0) {
                    PutPixel(pixels, px, y, {r, g, b, a});
                }
            }
        }
    }
}

// Drawn after the road fill, base on the horizon. Pans with heading, never moves vertically.
void Racing::RenderMountains(int horizonY) {
    if (!mountainSprite || !mountainSprite->GetSurface()) {
        RenderClouds(horizonY);   // no range loaded, but the sky still gets its stars
        return;
    }
    SDL_Surface* src = mountainSprite->GetSurface();
    // A direct function of heading, not an integral of speed, so it returns as the heading returns.
    const int heading = viewPlayer.heading;
    const int baseOffset = (((heading * 2) % src->w) + src->w) % src->w;

    // The silhouette/foothill boundary is the horizon; anchoring the lowest opaque row left a green gap.
    if (mountainBaseRow < 0) {
        auto rowKind = [&](int y) {  // 0 = transparent, 1 = all foothill green, 2 = silhouette
            bool opaque = false;
            for (int x = 0; x < src->w; ++x) {
                Uint32 sp = 0;
                const Uint8* p = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&sp, p, src->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, src->format, &r, &g, &b, &a);
                if (a == 0) { continue; }
                opaque = true;
                if (r != kGrassA.r || g != kGrassA.g || b != kGrassA.b) { return 2; }
            }
            return opaque ? 1 : 0;
        };
        mountainBaseRow = src->h - 1;
        for (int y = src->h - 1; y >= 0; --y) {
            const int kind = rowKind(y);
            if (kind == 0 || kind == 1) { continue; }  // padding / foothill band
            mountainBaseRow = y;
            break;
        }
    }

    // The silhouette's base sits just above the horizon; the foothill band below is clipped away.
    const int startY = horizonY - 1 - mountainBaseRow;

    // So nothing shows through the gaps between the peaks.
    const int bandTop = std::max(kClipTop, startY);
    for (int y = bandTop; y < horizonY && y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }

    // After every sky fill and before the range, so the peaks stand in front.
    RenderClouds(horizonY);

    for (int pass = 0; pass < 2; ++pass) {
        const int drawX = -baseOffset + pass * src->w;
        for (int y = 0; y < src->h; ++y) {
            const int py = startY + y;
            if (py < kClipTop || py >= horizonY || py >= GAME_RACING_HEIGHT) {
                continue;
            }
            for (int x = 0; x < src->w; ++x) {
                const int px = drawX + x;
                if (px < 0 || px >= GAME_WIDTH) {
                    continue;
                }
                Uint32 srcPixel = 0;
                const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 255;
                SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
                if (a > 0) {
                    PutPixel(pixels, px, py, {r, g, b, a});
                }
            }
        }
    }
}

// Street Rod 2's light art (jlights.c). The hanging light is two 48x82 pieces; the drag tree is three sizes across
// one 184x87 sheet, with lit bulbs in a reserve column at x 128.
namespace {
constexpr int kSr2LightW = 48;
constexpr int kSr2LightH = 82;
constexpr int kSr2LightTravel = 88;

constexpr int kTreeLods = 3;
constexpr int kTreeX[kTreeLods] = {0, 32, 72};
constexpr int kTreeW[kTreeLods] = {32, 40, 56};
constexpr int kTreeH[kTreeLods] = {58, 69, 87};
constexpr int kBulbSrcX = 128;
constexpr int kBulbColW[2][kTreeLods] = {{16, 24, 24}, {16, 18, 24}};
constexpr int kBulbH[3][kTreeLods] = {{7, 8, 11}, {4, 5, 7}, {7, 8, 11}};
constexpr int kBulbSrcY[3][kTreeLods] = {{0, 9, 18}, {66, 72, 78}, {33, 42, 51}};
constexpr int kBulbDstY[3][kTreeLods] = {{6, 7, 9}, {22, 25, 31}, {34, 41, 51}};
constexpr int kTreeLodForDistance[5] = {2, 2, 1, 1, 0};
}  // namespace

int Racing::StopLightLamp(int column) const {
    const bool green = sim.Phase() != srrace::RacePhase::Countdown;
    const bool amber = !green && sim.CountdownTicks() <= 40;
    if (column == 0) {
        return (green || amber) ? -1 : 0;
    }
    if (green) {
        return 2;
    }
    return amber ? 1 : -1;
}

// jlights.c DrawStopLights (0x244d58): mid-road at the fourth segment past the start, only on the nearest five rows.
void Racing::DrawStopLights(int k) {
    if (k < 0 || k >= kForwardRows || k > 4) {
        return;
    }
    Bitmap* sheet = Sr2Asset(SR2_DRAG_TREE);
    if (!sheet) {
        return;
    }
    const int lod = kTreeLodForDistance[k];
    const RowProjection& row = rows[k];
    const int w = kTreeW[lod];
    const int h = kTreeH[lod];
    const int x = (row.leftX + row.rightX) / 2 - w / 2;
    DrawSprite(sheet, x, row.y, false, true, kTreeX[lod], 0, w, h);

    const int top = row.y - h;
    for (int col = 0; col < 2; ++col) {
        const int lamp = StopLightLamp(col);
        if (lamp < 0) {
            continue;
        }
        const int cw = kBulbColW[col][lod];
        const int dx = col ? kBulbColW[0][lod] : 0;
        const int bh = kBulbH[lamp][lod];
        DrawSprite(sheet, x + dx, top + kBulbDstY[lamp][lod], false, false,
                   kBulbSrcX + dx, kBulbSrcY[lamp][lod], cw, bh);
    }
}

// jlights.c 0x244a48: a light and a clean plate, 48x82 each, sliding left together after launch.
void Racing::DrawSr2StartLight() {
    if (cruiseMode) {
        return;
    }
    Bitmap* light = Sr2Asset(SR2_FLAG_GIRL_HAND);
    Bitmap* plate = Sr2Asset(SR2_FLAG_GIRL_TORSO);
    if (!light || !plate) {
        return;
    }
    // While the light is still up, where the starter stands: the line, or just past the player if the opponent got
    // away first (A4+0x22bc). The mirrors show her from there once the light has gone.
    if (sr2LightSlide <= kSr2LightTravel) {
        const int pl = static_cast<int>(sim.Player().segPos);
        const int np = static_cast<int>(sim.Npc().segPos);
        sr2StarterSeg = np > pl ? pl + 1 : np;
    }
    const srrace::RacePhase phase = sim.Phase();
    if (phase == srrace::RacePhase::Ended || sr2LightSlide > kSr2LightTravel) {
        return;
    }
    if (phase != srrace::RacePhase::Countdown && sim.Player().speed > 0) {
        sr2LightSlide += 8;
        if (sr2LightSlide > kSr2LightTravel) {
            return;
        }
    }
    const int slide = sr2LightSlide;
    const int dstY = 18 - kViewOffsetY;
    const int plateW = kSr2LightW - slide;
    int lightX = 8;
    int lightW = kSr2LightW;
    int lightSrcX = 0;
    if (plateW > 0) {
        DrawSprite(plate, 8, dstY, false, false, slide, 0, plateW, kSr2LightH);
        lightX = 8 + plateW;
    } else {
        lightW = 2 * kSr2LightW - slide;
        lightSrcX = slide - kSr2LightW;
    }
    if (lightW <= 0) {
        return;
    }
    // Frame 0 red, 1 amber, 2 green.
    const int lamp = phase == srrace::RacePhase::Countdown
                         ? (sim.CountdownTicks() <= 40 ? 1 : 0)
                         : 2;
    DrawSprite(light, lightX, dstY, false, false, lightSrcX + lamp * kSr2LightW, 0, lightW, kSr2LightH);
}

// jlights.c FUN_00244b42 (road mirror) / FUN_00244c12 (Aqueduct mirror): the starter, left behind at the line,
// shrinking in the mirror once the hanging light has slid away. `r` is the mirror row, 0 nearest; row 6 is skipped
// in the original.
void Racing::DrawStarterInMirror(int r, int x, int y, bool aqueduct, const SDL_Rect& clip) {
    // A cruise has no start, so nobody is left at the line.
    if (cruiseMode || sr2LightSlide <= kSr2LightTravel || r == 6 || r < 0) {
        return;
    }
    const int idx = std::min(r < 3 ? r >> 1 : r - 1, 3);
    Bitmap* frame = Sr2Asset(SR2_FLAG_GIRL_BEHIND_1 + idx);
    SDL_Surface* s = frame ? frame->GetSurface() : nullptr;
    if (!s) {
        return;
    }
    // ponytail: half the width; FUN_002233f4 also trims padding the chunky conversion already dropped.
    const int left = x - s->w / 2;
    int top = y - s->h;
    if (!aqueduct) {
        // Feet three above the row, which with any row below the top of the screen pins them at Amiga y 40.
        top = (y + kViewOffsetY > 0) ? 0x2b - 3 - s->h - kViewOffsetY : y - s->h - 3;
    }
    const int x0 = std::max(left, clip.x);
    const int y0 = std::max(top, clip.y);
    const int x1 = std::min(left + s->w - 1, clip.x + clip.w - 1);
    const int y1 = std::min(top + s->h - 1, clip.y + clip.h - 1);
    if (x0 > x1 || y0 > y1) {
        return;
    }
    DrawSprite(frame, x0, y0, false, false, x0 - left, y0 - top, x1 - x0 + 1, y1 - y0 + 1);
}

// lib.p asset 97, two 24x32 frames: red during staging, green at launch, then retracts 4 a step as the car rolls.
void Racing::DrawStartLight() {
    if (flybyMode) {
        return;
    }
    // A grudge race starts from the drag tree.
    if (sim.Kind() == srrace::RaceKind::Grudge) {
        return;
    }
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        DrawSr2StartLight();
        return;
    }
    if (!startLightSprite || !startLightSprite->GetSurface()) {
        return;
    }
    const srrace::RacePhase phase = sim.Phase();
    if (phase == srrace::RacePhase::Ended || startLightHeight <= 0) {
        return;
    }

    const bool green = (phase != srrace::RacePhase::Countdown);

    const SpriteRect& frame = RACING_LIGHT_FRAMES[green ? 1 : 0];
    SDL_Surface* src = startLightSprite->GetSurface();
    const int dstX = 130;               // original dest (0x82, 0x12)
    // 18 in the full frame, converted to this surface.
    const int dstY = 18 - kViewOffsetY;
    const int srcYoff = 32 - startLightHeight;  // retract from the bottom (srcY = 32 - h)
    for (int y = 0; y < startLightHeight; ++y) {
        const int sy = frame.y + srcYoff + y;
        const int py = dstY + y;
        if (py < 0 || py >= GAME_RACING_HEIGHT) {
            continue;
        }
        for (int x = 0; x < frame.w; ++x) {
            const int sx = frame.x + x;
            const int px = dstX + x;
            if (px < 0 || px >= GAME_WIDTH) {
                continue;
            }
            Uint32 srcPixel = 0;
            const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + sy * src->pitch + sx * src->format->BytesPerPixel;
            std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
            if (a > 0) {
                PutPixel(pixels, px, py, {r, g, b, a});
            }
        }
    }
}

// The Aqueduct concrete.
void Racing::RenderAqueduct() {
    static const SDL_Color kSkyFill = {0, 0, 0, 255};

    const srrace::CarState& p = sim.Player();
    const int roadPos = ClampInt(static_cast<int>(p.segPos), 0,
                                 srrace::AQUEDUCT_SEGMENTS - 1);
    // The sim has no sub-segment counter of the Aqueduct's own, so the scroll
    // accumulator stands in: 0..7 spread over the channel's 0..0x3f.
    const int subPos = ClampInt(p.scrollAcc * 8, 0, 0x3f);
    const int camX = p.lateral;

    // Riding a wall banks the view. The ride is read from the same camera the
    // rows are projected from, so the tilt matches what the car is doing.
    const srrace::AqueductWallRide ride = srrace::AqueductRideWall(camX);
    // A rollover spins the channel: the roll accumulates into the bank every frame.
    const int bankAngle = srrace::AqueductBankAngle(ride.bank, sim.AqueductRoll());

    // The channel has no curve array; what bends it is the car's heading, the
    // same term the road projection subtracts its curve from.
    srrace::AqueductBuildFrame(aqueductFrame, camX, aqueductCameraHeight,
                               roadPos, subPos, p.heading, sim.AqueductVjerk());

    // The projections return Amiga SCREEN y.
    for (srrace::AqueductRow& r : aqueductFrame.viewRows) {
        r.y -= kViewOffsetY;
        r.y2 -= kViewOffsetY;
    }
    for (srrace::AqueductRow& r : aqueductFrame.mirrorRows) {
        r.y -= kViewOffsetY;
        r.y2 -= kViewOffsetY;
    }

    for (int y = kClipTop; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyFill);
        }
    }

    // Only the view.
    {
        const srrace::AqueductBank skyBank = srrace::AqueductBankForRows(
            aqueductFrame.viewRows, srrace::AQU_VIEW_ROWS, bankAngle);
        srrace::AqueductStar stars[srrace::AQU_STARS_TOTAL];
        // The backdrop pans with the heading and wraps at the screen edge; the
        // stars take half that offset, the horizon sprites all of it.
        const int skyBase = srrace::AqueductSkyBase(p.heading);
        const int n = srrace::AqueductBuildSky(skyBase / 2, skyBank, bankAngle, stars,
                                               static_cast<int>(std::size(stars)));
        for (int i = 0; i < n; ++i) {
            const int y = stars[i].y - kViewOffsetY;
            if (y < kClipTop || y > kClipBottom) continue;
            if (stars[i].x < 0 || stars[i].x >= GAME_WIDTH) continue;
            PutPixel(pixels, stars[i].x, y, AqueductPen(stars[i].color, {170, 170, 170, 255}));
        }

        // The three fixed sprites sit on the horizon with the stars.
        srrace::AqueductObstacle horizon[srrace::AQU_HORIZON_SPRITES];
        const int hn = srrace::AqueductBuildHorizon(skyBase, skyBank, bankAngle,
                                                    horizon,
                                                    srrace::AQU_HORIZON_SPRITES);
        for (int i = 0; i < hn; ++i) {
            srrace::AqueductObstacle h = horizon[i];
            h.y -= kViewOffsetY;
            DrawAqueductShape(h, kAqueductViewClip);
        }
    }

    aqueductBankAngle = bankAngle;
    aqueductConcrete = AqueductPen(srrace::AQU_PEN_CONCRETE, {153, 153, 153, 255});
    // Everything drawn after the channel - the opponent above all - turns with it.
    aqueductBank = DrawAqueductPass(aqueductFrame.viewRows, srrace::AQU_VIEW_OBJECT_ROWS, aqueductTrack.flags,
                                    aqueductFrame.viewFlagIndex, kAqueductViewClip, true);
}

// A Street Rod 2 driving-palette pen.
SDL_Color Racing::AqueductPen(int idx, SDL_Color fallback) const {
    const std::vector<uint16_t> pal = state.GetAssets().GetAssetPalette(
        {LibAssetRegular, SrLibFileAmigaLibP, SR2_DRIVING_PALETTE_ASSET, SrGame::SR2});
    if (idx < 0 || idx >= static_cast<int>(pal.size())) return fallback;
    const uint16_t v = pal[idx];
    return SDL_Color{static_cast<Uint8>(((v >> 8) & 0xf) * 17), static_cast<Uint8>(((v >> 4) & 0xf) * 17),
                     static_cast<Uint8>((v & 0xf) * 17), 255};
}

// jdraw_a.c FUN_00242bea, which both the view (14 object rows) and the mirror (8) run: the concrete, its line
// work, the floor band, the slope patches, the obstacle sprites, the midline, then the blocks. Returns the bank the
// rows were turned with.
srrace::AqueductBank Racing::DrawAqueductPass(const srrace::AqueductRow* rows, int objectRows,
                                              const uint16_t* flags, int flagBase, const SDL_Rect& clip,
                                              bool hitTest) {
    const int bankAngle = aqueductBankAngle;
    const srrace::AqueductBank bank = srrace::AqueductBankForRows(rows, objectRows + 1, bankAngle);
    const int clipRight = clip.x + clip.w - 1;
    const int clipBottom = clip.y + clip.h - 1;
    auto fill = [&](const srrace::AqueductQuad& q, SDL_Color c) {
        srrace::AqueductSpan spans[GAME_RACING_HEIGHT];
        const int m = srrace::AqueductScanQuad(q, clip.y, clipBottom, GAME_WIDTH, spans,
                                               static_cast<int>(std::size(spans)));
        for (int k = 0; k < m; ++k) {
            for (int x = std::max(spans[k].x0, clip.x); x <= std::min(spans[k].x1, clipRight); ++x) {
                PutPixel(pixels, x, spans[k].y, c);
            }
        }
    };

    // All three surfaces are the one grey.
    {
        const SDL_Color concrete = AqueductPen(srrace::AQU_PEN_CONCRETE, {153, 153, 153, 255});
        srrace::AqueductQuad quads[(srrace::AQU_VIEW_ROWS - 1) * srrace::AqueductSurfaceCount];
        const int n = srrace::AqueductBuildQuads(rows, objectRows + 1, bank, quads, static_cast<int>(std::size(quads)));
        for (int i = n - 1; i >= 0; --i) fill(quads[i], concrete);
    }

    // The floor/slope junctions and the concrete joints in black, the slope tops in pen 4.
    {
        const SDL_Color black = AqueductPen(srrace::AQU_PEN_EDGE, {0, 0, 0, 255});
        const SDL_Color slopeTop = AqueductPen(srrace::AQU_PEN_SLOPE_TOP, {136, 85, 0, 255});
        srrace::AqueductLine lines[4 + srrace::AQU_VIEW_ROWS * 3];
        const int nl = srrace::AqueductBuildLines(rows, objectRows, flags, flagBase, bank, bankAngle, lines,
                                                  static_cast<int>(std::size(lines)));
        for (int i = 0; i < nl; ++i) {
            DrawAqueductLine(lines[i], lines[i].pen == srrace::AQU_PEN_EDGE ? black : slopeTop, clip);
        }
    }

    // The finish marker across the floor.
    {
        srrace::AqueductQuad bands[4];
        const int nb = srrace::AqueductBuildFloorBands(rows, objectRows, flags, flagBase, bank, bankAngle, bands,
                                                       static_cast<int>(std::size(bands)));
        const SDL_Color bandColor = AqueductPen(srrace::AQU_PEN_BAND, {136, 85, 0, 255});
        for (int i = 0; i < nb; ++i) fill(bands[i], bandColor);
    }

    // The patches on the slopes - solid concrete you crash into.
    {
        srrace::AqueductQuad patches[srrace::AQU_VIEW_OBJECT_ROWS];
        const int np = srrace::AqueductBuildSlopeObstacles(rows, objectRows, flags, flagBase, bank, bankAngle,
                                                           patches, static_cast<int>(std::size(patches)));
        const SDL_Color patchColor = AqueductPen(srrace::AQU_PEN_SLOPE_OBSTACLE, {136, 85, 0, 255});
        for (int i = 0; i < np; ++i) fill(patches[i], patchColor);
    }

    // The obstacle sprites against the walls.
    {
        srrace::AqueductObstacle obstacles[srrace::AQU_VIEW_OBJECT_ROWS * 2];
        const int no = srrace::AqueductBuildObstacles(rows, objectRows, flags, flagBase, bank, bankAngle, obstacles,
                                                      static_cast<int>(std::size(obstacles)));
        for (int i = 0; i < no; ++i) DrawAqueductShape(obstacles[i], clip);
    }

    // The long dashes down the middle.
    {
        srrace::AqueductQuad mid[2];
        const int nm = srrace::AqueductBuildMidline(rows, objectRows, flags, flagBase, bank, mid);
        const SDL_Color midColor = AqueductPen(srrace::AQU_PEN_MIDLINE, {136, 85, 0, 255});
        for (int i = 0; i < nm; ++i) fill(mid[i], midColor);
    }

    // The blocks in the channel.
    {
        const SDL_Color side = AqueductPen(srrace::AQU_PEN_BLOCK_SIDE, {119, 119, 119, 255});
        const SDL_Color top = AqueductPen(srrace::AQU_PEN_BLOCK_TOP, {170, 170, 170, 255});
        srrace::AqueductBlock blocks[srrace::AQU_VIEW_OBJECT_ROWS * 2];
        const int nb = srrace::AqueductBuildBlocks(rows, objectRows, flags, flagBase, blocks,
                                                   static_cast<int>(std::size(blocks)));
        bool hit = false;
        for (int i = 0; i < nb; ++i) {
            srrace::AqueductFace faces[4];
            const int nf = srrace::AqueductBlockFaces(blocks[i], bank, faces);
            for (int fi = 0; fi < nf; ++fi) fill(faces[fi].quad, faces[fi].pen == srrace::AQU_PEN_BLOCK_TOP ? top : side);
            srrace::AqueductQuad hq;
            if (hitTest && srrace::AqueductBlockHitQuad(blocks[i], bank, hq) &&
                srrace::AqueductQuadHitsCar(hq, clip.y, clipBottom, GAME_WIDTH)) {
                hit = true;
            }
        }
        // Only the view's pass feeds the hit flag; jroad.c FUN_00247154 crashes on it next frame.
        if (hitTest) sim.SetAqueductBlockHit(hit);
    }
    return bank;
}

// One line of the channel's edge work: Bresenham, clipped to the viewport.
void Racing::DrawAqueductLine(const srrace::AqueductLine& line, SDL_Color c, const SDL_Rect& clip) {
    int x0 = line.x0, y0 = line.y0, x1 = line.x1, y1 = line.y1;
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    // The extrapolated ends run well off screen, so bound the walk rather
    // than trusting the endpoints to be reachable.
    const int guard = GAME_WIDTH + GAME_RACING_HEIGHT * 4;
    for (int step = 0; step < guard; ++step) {
        if (x0 >= clip.x && x0 < clip.x + clip.w && y0 >= clip.y && y0 < clip.y + clip.h) {
            PutPixel(pixels, x0, y0, c);
        }
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// One aqushape record, centred on its point. aqushape.c centres both ways -
// the roadside sprites are bottom aligned - so this cannot share their blit.
void Racing::DrawAqueductShape(const srrace::AqueductObstacle& o, const SDL_Rect& clip) {
    const srrace::AqueductShapeRect& rect =
        srrace::AqueductShapeRectFor(o.shape, o.phase);
    if (rect.w == 0 || rect.h == 0) {
        return;
    }
    Bitmap* sheet = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP,
         srrace::AqueductShapeAsset(o.shape), SrGame::SR2});
    SDL_Surface* surf = sheet ? sheet->GetSurface() : nullptr;
    if (!surf) {
        return;
    }
    const int dstLeft = o.x - rect.w / 2;
    const int dstTop = o.y - rect.h / 2;
    for (int yy = 0; yy < rect.h; ++yy) {
        const int py = dstTop + yy;
        if (py < clip.y || py >= clip.y + clip.h) continue;
        if (rect.srcY + yy >= surf->h) break;
        for (int xx = 0; xx < rect.w; ++xx) {
            const int px = dstLeft + xx;
            if (px < clip.x || px >= clip.x + clip.w) continue;
            if (rect.srcX + xx >= surf->w) break;
            Uint32 sp = 0;
            const Uint8* ptr = static_cast<const Uint8*>(surf->pixels) +
                               (rect.srcY + yy) * surf->pitch +
                               (rect.srcX + xx) * surf->format->BytesPerPixel;
            std::memcpy(&sp, ptr, surf->format->BytesPerPixel);
            Uint8 r = 0, g = 0, b = 0, a = 255;
            SDL_GetRGBA(sp, surf->format, &r, &g, &b, &a);
            if (a > 0) PutPixel(pixels, px, py, {r, g, b, a});
        }
    }
}

// Bit 4 picks the other colour, and which is which depends on the game.
SDL_Color Racing::DashColorFor(uint16_t flags) const {
    const bool marked = (flags & 0x0010) != 0;
    // A grudge race is Street Rod 2 content whichever game booted it, so it
    // takes Street Rod 2's colours even when started from Street Rod.
    if (state.GetAssets().GetGame() == SrGame::SR2 ||
        sim.Kind() == srrace::RaceKind::Grudge) {
        return marked ? kDashYellow : kDashWhite;
    }
    return marked ? kDashWhite : kDashYellow;
}

void Racing::RenderRoad(int lastVisible, int onlyBand) {
    auto drawDash = [&](int y, int cx, int drawL, int drawR, uint16_t flags) {
        if ((flags & 0x0008) == 0) {
            return;
        }
        const SDL_Color dashColor = DashColorFor(flags);
        for (int i = 0; i < 4; ++i) {
            const int px = cx - 1 + i;
            if (px >= drawL && px <= drawR) {
                PutPixel(pixels, px, y, dashColor);
            }
        }
    };

    // Mulholland's backdrop picture is the whole view, ground included. Its road is drawn a band at a time from
    // the row loop (onlyBand), between each row's walls and its sprites, as FUN_0023f604 does.
    const bool mulholland = IsMulholland();
    if (onlyBand < 0) {
        if (mulholland) {
            mulhollandFarBridgeEnd = false;
            for (int r = kForwardRows - 1; r >= 10; --r) {
                if ((track.flags[rows[r].seg] & 0x0080) == 0 && (track.flags[rows[r - 1].seg] & 0x0080) != 0) {
                    mulhollandFarBridgeEnd = true;
                }
            }
            RenderMulhollandBackdrop();
            return;
        }
        // The whole surface: the bottom rows show through the side-mirror gap.
        for (int y = kClipTop; y < GAME_RACING_HEIGHT; ++y) {
            for (int x = 0; x < GAME_WIDTH; ++x) {
                PutPixel(pixels, x, y, kGrassA);
            }
        }
    }
    const SDL_Color vergeColor = Sr2Pen(4, {136, 85, 0, 255});
    const SDL_Color grassColor = Sr2Pen(0xe, kGrassA);
    // jdraw.c FUN_0023b97c for band r = k + 1: the verge in pen 4 except on a bridge, then grass in pen 0xe out to
    // the screen edge on a side whose row r-1 has no wall. A bridge ending in rows 10..14 trims the grass.
    auto drawVerge = [&](int y, int vl, int lx, int rx, int vr, uint16_t flags, int k) {
        if (!mulholland) return;
        if ((flags & 0x0080) == 0) {
            for (int x = std::max(0, vl); x <= std::min(GAME_WIDTH - 1, lx); ++x) PutPixel(pixels, x, y, vergeColor);
            for (int x = std::max(0, rx); x <= std::min(GAME_WIDTH - 1, vr); ++x) PutPixel(pixels, x, y, vergeColor);
        }
        const RowProjection& a = rows[k];
        const int r = k + 1;
        const int trim = (mulhollandFarBridgeEnd && r >= 10) ? kBridgeEndTable[2 + 2 * (16 - r)] : -1;
        if (a.wallLY == a.yRaw) {
            for (int x = std::max(0, trim < 0 ? 0 : lx - trim); x <= std::min(GAME_WIDTH - 1, lx); ++x) {
                PutPixel(pixels, x, y, grassColor);
            }
        }
        if (a.wallRY == a.yRaw) {
            for (int x = std::max(0, rx); x <= std::min(GAME_WIDTH - 1, trim < 0 ? GAME_WIDTH - 1 : rx + trim); ++x) {
                PutPixel(pixels, x, y, grassColor);
            }
        }
    };

    for (int k = 0; k < lastVisible; ++k) {
        if (onlyBand >= 0 && k != onlyBand) {
            continue;
        }
        const RowProjection& nearRow = rows[k];
        const RowProjection& farRow = rows[k + 1];
        if (nearRow.y <= farRow.y) {
            continue;
        }

        // Extended to the bottom so the road shows through the side-mirror gap.
        const int yStart = ClampInt(nearRow.y - 1, kClipTop, GAME_RACING_HEIGHT - 1);
        const int yEnd = ClampInt(farRow.y, kClipTop, kClipBottom);
        for (int y = yStart; y >= yEnd; --y) {
            const int dy = std::max(1, nearRow.y - farRow.y);
            const int fromNear = nearRow.y - y;
            const int lx = nearRow.leftX + ((farRow.leftX - nearRow.leftX) * fromNear) / dy;
            const int rx = nearRow.rightX + ((farRow.rightX - nearRow.rightX) * fromNear) / dy;
            const int cx = nearRow.centerX + ((farRow.centerX - nearRow.centerX) * fromNear) / dy;
            const int left = ClampInt(std::min(lx, rx), -200, 520);
            const int right = ClampInt(std::max(lx, rx), -200, 520);
            // fromNear counts down from the near row.
            const int seg = (fromNear * 2 < dy) ? nearRow.seg : farRow.seg;
            const uint16_t flags = track.flags[seg];
            if (mulholland) {
                const int vl = nearRow.vergeLX + ((farRow.vergeLX - nearRow.vergeLX) * fromNear) / dy;
                const int vr = nearRow.vergeRX + ((farRow.vergeRX - nearRow.vergeRX) * fromNear) / dy;
                drawVerge(y, vl, left, right, vr, flags, k);
            }
            if (right < 0 || left >= GAME_WIDTH) {
                continue;
            }
            const bool city = (flags & 0x0001) != 0;
            // The road is black everywhere, town included (playtested).
            const SDL_Color roadColor = kRoadA;
            const int drawL = std::max(0, left);
            const int drawR = std::min(GAME_WIDTH - 1, right);
            for (int x = drawL; x <= drawR; ++x) {
                PutPixel(pixels, x, y, roadColor);
            }

            if (city) {
                for (int i = 0; i < 2; ++i) {
                    const int curbL = drawL + i;
                    const int curbR = drawR - i;
                    if (curbL >= 0 && curbL < GAME_WIDTH) {
                        PutPixel(pixels, curbL, y, kCurbGray);
                    }
                    if (curbR >= 0 && curbR < GAME_WIDTH) {
                        PutPixel(pixels, curbR, y, kCurbGray);
                    }
                }
            }

            // The finish line.
            if ((flags & 0x0010) != 0) {
                const SDL_Color lineColor = DashColorFor(flags);
                for (int x = drawL; x <= drawR; ++x) {
                    PutPixel(pixels, x, y, lineColor);
                }
            } else {
                drawDash(y, cx, drawL, drawR, flags);
            }
        }
    }

    if (lastVisible >= 1 && onlyBand <= 0) {
        const RowProjection& row0 = rows[0];
        const RowProjection& row1 = rows[1];
        const int yStart = ClampInt(row0.y, kClipTop, GAME_RACING_HEIGHT - 1);
        const int dy = std::max(1, row0.y - row1.y);
        const uint16_t nearFlags = track.flags[row0.seg];
        const SDL_Color nearRoadColor = kRoadA;
        for (int y = yStart; y < GAME_RACING_HEIGHT; ++y) {
            const int extra = y - row0.y;
            const int lx = row0.leftX + ((row0.leftX - row1.leftX) * extra) / dy;
            const int rx = row0.rightX + ((row0.rightX - row1.rightX) * extra) / dy;
            const int cx = row0.centerX + ((row0.centerX - row1.centerX) * extra) / dy;
            const int left = std::max(0, std::min(lx, rx));
            const int right = std::min(GAME_WIDTH - 1, std::max(lx, rx));
            if (mulholland) {
                const int vl = row0.vergeLX + ((row0.vergeLX - row1.vergeLX) * extra) / dy;
                const int vr = row0.vergeRX + ((row0.vergeRX - row1.vergeRX) * extra) / dy;
                drawVerge(y, vl, std::min(lx, rx), std::max(lx, rx), vr, nearFlags, 0);
            }
            for (int x = left; x <= right; ++x) {
                PutPixel(pixels, x, y, nearRoadColor);
            }
            if ((nearFlags & 0x0010) != 0) {
                const SDL_Color lineColor = DashColorFor(nearFlags);
                for (int x = left; x <= right; ++x) {
                    PutPixel(pixels, x, y, lineColor);
                }
            } else {
                drawDash(y, cx, left, right, nearFlags);
            }
        }
    }

    if (mulholland) {
        return;   // the backdrop already holds the sky
    }
    // Everything above the road's vanishing row is sky: the range stands on it.
    const int skyEnd = ClampInt(rows[lastVisible].y, kClipTop, kClipBottom);
    for (int y = kClipTop; y < skyEnd; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }
}

void Racing::DrawOverheadArch(const RowProjection& row, uint16_t flags, int sr) {
    const int left = row.leftX;
    const int right = row.rightX;
    if (right - left < 4) {
        return;
    }

    const int yTop = ClampInt(row.archY, 0, GAME_RACING_HEIGHT - 1);
    const int yBottom = ClampInt(row.y, 0, GAME_RACING_HEIGHT - 1);
    if (yBottom <= yTop) {
        return;
    }

    for (int y = yTop; y <= yBottom; ++y) {
        PutPixel(pixels, left, y, kArchColor);
        PutPixel(pixels, left + 1, y, kArchColor);
        PutPixel(pixels, right, y, kArchColor);
        PutPixel(pixels, right - 1, y, kArchColor);
    }

    const int barHeight = ClampInt((yBottom - yTop) / 20, 2, 8);
    for (int y = yTop; y < std::min(GAME_RACING_HEIGHT, yTop + barHeight); ++y) {
        for (int x = std::max(0, left); x <= std::min(GAME_WIDTH - 1, right); ++x) {
            PutPixel(pixels, x, y, kArchColor);
        }
    }

    int bannerWidth = ((right - left) * 3) / 4;
    if (bannerWidth < 8) {
        return;
    }

    Bitmap* banner = nullptr;
    uint16_t bannerId = 0;
    if (flags & 0x8000) {
        banner = signRoute69Sprite.get();
        bannerId = RACING_SIGN_ROUTE_69;
    } else if (flags & 0x4000) {
        banner = signExit48Sprite.get();
        bannerId = RACING_SIGN_EXIT_48;
    } else if (flags & 0x2000 || flags & 0x1000) {
        banner = signCountyLineSprite.get();
        bannerId = RACING_SIGN_COUNTY_LINE;
    }
    if (!banner || !banner->GetSurface()) {
        return;
    }

    const int centerX = (left + right) / 2;

    // Composite sheets of pre-drawn scale frames, stepped through by distance at native size.
    const SpriteAnimationInfo* anim = GetSpriteAnimationInfo(bannerId);
    if (anim && anim->frames && anim->frameCount > 0) {
        const SpriteRect& fr = anim->frames[SelectFrameIndexForDistance(anim, sr)];
        DrawSprite(banner, centerX - fr.w / 2, yTop + 1, false, false, fr.x, fr.y, fr.w, fr.h);
    }
}

// The town, drawn only during a cruise: per row and side, a wall quad on the shoulder line, topped at the
// elevated row.
void Racing::DrawTownWalls(int lastVisible) {
    if (!cruiseMode) {
        return;
    }

    auto quad = [&](int xA, int topA, int botA, int xB, int topB, int botB, SDL_Color c) {
        const int span = xB - xA;
        const int lo = std::min(xA, xB);
        const int hi = std::max(xA, xB);
        for (int x = std::max(0, lo); x <= std::min(GAME_WIDTH - 1, hi); ++x) {
            const int t = (span == 0) ? 0 : ((x - xA) * 256) / span;
            const int top = topA + ((topB - topA) * t) / 256;
            const int bot = botA + ((botB - botA) * t) / 256;
            for (int y = std::max(kClipTop, top); y <= std::min(kClipBottom, bot); ++y) {
                PutPixel(pixels, x, y, c);
            }
        }
    };

    // Far to near, so nearer blocks overdraw. k stops at 1: row 0 is an extra near row the original lacks.
    for (int k = std::min(lastVisible, kForwardRows - 2); k >= 1; --k) {
        const RowProjection& nearRow = rows[k];
        const RowProjection& farRow = rows[k + 1];
        // The elevated row is the 1900 projection, y - 400/d.
        const int farTop = farRow.y - 400 / std::max(1, farRow.distance);
        const int nearTop = nearRow.y - 400 / std::max(1, nearRow.distance);

        // A cross street every sixteen segments (centre flag bit 2). Its surface is drawn into the gap from the road
        // edge, not the shoulder, or an empty gap reads as a field.
        if (track.flags[nearRow.seg] & 0x0004) {
            for (int side = 0; side < 2; ++side) {
                const int xEdge = side ? nearRow.rightX : nearRow.leftX;
                const int lo = side ? std::max(0, xEdge) : 0;
                const int hi = side ? GAME_WIDTH - 1 : std::min(GAME_WIDTH - 1, xEdge);
                for (int y = std::max(kClipTop, nearTop);
                     y <= std::min(kClipBottom, nearRow.y - 2); ++y) {
                    for (int x = lo; x <= hi; ++x) {
                        PutPixel(pixels, x, y, kTownWallDark);
                    }
                }
                const int kerb = ClampInt(nearRow.y - 1, kClipTop, kClipBottom);
                for (int x = lo; x <= hi; ++x) {
                    PutPixel(pixels, x, kerb, kTownUpper);
                }
            }
        }
        // Either row, or a pair with one foot in the gap draws a wall across it.
        if (((track.flags[farRow.seg] | track.flags[nearRow.seg]) & 0x0004) != 0) {
            continue;
        }

        for (int side = 0; side < 2; ++side) {
            const uint16_t f = side ? track.right[farRow.seg] : track.left[farRow.seg];
            const int xFar = side ? farRow.shoulderRX : farRow.shoulderLX;
            const int xNear = side ? nearRow.shoulderRX : nearRow.shoulderLX;

            quad(xFar, farTop, farRow.y, xNear, nearTop, nearRow.y,
                 (f & 0x0010) ? kTownWallDark : kTownWallGrey);

            // Bit 2 stacks a storey above the kerb wall: full height, or half when bit 3 is clear.
            if (f & 0x0004) {
                const int farH = farRow.y - farTop;
                const int nearH = nearRow.y - nearTop;
                const int farUp = (f & 0x0008) ? farH : farH / 2;
                const int nearUp = (f & 0x0008) ? nearH : nearH / 2;
                quad(xFar, farTop - farUp, farTop, xNear, nearTop - nearUp, nearTop,
                     kTownUpper);

                // Where a block starts, cap it with a flat end face so a cross street reads as a street.
                const uint16_t fNear = side ? track.right[nearRow.seg]
                                            : track.left[nearRow.seg];
                if ((fNear & 0x0004) == 0) {
                    const int xOuter = 2 * xNear - xFar;
                    const int yTop = nearTop - nearUp;
                    quad(xOuter, yTop, nearTop - 1, xNear - 1, yTop, nearTop - 1,
                         kTownWallDark);
                }
            }
        }
    }
}

// The rock pile's offsets by row, A4-0x2822 and A4-0x2856.
static constexpr int kPileT[13][2] = {{15, 17}, {13, 15}, {10, 12}, {9, 10}, {7, 10}, {5, 10}, {5, 10},
                                      {2, 10}, {2, 10}, {2, 10}, {2, 10}, {2, 10}, {2, 10}};
static constexpr int kPileU[15][2] = {{13, 13}, {11, 8}, {9, 6}, {7, 5}, {4, 5}, {3, 3}, {2, 3}, {2, 3},
                                      {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {15, 17}, {13, 15}};

void Racing::DrawRoadsideAndArchSprites(int lastVisible) {
    auto drawSlot = [&](int slot, int k, int side, bool centerInRoad) {
        if (slot < 0 || slot >= roadsideSlotCount || !roadsideSprites[slot]) {
            return;
        }
        const RowProjection& row = rows[k];
        const int sr = ClampInt(k - 2, 0, 10);

        int w = 0, h = 0;
        int srcX0 = 0, srcY0 = 0, srcW = -1, srcH = -1;
        const SpriteAnimationInfo* anim = roadsideAnim[slot];
        if (anim && anim->frames && anim->frameCount > 0) {
            // The distance's frame at native size.
            const SpriteRect& fr = anim->frames[SelectFrameIndexForDistance(anim, sr)];
            w = fr.w;
            h = fr.h;
            srcX0 = fr.x;
            srcY0 = fr.y;
            srcW = fr.w;
            srcH = fr.h;
        } else {
            const SpriteRect& fr = roadsideFrames[slot][ClampInt(sr, 0, kSpriteScaleFrameCount - 1)];
            w = fr.w;
            h = fr.h;
            srcX0 = fr.x;
            srcY0 = fr.y;
            srcW = fr.w;
            srcH = fr.h;
        }
        if (w <= 0 || h <= 0) {
            return;
        }

        Bitmap* sprite = roadsideSprites[slot].get();
        if (centerInRoad) {
            const int x = (row.leftX + row.rightX) / 2 - w / 2;
            DrawSprite(sprite, x, row.y, false, true, srcX0, srcY0, srcW, srcH);
        } else if (side < 0) {
            DrawSprite(sprite, row.leftX - 2, row.y, true, true, srcX0, srcY0, srcW, srcH);
        } else {
            DrawSprite(sprite, row.rightX + 2, row.y, false, true, srcX0, srcY0, srcW, srcH);
        }
    };

    // A rock or oil patch in the road itself, in lane 1..16 across it, where CheckOnRoadHazards collides.
    auto drawHazard = [&](int k, int kind) {
        const RowProjection& row = rows[k];
        const int lane = track.lane[row.seg];
        if (lane <= 0) {
            return;
        }
        Bitmap* sprite = hazardSprites[kind].get();
        if (!sprite) {
            return;
        }
        const srrace::HazardRect& rect =
            srrace::HazardRectFor(kind, ClampInt(k - 2, 0, srrace::HAZARD_PHASES - 1));
        if (rect.w == 0 || rect.h == 0) {
            return;
        }
        const int x = row.leftX + ((row.rightX - row.leftX) * lane) / 16;
        DrawSprite(sprite, x - rect.w / 2, row.y, false, true, rect.srcX, rect.srcY, rect.w, rect.h);
    };

    // jdraw.c FUN_0023ca96: the rock pile an object's 0x10 bit stands for on Mulholland, five tshape sprites
    // heaped inward from the road edge (s = +1 on the left, -1 on the right).
    auto drawShape = [&](int kind, int x, int y, int r) {
        Bitmap* sprite = hazardSprites[kind].get();
        const srrace::HazardRect& rect = srrace::HazardRectFor(kind, ClampInt(r - 2, 0, srrace::HAZARD_PHASES - 1));
        if (!sprite || rect.w == 0 || rect.h == 0) return;
        DrawSprite(sprite, x - rect.w / 2, y, false, true, rect.srcX, rect.srcY, rect.w, rect.h);
    };
    auto drawPile = [&](int r, int edgeX, int s) {
        const int y = rows[r].y;
        const int x = edgeX + s * (r == 1 ? 40 : r == 2 ? 10 : 0);
        const int a = kPileT[r][0], b = kPileT[r][1];
        drawShape(srrace::HazardPile, x, y, r);
        drawShape(srrace::HazardRock, x + s * b, y, r);
        drawShape(srrace::HazardPile, x + s * (b + kPileU[r][0]), y, r);
        drawShape(srrace::HazardRock, x + s * (b >> 1), y - a, r + 2);
        drawShape(srrace::HazardPile, x + s * ((b >> 1) + kPileU[r + 2][1]), y - a, r + 2);
    };

    // At the roadside, on its flag's side, not over the middle of the road.
    auto drawCruiseSign = [&](int k, int side) {
        const RowProjection& row = rows[k];
        const int sr = ClampInt(k - 2, 0, 10);
        if (!cruiseSignAnim || !cruiseSignAnim->frames || cruiseSignAnim->frameCount <= 0) {
            return;
        }
        const SpriteRect& fr = cruiseSignAnim->frames[SelectFrameIndexForDistance(cruiseSignAnim, sr)];
        const int x = (side < 0) ? row.leftX - 2 : row.rightX + 2;
        DrawSprite(cruiseSignSprite.get(), x, row.y, side < 0, true, fr.x, fr.y, fr.w, fr.h);
    };

    const int grudgeTreeSeg = srrace::RaceWindowFor(srrace::RaceKind::Grudge).start + 4;
    // jdraw.c FUN_0023e3fa draws bits 0x40 and 0x80 through its `race_type & 0x0f` branch.
    const bool grudgeRace = sim.Kind() == srrace::RaceKind::Grudge;
    // Mulholland: per row far to near, walls, then the road band, then sprites, then the bridge. Walls and bridge
    // on every row, road and sprites only up to the crest.
    const bool mulholland = IsMulholland();
    for (int r = kForwardRows - 1; mulholland && r >= 1; --r) {
        if ((track.right[rows[r].seg] & 0x8000) != 0 && (track.right[rows[r - 1].seg] & 0x8000) == 0) {
            railPhaseSeg = rows[r].seg;
            railPhaseRow = r;
            break;
        }
    }
    for (int k = mulholland ? kForwardRows - 1 : std::min(lastVisible, kForwardRows - 1); k >= 1; --k) {
        if (mulholland) {
            DrawMulhollandWalls(k, mulhollandFarBridgeEnd);
            if (k <= lastVisible) {
                RenderRoad(lastVisible, k - 1);
            }
            if (k > lastVisible) {
                DrawMulhollandBridge(k);
                DrawMulhollandRail(k);
                continue;
            }
        }
        const int seg = rows[k].seg;
        if (seg >= 0 && seg < kTrackSegments) {
            // Nearest last, so a closer hazard covers one behind it.
            if ((track.flags[seg] & srrace::HAZARD_OIL_FLAG) != 0) {
                drawHazard(k, srrace::HazardOil);
            }
            if ((track.flags[seg] & srrace::HAZARD_ROCK_FLAG) != 0) {
                drawHazard(k, srrace::HazardRock);
            }
        }
        if (seg < 0 || seg >= kTrackSegments) {
            continue;
        }
        const uint16_t L = track.left[seg];
        const uint16_t R = track.right[seg];
        const uint16_t F = track.flags[seg];
        const bool city = (F & 0x0001) != 0;

        // The original skips a row only for a side street, flags 0x0004 in town; on Mulholland 0x0004 is a rock.
        if ((F & 0x0004) == 0 || !city) {
            if (mulholland && k < 12 && (L & 0x0010)) drawPile(k, rows[k].leftX, 1);
            if (mulholland && k < 12 && (R & 0x0010)) drawPile(k, rows[k].rightX, -1);
            if (L & 0x0020) drawSlot(0, k, -1, false);
            if (L & 0x0100) drawSlot(4, k, -1, false);
            if (L & 0x0800) drawSlot(3, k, -1, false);
            if (L & 0x1000) drawSlot(6, k, -1, false);
            // On a cruise the destination's own sign; the drive stops here.
            if (L & 0x0001) {
                if (cruiseMode && cruiseSignSprite) {
                    drawCruiseSign(k, -1);
                } else {
                    drawSlot(1, k, -1, city);
                }
            }
            // Rocks and the second tree are countryside only; in town the 0x40 object swaps sprites.
            if (!city && (L & 0x0004)) drawSlot(7, k, -1, false);
            if (!city && (L & 0x0008)) drawSlot(8, k, -1, false);
            if (L & 0x0040) drawSlot(city ? 2 : 10, k, -1, false);
            if ((!city || grudgeRace) && (L & 0x0080)) drawSlot(11, k, -1, false);

            if (!city && (R & 0x0004)) drawSlot(7, k, +1, false);
            if (!city && (R & 0x0008)) drawSlot(8, k, +1, false);
            if (R & 0x0040) drawSlot(city ? 2 : 10, k, +1, false);
            if ((!city || grudgeRace) && (R & 0x0080)) drawSlot(11, k, +1, false);
            if (R & 0x0020) drawSlot(0, k, +1, false);
            if (R & 0x0100) drawSlot(4, k, +1, false);
            if (R & 0x0800) drawSlot(3, k, +1, false);
            if (R & 0x1000) drawSlot(6, k, +1, false);
            if (R & 0x2000) drawSlot(18, k, +1, false);
            if (R & 0x4000) drawSlot(19, k, +1, false);
            if (R & 0x0001) {
                if (cruiseMode && cruiseSignSprite) {
                    drawCruiseSign(k, +1);
                } else {
                    drawSlot(1, k, +1, city);
                }
            }

            if (!city && (F & 0x0002)) drawSlot(9, k, +1, false);
            if (F & 0x0100) drawSlot(5, k, +1, false);
            if (F & 0x0800) drawSlot(15, k, +1, false);
            if (F & 0x0200) drawSlot(16, k, +1, false);
            if (F & 0x0400) drawSlot(17, k, +1, false);
        }

        // jdraw.c gates the arches on `(race_type & 0x2c) == 0`: none on Mulholland or a grudge strip.
        if ((F & 0xf000) && !mulholland && !grudgeRace) {
            DrawOverheadArch(rows[k], F, ClampInt(k - 2, 0, 10));
        }

        // jdraw.c 0x240a44: `race_type & 0x0c` and the row sitting on
        // start_seg + 4 are the whole gate on the drag tree.
        if (sim.Kind() == srrace::RaceKind::Grudge && seg == grudgeTreeSeg) {
            DrawStopLights(k);
        }
        if (mulholland) {
            DrawMulhollandBridge(k);
            DrawMulhollandRail(k);
        }
    }

    // Row 0 is the extra near row this port adds ahead of the original's.
    if (sim.Kind() == srrace::RaceKind::Grudge && rows[0].seg == grudgeTreeSeg) {
        DrawStopLights(0);
    }
}

// 13 sprites per car in one asset run: front-side, rear-side, 7 rear LOD frames, 4 mirror frames, indexed by
// view state.
Bitmap* Racing::OpponentSpriteForState(int state) const {
    if (state < 0 || state > 12) {
        return nullptr;
    }
    if (::state.GetAssets().GetGame() == SrGame::SR2 && !sr1BodyInSr2) {
        return opponentCarType == 2 ? sr2Cop[state].get() : sr2Opp[state][srrace::AQU_OPP_LEVEL_TILT].get();
    }
    const std::unique_ptr<Bitmap>* frontSide = &opponentChevyFrontSide;
    const std::unique_ptr<Bitmap>* rearSide = &opponentChevyRearSide;
    const std::unique_ptr<Bitmap>* rear = opponentChevyRear;
    const std::unique_ptr<Bitmap>* behind = opponentChevyBehind;
    switch (opponentCarType) {
        case 1:
            frontSide = &opponentChryslerFrontSide; rearSide = &opponentChryslerRearSide;
            rear = opponentChryslerRear; behind = opponentChryslerBehind; break;
        case 2:
            frontSide = &opponentCopFrontSide; rearSide = &opponentCopRearSide;
            rear = opponentCopRear; behind = opponentCopBehind; break;
        case 3:
            frontSide = &opponentOldsmobileFrontSide; rearSide = &opponentOldsmobileRearSide;
            rear = opponentOldsmobileRear; behind = opponentOldsmobileBehind; break;
        case 4:
            frontSide = &opponentChevy2FrontSide; rearSide = &opponentChevy2RearSide;
            rear = opponentChevy2Rear; behind = opponentChevy2Behind; break;
        case 5:
            frontSide = &opponentCorvette63FrontSide; rearSide = &opponentCorvette63RearSide;
            rear = opponentCorvette63Rear; behind = opponentCorvette63Behind; break;
        case 6:
            frontSide = &opponentFordFrontSide; rearSide = &opponentFordRearSide;
            rear = opponentFordRear; behind = opponentFordBehind; break;
        default: break;
    }
    if (state == 0) return frontSide->get();
    if (state == 1) return rearSide->get();
    if (state < 9) return rear[state - 2].get();
    return behind[state - 9].get();
}

void Racing::DrawNpcCar(int lastVisible) {
    if (flybyMode) {
        if (!flybyNpcVisible) {
            return;
        }
    } else if (!opponentActive) {
        return;
    }

    const srrace::CarState& p = viewPlayer;
    const srrace::CarState& np = viewNpc;
    const srrace::NpcState& ai = sim.NpcAi();

    if (ai.viewHidden) {
        return;   // walked off the end of the range
    }
    const int state = ai.viewState;
    if (state >= 9) {
        return;   // behind us: DrawRearviewMirror has it
    }
    if (aqueductPreview) {
        DrawAqueductOpponent(state);
        return;
    }
    Bitmap* sprite = OpponentSpriteForState(state);
    if (!sprite || !sprite->GetSurface()) {
        return;
    }
    SDL_Surface* src = sprite->GetSurface();

    // From the fine gap, not the row table, which mixed in the player's cycling scroll accumulator.
    const int longDist = (static_cast<int>(np.segPos) - static_cast<int>(p.segPos)) * 8
                       + (np.scrollAcc - p.scrollAcc);
    const int d = std::max(4, longDist + (state == 0 ? 20 : 4));
    const int npcSeg = ClampInt(static_cast<int>(np.segPos) + 1, 0, kTrackSegments - 1);
    const int roadDelta = p.heading - track.curve[npcSeg];
    const int elevDelta = p.elevAtCar - track.elev[npcSeg];
    const int x = ProjectX(p, d, roadDelta, np.lateral);
    int y = ProjectY(d, elevDelta);

    if (state == 0) {
        // Right alongside, off to our left: right edge 64 px left of x, never right of the screen edge, hanging down
        // from the road row.
        DrawSprite(sprite, std::min(0, x - 64 - src->w), y, false, false);
        return;
    }
    if (state == 1) {
        // Just ahead and well left: right edge on x, baseline 17 px low.
        DrawSprite(sprite, x, y + 0x11, true, true);
        return;
    }
    // Ahead: centred, never above the horizon row. With Hill cover on, clip to the sky above the crest row so a car
    // in a dip is hidden. Flat road is excluded, where the opponent can be beyond the row table.
    const RowProjection& crest = rows[static_cast<size_t>(lastVisible)];
    if (state != 0 && lastVisible < kForwardRows - 1 && npcSeg > crest.seg &&
        ::state.GetAssets().GetHillOcclusionRef()) {   // local `state` is the view state
        const int visible = crest.y - (y - src->h);
        if (visible <= 0) {
            return;
        }
        DrawSprite(sprite, x - src->w / 2, y - src->h, false, false, 0, 0, src->w, visible);
        return;
    }
    y = std::max(crest.y, y);
    if (state == 2) {
        y = std::min(y, kClipBottom);   // the nearest frame stops at the viewport floor
    }
    DrawSprite(sprite, x - src->w / 2, y, false, true);
}

// jdraw.c FUN_0023d7b8's Aqueduct branch. Nothing is rotated pixel by pixel: a pre-tilted frame is picked, the
// projected point is swung with the bank, and the frame's anchor is put on it. States 9..12 go in the mirror.
void Racing::DrawAqueductOpponent(int viewState) {
    const srrace::CarState& p = viewPlayer;
    const srrace::CarState& np = viewNpc;
    const bool mirror = viewState >= 9;
    const srrace::AqueductOpponentTilt tilt =
        srrace::AqueductOpponentTiltFor(np.lateral, aqueductFrame.drawn, aqueductBankAngle);
    if (tilt.tilt != srrace::AQU_OPP_LEVEL_TILT) {
        // ponytail: the original still draws a tilted state 0 while the opponent is pinned alongside (A4+0x2342).
        if (viewState == 0) return;
        if (viewState == 1) viewState = 2;
    }
    // The cop has only the level frame. It is drawn as the original draws a car past its last tilt frame: level,
    // with the anchor turned by the real angle. Street Rod's bodies have turned frames made at load.
    const bool levelOnly = opponentCarType == 2;
    const int t = (tilt.tilt < 0 || levelOnly) ? srrace::AQU_OPP_LEVEL_TILT : tilt.tilt;
    Bitmap* sprite = levelOnly ? OpponentSpriteForState(viewState) : sr2Opp[viewState][t].get();
    SDL_Surface* src = sprite ? sprite->GetSurface() : nullptr;
    if (!src || src->w <= 0 || src->h <= 0) {
        return;
    }
    const int levelXOff = levelOnly ? (viewState >= 2 ? src->w / 2 : 0)
                                    : sr2OppXOff[viewState][srrace::AQU_OPP_LEVEL_TILT];
    int xOff = levelOnly ? levelXOff : sr2OppXOff[viewState][t];
    int yOff = levelOnly ? 0 : sr2OppYOff[viewState][t];
    if (tilt.tilt < 0 || (levelOnly && tilt.tilt != srrace::AQU_OPP_LEVEL_TILT)) {
        srrace::AqueductTiltAnchor(src->w, src->h, tilt.angle, levelXOff, xOff, yOff);
    }

    int gap = (static_cast<int>(np.segPos) - static_cast<int>(p.segPos)) * 8 + (np.scrollAcc - p.scrollAcc);
    if (viewState == 0) gap += 0x10;
    if (gap < -2 && viewState == 2) gap += 8;
    const int ground = srrace::AqueductOpponentGroundY(np.lateral, aqueductFrame.drawn);
    int x = 0, y = 0;
    if (!mirror) {
        const int depth = std::abs(gap) + 4;
        x = srrace::AqueductProjectViewX(p.lateral, depth, np.lateral, p.heading);
        y = srrace::AqueductProjectY(aqueductCameraHeight, depth, ground, sim.AqueductVjerk());
        if (viewState == 2 || viewState == 3) y = std::min(y, 0x62);
    } else {
        x = srrace::AqueductProjectMirrorX(p.lateral, -gap, np.lateral, 10 - p.heading);
        y = srrace::AqueductProjectMirrorY(aqueductCameraHeight, -gap, ground);
    }
    y -= kViewOffsetY;
    // ponytail: the mirror car turns about the mirror's own pivot, as its rows do; which pivot the original
    // leaves set for it is not traced.
    const srrace::AqueductBank bank = mirror
        ? srrace::AqueductBankForRows(aqueductFrame.mirrorRows, srrace::AQU_MIRROR_ROWS, aqueductBankAngle)
        : aqueductBank;
    srrace::AqueductBankPoint(bank, x, y, x, y);
    x -= xOff;
    y += yOff;

    // The two side views keep the road race's placement: alongside hangs off the left screen edge, cut by how far
    // ahead x is; just ahead sits right-edge on x, 17 px low.
    int left = x + 3;
    int top = y - src->h;
    if (viewState == 0) {
        left = std::min(0, x - 0x40 - src->w);
        top = y;
    } else if (viewState == 1) {
        left = x - src->w;
        top = y - src->h + 0x11;
    } else if ((viewState == 2 || viewState == 3) && top + src->h >= 0x63 - kViewOffsetY) {
        top = 0x63 - kViewOffsetY - src->h;
    }
    const SDL_Rect& clip = mirror ? kAqueductMirrorClip : kAqueductViewClip;
    const int clipL = clip.x;
    const int clipR = clip.x + clip.w - 1;
    const int clipT = clip.y;
    const int clipB = clip.y + clip.h - 1;
    const int x0 = std::max(left, clipL);
    const int y0 = std::max(top, clipT);
    const int x1 = std::min(left + src->w - 1, clipR);
    const int y1 = std::min(top + src->h - 1, clipB);
    if (x0 > x1 || y0 > y1) {
        return;
    }
    DrawSprite(sprite, x0, y0, false, false, x0 - left, y0 - top, x1 - x0 + 1, y1 - y0 + 1);
}

// The Aqueduct's mirror.
void Racing::DrawAqueductMirror() {
    // FUN_00243b92: the glass cleared black (x 0xf2..0x13d, y 0x16..0x2b), then the same row drawing as the view
    // on the nine mirror rows, banked about their own far end. Its flags start one segment further back.
    for (int y = kAqueductMirrorClip.y; y < kAqueductMirrorClip.y + kAqueductMirrorClip.h; ++y) {
        for (int x = kAqueductMirrorClip.x; x < kAqueductMirrorClip.x + kAqueductMirrorClip.w; ++x) {
            PutPixel(pixels, x, y, {0, 0, 0, 255});
        }
    }
    // Its own night sky, before the channel (FUN_00241f88), turned about the mirror's pivot.
    {
        const srrace::AqueductBank skyBank = srrace::AqueductBankForRows(
            aqueductFrame.mirrorRows, srrace::AQU_MIRROR_OBJECT_ROWS + 1, aqueductBankAngle);
        srrace::AqueductStar stars[srrace::AQU_MIRROR_STARS_BRIGHT + srrace::AQU_MIRROR_STARS_DIM];
        const int n = srrace::AqueductBuildMirrorSky(srrace::AqueductSkyBase(sim.Player().heading), skyBank, 0,
                                                     stars, static_cast<int>(std::size(stars)));
        const SDL_Rect& c = kAqueductMirrorClip;
        for (int i = 0; i < n; ++i) {
            int x = stars[i].x;
            int y = stars[i].y - kViewOffsetY;
            if (aqueductBankAngle != 0) srrace::AqueductBankPoint(skyBank, x, y, x, y);
            if (x < c.x || x >= c.x + c.w || y < c.y || y >= c.y + c.h) continue;
            PutPixel(pixels, x, y, AqueductPen(stars[i].color, {170, 170, 170, 255}));
        }
    }

    const srrace::AqueductBank bank = DrawAqueductPass(aqueductFrame.mirrorRows, srrace::AQU_MIRROR_OBJECT_ROWS,
                                                       aqueductTrack.mirror, aqueductFrame.mirrorFlagIndex + 1,
                                                       kAqueductMirrorClip, false);

    // The starter at her row, on the left edge of the midline (FUN_00244c12).
    const int r = static_cast<int>(sim.Player().segPos) - sr2StarterSeg - 1;
    if (sr2StarterSeg >= 0 && r >= 0 && r < srrace::AQU_MIRROR_OBJECT_ROWS) {
        const srrace::AqueductRow* rows = aqueductFrame.mirrorRows;
        const srrace::AqueductRow& far = rows[srrace::AQU_MIRROR_OBJECT_ROWS];
        const srrace::AqueductRow& near = rows[0];
        const int y = rows[r].y;
        int x = far.y == near.y ? near.stripeLeftX
                                : far.stripeLeftX + (y - far.y) * (near.stripeLeftX - far.stripeLeftX) / (near.y - far.y);
        x += 1;
        int by = y;
        if (aqueductBankAngle != 0) srrace::AqueductBankPoint(bank, x, y, x, by);
        DrawStarterInMirror(r, x, by, true, kAqueductMirrorClip);
    }

    const srrace::NpcState& ai = sim.NpcAi();
    if (opponentActive && !ai.viewHidden && ai.viewState >= 9) {
        DrawAqueductOpponent(ai.viewState);
    }
}

// jdraw.c RenderMirrorView (FUN_00238318) and BuildMirrorRowTables (0x23b016). Screen coordinates throughout.
void Racing::DrawRearviewMirror() {
    if (aqueductPreview) {
        // The channel has its own mirror.
        DrawAqueductMirror();
        return;
    }
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        DrawRearviewMirrorSr2();
        return;
    }
    constexpr int kRows = 8;
    const srrace::CarState& p = viewPlayer;
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);
    const bool grudge = sim.Kind() == srrace::RaceKind::Grudge;

    auto mirrorX = [&](int d, int lat, int roadDelta) { return srrace::ProjectMirrorX(p.lateral, d, lat, roadDelta); };
    auto mirrorY = [](int d, int base, int rowC) { return srrace::ProjectMirrorY(d, base, rowC); };

    // Height and curve deltas are negated relative to the forward view.
    struct MirrorRow {
        int y, left, right, shoulderL, shoulderR, archY;
    };
    MirrorRow mrows[kRows];
    for (int j = 0; j < kRows; ++j) {
        const int seg = ClampInt(segBase - j, 0, kTrackSegments - 1);
        const int d = j == 0 ? 8 : p.scrollAcc + 8 * j;
        const int rowC = grudge ? 0 : track.elev[seg] - p.elevAtCar;
        const int roadDelta = track.curve[seg] - p.heading;
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);
        MirrorRow& row = mrows[j];
        row.y = mirrorY(d, 2000, rowC);
        row.left = mirrorX(d, edgeLeft, roadDelta);
        row.right = mirrorX(d, edgeRight, roadDelta);
        row.shoulderL = mirrorX(d, 1100, roadDelta);
        row.shoulderR = mirrorX(d, 3700, roadDelta);
        row.archY = mirrorY(d, 1900, rowC);
    }

    MirrorRect(kMirrorLeft, kMirrorRight, kMirrorTop, kMirrorBottom, kSkyColor);
    if (rearviewMirrorSprite && rearviewMirrorSprite->GetSurface()) {
        const SDL_Surface* h = rearviewMirrorSprite->GetSurface();
        MirrorBlit(rearviewMirrorSprite.get(),
                   {0, 0, static_cast<uint16_t>(h->w), static_cast<uint16_t>(std::min(2, h->h))}, 240,
                   mrows[kRows - 1].y - 2);
    }

    auto libSprite = [&](int id) {
        return state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(id)});
    };

    const SDL_Color grass = kGrassA;
    const SDL_Color sidewalk = kCurbGray;
    const srrace::NpcState& ai = sim.NpcAi();
    const int npcRow = static_cast<int>(viewPlayer.segPos) - static_cast<int>(viewNpc.segPos);

    for (int k = kRows - 1; k >= 1; --k) {
        const MirrorRow& row = mrows[k];
        const MirrorRow& nearRow = mrows[k - 1];
        // The original reads flags one segment behind the row's geometry.
        const int seg = ClampInt(segBase - k - 1, 0, kTrackSegments - 1);
        const uint16_t F = track.flags[seg];
        const uint16_t Lf = track.left[seg];
        const uint16_t Rf = track.right[seg];
        const bool city = (F & 0x0001) != 0;

        auto slope = [](int xNear, int yNear, int xFar, int yFar) {
            return yFar == yNear ? 0 : static_cast<int16_t>((xFar - xNear) * 16) / (yFar - yNear);
        };
        const int dl = slope(nearRow.left, nearRow.y, row.left, row.y);
        const int dr = slope(nearRow.right, nearRow.y, row.right, row.y);
        const int dsl = slope(nearRow.shoulderL, nearRow.y, row.shoulderL, row.y);
        const int dsr = slope(nearRow.shoulderR, nearRow.y, row.shoulderR, row.y);
        int l16 = row.left << 4, r16 = row.right << 4, sl16 = row.shoulderL << 4, sr16 = row.shoulderR << 4;
        int l = row.left, r = row.right, sl = row.shoulderL, sr = row.shoulderR;
        for (int y = row.y, n = nearRow.y - row.y; n > 0 && y <= kMirrorBottom && !(city && (F & 0x0004)); --n, ++y) {
            if (city) {
                if (Lf & 0x0080) {
                    const int mid = sl + ((l - sl) >> 1);
                    MirrorSpan(sl, mid, y, sidewalk);
                    MirrorSpan(mid, l, y, grass);
                } else {
                    MirrorSpan(sl, l, y, sidewalk);
                }
                if (Rf & 0x0080) {
                    const int mid = sr - ((sr - r) >> 1);
                    MirrorSpan(mid, sr, y, sidewalk);
                    MirrorSpan(r, mid, y, grass);
                } else {
                    MirrorSpan(r, sr, y, sidewalk);
                }
            } else {
                MirrorSpan(kMirrorLeft, l, y, grass);
                MirrorSpan(r, kMirrorRight, y, grass);
                MirrorSpan(l, r, y, kRoadA);
            }
            // The mirror's centre line width table is all zeros.
            if (F & 0x0008) {
                const int cx = (l + r) >> 1;
                MirrorSpan(cx, cx, y, DashColorFor(F));
            }
            l16 += dl;
            r16 += dr;
            l = l16 >> 4;
            r = r16 >> 4;
            if (city) {
                sl16 += dsl;
                sr16 += dsr;
                sl = sl16 >> 4;
                sr = sr16 >> 4;
            }
        }

        if ((F & 0x0004) == 0) {
            if (F & 0xf000) {
                const int top = static_cast<int16_t>(row.y - 4 * (row.y - row.archY));
                MirrorRect(row.left, row.left, top, row.y, kArchColor);
                MirrorRect(row.right, row.right, top, row.y, kArchColor);
                MirrorRect(row.left + 1, row.right - 1, top, top, kArchColor);
                const int cx = (row.left + row.right) >> 1;
                const int scale = k + 5;
                if (F & 0x1000) {
                    MirrorRoadside(13, cx, top, scale);
                } else if (F & 0x2000) {
                    MirrorBanner(cruiseMode && cruiseSignSprite ? cruiseSignSprite.get() : signCountyLineSprite.get(),
                           cx, top, scale);
                } else if (city) {
                    MirrorBanner(libSprite((F & 0x4000) ? RACING_SIGN_TRY_VEGAS_GAMBLER : RACING_SIGN_PLAY_BLOCKOUT), cx,
                           top, scale);
                } else {
                    MirrorRoadside((F & 0x4000) ? 12 : 14, cx, top, scale);
                }
            }

            if (k < kRows - 1) {
                const int x1 = row.left, x2 = row.right, s1 = row.shoulderL, s2 = row.shoulderR, y = row.y;
                const int scale = k + 5;
                if (Lf & 0x0020) MirrorRoadside(0, city ? x1 : s1, y, scale);
                if (Lf & 0x0100) MirrorRoadside(4, city ? x1 : (Lf & 0x0200) ? 2 * s1 - x1 : s1, y, scale);
                if (Lf & 0x0800) MirrorRoadside(3, x1, y, scale);
                if (Lf & 0x1000) MirrorRoadside(6, x1, y, scale);
                if (city) {
                    if (Lf & 0x0040) MirrorRoadside(2, x1, y, scale);
                    if (Rf & 0x0040) MirrorRoadside(2, x2, y, scale);
                } else {
                    if (Lf & 0x0001) MirrorRoadside(1, x1, y, scale);
                    if (Lf & 0x0004) MirrorRoadside(7, x1, y, scale);
                    if (Lf & 0x0008) MirrorRoadside(8, x1, y, scale);
                    if (Lf & 0x0040) MirrorRoadside(10, s1 - ((x1 - s1) >> 1), y, scale);
                    if (Lf & 0x0080) MirrorRoadside(11, s1, y, scale);
                    if (Rf & 0x0001) MirrorRoadside(1, x2, y, scale);
                    if (Rf & 0x0004) MirrorRoadside(7, x2, y, scale);
                    if (Rf & 0x0008) MirrorRoadside(8, x2, y, scale);
                    if (Rf & 0x0040) MirrorRoadside(10, s2, y, scale);
                    if (Rf & 0x0080) MirrorRoadside(11, s2 + ((s2 - x2) >> 1), y, scale);
                    if (F & 0x0002) MirrorRoadside(9, x2, y, scale);
                }
                if (Rf & 0x0020) MirrorRoadside(0, city ? x2 : s2, y, scale);
                if (Rf & 0x0100) MirrorRoadside(4, city ? x2 : (Rf & 0x0200) ? 2 * s2 - x2 : s2, y, scale);
                if (Rf & 0x2000) MirrorRoadside(18, x2, y, scale);
                if (Rf & 0x4000) MirrorRoadside(19, x2, y, scale);
                if (Rf & 0x0800) MirrorRoadside(3, x2, y, scale);
                if (Rf & 0x1000) MirrorRoadside(6, x2, y, scale);
                if (F & 0x0100) MirrorRoadside(5, x2, y, scale);
                if (F & 0x0200) MirrorRoadside(16, x2, y, scale);
                if (F & 0x0400) MirrorRoadside(17, x2, y, scale);
                if (F & 0x0800) MirrorRoadside(15, x2, y, scale);
            }
        }

        // In row order, so nearer roadside objects cover it.
        if (opponentActive && !ai.viewHidden && ai.viewState >= 9 && npcRow == k) {
            Bitmap* behind = OpponentSpriteForState(ai.viewState);
            SDL_Surface* bs = behind ? behind->GetSurface() : nullptr;
            if (bs) {
                const srrace::CarState& np = viewNpc;
                const int npcSeg = ClampInt(static_cast<int>(np.segPos), 0, kTrackSegments - 1);
                const int d = -((static_cast<int>(np.segPos) - static_cast<int>(p.segPos)) * 8 +
                                (np.scrollAcc - p.scrollAcc));
                const int x = mirrorX(d, np.lateral, track.curve[npcSeg] - p.heading);
                const int y = mirrorY(d, 2000, grudge ? 0 : track.elev[npcSeg] - p.elevAtCar);
                MirrorBlit(behind, {0, 0, static_cast<uint16_t>(bs->w), static_cast<uint16_t>(bs->h)},
                           x - (bs->w >> 1), y - bs->h);
            }
        }
    }
}

void Racing::MirrorSpan(int x0, int x1, int y, SDL_Color c) {
    if (y < kMirrorTop || y > kMirrorBottom) return;
    if (x0 > x1) std::swap(x0, x1);
    for (int x = std::max(x0, kMirrorLeft); x <= std::min(x1, kMirrorRight); ++x) {
        PutPixel(pixels, x, y - kViewOffsetY, c);
    }
}

void Racing::MirrorRect(int x0, int x1, int y0, int y1, SDL_Color c) {
    if (y0 > y1) std::swap(y0, y1);
    for (int y = std::max(y0, kMirrorTop); y <= std::min(y1, kMirrorBottom); ++y) MirrorSpan(x0, x1, y, c);
}

void Racing::MirrorBlit(Bitmap* bmp, const SpriteRect& fr, int dstX, int dstY) {
    SDL_Surface* s = bmp ? bmp->GetSurface() : nullptr;
    if (!s) return;
    for (int yy = 0; yy < static_cast<int>(fr.h); ++yy) {
        const int y = dstY + yy;
        if (y < kMirrorTop || y > kMirrorBottom) continue;
        for (int xx = 0; xx < static_cast<int>(fr.w); ++xx) {
            const int x = dstX + xx;
            if (x < kMirrorLeft || x > kMirrorRight) continue;
            Uint32 sp = 0;
            const Uint8* ptr = static_cast<const Uint8*>(s->pixels) + (fr.y + yy) * s->pitch +
                               (fr.x + xx) * s->format->BytesPerPixel;
            std::memcpy(&sp, ptr, s->format->BytesPerPixel);
            Uint8 r = 0, g = 0, b = 0, a = 255;
            SDL_GetRGBA(sp, s->format, &r, &g, &b, &a);
            if (a > 0) PutPixel(pixels, x, y - kViewOffsetY, {r, g, b, a});
        }
    }
}

// RenderRoadsideSprite in mirror mode: frame min(scale, 11) - 1, centred on x, standing on y.
void Racing::MirrorRoadside(int slot, int x, int y, int scale) {
    const int sr = std::max(0, std::min(scale, 11) - 1);
    y = std::min(y, 99);
    if (slot == 1 && cruiseMode && cruiseSignSprite && cruiseSignAnim && cruiseSignAnim->frames &&
        cruiseSignAnim->frameCount > 0) {
        const SpriteRect& fr = cruiseSignAnim->frames[SelectFrameIndexForDistance(cruiseSignAnim, sr)];
        MirrorBlit(cruiseSignSprite.get(), fr, x - (fr.w >> 1), y - fr.h + 1);
        return;
    }
    if (slot < 0 || slot >= roadsideSlotCount || !roadsideSprites[slot]) return;
    const SpriteAnimationInfo* anim = roadsideAnim[slot];
    const SpriteRect& fr = (anim && anim->frames && anim->frameCount > 0)
                               ? anim->frames[SelectFrameIndexForDistance(anim, sr)]
                               : roadsideFrames[slot][ClampInt(sr, 0, kSpriteScaleFrameCount - 1)];
    if (fr.w == 0 || fr.h == 0) return;
    MirrorBlit(roadsideSprites[slot].get(), fr, x - (fr.w >> 1), y - fr.h + 1);
}

// RenderCenteredSprite in mirror mode: the banners' eleven-frame sheet, centred both ways.
void Racing::MirrorBanner(Bitmap* bmp, int x, int y, int scale) {
    const SpriteRect& fr = RACING_SIGN_COUNTY_LINE_FRAMES[10 - ClampInt(scale, 0, 10)];
    MirrorBlit(bmp, fr, x - (fr.w >> 1), y - (fr.h >> 1));
}

// jdraw.c FUN_0023e3fa over jroad.c FUN_00245c76. Screen coordinates throughout.
void Racing::DrawRearviewMirrorSr2() {
    constexpr int kRows = 9;
    const srrace::CarState& p = viewPlayer;
    const int pos = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);
    const srrace::RaceKind kind = sim.Kind();
    const bool mulholland = kind == srrace::RaceKind::Mulholland;
    const bool grudge = kind == srrace::RaceKind::Grudge;
    const bool drag = kind == srrace::RaceKind::Drag;
    const int verge = mulholland ? 200 : 400;
    const int cameraY = 1900 - FallDrop();
    auto segAt = [](int s) { return ClampInt(s, 0, kTrackSegments - 1); };
    auto mirrorX = [&](int d, int lat, int roadDelta) {
        return srrace::ProjectMirrorXSr2(p.lateral, d, lat, roadDelta);
    };
    auto mirrorY = [&](int d, int base, int rowC) { return srrace::ProjectMirrorYSr2(d, base, rowC, cameraY); };

    // Height and curve deltas are negated relative to the forward view. Rows 6..8 drop the scroll term, and the
    // wall heights are read from the segments ahead.
    struct Row {
        int y, left, right, vergeL, vergeR, outerR, yTop, yMid, wallL, wallR;
    };
    Row rows2[kRows];
    for (int j = 0; j < kRows; ++j) {
        const int seg = segAt(pos - j);
        const int d = j == 0 ? 8 : j <= 5 ? p.scrollAcc + 8 * j : 8 * j;
        const int rowC = grudge ? 0 : track.elev[seg] - p.elevAtCar;
        int roadDelta = track.curve[seg] - p.heading;
        if (j == 0 && mulholland) {
            roadDelta = roadDelta > 60 ? 70 : roadDelta < -60 ? -70 : roadDelta;
        }
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdgesSr2(track.flags[seg], true, edgeLeft, edgeRight);
        const int wallSeg = segAt(pos + j);
        Row& r = rows2[j];
        r.y = mirrorY(d, 2000, rowC);
        r.left = mirrorX(d, edgeLeft, roadDelta);
        r.right = mirrorX(d, edgeRight, roadDelta);
        r.vergeL = mirrorX(d, 1500 - verge, roadDelta);
        r.vergeR = mirrorX(d, 3300 + verge, roadDelta);
        r.outerR = mirrorX(d, 3300 + 4 * verge, roadDelta);
        r.yTop = mirrorY(d, 1900, rowC);
        r.yMid = mirrorY(d, 1950, rowC);
        r.wallL = mirrorY(d, 2000 - track.wallLeft[wallSeg], rowC);
        r.wallR = mirrorY(d, 2000 - track.wallRight[wallSeg], rowC);
    }

    const SDL_Color black = Sr2Pen(0, kSkyColor);
    const SDL_Color pen3 = Sr2Pen(3, kCurbGray);
    const SDL_Color pen4 = Sr2Pen(4, {136, 85, 0, 255});
    const SDL_Color centre = Sr2Pen(10, {240, 240, 240, 255});
    const SDL_Color finish = Sr2Pen(0xc, kCurbGray);
    const SDL_Color grass = Sr2Pen(0xe, kGrassA);
    const SDL_Color red = Sr2Pen(0xf, {204, 34, 34, 255});
    const SDL_Color brown = Sr2Pen(0x11, {136, 85, 0, 255});

    MirrorRect(kMirrorLeft, kMirrorRight, kMirrorTop, kMirrorBottom, black);
    if (Bitmap* h = Sr2Asset(mulholland ? SR2_MULHOLLAND_REARVIEW : SR2_RACING_HORIZON_REARVIEWMIRROR)) {
        if (const SDL_Surface* hs = h->GetSurface()) {
            const int rows = std::min(mulholland ? 21 : 2, hs->h);
            MirrorBlit(h, {0, 0, static_cast<uint16_t>(hs->w), static_cast<uint16_t>(rows)}, 240,
                       rows2[7].y - (mulholland ? 10 : 2));
        }
    }

    auto poly = [&](std::initializer_list<SDL_Point> pts, SDL_Color c) {
        int xs[6], ys[6], n = 0;
        for (const SDL_Point& q : pts) {
            xs[n] = q.x;
            ys[n] = q.y - kViewOffsetY;
            ++n;
        }
        FillPolygon(xs, ys, n, c, &kAqueductMirrorClip);
    };
    auto line = [&](int x0, int y0, int x1, int y1, SDL_Color c) {
        DrawAqueductLine({x0, y0 - kViewOffsetY, x1, y1 - kViewOffsetY}, c, kAqueductMirrorClip);
    };
    auto portal = [&](int l, int r, int top, int bottom) {
        MirrorRect(l, l, top, bottom, red);
        MirrorRect(r, r, top, bottom, red);
        MirrorRect(l + 1, r - 1, top, top, red);
    };
    auto pile = [&](int edgeX, int y, int r, int s) {
        const int a = kPileT[r][0], b = kPileT[r][1];
        MirrorRoadside(8, edgeX, y, r);
        MirrorRoadside(7, edgeX + s * b, y, r);
        MirrorRoadside(8, edgeX + s * (b + kPileU[r][0]), y, r);
        MirrorRoadside(7, edgeX + s * (b >> 1), y - a, r + 2);
        MirrorRoadside(8, edgeX + s * ((b >> 1) + kPileU[r + 2][1]), y - a, r + 2);
    };

    const srrace::NpcState& ai = sim.NpcAi();
    const int npcRow = static_cast<int>(viewPlayer.segPos) - static_cast<int>(viewNpc.segPos);
    const int fall = sim.FallDirection();

    for (int k = kRows - 1; k >= 1; --k) {
        const Row& B = rows2[k];
        const Row& A = rows2[k - 1];
        const Row& C = rows2[std::max(k - 2, 0)];
        // The original reads flags one segment behind the row's geometry.
        const int seg = segAt(pos - k - 1);
        const uint16_t F = track.flags[seg];
        const uint16_t Lo = track.left[seg];
        const uint16_t Ro = track.right[seg];
        const uint16_t farF = track.flags[segAt(seg - 1)];
        const bool city = (F & 0x0001) != 0;
        const bool noWallL = A.wallL == A.y;
        const bool noWallR = A.wallR == A.y;

        if (mulholland && (F & 0x0080) == 0) {
            auto walls = [&](bool right) {
                const int edge = right ? kMirrorRight : kMirrorLeft;
                const int aW = right ? A.wallR : A.wallL, bW = right ? B.wallR : B.wallL;
                const int cW = right ? C.wallR : C.wallL;
                const int aV = right ? A.vergeR : A.vergeL, bV = right ? B.vergeR : B.vergeL;
                const int aE = right ? A.right : A.left, cE = right ? C.right : C.left;
                auto inside = [&](int x) { return right ? x < kMirrorRight : x > kMirrorLeft; };
                if (fall == (right ? -1 : 1) || aW == A.y) return;
                if (k == 1 || (farF & 0x0080) == 0) {
                    poly({{edge, aW}, {aV - fall, A.y}, {bV + fall, B.y}, {edge, bW}}, brown);
                    if (k == 1 && inside(bV)) {
                        poly({{edge, aW}, {bV, B.y}, {edge, B.y}}, brown);
                    } else if (cW == C.y && inside(aV)) {
                        poly({{edge, aW}, {aV, A.y}, {edge, A.y}}, brown);
                    }
                } else {
                    poly({{edge, aW}, {aV, A.y}, {bV, B.y}, {edge, bW}}, brown);
                    poly({{edge, aW}, {aV, A.y}, {aE, A.y}, {cE, C.y}, {cE, kMirrorBottom}, {edge, kMirrorBottom}},
                         brown);
                }
            };
            walls(false);
            walls(true);
        }

        // FUN_0023bf84. Only a town row steps the verges down with the edges.
        {
            auto slope = [](int xNear, int yNear, int xFar, int yFar) {
                return yFar == yNear ? 0 : static_cast<int16_t>((xFar - xNear) << 4) / (yFar - yNear);
            };
            const int dl = slope(A.left, A.y, B.left, B.y);
            const int dr = slope(A.right, A.y, B.right, B.y);
            const int dvl = slope(A.vergeL, A.y, B.vergeL, B.y);
            const int dvr = slope(A.vergeR, A.y, B.vergeR, B.y);
            const bool fin = (F & 0x0010) && pos >= (mulholland ? 0x3ad : 0x169);
            const bool verges = mulholland && (F & 0x0080) == 0 && track.elev[pos] - 11 <= p.elevAtCar;
            const SDL_Color centreLine = grudge && pos - k < sim.StartSegment() + 5 ? pen4 : centre;
            int l16 = B.left << 4, r16 = B.right << 4, vl16 = B.vergeL << 4, vr16 = B.vergeR << 4;
            int l = B.left, r = B.right, vl = B.vergeL, vr = B.vergeR;
            for (int y = B.y, n = A.y - B.y; n > 0 && y <= kMirrorBottom && !(city && (F & 0x0004)); --n, ++y) {
                if (city) {
                    if (Lo & 0x0080) {
                        const int mid = vl + ((l - vl) >> 1);
                        MirrorSpan(vl, mid, y, pen3);
                        MirrorSpan(mid, l, y, grass);
                    } else {
                        MirrorSpan(vl, l, y, pen3);
                    }
                    if (Ro & 0x0080) {
                        const int mid = vr - ((vr - r) >> 1);
                        MirrorSpan(mid, vr, y, pen3);
                        MirrorSpan(r, mid, y, grass);
                    } else {
                        MirrorSpan(r, vr, y, pen3);
                    }
                } else {
                    if (verges) {
                        MirrorSpan(vl - 6, l, y, pen4);
                        MirrorSpan(r, vr + 6, y, pen4);
                    }
                    if (!mulholland || noWallL) MirrorSpan(kMirrorLeft, l, y, grass);
                    if (!mulholland || noWallR) MirrorSpan(r, kMirrorRight, y, grass);
                    MirrorSpan(l, r, y, fin ? finish : black);
                }
                // The mirror's centre line width table is all zeros.
                if (F & 0x0008) {
                    const int cx = (l + r) >> 1;
                    MirrorSpan(cx, cx, y, centreLine);
                }
                l16 += dl;
                r16 += dr;
                l = l16 >> 4;
                r = r16 >> 4;
                if (city) {
                    vl16 += dvl;
                    vr16 += dvr;
                    vl = vl16 >> 4;
                    vr = vr16 >> 4;
                }
            }
        }

        if ((F & 0x0004) == 0 || !city) {
            if ((F & 0xf000) && !grudge && !mulholland) {
                const int top = B.y - 4 * (B.y - B.yTop);
                portal(B.left, B.right, top, B.y);
                const int cx = (B.left + B.right) >> 1;
                const int scale = k + 5;
                if (F & 0x1000) {
                    MirrorRoadside(13, cx, top, scale);
                } else if (F & 0x2000) {
                    MirrorBanner(cruiseMode && cruiseSignSprite ? cruiseSignSprite.get() : signCountyLineSprite.get(),
                                 cx, top, scale);
                } else if (city) {
                    MirrorBanner(Sr2Asset((F & 0x4000) ? SR2_RACING_SIGN_TRY_VEGAS_GAMBLER
                                                       : SR2_RACING_SIGN_PLAY_BLOCKOUT),
                                 cx, top, scale);
                } else {
                    MirrorRoadside((F & 0x4000) ? 12 : 14, cx, top, scale);
                }
            }

            if (k < 7) {
                const int L = B.left, R = B.right, vL = B.vergeL, vR = B.vergeR, y = B.y;
                const int s = (k >> 1) + 6;
                if (!mulholland && (Lo & 0x0020)) MirrorRoadside(0, L, y, s);
                if (Lo & 0x0100) {
                    if (city) {
                        MirrorRoadside(4, L, y, s);
                    } else if (!mulholland || noWallL) {
                        MirrorRoadside(mulholland ? 20 : 4, (Lo & 0x0200) ? 2 * vL - L : vL, y, s);
                    }
                }
                if (!drag && !grudge && (Lo & 0x0800)) MirrorRoadside(3, L, y, s);
                if (!drag && !grudge && (Lo & 0x1000)) MirrorRoadside(6, L, y, s);
                if (!grudge && (Lo & 0x0001) && noWallL) MirrorRoadside(1, L, y, s);
                if (!grudge && (Ro & 0x0001) && noWallR) MirrorRoadside(1, R, y, s);
                if (!mulholland || noWallL) {
                    if (Lo & 0x0040) MirrorRoadside(10, vL, y, s);
                    if ((Lo & 0x0080) && !city) {
                        MirrorRoadside(mulholland ? 21 : 11, (Lo & 0x0002) ? (vL + kMirrorLeft) >> 1 : vL, y, s);
                    }
                }
                if (!mulholland || noWallR) {
                    if (Ro & 0x0040) MirrorRoadside(10, vR, y, s);
                    if ((Ro & 0x0080) && !city) {
                        MirrorRoadside(mulholland ? 21 : 11, (Ro & 0x0002) ? (B.outerR + kMirrorRight) >> 1 : vR, y, s);
                    }
                }
                if (mulholland && (Lo & 0x0010)) pile(L, y, s, 1);
                if (mulholland && (Ro & 0x0010)) pile(R, y, s, -1);
                if (city) {
                    if (Lo & 0x0040) MirrorRoadside(2, L, y, s);
                    if (Ro & 0x0040) MirrorRoadside(2, R, y, s);
                } else {
                    if (drag || noWallL) {
                        if (Lo & 0x0004) MirrorRoadside(7, vL, y, s);
                        if (Lo & 0x0008) MirrorRoadside(8, vL, y, s);
                    }
                    if (drag || noWallR) {
                        if (Ro & 0x0004) MirrorRoadside(7, vR, y, s);
                        if (Ro & 0x0008) MirrorRoadside(8, vR, y, s);
                    }
                }
                if (!city && (F & 0x0002) && (!mulholland || noWallR) && !grudge) {
                    MirrorRoadside(9, vR - ((R - L) >> 4), y, s);
                }
                if (!mulholland && (Ro & 0x0020)) MirrorRoadside(0, R, y, s);
                if (Ro & 0x0100) {
                    if (city) {
                        MirrorRoadside(4, R, y, s);
                    } else if (!mulholland || noWallR) {
                        MirrorRoadside(mulholland ? 20 : 4, (Ro & 0x0200) ? 2 * vR - R : vR, y, s);
                    }
                }
                if (Ro & 0x2000) MirrorRoadside(18, R, y, s);
                if (Ro & 0x4000) MirrorRoadside(19, R, y, s);
                if (Ro & 0x0800) MirrorRoadside(3, R, y, s);
                if (Ro & 0x1000) MirrorRoadside(6, R, y, s);
                const int laneX = L + (static_cast<int16_t>((R - L) * track.lane[seg]) >> 4);
                if (!drag && !grudge && (F & 0x0004)) MirrorRoadside(7, laneX, y, s);
                if (mulholland && (F & 0x1000)) MirrorRoadside(22, laneX, y, s);
                if (F & 0x0100) MirrorRoadside(5, R, y, s);
                if (F & 0x0200) MirrorRoadside(16, R, y, s);
                if (F & 0x0400) MirrorRoadside(17, R, y, s);
                if (F & 0x0800) MirrorRoadside(15, R, y, s);
            }

            // The starter, left at the line (jlights.c FUN_00244b42).
            if ((drag || mulholland) && sr2StarterSeg >= 0 && pos - sr2StarterSeg == k) {
                DrawStarterInMirror(k - 1, (B.left + B.right) >> 1, B.y - kViewOffsetY, false, kAqueductMirrorClip);
            }

            if (!city && mulholland) {
                const int aTop = A.y - 4 * (A.y - A.yTop);
                const int bTop = B.y - 4 * (B.y - B.yTop);
                if ((F & 0x0080) && (track.flags[segAt(seg + 1)] & 0x0080) && (track.flags[segAt(seg + 2)] & 0x0080)) {
                    portal(A.left, A.right, aTop, A.y);
                    if (((pos - k) & 1) && k < 4) {
                        line(A.left - 1, aTop, B.left - 1, B.y, red);
                        line(A.right + 1, aTop, B.right + 1, B.y, red);
                    } else {
                        line(A.left - 1, A.y, B.left - 1, bTop, red);
                        line(A.right + 1, A.y, B.right + 1, bTop, red);
                    }
                    line(A.left - 1, aTop, B.left - 1, bTop, red);
                    line(A.right + 1, aTop, B.right + 1, bTop, red);
                    line(A.left - 1, A.y, B.left - 1, B.y, red);
                    line(A.right + 1, A.y, B.right + 1, B.y, red);
                }
                if ((F & 0x0080) && (farF & 0x0080) == 0) {
                    portal(B.left, B.right, bTop, B.y);
                }
                if ((Ro & 0x8000) && (F & 0x0080) == 0) {
                    if ((railPhaseRow - k - pos + 8) & 1) {
                        const int w = 10 / (k + 1);
                        poly({{A.vergeR - w, A.yMid - 1}, {A.vergeR - w, A.y}, {A.vergeR, A.y}, {A.vergeR, A.yMid - 1}},
                             pen4);
                    }
                    poly({{B.vergeR, B.yTop - 1}, {B.vergeR, B.yMid}, {A.vergeR, A.yMid}, {A.vergeR, A.yTop - 1}}, pen3);
                    line(B.vergeR, B.yTop - 1, A.vergeR, A.yTop - 1, pen4);
                    line(B.vergeR, B.yMid, A.vergeR, A.yMid, pen4);
                }
            }
        }

        // In row order, so nearer roadside objects cover it.
        if (opponentActive && !ai.viewHidden && ai.viewState >= 9 && npcRow == k) {
            Bitmap* behind = OpponentSpriteForState(ai.viewState);
            SDL_Surface* bs = behind ? behind->GetSurface() : nullptr;
            if (bs) {
                const srrace::CarState& np = viewNpc;
                const int npcSeg = ClampInt(static_cast<int>(np.segPos), 0, kTrackSegments - 1);
                const int d = -((static_cast<int>(np.segPos) - static_cast<int>(p.segPos)) * 8 +
                                (np.scrollAcc - p.scrollAcc));
                const int x = mirrorX(d, np.lateral, 10 + track.curve[npcSeg] - p.heading);
                const int y = mirrorY(d, 2000, grudge ? 0 : track.elev[npcSeg] - p.elevAtCar);
                MirrorBlit(behind, {0, 0, static_cast<uint16_t>(bs->w), static_cast<uint16_t>(bs->h)},
                           x - (bs->w >> 1), y - bs->h);
            }
        }
    }
}

// The door mirror is the roof mirror's left-hand corner, re-blitted every frame; Driving punches the interior's
// glass so it shows through.
void Racing::DrawSideMirror() {
    if (!pixels || pixels->format->BytesPerPixel != 4) {
        return;
    }
    Uint32* px = static_cast<Uint32*>(pixels->pixels);
    const int stride = pixels->pitch / 4;
    for (int row = 0; row < kSideMirrorRows; ++row) {
        const SideMirrorRow& g = kSideMirrorGlass[row];
        const int w = g.right - g.left + 1;
        const int srcY = kSideMirrorSrcY - kViewOffsetY + row;
        const int dstY = kSideMirrorTop - kViewOffsetY + row;
        if (kSideMirrorSrcX + w > GAME_WIDTH || g.right >= GAME_WIDTH ||
            srcY < 0 || dstY + 1 > GAME_RACING_HEIGHT) {
            continue;
        }
        std::memcpy(&px[dstY * stride + g.left],
                    &px[srcY * stride + kSideMirrorSrcX], w * sizeof(Uint32));
    }
}

void Racing::DrawHud() {
    if (!hud) {
        return;
    }
    if (cruiseMode) {
        // A cutscene has no gear readout.
        return;
    }
    char gearText[16];
    if (IsAutomatic()) {
        std::snprintf(gearText, sizeof(gearText), "Gear: %s",
                      sim.Player().gear == 0 ? "N" : "D");
    } else if (sim.Player().gear == 0) {
        std::snprintf(gearText, sizeof(gearText), "Gear: N");
    } else {
        std::snprintf(gearText, sizeof(gearText), "Gear: %d", sim.Player().gear);
    }
    hud->Clear();
    hud->AddText(6, 0, gearText);
    if (sim.InCountdown()) {
        hud->AddText(104, 45, "STAGE - WAIT FOR GREEN");
    }
    hud->Draw();
}

// The dial table is relative to the dashboard, which starts at y=100 (scanline 144 in the copperlist).
void Racing::DrawSr2Needle(SDL_Renderer* renderer, int type, int gaugeValue) {
    if (!renderer || !sr2Gauges.Ready()) {
        return;
    }
    const srrace::Sr2Needle n = sr2Gauges.Tip(type + 2 * sr2DashboardVariant, gaugeValue);
    SDL_SetRenderDrawColor(renderer, 96, 64, 0, 255);
    SDL_RenderDrawLine(renderer, n.x0, 100 + n.y0, n.x1, 100 + n.y1);
}

void Racing::DrawTachometerNeedleToRenderer(SDL_Renderer* renderer) {
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        DrawSr2Needle(renderer, 1, sim.Player().rpm / 43);
        return;
    }
    // The dial takes rpm / 43 and clamps at 130, so full scale is 5590: the needle pegs just before the 5700 redline,
    // the over-rev warning.
    DrawGaugeNeedle(renderer, 145, 129, sim.Player().rpm / 43);
}

void Racing::DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer) {
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        DrawSr2Needle(renderer, 0, sim.Player().speed);
        return;
    }
    // The dial clamps at 130, so full scale is 130 mph.
    DrawGaugeNeedle(renderer, 196, 129, sim.Player().speed);
}
