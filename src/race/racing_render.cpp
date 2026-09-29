#include "race/racing.h"
#include "race/racing_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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

void Racing::UpdateRenderView() {
    viewPlayer = sim.Player();
    viewNpc = sim.Npc();
    if (flybyMode || !havePrevStep || !state.GetAssets().GetSmoothRacingRef()) return;
    const float t = std::clamp(fixedAccumulator / StepSeconds(), 0.0f, 1.0f);
    srrace::BlendCarView(prevPlayer, viewPlayer, t);
    srrace::BlendCarView(prevNpc, viewNpc, t);
}

int Racing::BuildRowTableAndGetLastVisible() {
    const srrace::CarState& p = viewPlayer;
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);
    const int elevAtCar = p.elevAtCar;

    for (int k = 0; k < kForwardRows; ++k) {
        int d = 4;
        if (k > 0) {
            d = 12 + 8 * (k - 1) - p.scrollAcc;
            d = std::max(4, d);
        }
        // The horizontal projection reads curve[], the vertical elev[]. Do not swap them.
        const int seg = ClampInt(segBase + k + 1, 0, kTrackSegments - 1);
        const int roadDelta = p.heading - track.curve[seg];
        const int rowElevDelta = elevAtCar - track.elev[seg];
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);

        RowProjection row;
        row.distance = d;
        row.seg = seg;
        row.roadDelta = roadDelta;
        row.leftX = ProjectX(p, d, roadDelta, edgeLeft);
        row.rightX = ProjectX(p, d, roadDelta, edgeRight);
        row.centerX = ProjectX(p, d, roadDelta, kRoadCenterLat);
        row.shoulderLX = ProjectX(p, d, roadDelta, edgeLeft - 400);
        row.shoulderRX = ProjectX(p, d, roadDelta, edgeRight + 400);
        row.y = ClampInt(kHorizonY + 400 / d + 4 * rowElevDelta, kClipTop, kClipBottom + 30);
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

// lib.p asset 97, two 24x32 frames: red during staging, green at launch, then retracts 4 a step as the car rolls.
void Racing::DrawStartLight() {
    if (flybyMode || !startLightSprite || !startLightSprite->GetSurface()) {
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

void Racing::RenderRoad(int lastVisible) {
    auto drawDash = [&](int y, int cx, int drawL, int drawR, uint16_t flags) {
        if ((flags & 0x0008) == 0) {
            return;
        }
        const SDL_Color dashColor = (flags & 0x0010) ? kDashWhite : kDashYellow;
        for (int i = 0; i < 4; ++i) {
            const int px = cx - 1 + i;
            if (px >= drawL && px <= drawR) {
                PutPixel(pixels, px, y, dashColor);
            }
        }
    };

    // The whole surface: the bottom rows show through the side-mirror gap.
    for (int y = kClipTop; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kGrassA);
        }
    }

    for (int k = 0; k < lastVisible; ++k) {
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
            if (right < 0 || left >= GAME_WIDTH) {
                continue;
            }

            const int seg = (fromNear * 2 >= dy) ? nearRow.seg : farRow.seg;
            const uint16_t flags = track.flags[seg];
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

            drawDash(y, cx, drawL, drawR, flags);
        }
    }

    if (lastVisible >= 1) {
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
            for (int x = left; x <= right; ++x) {
                PutPixel(pixels, x, y, nearRoadColor);
            }
            drawDash(y, cx, left, right, nearFlags);
        }
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

void Racing::DrawRoadsideAndArchSprites(int lastVisible) {
    auto drawSlot = [&](int slot, int k, int side, bool centerInRoad) {
        if (slot < 0 || slot >= 20 || !roadsideSprites[slot]) {
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

    for (int k = std::min(lastVisible, kForwardRows - 1); k >= 1; --k) {
        const int seg = rows[k].seg;
        if (seg < 0 || seg >= kTrackSegments) {
            continue;
        }
        const uint16_t L = track.left[seg];
        const uint16_t R = track.right[seg];
        const uint16_t F = track.flags[seg];
        const bool city = (F & 0x0001) != 0;

        if ((F & 0x0004) == 0) {
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
            if (!city && (L & 0x0080)) drawSlot(11, k, -1, false);

            if (!city && (R & 0x0004)) drawSlot(7, k, +1, false);
            if (!city && (R & 0x0008)) drawSlot(8, k, +1, false);
            if (R & 0x0040) drawSlot(city ? 2 : 10, k, +1, false);
            if (!city && (R & 0x0080)) drawSlot(11, k, +1, false);
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

        if (F & 0xf000) {
            DrawOverheadArch(rows[k], F, ClampInt(k - 2, 0, 10));
        }
    }
}

// 13 sprites per car in one asset run: front-side, rear-side, 7 rear LOD frames, 4 mirror frames, indexed by
// view state.
Bitmap* Racing::OpponentSpriteForState(int state) const {
    if (state < 0 || state > 12) {
        return nullptr;
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
    int y = kHorizonY + 400 / d + 4 * elevDelta;

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

void Racing::DrawRearviewMirror() {
    const int mirrorX = 242;
    const int mirrorY = 9;
    const int mirrorW = 76;
    const int mirrorH = 21;

    // Horizon backdrop, the road behind with centre dashes, roadside LODs, then the NPC, clipped to the glass.
    const int gLeft = mirrorX + 1;
    const int gRight = mirrorX + mirrorW - 2;
    const int gTop = mirrorY + 1;
    const int gBottom = mirrorY + mirrorH - 2;
    const int centerX = mirrorX + mirrorW / 2;   // 280, matches ProjectMirrorX
    constexpr int kMirrorRows = 8;               // BuildMirrorRowTables walks 8
    const srrace::CarState& p = viewPlayer;
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);

    // Bottom-left anchored at (dstLeft, dstBottom), never spilling outside the glass.
    auto blitClipped = [&](Bitmap* bmp, const SpriteRect& fr, int dstLeft, int dstBottom) {
        SDL_Surface* s = bmp ? bmp->GetSurface() : nullptr;
        if (!s) return;
        for (int yy = 0; yy < fr.h; ++yy) {
            const int py = dstBottom - (fr.h - 1) + yy;
            if (py < gTop || py > gBottom) continue;
            for (int xx = 0; xx < fr.w; ++xx) {
                const int px = dstLeft + xx;
                if (px < gLeft || px > gRight) continue;
                Uint32 sp = 0;
                const Uint8* ptr = static_cast<const Uint8*>(s->pixels) + (fr.y + yy) * s->pitch + (fr.x + xx) * s->format->BytesPerPixel;
                std::memcpy(&sp, ptr, s->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, s->format, &r, &g, &b, &a);
                if (a > 0) PutPixel(pixels, px, py, {r, g, b, a});
            }
        }
    };

    for (int y = gTop; y <= gBottom; ++y) {
        for (int x = gLeft; x <= gRight; ++x) {
            PutPixel(pixels, x, y, kGrassA);
        }
    }

    int horizonY = gTop + 6;
    if (rearviewMirrorSprite && rearviewMirrorSprite->GetSurface()) {
        SDL_Surface* h = rearviewMirrorSprite->GetSurface();
        const int hh = std::min(h->h, gBottom - gTop);
        for (int yy = 0; yy < hh; ++yy) {
            for (int x = gLeft; x <= gRight; ++x) {
                const int sx = 1 + (x - gLeft);           // skip the 1px border
                if (sx >= h->w) continue;
                Uint32 sp = 0;
                const Uint8* ptr = static_cast<const Uint8*>(h->pixels) + yy * h->pitch + sx * h->format->BytesPerPixel;
                std::memcpy(&sp, ptr, h->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, h->format, &r, &g, &b, &a);
                if (a > 0) PutPixel(pixels, x, gTop + yy, {r, g, b, a});
            }
        }
        horizonY = gTop + hh - 1;
    }
    // The mirror's horizon is fixed at screen y 36.
    horizonY = ClampInt(36 - kViewOffsetY, gTop, gBottom);

    // The forward projection at quarter scale centred on 280: 8 rows backwards, one segment apart, at depths
    // scrollAcc + 8*(row+1), with the same edges and narrowing test.
    auto projectMirrorX = [&](int d, int roadDelta, int lateral) {
        const int adjustedLat = lateral + (roadDelta * d) / 4;
        const int numerator = (d >> 1) + (p.lateral - adjustedLat);
        return centerX - numerator / d;
    };

    struct MirrorRow {
        int y, left, right, shoulderL, shoulderR, center, seg;
    };
    MirrorRow mrows[kMirrorRows];
    for (int k = 0; k < kMirrorRows; ++k) {     // k 0 = nearest
        const int d = std::max(1, p.scrollAcc + 8 * (k + 1));
        const int seg = ClampInt(segBase - (k + 1), 0, kTrackSegments - 1);
        const int roadDelta = p.heading - track.curve[seg];
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);
        MirrorRow& row = mrows[k];
        row.seg = seg;
        // 36 + 100/d + row_c at quarter scale.
        row.y = horizonY + 100 / d + (p.elevAtCar - track.elev[seg]);
        row.left = projectMirrorX(d, roadDelta, edgeLeft);
        row.right = projectMirrorX(d, roadDelta, edgeRight);
        row.shoulderL = projectMirrorX(d, roadDelta, edgeLeft - 400);
        row.shoulderR = projectMirrorX(d, roadDelta, edgeRight + 400);
        row.center = projectMirrorX(d, roadDelta, kRoadCenterLat);
    }

    auto paintRow = [&](int y, int left, int right, int center, int seg) {
        if (y < gTop || y > gBottom) return;
        const int rl = std::max(gLeft, left);
        const int rr = std::min(gRight, right);
        for (int x = rl; x <= rr; ++x) {
            PutPixel(pixels, x, y, kRoadA);
        }
        // Alternate rows so the centre line reads as dashes.
        if ((track.flags[seg] & 0x0008) && ((y & 1) == 0) && center >= rl && center <= rr) {
            PutPixel(pixels, center, y, (track.flags[seg] & 0x0010) ? kDashWhite : kDashYellow);
        }
    };
    for (int k = kMirrorRows - 1; k >= 0; --k) {
        const MirrorRow& f = mrows[k];
        const int yEnd = (k == 0) ? gBottom : mrows[k - 1].y - 1;
        const int span = std::max(1, yEnd - f.y + 1);
        for (int y = f.y; y <= yEnd; ++y) {
            int l = f.left;
            int r = f.right;
            int c = f.center;
            if (k > 0) {
                const MirrorRow& n = mrows[k - 1];
                const int t = ((y - f.y) * 256) / span;
                l += ((n.left - f.left) * t) / 256;
                r += ((n.right - f.right) * t) / 256;
                c += ((n.center - f.center) * t) / 256;
            }
            paintRow(y, l, r, c, f.seg);
        }
    }

    auto slotForFlags = [](uint16_t f) -> int {
        if (f & 0x0001) return 1;   // building / main
        if (f & 0x0020) return 0;   // palm
        if (f & 0x0100) return 4;
        if (f & 0x0800) return 3;
        if (f & 0x0040) return 10;
        if (f & 0x0004) return 7;
        if (f & 0x0008) return 8;
        return -1;
    };
    for (int k = 1; k < kMirrorRows; ++k) {
        const MirrorRow& row = mrows[k];
        if (row.y < horizonY || row.y > gBottom) continue;
        for (int side = 0; side < 2; ++side) {
            const uint16_t f = side ? track.right[row.seg] : track.left[row.seg];
            const int slot = slotForFlags(f);
            if (slot < 0 || !roadsideSprites[slot]) continue;
            const SpriteAnimationInfo* anim = roadsideAnim[slot];
            if (!anim || !anim->frames || anim->frameCount <= 0) continue;
            const SpriteRect& fr = anim->frames[anim->frameCount - 1];   // farthest/smallest
            const int dstLeft = side ? row.shoulderR : (row.shoulderL - fr.w);
            blitClipped(roadsideSprites[slot].get(), fr, dstLeft, row.y);
        }
    }

    // Quarter scale: elevated y is row.y - 400/d, crossbar four times that above the road. No banner art at 74 px.
    auto putGlass = [&](int x, int y) {
        if (x >= gLeft && x <= gRight && y >= gTop && y <= gBottom) {
            PutPixel(pixels, x, y, kArchColor);
        }
    };
    for (int k = 0; k < kMirrorRows; ++k) {
        const MirrorRow& row = mrows[k];
        if ((track.flags[row.seg] & 0xf000) == 0 || row.right - row.left < 4) {
            continue;
        }
        const int d = std::max(1, p.scrollAcc + 8 * (k + 1));
        const int yBottom = std::min(row.y, gBottom);
        const int yTop = std::max(gTop, row.y - 400 / d);
        if (yBottom <= yTop) {
            continue;
        }
        for (int y = yTop; y <= yBottom; ++y) {
            putGlass(row.left, y);
            putGlass(row.left + 1, y);
            putGlass(row.right, y);
            putGlass(row.right - 1, y);
        }
        const int barHeight = ClampInt((yBottom - yTop) / 6, 1, 3);
        for (int y = yTop; y < yTop + barHeight; ++y) {
            for (int x = row.left; x <= row.right; ++x) {
                putGlass(x, y);
            }
        }
    }

    if (opponentActive) {
        const int delta = static_cast<int>(viewNpc.segPos - viewPlayer.segPos);
        if (delta < 0 && delta >= -8) {
            const int idx = ClampInt((-delta) / 2, 0, 3);
            Bitmap* behind = nullptr;
            switch (opponentCarType) {
                case 1: behind = opponentChryslerBehind[idx].get(); break;
                case 2: behind = opponentCopBehind[idx].get(); break;
                case 3: behind = opponentOldsmobileBehind[idx].get(); break;
                case 4: behind = opponentChevy2Behind[idx].get(); break;
                case 5: behind = opponentCorvette63Behind[idx].get(); break;
                case 6: behind = opponentFordBehind[idx].get(); break;
                default: behind = opponentChevyBehind[idx].get(); break;
            }
            if (behind && behind->GetSurface()) {
                SDL_Surface* bs = behind->GetSurface();
                // Native LOD frame, cropped to fit the glass.
                const int w = std::min(bs->w, mirrorW - 2);
                const int h = std::min(bs->h, mirrorH - 2);
                const int cx = mirrorX + mirrorW / 2 - w / 2;
                const int cy = mirrorY + mirrorH - h - 1;
                DrawSprite(behind, cx, cy, false, false, 0, 0, w, h);
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

void Racing::DrawTachometerNeedleToRenderer(SDL_Renderer* renderer) {
    // The dial takes rpm / 43 and clamps at 130, so full scale is 5590: the needle pegs just before the 5700 redline,
    // the over-rev warning.
    DrawGaugeNeedle(renderer, 145, 129, sim.Player().rpm / 43);
}

void Racing::DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer) {
    // The dial clamps at 130, so full scale is 130 mph.
    DrawGaugeNeedle(renderer, 196, 129, sim.Player().speed);
}
