#include "race/aqueduct.h"

#include "parsers/libp.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace srrace {

// The shape records store their asset number one based.
static_assert(AQU_SHAPE_ASSET_FIRST - 1 == SR2_AQUEDUCT_CLOUD_WIDE,
              "aqushape record 0 is the wide cloud");
static_assert(AQU_SHAPE_ASSET_FIRST - 1 + AQU_SHAPE_LADDER_FIRST ==
                  SR2_AQUEDUCT_ROCK_NEAREST,
              "the ladder starts at the nearest rock");
static_assert(AQU_SHAPE_ASSET_FIRST - 1 + AQU_SHAPE_LADDER_FIRST +
                      AQU_SHAPE_LADDER_COUNT - 1 == SR2_AQUEDUCT_ROCK_FARTHEST,
              "the ladder ends at the farthest rock");
static_assert(AQU_SHAPE_ASSET_FIRST - 1 + AQU_SHAPE_COUNT - 1 ==
                  SR2_AQUEDUCT_ROCK_FARTHEST,
              "nine records, ending at the farthest rock");

void BuildAqueductRoad(TrackData& road) {
    for (int s = 0; s < TRACK_SEGMENTS; ++s) {
        road.left[s] = 0;
        road.right[s] = 0;
        road.flags[s] = 0;   // width bits clear: the widest road, 1500..3300
        road.elev[s] = 0;
        road.curve[s] = 0;
        road.lane[s] = 0;
        road.wallLeft[s] = 0;
        road.wallRight[s] = 0;
    }
}

void BuildAqueductTrack(AqueductTrack& track) {
    uint16_t* flags = track.flags;
    for (int s = 0; s < AQUEDUCT_SEGMENTS; ++s) flags[s] = 0;

    // Four segments in every twenty-four are set outright, not or'd, so this
    // has to run before anything that adds bits.
    for (int s = 6; s < AQUEDUCT_SEGMENTS; s += 24) {
        for (int i = s - 3; i <= s; ++i) flags[i] = 2;
    }
    for (int s = 1; s < AQUEDUCT_SEGMENTS; s += 6) flags[s] |= 0x0001;
    for (int s = 18; s < AQUEDUCT_SEGMENTS; s += 24) {
        flags[s - 1] |= 0x0004;
        flags[s - 2] |= 0x0008;
    }

    // Two places swap that pair for a different one.
    for (int s : {137, 281}) {
        flags[s] &= static_cast<uint16_t>(~0x0004);
        flags[s] |= 0x0100;
    }
    for (int s : {136, 280}) {
        flags[s] &= static_cast<uint16_t>(~0x0008);
        flags[s] |= 0x0200;
    }

    for (int s : {4, 76, 220, 316, 364}) flags[s] |= AQU_OBSTACLE_SPAN;
    for (int s : {124, 268}) flags[s] |= AQU_OBSTACLE_SIDES;
    for (int s : {5, 34, 59, 100, 179, 214, 322}) flags[s] |= 0x0020;
    for (int s : {23, 82, 130, 155, 238, 262, 358}) flags[s] |= 0x0040;
    flags[342] |= 0x0400;

    for (int s = 0; s < AQUEDUCT_SEGMENTS; ++s) {
        track.mirror[s] = flags[AQUEDUCT_SEGMENTS - 1 - s];
    }
}

namespace {

// One byte per degree, 0..90. Filled from the executable by
// AqueductLoadSineTable; the values here are the computed fallback.
uint8_t gSine[AQU_SINE_TABLE_ENTRIES];
bool gSineReady = false;

void EnsureSineTable() {
    if (gSineReady) return;
    for (int d = 0; d < AQU_SINE_TABLE_ENTRIES; ++d) {
        const double v = AQU_TRIG_SCALE * std::sin(d * 3.14159265358979323846 / 180.0);
        gSine[d] = static_cast<uint8_t>(std::min(255.0, v + 0.5));
    }
    gSineReady = true;
}

}  // namespace

bool AqueductLoadSineTable(const std::vector<uint8_t>& exeData) {
    if (exeData.size() < AQU_SINE_TABLE_OFFSET + AQU_SINE_TABLE_ENTRIES) {
        EnsureSineTable();
        return false;
    }
    const uint8_t* src = exeData.data() + AQU_SINE_TABLE_OFFSET;
    // A sine table rises to its peak and never exceeds a byte.
    for (int d = 1; d < AQU_SINE_TABLE_ENTRIES; ++d) {
        if (src[d] < src[d - 1]) { EnsureSineTable(); return false; }
    }
    if (src[0] != 0 || src[AQU_SINE_TABLE_ENTRIES - 1] < 250) {
        EnsureSineTable();
        return false;
    }
    std::memcpy(gSine, src, AQU_SINE_TABLE_ENTRIES);
    gSineReady = true;
    return true;
}

int AqueductSine(int degrees) {
    EnsureSineTable();

    int a = degrees;
    if (a < -360) a += 360;
    else if (a > 360) a -= 360;

    bool negate = a < 0;
    if (negate) a = -a;

    if (a > 89) {
        if (a < 180) {
            a = 180 - a;
        } else if (a < 270) {
            a -= 180;
            negate = !negate;
        } else {
            if (a > 359) return 0;
            a = 360 - a;
            negate = !negate;
        }
    }
    const int v = gSine[a & 0xff];
    return negate ? -v : v;
}

int AqueductBankAngle(int rideBank, int roll) {
    int a = rideBank + roll;
    if (a > 0x167) a -= 0x168;
    if (a < -0x167) a += 0x168;
    return a;
}

AqueductBank AqueductBankFor(int pivotX, int pivotY, int bankDegrees) {
    AqueductBank bank;
    bank.pivotX = pivotX;
    bank.pivotY = pivotY;
    bank.sin = AqueductSine(bankDegrees);
    bank.cos = AqueductSine(bankDegrees + 90);
    return bank;
}

void AqueductBankPoint(const AqueductBank& bank, int x, int y, int& outX, int& outY) {
    const int dx = x - bank.pivotX;
    const int dy = y - bank.pivotY;
    outX = bank.pivotX + ((dy * bank.sin + dx * bank.cos) >> 8);
    outY = bank.pivotY + ((dy * bank.cos - dx * bank.sin) >> 8);
}

int AqueductScanQuad(const AqueductQuad& quad, int clipTop, int clipBottom,
                     int screenWidth, AqueductSpan* out, int maxSpans) {
    if (out == nullptr || maxSpans <= 0 || screenWidth <= 0) return 0;

    int top = quad.y[0];
    int bottom = quad.y[0];
    for (int i = 1; i < 4; ++i) {
        top = std::min(top, quad.y[i]);
        bottom = std::max(bottom, quad.y[i]);
    }
    top = std::max(top, clipTop);
    bottom = std::min(bottom, clipBottom);

    int written = 0;
    for (int y = top; y <= bottom && written < maxSpans; ++y) {
        // Where each edge crosses this scanline. The shape is convex, so the
        // leftmost and rightmost crossings bound the run.
        int lo = 0;
        int hi = 0;
        bool any = false;

        for (int i = 0; i < 4; ++i) {
            const int j = (i + 1) & 3;
            const int ya = quad.y[i];
            const int yb = quad.y[j];
            if (ya == yb) continue;                       // horizontal edge
            if (y < std::min(ya, yb) || y > std::max(ya, yb)) continue;

            const int xa = quad.x[i];
            const int xb = quad.x[j];
            const int x = xa + ((xb - xa) * (y - ya)) / (yb - ya);
            if (!any) {
                lo = hi = x;
                any = true;
            } else {
                lo = std::min(lo, x);
                hi = std::max(hi, x);
            }
        }
        if (!any) continue;

        lo = std::max(lo, 0);
        hi = std::min(hi, screenWidth - 1);
        if (lo > hi) continue;                            // entirely off screen

        out[written].y = y;
        out[written].x0 = lo;
        out[written].x1 = hi;
        ++written;
    }
    return written;
}

AqueductBank AqueductBankForRows(const AqueductRow* rows, int rowCount, int bankDegrees) {
    if (rows == nullptr || rowCount <= 0) return AqueductBankFor(0, 0, bankDegrees);
    // The pivot is the furthest row's midpoint, the vanishing point.
    const AqueductRow& far = rows[rowCount - 1];
    const int pivotX = (far.leftX + far.rightX) >> 1;
    return AqueductBankFor(pivotX, far.y, bankDegrees);
}

int AqueductBuildQuads(const AqueductRow* rows, int rowCount, const AqueductBank& bank,
                       AqueductQuad* out, int maxQuads) {
    if (rows == nullptr || out == nullptr) return 0;

    int written = 0;
    for (int r = 0; r + 1 < rowCount; ++r) {
        const AqueductRow& near = rows[r];
        const AqueductRow& far = rows[r + 1];

        // Each band is wound near-left, near-right, far-right, far-left, so a
        // filler can walk the points in order without sorting them.
        const int xs[AqueductSurfaceCount][4] = {
            {near.leftX, near.rightX, far.rightX, far.leftX},
            {near.outerLeftX, near.leftX, far.leftX, far.outerLeftX},
            {near.rightX, near.outerRightX, far.outerRightX, far.rightX},
        };
        // The walls rise from the channel edge at y to the outer edge at y2.
        const int ys[AqueductSurfaceCount][4] = {
            {near.y, near.y, far.y, far.y},
            {near.y2, near.y, far.y, far.y2},
            {near.y, near.y2, far.y2, far.y},
        };

        for (int s = 0; s < AqueductSurfaceCount && written < maxQuads; ++s, ++written) {
            AqueductQuad& q = out[written];
            q.surface = static_cast<AqueductSurface>(s);
            q.nearRow = r;
            for (int i = 0; i < 4; ++i) {
                AqueductBankPoint(bank, xs[s][i], ys[s][i], q.x[i], q.y[i]);
            }
        }
    }
    return written;
}

int AqueductClampCameraX(int camX) {
    if (camX < AQU_CAM_CLAMP_MIN) return AQU_CAM_CLAMP_MIN;
    if (camX > AQU_CAM_CLAMP_MAX) return AQU_CAM_CLAMP_MAX;
    return camX;
}

namespace {

// The bend term both horizontal projections share.
int BentOffset(int depth, int offset, int slope) {
    return ((slope * depth) >> 2) + offset;
}

int SafeDepth(int depth) { return depth == 0 ? 1 : depth; }

}  // namespace

int AqueductProjectViewX(int camX, int depth, int offset, int slope) {
    const int t = AqueductClampCameraX(camX) - BentOffset(depth, offset, slope);
    return 0xa0 - (t * 4) / SafeDepth(depth);
}

int AqueductProjectMirrorX(int camX, int depth, int offset, int slope) {
    const int t = AqueductClampCameraX(camX) - BentOffset(depth, offset, slope);
    return 0x118 - t / SafeDepth(depth);
}

int AqueductProjectY(int camY, int depth, int offset, int vjerk) {
    const int t = offset - camY;
    return (t * 4) / SafeDepth(depth) + vjerk + 0x3c;
}

int AqueductProjectMirrorY(int camY, int depth, int offset) {
    const int t = offset - camY;
    return t / SafeDepth(depth) + 0x24;
}

int AqueductBuildBlocks(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                        int flagBase, AqueductBlock* out, int maxOut) {
    if (rows == nullptr || flags == nullptr || out == nullptr) return 0;
    int n = 0;

    // Far to near, which is the order they have to be painted in.
    for (int r = rowCount; r > 0 && n < maxOut; --r) {
        const int seg = flagBase + r;
        if (seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        const uint16_t f = flags[seg];
        if ((f & AQU_OBSTACLE_MASK) == 0) continue;

        const AqueductRow& fr = rows[r];       // the far end of the band
        const AqueductRow& nr = rows[r - 1];   // the near end

        if ((f & AQU_OBSTACLE_SPAN) != 0) {
            AqueductBlock& b = out[n++];
            b.farLeft = fr.leftX + (fr.midX - fr.leftX) / 3;
            b.farRight = fr.rightX - (fr.rightX - fr.midX) / 3;
            b.farBottom = fr.y;
            b.farTop = fr.y - (fr.y - fr.y2) / 7;
            b.nearLeft = nr.leftX + (nr.midX - nr.leftX) / 3;
            b.nearRight = nr.rightX - (nr.rightX - nr.midX) / 3;
            b.nearBottom = nr.y;
            b.nearTop = nr.y - (nr.y - nr.y2) / 7;
            continue;
        }

        // A pillar against each wall, and these run the full wall height.
        const int fInset = (fr.rightX - fr.leftX) / 7;
        const int nInset = (nr.rightX - nr.leftX) / 7;
        const int fThick = fInset >> 3;
        const int nThick = nInset >> 3;
        for (int side = 0; side < 2 && n < maxOut; ++side) {
            AqueductBlock& b = out[n++];
            b.pillar = true;
            if (side == 0) {
                b.farLeft = fr.leftX + fInset;
                b.nearLeft = nr.leftX + nInset;
            } else {
                b.farLeft = fr.rightX - fInset - fThick;
                b.nearLeft = nr.rightX - nInset - nThick;
            }
            b.farRight = b.farLeft + fThick;
            b.nearRight = b.nearLeft + nThick;
            b.farBottom = fr.y;
            b.farTop = fr.y - (fr.y - fr.y2);
            b.nearBottom = nr.y;
            b.nearTop = nr.y - (nr.y - nr.y2);
        }
    }
    return n;
}

namespace {

AqueductQuad MakeQuad(const int (&x)[8], const int (&y)[8], int a, int b, int c, int d) {
    AqueductQuad q;
    const int idx[4] = {a, b, c, d};
    for (int k = 0; k < 4; ++k) {
        q.x[k] = x[idx[k]];
        q.y[k] = y[idx[k]];
    }
    return q;
}

// The eight corners as FUN_00242bea passes them: p1..p4 on the FAR row, p5..p8 on the near one, banked.
void BlockCorners(const AqueductBlock& b, const AqueductBank& bank, int (&x)[8], int (&y)[8]) {
    const int wx[8] = {b.farLeft, b.farRight, b.farLeft, b.farRight, b.nearLeft, b.nearRight, b.nearLeft, b.nearRight};
    const int wy[8] = {b.farBottom, b.farBottom, b.farTop, b.farTop, b.nearBottom, b.nearBottom, b.nearTop, b.nearTop};
    for (int k = 0; k < 8; ++k) AqueductBankPoint(bank, wx[k], wy[k], x[k], y[k]);
}

}  // namespace

int AqueductBlockFaces(const AqueductBlock& b, const AqueductBank& bank, AqueductFace* out) {
    int x[8], y[8];
    BlockCorners(b, bank, x, y);
    int n = 0;
    if (x[0] < x[4]) out[n++] = {MakeQuad(x, y, 0, 2, 6, 4), AQU_PEN_BLOCK_SIDE};
    if (x[1] > x[5]) out[n++] = {MakeQuad(x, y, 1, 3, 7, 5), AQU_PEN_BLOCK_SIDE};
    if (y[6] > y[2]) out[n++] = {MakeQuad(x, y, 2, 3, 7, 6), AQU_PEN_BLOCK_TOP};
    out[n++] = {MakeQuad(x, y, 5, 4, 6, 7), AQU_PEN_BLOCK_SIDE};
    return n;
}

bool AqueductBlockHitQuad(const AqueductBlock& b, const AqueductBank& bank, AqueductQuad& out) {
    int x[8], y[8];
    BlockCorners(b, bank, x, y);
    if (b.pillar) {
        out = MakeQuad(x, y, 0, 1, 5, 4);
        return true;
    }
    if (y[6] <= y[2]) return false;
    out = MakeQuad(x, y, 2, 3, 7, 6);
    return true;
}

bool AqueductQuadHitsCar(const AqueductQuad& q, int clipTop, int clipBottom, int screenWidth) {
    // Only the bottom scanline matters, and the clip window ends at x 319, so the right-hand count never moves:
    // a polygon on that line hits unless all of it is left of x 80.
    AqueductSpan span;
    if (AqueductScanQuad(q, std::max(clipTop, clipBottom), clipBottom, screenWidth, &span, 1) == 0) return false;
    return span.x1 >= AQU_HIT_PROBE_LEFT;
}

int AqueductObstacleBoxes(uint16_t flags, AqueductObstacleBox* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) return 0;
    const int left = AQU_FLOOR_LEFT;
    const int right = AQU_FLOOR_RIGHT;
    const int mid = (left + right) / 2;
    const int width = right - left;

    if ((flags & AQU_OBSTACLE_SPAN) != 0) {
        out[0].left = left + (mid - left) / 3;
        out[0].right = right - (right - mid) / 3;
        return 1;
    }
    if ((flags & AQU_OBSTACLE_SIDES) != 0) {
        const int inset = width / 7;
        const int thick = inset >> 3;
        int n = 0;
        out[n].left = left + inset;
        out[n].right = left + inset + thick;
        ++n;
        if (n < maxOut) {
            out[n].left = right - inset - thick;
            out[n].right = right - inset;
            ++n;
        }
        return n;
    }
    return 0;
}

bool AqueductObstacleHits(uint16_t flags, int camX) {
    AqueductObstacleBox boxes[2];
    const int n = AqueductObstacleBoxes(flags, boxes, 2);
    if (n == 0) return false;

    // Any part of a box between the two screen points: FUN_00242024 hits whatever reaches the car's scanline
    // right of x 80, up to the window's edge.
    const int lo = AqueductProbeX(camX, AQU_HIT_PROBE_LEFT);
    const int hi = AqueductProbeX(camX, AQU_HIT_PROBE_RIGHT);
    for (int i = 0; i < n; ++i) {
        if (boxes[i].right >= lo && boxes[i].left <= hi) return true;
    }
    return false;
}

namespace {

// FUN_0021f248: the y on the line (x0,y0)-(x1,y1) at x.
int SlopeInterp(int x0, int y0, int x1, int y1, int x) {
    if (x0 == x1) return y1;
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

// The original's divide by four, truncating toward zero.
int QuarterToward0(int v) {
    return (v < 0 ? v + 3 : v) >> 2;
}

}  // namespace

bool AqueductSlopePatchHits(uint16_t flags, int camX) {
    const int quarter = AQU_SLOPE_WIDTH / 4;
    const int lo = ((flags & AQU_SLOPE_OBSTACLE_INSET) != 0) ? quarter : 0;
    const int hi = AQU_SLOPE_WIDTH - quarter;

    const int probes[2] = {AqueductProbeX(camX, AQU_HIT_PROBE_LEFT),
                           AqueductProbeX(camX, AQU_HIT_PROBE_RIGHT)};
    for (int k = 0; k < 2; ++k) {
        if ((flags & AQU_SLOPE_OBSTACLE_LEFT) != 0) {
            const int up = AQU_FLOOR_LEFT - probes[k];
            if (up >= lo && up <= hi) return true;
        }
        if ((flags & AQU_SLOPE_OBSTACLE_RIGHT) != 0) {
            const int up = probes[k] - AQU_FLOOR_RIGHT;
            if (up >= lo && up <= hi) return true;
        }
    }
    return false;
}

int AqueductBuildSlopeObstacles(const AqueductRow* rows, int rowCount,
                                const uint16_t* flags, int flagBase,
                                const AqueductBank& bank, int bankDegrees,
                                AqueductQuad* out, int maxOut) {
    if (rows == nullptr || flags == nullptr || out == nullptr) return 0;
    int n = 0;

    for (int r = rowCount; r > 0 && n < maxOut; --r) {
        const int seg = flagBase + r;
        if (seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        const uint16_t f = flags[seg];
        const bool left = (f & AQU_SLOPE_OBSTACLE_LEFT) != 0;
        const bool right = (f & AQU_SLOPE_OBSTACLE_RIGHT) != 0;
        if (!left && !right) continue;

        // The right slope wins if both are set, as in the original.
        const bool onRight = right;
        // Whether it starts a quarter up the slope or right at the floor.
        const bool inset = (f & AQU_SLOPE_OBSTACLE_INSET) != 0;

        int wx[4], wy[4];
        for (int e = 0; e < 2; ++e) {
            const AqueductRow& row = rows[e == 0 ? r : r - 1];
            const int footX = onRight ? row.rightX : row.leftX;
            const int footY = row.y;
            const int topX = onRight ? row.outerRightX : row.outerLeftX;
            const int topY = row.y2;
            const int quarter = QuarterToward0(footX - topX);

            const int x0 = inset ? (footX - quarter) : footX;
            const int y0 = inset ? SlopeInterp(footX, footY, topX, topY, x0) : footY;
            const int x1 = topX + quarter;
            const int y1 = SlopeInterp(footX, footY, topX, topY, x1);

            // Written foot-side then top-side on the far row, and top-side
            // then foot-side on the near one, which winds the quad.
            if (e == 0) {
                wx[0] = x0; wy[0] = y0;
                wx[1] = x1; wy[1] = y1;
            } else {
                wx[2] = x1; wy[2] = y1;
                wx[3] = x0; wy[3] = y0;
            }
        }

        AqueductQuad& q = out[n++];
        for (int k = 0; k < 4; ++k) {
            if (bankDegrees != 0) {
                AqueductBankPoint(bank, wx[k], wy[k], q.x[k], q.y[k]);
            } else {
                q.x[k] = wx[k];
                q.y[k] = wy[k];
            }
        }
        q.surface = onRight ? AqueductRightWall : AqueductLeftWall;
        q.nearRow = r - 1;
    }
    return n;
}

int AqueductBuildFloorBands(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                            int flagBase, const AqueductBank& bank, int bankDegrees,
                            AqueductQuad* out, int maxOut) {
    if (rows == nullptr || flags == nullptr || out == nullptr) return 0;
    int n = 0;
    for (int r = rowCount; r > 0 && n < maxOut; --r) {
        const int seg = flagBase + r;
        if (seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        if ((flags[seg] & AQU_FLAG_FLOOR_BAND) == 0) continue;

        const AqueductRow& fr = rows[r];
        const AqueductRow& nr = rows[r - 1];
        const int wx[4] = {fr.leftX, fr.rightX, nr.rightX, nr.leftX};
        const int wy[4] = {fr.y, fr.y, nr.y, nr.y};
        AqueductQuad& q = out[n++];
        for (int k = 0; k < 4; ++k) {
            if (bankDegrees != 0) {
                AqueductBankPoint(bank, wx[k], wy[k], q.x[k], q.y[k]);
            } else {
                q.x[k] = wx[k];
                q.y[k] = wy[k];
            }
        }
        q.surface = AqueductFloor;
        q.nearRow = r - 1;
    }
    return n;
}

int AqueductBuildLines(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                       int flagBase, const AqueductBank& bank, int bankDegrees,
                       AqueductLine* out, int maxOut) {
    if (rows == nullptr || out == nullptr || rowCount <= 0) return 0;

    // The four points each row contributes, in the original's order.
    auto pointX = [&](const AqueductRow& r, int which) {
        switch (which) {
            case 0: return r.leftX;
            case 1: return r.rightX;
            case 2: return r.outerLeftX;
            default: return r.outerRightX;
        }
    };
    auto pointY = [&](const AqueductRow& r, int which) {
        return (which < 2) ? r.y : r.y2;
    };
    auto bankIt = [&](int x, int y, int& ox, int& oy) {
        if (bankDegrees != 0) {
            AqueductBankPoint(bank, x, y, ox, oy);
        } else {
            ox = x;
            oy = y;
        }
    };

    int n = 0;
    const AqueductRow& far = rows[rowCount];
    const AqueductRow& near = rows[0];
    for (int which = 0; which < 4 && n < maxOut; ++which) {
        // Past the near row, so the edge runs off the bottom of the screen.
        const int ex = pointX(near, which) * 4 - pointX(far, which) * 3;
        const int ey = pointY(near, which) * 4 - pointY(far, which) * 3;
        AqueductLine& l = out[n++];
        bankIt(pointX(far, which), pointY(far, which), l.x0, l.y0);
        bankIt(ex, ey, l.x1, l.y1);
        l.pen = (which < 2) ? AQU_PEN_EDGE : AQU_PEN_SLOPE_TOP;
    }

    // The concrete joints.
    for (int r = 0; r <= rowCount && n + 3 <= maxOut; ++r) {
        const int seg = flagBase + r;
        if (flags == nullptr || seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        if ((flags[seg] & AQU_FLAG_JOINT) == 0) continue;

        const AqueductRow& row = rows[r];
        // Across the floor, then up each slope.
        const int pairs[3][2] = {{0, 1}, {0, 2}, {1, 3}};
        for (int k = 0; k < 3; ++k) {
            AqueductLine& l = out[n++];
            bankIt(pointX(row, pairs[k][0]), pointY(row, pairs[k][0]), l.x0, l.y0);
            bankIt(pointX(row, pairs[k][1]), pointY(row, pairs[k][1]), l.x1, l.y1);
            l.pen = AQU_PEN_EDGE;
        }
    }
    return n;
}

AqueductWallRide AqueductRideWall(int camX) {
    AqueductWallRide ride;

    const int carLeft = AqueductCarLeft(camX);
    if (carLeft < AQU_FLOOR_LEFT) {
        const int raw = AQU_FLOOR_LEFT - carLeft;
        ride.pastCap = raw > AQU_LEFT_CLIMB_CAP;
        ride.climb = std::min(raw, AQU_LEFT_CLIMB_CAP);
        ride.bank = ride.climb * 0x6c / 400;
        ride.camXDelta = 0xf + (ride.pastCap ? 0x19 : 0);

        const int quarter = ride.climb >> 2;
        ride.channel.leftFloor = quarter;
        ride.channel.rightFloor = quarter;
        ride.channel.outerRight = quarter;
        ride.channel.yBase = ride.climb - (ride.climb >> 3);
        if (ride.pastCap) {
            // Past the top the whole channel is pushed further over, which is
            // what makes the view slide as the car rides the concrete.
            const int extra = std::min(AQU_FLOOR_LEFT - AQU_LEFT_CLIMB_CAP - carLeft, 200);
            ride.channel.outerLeft = extra;
            ride.channel.leftFloor += extra;
            ride.channel.yBase += extra;
            ride.channel.yBase2 = extra;
        }
        return ride;
    }

    const int carRight = AqueductCarRight(camX);
    if (carRight > AQU_FLOOR_RIGHT) {
        const int raw = carRight - AQU_FLOOR_RIGHT;
        ride.pastCap = raw > AQU_RIGHT_CLIMB_CAP;
        ride.climb = std::min(raw, AQU_RIGHT_CLIMB_CAP);
        ride.bank = ride.climb * -0x5a / 400;
        ride.camXDelta = -(0xf + (ride.pastCap ? 0x19 : 0));

        // The right wall skews the channel differently from the left.
        const int quarter = ride.climb >> 2;
        ride.channel.rightFloor = ride.climb * -3;
        ride.channel.yBase = ride.climb - quarter;
        ride.channel.outerRight = ride.climb - quarter;
        ride.channel.yBase2 = quarter - ride.climb;
        ride.channel.leftFloor = ride.climb * -2;
        ride.channel.outerLeft = ride.climb * -2;
        if (ride.pastCap) {
            const int over = carRight - (AQU_FLOOR_RIGHT + AQU_RIGHT_CLIMB_CAP);
            ride.channel.outerRight -= over;
            ride.channel.rightFloor -= over;
            ride.channel.yBase2 += over;
            ride.channel.yBase += over;
        }
        return ride;
    }

    return ride;   // on the floor: flat channel, no bank
}

void AqueductBuildMirrorRows(const AqueductChannel& previous, int camX, int camY,
                          int subPos, int slope, AqueductRow* rows) {
    if (rows == nullptr) return;

    const int midpoint = (previous.leftFloor + previous.rightFloor + 0x12c0) >> 1;
    const int slip = subPos / 8;

    int depth = 2;
    for (int i = 0; i < AQU_MIRROR_ROWS; ++i) {
        const int row = AQU_MIRROR_ROWS - i;
        // The slip is added here and taken back at row 3 - the opposite of the
        // near pass, which subtracts it first.
        if (row == 8) depth = slip + 8;
        if (row == 3) depth -= slip;

        AqueductRow& r = rows[i];
        r.y = AqueductProjectMirrorY(camY, depth, previous.yBase + 2000);
        r.leftX = AqueductProjectMirrorX(camX, depth, previous.leftFloor + AQU_FLOOR_LEFT, slope);
        r.rightX = AqueductProjectMirrorX(camX, depth, previous.rightFloor + AQU_FLOOR_RIGHT, slope);
        r.midX = AqueductProjectMirrorX(camX, depth, previous.rightFloor + midpoint, slope);
        r.y2 = AqueductProjectMirrorY(camY, depth, previous.yBase2 + 0x67c);
        r.outerLeftX = AqueductProjectMirrorX(camX, depth, previous.outerLeft + 0x438, slope);
        r.outerRightX = AqueductProjectMirrorX(camX, depth, previous.outerRight + 0xe88, slope);
        r.stripeLeftX = AqueductProjectMirrorX(camX, depth, midpoint + previous.leftFloor - 0x28, slope);
        r.stripeRightX = AqueductProjectMirrorX(camX, depth, midpoint + previous.rightFloor + 0x28, slope);

        depth += 8;
    }
}

void AqueductBuildViewRows(const AqueductChannel& channel, int camX, int camY,
                           int subPos, int slope, int vjerk,
                           AqueductRow* rows) {
    if (rows == nullptr) return;

    // The mid marker sits halfway across the channel, computed once.
    const int midpoint = (channel.leftFloor + channel.rightFloor + 0x12c0) >> 1;
    const int slip = subPos / 8;

    int depth = 4;
    for (int i = 0; i < AQU_VIEW_ROWS; ++i) {
        // Row 15 is drawn first, then 14 down to 1.
        const int row = AQU_VIEW_ROWS - i;
        // The middle rows slide with the sub-segment position.
        if (row == 14) depth = 0xc - slip;
        if (row == 3) depth += slip;

        AqueductRow& r = rows[i];
        r.y = AqueductProjectY(camY, depth, channel.yBase + 2000, vjerk);
        r.leftX = AqueductProjectViewX(camX, depth, channel.leftFloor + AQU_FLOOR_LEFT, slope);
        r.rightX = AqueductProjectViewX(camX, depth, channel.rightFloor + AQU_FLOOR_RIGHT, slope);
        r.midX = AqueductProjectViewX(camX, depth, channel.rightFloor + midpoint, slope);
        r.y2 = AqueductProjectY(camY, depth, channel.yBase2 + 0x67c, vjerk);
        r.outerLeftX = AqueductProjectViewX(camX, depth, channel.outerLeft + 0x438, slope);
        r.outerRightX = AqueductProjectViewX(camX, depth, channel.outerRight + 0xe88, slope);
        r.stripeLeftX = AqueductProjectViewX(camX, depth, midpoint + channel.leftFloor - 0x28, slope);
        r.stripeRightX = AqueductProjectViewX(camX, depth, midpoint + channel.rightFloor + 0x28, slope);

        depth += 8;
    }
}

void AqueductBuildFrame(AqueductFrame& frame, int camX, int camY,
                        int roadPos, int subPos, int slope, int vjerk) {
    // Both passes project the channel as the last ride left it.
    const AqueductChannel drawn = frame.channel;
    frame.drawn = drawn;

    AqueductBuildViewRows(drawn, camX, camY, subPos, slope, vjerk, frame.viewRows);
    frame.viewFlagIndex = roadPos;

    // The near object pass: the ride replaces the channel for the next frame.
    frame.ride = AqueductRideWall(camX);
    frame.channel = frame.ride.channel;

    // Looking back, the channel bends the other way (neg.w at 0x246f4c).
    AqueductBuildMirrorRows(drawn, camX, camY, subPos, -slope, frame.mirrorRows);
    // The far half is walked from the other end of the mirrored copy.
    frame.mirrorFlagIndex = AQUEDUCT_SEGMENTS - 1 - roadPos;
}

int AqueductBuildMidline(const AqueductRow* rows, int rowCount, const uint16_t* flags, int flagBase,
                         const AqueductBank& bank, AqueductQuad* out) {
    if (rows == nullptr || out == nullptr || rowCount <= 0) return 0;
    // Flag i belongs to row rowCount - i: i = 0 is the far row.
    auto gap = [&](int i) {
        const int seg = flagBase + rowCount - i;
        return flags != nullptr && seg >= 0 && seg < AQUEDUCT_SEGMENTS && (flags[seg] & AQU_FLAG_MIDLINE_GAP) != 0;
    };
    int start = -1;
    for (int i = 0; i < rowCount; ++i) {
        if (gap(i)) { start = i; break; }
    }
    int end = start + 1;
    while (start >= 0 && end < rowCount && gap(end)) ++end;

    const AqueductRow& far = rows[rowCount];
    const AqueductRow& near = rows[0];
    // FUN_0021f210: x on the strip's edge at row y.
    auto edgeAt = [](int xFar, int yFar, int xNear, int yNear, int y) {
        return yFar == yNear ? xNear : xFar + (y - yFar) * (xNear - xFar) / (yNear - yFar);
    };
    auto quad = [&](int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
        AqueductQuad q;
        const int xs[4] = {x0, x1, x2, x3}, ys[4] = {y0, y1, y2, y3};
        for (int k = 0; k < 4; ++k) AqueductBankPoint(bank, xs[k], ys[k], q.x[k], q.y[k]);
        return q;
    };

    if (end < 2) {
        out[0] = quad(far.stripeLeftX, far.y, near.stripeLeftX, near.y, near.stripeRightX, near.y,
                      far.stripeRightX, far.y);
        return 1;
    }
    int n = 0;
    if (start > 0) {
        const int y = rows[rowCount - start].y;
        out[n++] = quad(far.stripeLeftX, far.y, edgeAt(far.stripeLeftX, far.y, near.stripeLeftX, near.y, y), y,
                        edgeAt(far.stripeRightX, far.y, near.stripeRightX, near.y, y), y, far.stripeRightX, far.y);
    }
    if (end < rowCount - 1) {
        const int y = rows[rowCount - end].y;
        out[n++] = quad(edgeAt(far.stripeLeftX, far.y, near.stripeLeftX, near.y, y), y, near.stripeLeftX, near.y,
                        near.stripeRightX, near.y, edgeAt(far.stripeRightX, far.y, near.stripeRightX, near.y, y), y);
    }
    return n;
}

AqueductOpponentTilt AqueductOpponentTiltFor(int npcX, const AqueductChannel& drawn, int bankDegrees) {
    const int mid = (drawn.leftFloor + drawn.rightFloor + 0x12c0) >> 1;
    int climb = 0;
    if (npcX < 0x5fa + drawn.leftFloor) {
        climb = drawn.leftFloor + 0x5fa - npcX;
    } else if (npcX > 0xcc6 + drawn.rightFloor) {
        climb = npcX - drawn.rightFloor - 0xcc6;
    }
    int a = climb >= 100 ? 45 : climb <= 0 ? 0 : climb * 45 / 100;
    if (npcX - mid < 0) a = -a;
    a += bankDegrees;
    if (bankDegrees < 0 && bankDegrees > -90) a += bankDegrees >> 2;
    if (a > 0x167) a -= 0x168;
    if (a < -0x167) a += 0x168;

    const int m = std::abs(a);
    AqueductOpponentTilt t;
    t.angle = a;
    t.tilt = m > 55 ? -1 : m > 36 ? 0 : m > 22 ? 1 : m > 12 ? 2 : 3;
    if (a > 0 && t.tilt != -1) t.tilt = 6 - t.tilt;
    return t;
}

int AqueductOpponentGroundY(int npcX, const AqueductChannel& drawn) {
    const int rise = drawn.yBase2 - drawn.yBase + 0x154;
    int lift = 0;
    if (npcX < AQU_FLOOR_LEFT + drawn.leftFloor) {
        lift = (drawn.leftFloor + 0x5a0 - npcX) * rise / (drawn.leftFloor + 400 - drawn.outerLeft);
    } else if (npcX > AQU_FLOOR_RIGHT + drawn.rightFloor) {
        lift = (npcX - drawn.rightFloor - AQU_FLOOR_RIGHT) * rise / (400 + drawn.outerRight - drawn.rightFloor);
    }
    return 2000 + drawn.yBase - lift;
}

void AqueductTiltAnchor(int w, int h, int angle, int anchorX, int& xOff, int& yOff) {
    const AqueductBank turn = AqueductBankFor(0, 0, angle);
    int x0, y0, x1, y1, x2, y2, ax, ay;
    AqueductBankPoint(turn, 0, h - 1, x0, y0);
    AqueductBankPoint(turn, w - 1, h - 1, x1, y1);
    AqueductBankPoint(turn, w - 1, 0, x2, y2);
    AqueductBankPoint(turn, anchorX, h - 1, ax, ay);
    const int rotH = std::max(std::abs(y1), std::abs(y2 - y0)) + 1;
    const int minX = std::min({0, x0, x1, x2});
    const int minY = std::min({0, y0, y1, y2});
    // Kept in signed bytes, as the original's tables are.
    xOff = static_cast<int8_t>(ax - minX);
    yOff = static_cast<int8_t>(rotH - (ay - minY));
}

int AqueductOpponentStep(int npcX, int delta, int npcSeg, int playerX, int playerSeg,
                         const AqueductChannel& drawn, const uint16_t* flags) {
    const int outerL = AQU_OUTER_LEFT + drawn.outerLeft;
    const int floorL = AQU_FLOOR_LEFT + drawn.leftFloor;
    const int floorR = AQU_FLOOR_RIGHT + drawn.rightFloor;
    const int outerR = AQU_OUTER_RIGHT + drawn.outerRight;
    const int half = (floorL + floorR) / 2;
    // Up the left slope, the left floor, just left of centre, the right floor, up the right slope.
    const int lanes[5] = {outerL + (floorL - outerL) / 3, floorL + (floorR - floorL) / 4, half - 0x1e,
                          (half + floorR) / 2, (floorR + outerR) / 2};
    const int mid = (floorL + floorR) >> 1;

    enum { kAhead = 1, kLeftOf = 2, kRightOf = 4, kBehind = 8 };
    const int dseg = npcSeg - playerSeg;
    const int mode = dseg > 2 ? kAhead : dseg < 0 ? kBehind : npcX < playerX ? kLeftOf : kRightOf;
    const int outerSide = npcX - mid < 0 ? 0 : 4;

    int target = npcX + delta;
    int k = 0;
    bool found = false;
    for (; k < 8; ++k) {
        const int seg = npcSeg + k;
        if (flags == nullptr || seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        const uint16_t f = flags[seg];
        if ((f & AQU_OBSTACLE_SPAN) != 0) {
            target = lanes[mode == kLeftOf ? 0 : mode == kRightOf ? 4 : outerSide];
            found = true;
            break;
        }
        if ((f & AQU_OBSTACLE_SIDES) != 0) {
            target = lanes[mode == kAhead ? 2 : mode == kLeftOf ? 0 : mode == kRightOf ? 4 : outerSide];
            found = true;
            break;
        }
    }
    if (!found) {
        // Ahead it sits on the player's side of the floor; behind it takes the other.
        const bool playerLeft = playerX - mid < 0;
        if (mode == kAhead) target = lanes[playerLeft ? 1 : 3];
        if (mode == kBehind) target = lanes[playerLeft ? 3 : 1];
    }
    if (k > 0) --k;
    if (k > 0) --k;
    return ((target - npcX) * 6 / (k + 1)) / 6;
}

int AqueductWallState(int camX) {
    if (AqueductCarLeft(camX) < AQU_ROLL_LEFT) return 1;
    if (AqueductCarRight(camX) >= AQU_ROLL_RIGHT) return -1;
    return 0;
}

int AqueductRollover(int wallState, int speedMph, int drift,
                     AqueductRollFrame* out, int maxFrames) {
    if (wallState == 0 || out == nullptr || maxFrames <= 0) {
        return 0;
    }
    const bool offLeft = wallState > 0;

    if (speedMph < AQU_ROLL_MIN_MPH) {
        // Not enough speed to go over: it banks the other way and settles.
        out[0] = AqueductRollFrame{};
        out[0].roll = offLeft ? -6 : 6;
        out[0].vjerk = 10;
        return 1;
    }

    // Both divisors are taken once, before the loop, so the steps are equal.
    const int driftStep = drift / 6;
    const int jerkStep = speedMph / 6;

    int roll = 0;
    int frames = 0;
    for (int i = 0; i < 8 && frames < maxFrames; ++i, ++frames) {
        // Eight steps of 0x20 is 0x100, a full turn - the car goes right over.
        roll += offLeft ? 0x20 : -0x22;
        out[frames] = AqueductRollFrame{};
        out[frames].roll = roll;
        out[frames].camXDelta = offLeft ? 0x28 : -0x28;
        out[frames].driftDelta = -driftStep;
        // A launch arc: up for the first half of the turn, down for the rest.
        out[frames].vjerk = jerkStep * (i < 4 ? i : 8 - i);
    }

    // It lands flat.
    if (frames < maxFrames) {
        out[frames] = AqueductRollFrame{};
        ++frames;
    }
    return frames;
}

// aqushape.c's shape table, 9 records of 38 bytes at 0x236b3c (DATA_35).
namespace {

struct AqueductShapeRecord {
    uint16_t asset;
    AqueductShapeRect phase[AQU_SHAPE_PHASES];
};

const AqueductShapeRecord kShapeTable[AQU_SHAPE_COUNT] = {
    {275, {{ 0,  88, 85, 12}, { 0,  64, 82, 25}, {112,  80, 82, 20}, {122,  39, 74, 41}, {122,   0, 76, 39}, { 0,   0, 59, 62}, {59,   0, 63, 59}}},
    {276, {{ 0,   0, 44, 11}, { 0,  11, 44, 13}, { 0,  24, 43, 12}, { 0,  36, 42, 19}, { 0,  55, 39, 23}, { 0,  78, 33, 31}, { 0, 109, 30, 32}}},
    {277, {{ 0,   0, 40, 10}, { 0,  10, 38, 13}, { 0,  23, 38, 14}, { 0,  37, 36, 21}, { 0,  58, 34, 21}, { 0,  79, 28, 29}, { 0, 108, 28, 29}}},
    {278, {{ 0,   0, 28,  8}, { 0,   8, 27, 10}, { 0,  18, 27, 10}, { 0,  28, 25, 15}, { 0,  43, 25, 17}, { 0,  60, 21, 20}, { 0,  80, 19, 21}}},
    {279, {{ 0,   0, 22,  6}, { 0,   6, 22,  9}, { 0,  15, 21, 10}, { 0,  25, 21, 13}, { 0,  38, 19, 14}, { 0,  52, 18, 17}, { 0,  69, 17, 18}}},
    {280, {{ 0,   0, 15,  4}, { 0,   4, 14,  6}, { 0,  10, 14,  6}, { 0,  16, 13,  8}, { 0,  24, 12,  8}, { 0,  32, 10, 10}, { 0,  42, 10, 10}}},
    {281, {{ 0,   0, 11,  2}, { 0,   2, 12,  4}, { 0,   6, 10,  5}, { 0,  11, 10,  6}, { 0,  17,  9,  6}, { 0,  23,  9,  8}, { 0,  31,  8,  9}}},
    {282, {{ 0,   0,  8,  1}, { 0,   1,  8,  3}, { 0,   4,  8,  3}, { 0,   7,  7,  4}, { 0,  11,  7,  4}, { 0,  15,  5,  5}, { 0,  20,  5,  5}}},
    {283, {{ 0,   0,  6,  1}, { 0,   1,  6,  2}, { 0,   3,  6,  3}, { 0,   6,  5,  3}, { 0,   9,  3,  2}, { 0,  11,  5,  5}, { 0,  16,  4,  3}}},
};

AqueductShapeRecord gShapes[AQU_SHAPE_COUNT];
bool gShapesReady = false;

void EnsureShapeTable() {
    if (gShapesReady) return;
    std::memcpy(gShapes, kShapeTable, sizeof(gShapes));
    gShapesReady = true;
}

}  // namespace

bool AqueductLoadShapeTable(const std::vector<uint8_t>& exeData) {
    constexpr size_t kRecord = 38;
    constexpr size_t kSpan = kRecord * AQU_SHAPE_COUNT;
    if (exeData.size() < AQU_SHAPE_TABLE_OFFSET + kSpan) {
        EnsureShapeTable();
        return false;
    }
    const uint8_t* src = exeData.data() + AQU_SHAPE_TABLE_OFFSET;

    // The nine assets are consecutive from 275.
    for (int k = 0; k < AQU_SHAPE_COUNT; ++k) {
        const uint8_t* rec = src + k * kRecord;
        const int asset = (rec[0] << 8) | rec[1];
        if (asset != AQU_SHAPE_ASSET_FIRST + k) {
            EnsureShapeTable();
            return false;
        }
    }

    for (int k = 0; k < AQU_SHAPE_COUNT; ++k) {
        const uint8_t* rec = src + k * kRecord;
        gShapes[k].asset = static_cast<uint16_t>((rec[0] << 8) | rec[1]);
        for (int j = 0; j < AQU_SHAPE_PHASES; ++j) {
            const uint8_t* q = rec + 10 + j * 4;
            gShapes[k].phase[j] = AqueductShapeRect{q[0], q[1], q[2], q[3]};
        }
    }
    gShapesReady = true;
    return true;
}

int AqueductShapeAsset(int shape) {
    EnsureShapeTable();
    if (shape < 0 || shape >= AQU_SHAPE_COUNT) return AQU_SHAPE_ASSET_FIRST - 1;
    return gShapes[shape].asset - 1;
}

const AqueductShapeRect& AqueductShapeRectFor(int shape, int phase) {
    EnsureShapeTable();
    static const AqueductShapeRect kNone;
    if (shape < 0 || shape >= AQU_SHAPE_COUNT) return kNone;
    if (phase < 0 || phase >= AQU_SHAPE_PHASES) return kNone;
    return gShapes[shape].phase[phase];
}

// The original's ladder of comparisons, in its own order. Even phases are
// positive bank, odd negative.
int AqueductShapePhase(int bankDegrees) {
    if (bankDegrees > 0x1e) return 6;
    if (bankDegrees > 0x0f) return 4;
    if (bankDegrees > 0x07) return 2;
    if (bankDegrees >= -0x07) return 0;
    if (bankDegrees >= -0x0f) return 1;
    if (bankDegrees >= -0x1e) return 3;
    return 5;
}

int AqueductBuildObstacles(const AqueductRow* rows, int rowCount,
                           const uint16_t* flags, int flagBase,
                           const AqueductBank& bank, int bankDegrees,
                           AqueductObstacle* out, int maxOut) {
    if (rows == nullptr || flags == nullptr || out == nullptr) return 0;
    const int phase = AqueductShapePhase(bankDegrees);
    int n = 0;

    // Row zero is skipped on both sides.
    for (int i = 1; i < rowCount; ++i) {
        const int r = rowCount - i;
        const int width = rows[r].rightX - rows[r].leftX;

        // One rung every two rows, nearest row on the largest shape.
        const int z = rowCount - i - 1;
        const int rung = (rowCount == AQU_VIEW_OBJECT_ROWS) ? ((z + 1) >> 1)
                                                            : ((z >> 1) + 3);
        if (rung < 0 || rung >= AQU_SHAPE_LADDER_COUNT) continue;
        const int shape = AQU_SHAPE_LADDER_FIRST + rung;

        const int seg = flagBase + r;
        if (seg < 0 || seg >= AQUEDUCT_SEGMENTS) continue;
        const uint16_t rowFlags = flags[seg];

        const int xs[2] = {rows[r].leftX + (width >> 2),
                           rows[r].rightX - (width >> 3)};
        const uint16_t bits[2] = {AQU_OBSTACLE_DRAW_LEFT, AQU_OBSTACLE_DRAW_RIGHT};
        for (int side = 0; side < 2; ++side) {
            if ((rowFlags & bits[side]) == 0) continue;
            if (n >= maxOut) return n;
            AqueductObstacle& o = out[n];
            AqueductBankPoint(bank, xs[side], rows[r].y, o.x, o.y);
            o.shape = shape;
            o.phase = phase;
            ++n;
        }
    }
    return n;
}

// The two star tables, as shipped. AqueductLoadStarTable replaces them with
// the player's own copy; these are the documented fallback.
namespace {

struct StarPoint { int16_t x, y; };

const StarPoint kStarsBright[AQU_STARS_BRIGHT] = {
    { 16, 15}, { 95, 15}, { 96, 18}, {133, 24}, {137, 54}, {160, 34}, {158, 36}, {159, 36}, {158, 38}, {137, 54}, {175, 52}, {212, 17}, {213, 17}, {238, 16}, {242, 14}, {249, 13}, {244, 17}, {244, 22}
};
const StarPoint kStarsDim[AQU_STARS_DIM] = {
    { 56, 13}, { 73, 26}, {101, 17}, {190, 21}, {204, 41}, {248, 16}
};

// The mirror's own sky, plotted by FUN_00241f88 off FUN_0023d1e8's glass clear.
const StarPoint kMirrorStarsBright[AQU_MIRROR_STARS_BRIGHT] = {
    {246, 27}, {285, 9}, {262, 15}, {270, 31}, {278, 17}, {284, 28}, {280, -5}, {272, 4}, {290, 33}
};
const StarPoint kMirrorStarsDim[AQU_MIRROR_STARS_DIM] = {
    {296, 29}, {290, 17}, {265, 28}
};

StarPoint gStarsBright[AQU_STARS_BRIGHT];
StarPoint gStarsDim[AQU_STARS_DIM];
StarPoint gMirrorStarsBright[AQU_MIRROR_STARS_BRIGHT];
StarPoint gMirrorStarsDim[AQU_MIRROR_STARS_DIM];
bool gStarsReady = false;

void EnsureStarTable() {
    if (gStarsReady) return;
    std::memcpy(gStarsBright, kStarsBright, sizeof(gStarsBright));
    std::memcpy(gStarsDim, kStarsDim, sizeof(gStarsDim));
    std::memcpy(gMirrorStarsBright, kMirrorStarsBright, sizeof(gMirrorStarsBright));
    std::memcpy(gMirrorStarsDim, kMirrorStarsDim, sizeof(gMirrorStarsDim));
    gStarsReady = true;
}

bool ReadStars(const std::vector<uint8_t>& exe, size_t off, StarPoint* out, int n, int minY = 0) {
    if (exe.size() < off + static_cast<size_t>(n) * 4) return false;
    for (int i = 0; i < n; ++i) {
        const uint8_t* p = exe.data() + off + i * 4;
        const int x = static_cast<int16_t>((p[0] << 8) | p[1]);
        const int y = static_cast<int16_t>((p[2] << 8) | p[3]);
        // Stars sit in the sky band above the horizon.
        if (x < 0 || x >= AQU_SCREEN_WIDTH || y < minY || y > AQU_SKY_BOTTOM) return false;
        out[i].x = static_cast<int16_t>(x);
        out[i].y = static_cast<int16_t>(y);
    }
    return true;
}

}  // namespace

bool AqueductLoadStarTable(const std::vector<uint8_t>& exeData) {
    StarPoint bright[AQU_STARS_BRIGHT];
    StarPoint dim[AQU_STARS_DIM];
    StarPoint mBright[AQU_MIRROR_STARS_BRIGHT];
    StarPoint mDim[AQU_MIRROR_STARS_DIM];
    // One mirror star sits just above the screen, at y -5.
    if (!ReadStars(exeData, AQU_STARS_BRIGHT_OFFSET, bright, AQU_STARS_BRIGHT) ||
        !ReadStars(exeData, AQU_STARS_DIM_OFFSET, dim, AQU_STARS_DIM) ||
        !ReadStars(exeData, AQU_MIRROR_STARS_BRIGHT_OFFSET, mBright, AQU_MIRROR_STARS_BRIGHT, -16) ||
        !ReadStars(exeData, AQU_MIRROR_STARS_DIM_OFFSET, mDim, AQU_MIRROR_STARS_DIM, -16)) {
        EnsureStarTable();
        return false;
    }
    std::memcpy(gStarsBright, bright, sizeof(gStarsBright));
    std::memcpy(gStarsDim, dim, sizeof(gStarsDim));
    std::memcpy(gMirrorStarsBright, mBright, sizeof(gMirrorStarsBright));
    std::memcpy(gMirrorStarsDim, mDim, sizeof(gMirrorStarsDim));
    gStarsReady = true;
    return true;
}

int AqueductSkyBase(int heading) {
    return (heading * 2) % AQU_SCREEN_WIDTH;
}

int AqueductBuildHorizon(int base, const AqueductBank& bank, int bankDegrees,
                         AqueductObstacle* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) return 0;
    struct Fixed { int shape, x, y; };
    static const Fixed kFixed[AQU_HORIZON_SPRITES] = {
        {0, 0x5c, 0x20},
        {1, 0xd2, 0x2d},
        {1, 0xb4, 0x16},
    };
    const int phase = AqueductShapePhase(bankDegrees);
    int n = 0;
    for (int i = 0; i < AQU_HORIZON_SPRITES && n < maxOut; ++i) {
        AqueductObstacle& o = out[n++];
        const int x = base + kFixed[i].x;
        const int y = kFixed[i].y;
        if (bankDegrees != 0) {
            AqueductBankPoint(bank, x, y, o.x, o.y);
        } else {
            o.x = x;
            o.y = y;
        }
        o.shape = kFixed[i].shape;
        o.phase = phase;
    }
    return n;
}

namespace {

// FUN_00241dde for both skies: offset in x, then banked when the channel is.
int PlotStars(const StarPoint* bright, int nBright, const StarPoint* dim, int nDim, int baseX,
              const AqueductBank& bank, int bankDegrees, AqueductStar* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) return 0;

    int n = 0;
    auto emit = [&](const StarPoint* src, int count, int color) {
        for (int i = 0; i < count && n < maxOut; ++i) {
            AqueductStar& s = out[n];
            const int x = src[i].x + baseX;
            const int y = src[i].y;
            // The original only transforms when the channel is actually
            // banked, so a level sky is the raw table.
            if (bankDegrees != 0) {
                AqueductBankPoint(bank, x, y, s.x, s.y);
            } else {
                s.x = x;
                s.y = y;
            }
            s.color = color;
            ++n;
        }
    };
    emit(bright, nBright, AQU_STAR_COLOR_BRIGHT);
    emit(dim, nDim, AQU_STAR_COLOR_DIM);
    return n;
}

}  // namespace

int AqueductBuildSky(int baseX, const AqueductBank& bank, int bankDegrees,
                     AqueductStar* out, int maxOut) {
    EnsureStarTable();
    return PlotStars(gStarsBright, AQU_STARS_BRIGHT, gStarsDim, AQU_STARS_DIM, baseX, bank, bankDegrees, out,
                     maxOut);
}

int AqueductBuildMirrorSky(int skyBase, const AqueductBank& bank, int bankDegrees,
                           AqueductStar* out, int maxOut) {
    EnsureStarTable();
    // A quarter of the view's pan, the other way.
    return PlotStars(gMirrorStarsBright, AQU_MIRROR_STARS_BRIGHT, gMirrorStarsDim, AQU_MIRROR_STARS_DIM,
                     -(skyBase >> 2), bank, bankDegrees, out, maxOut);
}

}  // namespace srrace
