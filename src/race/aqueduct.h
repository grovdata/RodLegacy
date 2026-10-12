#pragma once
// The Aqueduct channel: its sloped concrete walls and the rollover.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "race/race_types.h"

namespace srrace {

// The channel floor, narrower than the road's 0x5dc..0xce4.
constexpr int AQU_FLOOR_LEFT = 0x5c8;   // 1480
constexpr int AQU_FLOOR_RIGHT = 0xcf8;  // 3320

// The car's own edges, measured off the camera X. Asymmetric in the original.
constexpr int AQU_CAR_LEFT_OFFSET = 0x6e;   // 110
constexpr int AQU_CAR_RIGHT_OFFSET = 0x208; // 520

// How far up each wall the car can ride before the slope runs out.
constexpr int AQU_LEFT_CLIMB_CAP = 0xb4;   // 180
constexpr int AQU_RIGHT_CLIMB_CAP = 0x88;  // 136

// Past these the car has left the slope entirely and rolls.
constexpr int AQU_ROLL_LEFT = 0x439;   // 1081
constexpr int AQU_ROLL_RIGHT = 0xded;  // 3565

// Below this the car has not the speed to go over: it banks and drops back.
constexpr int AQU_ROLL_MIN_MPH = 31;

// How far sideways each step of a rollover throws the car (jdraw_a.c
// FUN_00244364), always back towards the middle of the channel.
constexpr int AQU_ROLL_SHOVE = 0x28;

// A full turn on the original's 256-unit angle.
constexpr int AQU_ROLL_FULL_TURN = 0x100;

constexpr int AQUEDUCT_SEGMENTS = 371;

// Obstacles. The pair is the speed cap mask: touching either drops the car to
// 0x38 and runs a 38x38 box collision.
constexpr uint16_t AQU_OBSTACLE_SPAN = 0x0010;  // one box across the channel
constexpr uint16_t AQU_OBSTACLE_SIDES = 0x0080; // a box at each side
constexpr uint16_t AQU_OBSTACLE_MASK = AQU_OBSTACLE_SPAN | AQU_OBSTACLE_SIDES;

struct AqueductTrack {
    uint16_t flags[AQUEDUCT_SEGMENTS];
    uint16_t mirror[AQUEDUCT_SEGMENTS];   // flags[370 - s]
};

void BuildAqueductTrack(AqueductTrack& track);

void BuildAqueductRoad(TrackData& road);

inline int AqueductCarLeft(int camX) { return camX - AQU_CAR_LEFT_OFFSET; }
inline int AqueductCarRight(int camX) { return camX + AQU_CAR_RIGHT_OFFSET; }

// How the channel is displaced this frame.
struct AqueductChannel {
    int yBase = 0;       // A4+0x28d4, projected with +2000
    int leftFloor = 0;   // A4+0x28d6, +0x5c8
    int rightFloor = 0;  // A4+0x28d8, +0xcf8
    int yBase2 = 0;      // A4+0x28da, +0x67c
    int outerLeft = 0;   // A4+0x28dc, +0x438
    int outerRight = 0;  // A4+0x28de, +0xe88
};

// What the car is doing against the walls this frame.
struct AqueductWallRide {
    int climb = 0;        // how far up the slope, already capped
    int bank = 0;         // bank angle, A4+0x26e2
    int camXDelta = 0;    // camera nudge
    bool pastCap = false; // the slope has run out under the car
    AqueductChannel channel;  // how the climb skews the channel
};

constexpr int AQU_VIEW_ROWS = 15;

struct AqueductRow {
    int y = 0;            // A4+0x2792
    int leftX = 0;        // A4+0x2774
    int rightX = 0;       // A4+0x2756
    int midX = 0;         // A4+0x2828
    int y2 = 0;           // A4+0x280a
    int outerLeftX = 0;   // A4+0x27ce
    int outerRightX = 0;  // A4+0x27ec
    // The midline's edges, 0x28 either side of the channel centre (A4+0x28e0.. at the far and near rows).
    int stripeLeftX = 0;
    int stripeRightX = 0;
};

// slope is the lateral drift the projections bend by.
void AqueductBuildViewRows(const AqueductChannel& channel, int camX, int camY,
                           int subPos, int slope, int vjerk,
                           AqueductRow* rows);

// The second pass draws the rear-view mirror.
constexpr int AQU_MIRROR_ROWS = 9;

void AqueductBuildMirrorRows(const AqueductChannel& previous, int camX, int camY,
                          int subPos, int slope, AqueductRow* rows);

// An object spans a pair of rows, so N rows carry N - 1 objects.
constexpr int AQU_VIEW_OBJECT_ROWS = 14;
constexpr int AQU_MIRROR_OBJECT_ROWS = 8;

// One frame of the Aqueduct, in the order jroad.c FUN_002463b0 runs it.
struct AqueductFrame {
    AqueductChannel drawn;     // the channel this frame's rows were projected from
    AqueductChannel channel;   // what the last ride left; what gets drawn
    AqueductWallRide ride;     // this frame's ride, drawn next frame
    AqueductRow viewRows[AQU_VIEW_ROWS];
    AqueductRow mirrorRows[AQU_MIRROR_ROWS];
    int viewFlagIndex = 0;     // index into AqueductTrack::flags
    int mirrorFlagIndex = 0;      // index into AqueductTrack::mirror
};

void AqueductBuildFrame(AqueductFrame& frame, int camX, int camY,
                        int roadPos, int subPos, int slope, int vjerk);

constexpr int AQU_TRIG_SCALE = 256;
constexpr size_t AQU_SINE_TABLE_OFFSET = 0x3FC58;  // in the SR2 executable
constexpr int AQU_SINE_TABLE_ENTRIES = 91;         // 0..90 degrees

// Reads the sine table from the player's executable.
bool AqueductLoadSineTable(const std::vector<uint8_t>& exeData);

int AqueductSine(int degrees);

// The rotation the bank is applied with.
struct AqueductBank {
    int pivotX = 0;
    int pivotY = 0;
    int sin = 0;   // of the bank angle
    int cos = AQU_TRIG_SCALE;
};

AqueductBank AqueductBankFor(int pivotX, int pivotY, int bankDegrees);

// jdraw_a.c FUN_00244002: the climb bank plus the rollover roll, wrapped to +/-360.
int AqueductBankAngle(int rideBank, int roll);

enum AqueductSurface {
    AqueductFloor = 0,
    AqueductLeftWall = 1,
    AqueductRightWall = 2,
    AqueductSurfaceCount = 3,
};

struct AqueductQuad {
    int x[4] = {0, 0, 0, 0};
    int y[4] = {0, 0, 0, 0};
    AqueductSurface surface = AqueductFloor;
    int nearRow = 0;   // which row pair this came from, nearest first
};

// Builds the quads for a row table, banking every point as it goes. Returns
// how many were written; there are (rowCount - 1) * 3.
int AqueductBuildQuads(const AqueductRow* rows, int rowCount, const AqueductBank& bank,
                       AqueductQuad* out, int maxQuads);

// Banks about the rows' vanishing point.
AqueductBank AqueductBankForRows(const AqueductRow* rows, int rowCount, int bankDegrees);

// A horizontal run of pixels to plot.
struct AqueductSpan {
    int y = 0;
    int x0 = 0;   // inclusive
    int x1 = 0;   // inclusive
};

// Scan converts one quad into horizontal spans, clipped to the screen box.
int AqueductScanQuad(const AqueductQuad& quad, int clipTop, int clipBottom,
                     int screenWidth, AqueductSpan* out, int maxSpans);

void AqueductBankPoint(const AqueductBank& bank, int x, int y, int& outX, int& outY);

constexpr int AQU_CAM_CLAMP_MIN = 0x564;   // 1380
constexpr int AQU_CAM_CLAMP_MAX = 0xd5c;   // 3420
int AqueductClampCameraX(int camX);

// Screen x of a point `depth` away, `offset` across, on a road bending by `slope`.
int AqueductProjectViewX(int camX, int depth, int offset, int slope);
int AqueductProjectMirrorX(int camX, int depth, int offset, int slope);

// Screen y for the near pass.
int AqueductProjectY(int camY, int depth, int offset, int vjerk);

// Screen y for the mirror.
int AqueductProjectMirrorY(int camY, int depth, int offset);

// An obstacle stops the car dead.
constexpr int AQU_SUBPOS_PER_SEGMENT = 0x40;
constexpr int AQU_OBSTACLE_STOP_SUBPOS = 0x38;
constexpr int AQU_OBSTACLE_LOOKAHEAD = 2;
inline bool AqueductObstacleBlocks(uint16_t flagsAhead) {
    return (flagsAhead & AQU_OBSTACLE_MASK) != 0;
}

// jdraw_a.c FUN_00244484: 0x0010 is one band across the middle, 0x0080 a narrow box near each wall.
// 0x0080 is only tested when 0x0010 is clear.
struct AqueductObstacleBox {
    int left = 0;
    int right = 0;
};

// The obstacle as drawn: a cuboid spanning one band of the channel.
struct AqueductBlock {
    int nearLeft = 0, nearRight = 0, nearBottom = 0, nearTop = 0;
    int farLeft = 0, farRight = 0, farBottom = 0, farTop = 0;
    bool pillar = false;   // 0x0080; else the 0x0010 block across the middle
};

// FUN_002425b2's faces, in fill order after the bank: left and right side (pen 1) and top (pen 2) when they face
// the camera, then the near end (pen 1), always. Returns how many.
struct AqueductFace {
    AqueductQuad quad;
    int pen = 0;
};
int AqueductBlockFaces(const AqueductBlock& b, const AqueductBank& bank, AqueductFace* out);

// What the hit test sees of a block: the top of the middle block, only while it is drawn, or a pillar's footprint.
bool AqueductBlockHitQuad(const AqueductBlock& b, const AqueductBank& bank, AqueductQuad& out);

// FUN_00242024 on a quad clipped to the view: true when it reaches the bottom scanline anywhere right of x 80.
bool AqueductQuadHitsCar(const AqueductQuad& q, int clipTop, int clipBottom, int screenWidth);

// Builds the blocks a row pass puts on screen, farthest first so they can be drawn in that order.
int AqueductBuildBlocks(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                        int flagBase, AqueductBlock* out, int maxOut);

// Fills the boxes a segment's flags put in the way. Returns how many.
int AqueductObstacleBoxes(uint16_t flags, AqueductObstacleBox* out, int maxOut);

// FUN_00242024's hit test: point-in-polygon parity at two screen points on the car's scanline.
constexpr int AQU_HIT_PROBE_LEFT = 0x50;    // screen x 80
constexpr int AQU_HIT_PROBE_RIGHT = 0x13f;  // screen x 319
constexpr int AQU_HIT_SCANLINE = 99;
constexpr int AQU_NEAR_ROW_DEPTH = 4;

// Uses the clamped camera, so riding a wall stops the probes sliding out with the view.
inline int AqueductProbeX(int camX, int screenX) {
    return AqueductClampCameraX(camX) + screenX - 0xa0;
}

// Whether the car is actually in the way of what the segment carries.
bool AqueductObstacleHits(uint16_t flags, int camX);

// Obstacle sprite flags.
constexpr uint16_t AQU_OBSTACLE_DRAW_LEFT = 0x0020;
constexpr uint16_t AQU_OBSTACLE_DRAW_RIGHT = 0x0040;

// aqushape.c's nine records, each one lib.p asset sliced into seven tilts.
constexpr int AQU_SHAPE_COUNT = 9;
constexpr int AQU_SHAPE_PHASES = 7;
// The number stored in each record, which is ONE BASED: record 0 says 275 and means lib.p entry 274.
constexpr int AQU_SHAPE_ASSET_FIRST = 275;
constexpr int AQU_SHAPE_LADDER_FIRST = 2;   // the record holding lib.p 277
constexpr int AQU_SHAPE_LADDER_COUNT = 7;
constexpr size_t AQU_SHAPE_TABLE_OFFSET = 0x16970;  // in the SR2 executable

// One tilt of one shape: a sub-rect of that shape's decoded bitmap.
struct AqueductShapeRect {
    uint8_t srcX = 0, srcY = 0, w = 0, h = 0;
};

// Reads the table from the player's executable. The built-in copy is the fallback.
bool AqueductLoadShapeTable(const std::vector<uint8_t>& exeData);

// Zero-based lib.p index, i.e. the stored number less one.
int AqueductShapeAsset(int shape);
const AqueductShapeRect& AqueductShapeRectFor(int shape, int phase);

// Which of the seven pre-drawn tilts suits a bank angle. Even phases are
// positive bank, odd negative, and 0 is level.
int AqueductShapePhase(int bankDegrees);

// One sprite to draw: a centre already banked into screen space, and which
// slice to put there.
struct AqueductObstacle {
    int x = 0;
    int y = 0;
    int shape = 0;
    int phase = 0;
};

// Collects one row pass's obstacle sprites.
int AqueductBuildObstacles(const AqueductRow* rows, int rowCount,
                           const uint16_t* flags, int flagBase,
                           const AqueductBank& bank, int bankDegrees,
                           AqueductObstacle* out, int maxOut);

// jdraw_a.c FUN_00241f02: the night sky's stars, single pixels banked with the channel.
constexpr int AQU_SCREEN_WIDTH = 320;
constexpr int AQU_SKY_BOTTOM = 60;

constexpr int AQU_STARS_BRIGHT = 18;   // colour 10, at 0x3fcd8 in the exe
constexpr int AQU_STARS_DIM = 6;       // colour 1,  at 0x3fd20
constexpr int AQU_STARS_TOTAL = AQU_STARS_BRIGHT + AQU_STARS_DIM;
constexpr size_t AQU_STARS_BRIGHT_OFFSET = 0x3fcd8;
constexpr size_t AQU_STARS_DIM_OFFSET = 0x3fd20;
constexpr int AQU_STAR_COLOR_BRIGHT = 10;
constexpr int AQU_STAR_COLOR_DIM = 1;

struct AqueductStar {
    int x = 0;
    int y = 0;
    int color = AQU_STAR_COLOR_BRIGHT;   // index into the driving palette
};

// Reads the tables from the player's executable.
bool AqueductLoadStarTable(const std::vector<uint8_t>& exeData);

// jdraw_a.c FUN_00242536: (heading * 2) % 320. Stars scroll by half of it, the horizon sprites by all of it.
int AqueductSkyBase(int heading);

// The three fixed sprites on the horizon, drawn by FUN_00241f02 off the same base.
constexpr int AQU_HORIZON_SPRITES = 3;
int AqueductBuildHorizon(int base, const AqueductBank& bank, int bankDegrees,
                         AqueductObstacle* out, int maxOut);

// The sky for one frame, offset by baseX and banked with the channel.
int AqueductBuildSky(int baseX, const AqueductBank& bank, int bankDegrees,
                     AqueductStar* out, int maxOut);

// The mirror's sky: FUN_0023d1e8 clears the glass and, in the Aqueduct only, plots its own 9 bright and 3 dim
// stars through FUN_00241f88, offset by -(skyBase >> 2) and banked about the mirror's pivot.
constexpr int AQU_MIRROR_STARS_BRIGHT = 9;
constexpr int AQU_MIRROR_STARS_DIM = 3;
constexpr size_t AQU_MIRROR_STARS_BRIGHT_OFFSET = 0x3fd38;
constexpr size_t AQU_MIRROR_STARS_DIM_OFFSET = 0x3fd5c;
int AqueductBuildMirrorSky(int skyBase, const AqueductBank& bank, int bankDegrees,
                           AqueductStar* out, int maxOut);

// jdraw_a.c FUN_00242bea's line work: four long edges, then three joints per segment with flag bit 0.
constexpr int AQU_PEN_EDGE = 0;      // black
// Floor and both slopes.
constexpr int AQU_PEN_CONCRETE = 2;
constexpr int AQU_PEN_SLOPE_TOP = 4;
constexpr uint16_t AQU_FLAG_JOINT = 0x0001;

// Concrete patches on the slopes that the car crashes into (jdraw_a.c FUN_002429b6).
// 0x0004/0x0008 start a quarter up the slope, 0x0100/0x0200 at the floor edge.
constexpr uint16_t AQU_SLOPE_OBSTACLE_LEFT = 0x0104;
constexpr uint16_t AQU_SLOPE_OBSTACLE_RIGHT = 0x0208;
constexpr uint16_t AQU_SLOPE_OBSTACLE_INSET = 0x000c;  // clear = starts at the floor
constexpr int AQU_PEN_SLOPE_OBSTACLE = 1;
// The solid blocks: sides and far end in pen 1, the top in pen 2.
constexpr int AQU_PEN_BLOCK_SIDE = 1;
constexpr int AQU_PEN_BLOCK_TOP = 2;

// The outer lip of each slope, which is where the concrete runs out.
constexpr int AQU_OUTER_LEFT = 0x438;   // 1080
constexpr int AQU_OUTER_RIGHT = 0xe88;  // 3720
constexpr int AQU_SLOPE_WIDTH = AQU_FLOOR_LEFT - AQU_OUTER_LEFT;  // 400

// Whether either probe is on the part of the slope a patch covers.
bool AqueductSlopePatchHits(uint16_t flags, int camX);

inline bool AqueductSlopeObstacle(uint16_t flags) {
    return (flags & (AQU_SLOPE_OBSTACLE_LEFT | AQU_SLOPE_OBSTACLE_RIGHT)) != 0;
}

// Fills the slope patches a row pass puts on screen, farthest first.
int AqueductBuildSlopeObstacles(const AqueductRow* rows, int rowCount,
                                const uint16_t* flags, int flagBase,
                                const AqueductBank& bank, int bankDegrees,
                                AqueductQuad* out, int maxOut);

// A band painted across the channel floor, in pen 4.
constexpr uint16_t AQU_FLAG_FLOOR_BAND = 0x0400;
constexpr int AQU_PEN_BAND = 4;

// Fills the floor bands a row pass puts on screen. Returns how many.
int AqueductBuildFloorBands(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                            int flagBase, const AqueductBank& bank, int bankDegrees,
                            AqueductQuad* out, int maxOut);

struct AqueductLine {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int pen = AQU_PEN_EDGE;
};

// Builds the line work for a row pass, banked. Returns how many were written;
// the four long edges come first, then the joints.
int AqueductBuildLines(const AqueductRow* rows, int rowCount, const uint16_t* flags,
                       int flagBase, const AqueductBank& bank, int bankDegrees,
                       AqueductLine* out, int maxOut);

// The opponent is drawn from pre-tilted frames (jdraw.c FUN_0023d7b8): seven per view state, -45..45 degrees.
constexpr int AQU_OPP_TILTS = 7;
constexpr int AQU_OPP_LEVEL_TILT = 3;
constexpr int AQU_OPP_TILT_ANGLE[AQU_OPP_TILTS] = {-45, -30, -15, 0, 15, 30, 45};   // A4-0x263c

struct AqueductOpponentTilt {
    int tilt = AQU_OPP_LEVEL_TILT;   // -1: past 55 degrees, the level frame at a rotated anchor
    int angle = 0;
};

// FUN_0023d45a: how far up a slope the opponent is, plus the bank.
AqueductOpponentTilt AqueductOpponentTiltFor(int npcX, const AqueductChannel& drawn, int bankDegrees);

// The world y the opponent stands at, lifted when it rides up a slope.
int AqueductOpponentGroundY(int npcX, const AqueductChannel& drawn);

// FUN_0023d592: where the bottom-centre anchor of a w x h frame lands inside its box once turned by `angle`.
// yOff is measured up from the box's bottom.
void AqueductTiltAnchor(int w, int h, int angle, int anchorX, int& xOff, int& yOff);

// A tilt frame made from a level one, for bodies the original never drew tilted: turned by `angle` as the bank
// turns the channel, nearest neighbour, into exactly the box AqueductTiltAnchor measures. Uncovered pixels are
// `clear`.
template <typename Px>
std::vector<Px> AqueductRotateSprite(const std::vector<Px>& px, int w, int h, int angle, Px clear, int& outW,
                                     int& outH) {
    const AqueductBank turn = AqueductBankFor(0, 0, angle);
    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    const int cx[3] = {0, w - 1, w - 1};
    const int cy[3] = {h - 1, h - 1, 0};
    for (int k = 0; k < 3; ++k) {
        int x = 0, y = 0;
        AqueductBankPoint(turn, cx[k], cy[k], x, y);
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    outW = maxX - minX + 1;
    outH = maxY - minY + 1;
    std::vector<Px> out(static_cast<size_t>(outW) * outH, clear);
    // The inverse of the forward turn x' = (x*cos + y*sin), y' = (y*cos - x*sin).
    const double c = turn.cos / 256.0, s = turn.sin / 256.0, det = c * c + s * s;
    for (int dy = 0; dy < outH; ++dy) {
        for (int dx = 0; dx < outW; ++dx) {
            const double X = dx + minX, Y = dy + minY;
            const int sx = static_cast<int>(std::lround((X * c - Y * s) / det));
            const int sy = static_cast<int>(std::lround((X * s + Y * c) / det));
            if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                out[static_cast<size_t>(dy) * outW + dx] = px[static_cast<size_t>(sy) * w + sx];
            }
        }
    }
    return out;
}

// lrace.c FUN_0025a034: the opponent's sideways step in the Aqueduct. `delta` is what the ordinary banding wants;
// a block within eight segments of the opponent overrides it with one of five lanes off the live channel, and the
// closer the block, the harder it swerves.
int AqueductOpponentStep(int npcX, int delta, int npcSeg, int playerX, int playerSeg,
                         const AqueductChannel& drawn, const uint16_t* flags);

// FUN_00242bea's midline, pen 1: one strip from the far row to the near one, cut where the first run of flag
// 0x0002 in view lies - four segments in every 24, so it reads as long dashes. Returns how many quads (0..2).
constexpr uint16_t AQU_FLAG_MIDLINE_GAP = 0x0002;
constexpr int AQU_PEN_MIDLINE = 1;
int AqueductBuildMidline(const AqueductRow* rows, int rowCount, const uint16_t* flags, int flagBase,
                         const AqueductBank& bank, AqueductQuad* out);

// One frame of riding the wall. Neutral when the car is on the floor.
AqueductWallRide AqueductRideWall(int camX);

// Which wall the car has gone over the top of: +1 off the left, -1 off the
// right, 0 still on the track. The original's wall_state.
int AqueductWallState(int camX);

// One frame of a rollover.
struct AqueductRollFrame {
    int roll = 0;        // absolute angle this frame, A4+0x26e6
    int camXDelta = 0;
    int vjerk = 0;       // vertical launch, A4+0x26d4 - Aqueduct only
    int driftDelta = 0;
};

// Fills the frames a rollover plays and returns how many.
int AqueductRollover(int wallState, int speedMph, int drift,
                     AqueductRollFrame* out, int maxFrames);

}  // namespace srrace
