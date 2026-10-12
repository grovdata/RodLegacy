#include <doctest/doctest.h>

#include "race/aqueduct.h"

#include <cstdlib>

// The Aqueduct channel. Behaviour and constants from
// RodLegacyResearch/docs/sr2_amiga/races.md, "The sloped walls".

TEST_CASE("the channel floor is flat between the walls") {
    // Anywhere the car fits between 1480 and 3320 it is on the floor: no
    // bank, no camera nudge, and nothing to roll over.
    const int centred = (srrace::AQU_FLOOR_LEFT + srrace::AQU_FLOOR_RIGHT) / 2;
    const srrace::AqueductWallRide ride = srrace::AqueductRideWall(centred);
    CHECK(ride.climb == 0);
    CHECK(ride.bank == 0);
    CHECK(ride.camXDelta == 0);
    CHECK_FALSE(ride.pastCap);
    CHECK(srrace::AqueductWallState(centred) == 0);
}

TEST_CASE("riding a wall banks the car and slides the camera") {
    // The car's edges are measured off the camera, and asymmetrically, so
    // the two walls do not start at the same camera position.
    const int onLeftWall = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 100;
    const srrace::AqueductWallRide left = srrace::AqueductRideWall(onLeftWall);
    CHECK(left.climb == 100);
    CHECK(left.bank == 100 * 0x6c / 400);
    CHECK(left.bank > 0);
    CHECK(left.camXDelta == 0xf);      // pushed back toward the middle
    CHECK_FALSE(left.pastCap);

    const int onRightWall = srrace::AQU_FLOOR_RIGHT - srrace::AQU_CAR_RIGHT_OFFSET + 100;
    const srrace::AqueductWallRide right = srrace::AqueductRideWall(onRightWall);
    CHECK(right.climb == 100);
    CHECK(right.bank == 100 * -0x5a / 400);
    CHECK(right.bank < 0);             // banks the other way
    CHECK(right.camXDelta == -0xf);
    CHECK_FALSE(right.pastCap);

    // The slopes are not mirror images: the left is wider and banks harder.
    CHECK(srrace::AQU_LEFT_CLIMB_CAP > srrace::AQU_RIGHT_CLIMB_CAP);
    const int leftTop = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET
                        - srrace::AQU_LEFT_CLIMB_CAP;
    CHECK(srrace::AqueductRideWall(leftTop).climb == srrace::AQU_LEFT_CLIMB_CAP);
}

TEST_CASE("past the top of the slope the camera is shoved harder") {
    const int wayUpLeft = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET
                          - srrace::AQU_LEFT_CLIMB_CAP - 50;
    const srrace::AqueductWallRide ride = srrace::AqueductRideWall(wayUpLeft);
    CHECK(ride.pastCap);
    CHECK(ride.climb == srrace::AQU_LEFT_CLIMB_CAP);   // the climb still caps
    CHECK(ride.camXDelta == 0xf + 0x19);
}

TEST_CASE("only past the far edge does the car leave the track") {
    // Riding the slope is a supported state; wall_state stays 0 for it.
    const int onSlope = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 150;
    CHECK(srrace::AqueductWallState(onSlope) == 0);

    const int overLeft = srrace::AQU_ROLL_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 1;
    CHECK(srrace::AqueductWallState(overLeft) == 1);

    const int overRight = srrace::AQU_ROLL_RIGHT - srrace::AQU_CAR_RIGHT_OFFSET;
    CHECK(srrace::AqueductWallState(overRight) == -1);
}

TEST_CASE("a slow car banks and drops back instead of rolling") {
    srrace::AqueductRollFrame frames[16];
    const int n = srrace::AqueductRollover(1, srrace::AQU_ROLL_MIN_MPH - 1, 60,
                                           frames, 16);
    REQUIRE(n == 1);
    CHECK(frames[0].roll == -6);        // banks back the other way
    CHECK(frames[0].vjerk == 10);
    CHECK(frames[0].camXDelta == 0);

    // Off the other wall it banks the opposite way.
    srrace::AqueductRollFrame other[16];
    REQUIRE(srrace::AqueductRollover(-1, 10, 60, other, 16) == 1);
    CHECK(other[0].roll == 6);

    // Still on the track is not a rollover at all.
    CHECK(srrace::AqueductRollover(0, 90, 60, frames, 16) == 0);
}

TEST_CASE("at speed the car goes right over") {
    srrace::AqueductRollFrame frames[16];
    const int n = srrace::AqueductRollover(1, 60, 120, frames, 16);
    REQUIRE(n == 9);                    // eight steps plus the landing

    // Eight steps of 0x20 is a full turn on a 256-unit angle.
    CHECK(frames[7].roll == srrace::AQU_ROLL_FULL_TURN);
    CHECK(frames[8].roll == 0);         // lands flat

    // Thrown across the channel the whole way.
    for (int i = 0; i < 8; ++i) {
        CHECK(frames[i].camXDelta == 0x28);
    }

    // The launch arc rises then falls, and is zero at both ends.
    CHECK(frames[0].vjerk == 0);
    // The index runs 0 1 2 3 4 3 2 1 - it peaks at i=4, where 8-i is still 4,
    // not at a flat 3-3 crest.
    CHECK(frames[4].vjerk > frames[3].vjerk);
    CHECK(frames[3].vjerk > frames[1].vjerk);
    CHECK(frames[5].vjerk == frames[3].vjerk);
    CHECK(frames[7].vjerk < frames[4].vjerk);

    // Drift bleeds off in equal steps, taken once before the roll starts.
    CHECK(frames[0].driftDelta == -(120 / 6));
    CHECK(frames[6].driftDelta == frames[0].driftDelta);

    // The right wall rolls the other way, and not by the same step: 0x22
    // against 0x20, so it overshoots a full turn.
    srrace::AqueductRollFrame right[16];
    REQUIRE(srrace::AqueductRollover(-1, 60, 120, right, 16) == 9);
    CHECK(right[7].roll == -8 * 0x22);
    CHECK(right[0].camXDelta == -0x28);
}

TEST_CASE("a standing car cannot roll on vjerk alone") {
    // vjerk scales with speed, so at the threshold the arc is still shallow.
    srrace::AqueductRollFrame frames[16];
    REQUIRE(srrace::AqueductRollover(1, srrace::AQU_ROLL_MIN_MPH, 0, frames, 16) == 9);
    CHECK(frames[3].vjerk == (srrace::AQU_ROLL_MIN_MPH / 6) * 3);
    CHECK(frames[0].driftDelta == 0);   // no drift to bleed
}

// ---------------------------------------------------------------------------
// Projection: the view's 15 rows centred on 160 at 4x, the mirror's 9 centred on 280.
// ---------------------------------------------------------------------------

TEST_CASE("the camera is clamped for projection, not for driving") {
    CHECK(srrace::AqueductClampCameraX(2400) == 2400);
    CHECK(srrace::AqueductClampCameraX(0) == srrace::AQU_CAM_CLAMP_MIN);
    CHECK(srrace::AqueductClampCameraX(9999) == srrace::AQU_CAM_CLAMP_MAX);

    // Riding a wall moves the car well past the clamp, and the view stops
    // following it - that is what keeps the channel on screen.
    const int farUpTheWall = srrace::AQU_ROLL_LEFT - 200;
    CHECK(srrace::AqueductClampCameraX(farUpTheWall) == srrace::AQU_CAM_CLAMP_MIN);
    CHECK(srrace::AqueductProjectViewX(farUpTheWall, 8, 0, 0) ==
          srrace::AqueductProjectViewX(0, 8, 0, 0));
}

TEST_CASE("the two horizontal projections differ by more than their centre") {
    const int camX = 2400, depth = 8, offset = 1480, slope = 0;
    // The near pass works at 4x, so the same point lands elsewhere.
    const int near = srrace::AqueductProjectViewX(camX, depth, offset, slope);
    const int far = srrace::AqueductProjectMirrorX(camX, depth, offset, slope);
    CHECK(near != far);
    CHECK(0xa0 - near == 4 * (0x118 - far));

    // Both centre the camera's own lateral.
    CHECK(srrace::AqueductProjectViewX(camX, depth, camX, 0) == 0xa0);
    CHECK(srrace::AqueductProjectMirrorX(camX, depth, camX, 0) == 0x118);

    // Further away is nearer the centre of the screen.
    const int channelRight = srrace::AQU_FLOOR_RIGHT;
    CHECK(std::abs(srrace::AqueductProjectViewX(camX, 64, channelRight, 0) - 0xa0) <
          std::abs(srrace::AqueductProjectViewX(camX, 8, channelRight, 0) - 0xa0));
}

TEST_CASE("vertical projection carries the launch arc") {
    const int camY = 0, depth = 8, offset = 2000;
    const int level = srrace::AqueductProjectY(camY, depth, offset, 0);
    // vjerk lifts the whole row, one screen unit per unit of jerk.
    CHECK(srrace::AqueductProjectY(camY, depth, offset, 10) == level + 10);
    // A zero depth must not divide by zero - the original substitutes 1.
    CHECK(srrace::AqueductProjectY(camY, 0, offset, 0) ==
          srrace::AqueductProjectY(camY, 1, offset, 0));
    CHECK(srrace::AqueductProjectViewX(2400, 0, 0, 0) ==
          srrace::AqueductProjectViewX(2400, 1, 0, 0));
}

TEST_CASE("steering does not pitch the channel") {
    // The heading bends the channel sideways and must not move it vertically.
    srrace::AqueductChannel flat;
    srrace::AqueductRow straight[srrace::AQU_VIEW_ROWS];
    srrace::AqueductRow turning[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, straight);
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 40, 0, turning);

    for (int i = 0; i < srrace::AQU_VIEW_ROWS; ++i) {
        CHECK(turning[i].y == straight[i].y);
        CHECK(turning[i].y2 == straight[i].y2);
    }
    // The sideways pan is real.
    CHECK(turning[1].leftX != straight[1].leftX);
    for (int i = 1; i < srrace::AQU_VIEW_ROWS; ++i) {
        const int pan = turning[i].leftX - straight[i].leftX;
        CHECK(pan >= 39);
        CHECK(pan <= 40);
    }
}

TEST_CASE("obstacles stop the car rather than crashing it") {
    srrace::AqueductTrack t{};
    srrace::BuildAqueductTrack(t);

    // Both obstacle kinds block; nothing else does.
    CHECK(srrace::AqueductObstacleBlocks(t.flags[4]));     // one box across
    CHECK(srrace::AqueductObstacleBlocks(t.flags[124]));   // one box per side
    CHECK_FALSE(srrace::AqueductObstacleBlocks(t.flags[3]));
    CHECK_FALSE(srrace::AqueductObstacleBlocks(t.flags[0]));

    // The check reads two segments ahead of where the car is.
    const int roadPos = 4 - srrace::AQU_OBSTACLE_LOOKAHEAD;
    CHECK(srrace::AqueductObstacleBlocks(t.flags[roadPos + srrace::AQU_OBSTACLE_LOOKAHEAD]));

    // The stop is a road sub-position, not a speed: the road position is a
    // segment plus 0..0x3f, and an obstacle pins it 56 steps of 64 in.
    CHECK(srrace::AQU_OBSTACLE_STOP_SUBPOS < srrace::AQU_SUBPOS_PER_SEGMENT);
    CHECK(srrace::AQU_SUBPOS_PER_SEGMENT == 0x40);
}

// ---------------------------------------------------------------------------
// The near pass: fifteen rows of projected channel. The channel offsets are
// wall-ride state, zeroed every frame before at most one handler writes them.
// ---------------------------------------------------------------------------

TEST_CASE("on the floor the channel is flat") {
    const int centred = (srrace::AQU_FLOOR_LEFT + srrace::AQU_FLOOR_RIGHT) / 2;
    const srrace::AqueductWallRide ride = srrace::AqueductRideWall(centred);
    CHECK(ride.channel.yBase == 0);
    CHECK(ride.channel.leftFloor == 0);
    CHECK(ride.channel.rightFloor == 0);
    CHECK(ride.channel.yBase2 == 0);
    CHECK(ride.channel.outerLeft == 0);
    CHECK(ride.channel.outerRight == 0);
}

TEST_CASE("the two walls skew the channel differently") {
    // Not a mirror image: the left wall pushes the floors the same way as
    // each other, the right one pushes them opposite ways.
    const int onLeft = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 100;
    const srrace::AqueductChannel left = srrace::AqueductRideWall(onLeft).channel;
    CHECK(left.leftFloor == 100 / 4);
    CHECK(left.rightFloor == 100 / 4);
    CHECK(left.outerRight == 100 / 4);
    CHECK(left.yBase == 100 - (100 >> 3));
    CHECK(left.outerLeft == 0);       // only past the top of the slope

    const int onRight = srrace::AQU_FLOOR_RIGHT - srrace::AQU_CAR_RIGHT_OFFSET + 100;
    const srrace::AqueductChannel right = srrace::AqueductRideWall(onRight).channel;
    CHECK(right.rightFloor == 100 * -3);
    CHECK(right.leftFloor == 100 * -2);
    CHECK(right.outerLeft == 100 * -2);
    CHECK(right.yBase == 100 - (100 >> 2));
    CHECK(right.yBase2 == (100 >> 2) - 100);
    CHECK(right.yBase2 < 0);
}

TEST_CASE("past the top of the slope the channel is pushed further over") {
    const int wayUp = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET
                      - srrace::AQU_LEFT_CLIMB_CAP - 50;
    const srrace::AqueductWallRide ride = srrace::AqueductRideWall(wayUp);
    REQUIRE(ride.pastCap);
    CHECK(ride.channel.outerLeft == 50);
    // The extra rides on top of the capped climb, so the floor keeps moving
    // even though the climb itself has stopped.
    CHECK(ride.channel.leftFloor == (srrace::AQU_LEFT_CLIMB_CAP >> 2) + 50);
    CHECK(ride.channel.yBase2 == 50);

    // And it is capped in its own right, so it cannot run away.
    const int miles = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET
                      - srrace::AQU_LEFT_CLIMB_CAP - 900;
    CHECK(srrace::AqueductRideWall(miles).channel.outerLeft == 200);
}

TEST_CASE("the near pass builds fifteen rows, near to far") {
    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0, 0, 0, rows);

    // The channel narrows toward the horizon, and the rows are ordered by
    // distance, so each is no wider than the one before it.
    for (int i = 1; i < srrace::AQU_VIEW_ROWS; ++i) {
        const int prev = rows[i - 1].rightX - rows[i - 1].leftX;
        const int here = rows[i].rightX - rows[i].leftX;
        CHECK(here <= prev);
    }
    // The nearest row is the widest by a long way.
    CHECK(rows[0].rightX - rows[0].leftX > rows[14].rightX - rows[14].leftX);

    // The mid marker sits between the two floors on every row.
    for (int i = 0; i < srrace::AQU_VIEW_ROWS; ++i) {
        CHECK(rows[i].midX >= rows[i].leftX);
        CHECK(rows[i].midX <= rows[i].rightX);
    }
}

TEST_CASE("the sub-segment position scrolls the middle rows only") {
    srrace::AqueductChannel flat;
    srrace::AqueductRow a[srrace::AQU_VIEW_ROWS];
    srrace::AqueductRow b[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0, 0, 0, a);
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0x20, 0, 0, b);

    // Row 15 is drawn first and sits at a fixed depth, so it does not move.
    CHECK(a[0].leftX == b[0].leftX);
    // The middle rows slide, which is what makes the channel scroll.
    CHECK(a[5].leftX != b[5].leftX);
    // The last three are pinned again: the slip is added back at row 3.
    CHECK(a[14].leftX == b[14].leftX);
    CHECK(a[13].leftX == b[13].leftX);
}

TEST_CASE("a rollover lifts every row together") {
    srrace::AqueductChannel flat;
    srrace::AqueductRow level[srrace::AQU_VIEW_ROWS];
    srrace::AqueductRow jerked[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0, 0, 0, level);
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0, 0, 10, jerked);

    for (int i = 0; i < srrace::AQU_VIEW_ROWS; ++i) {
        CHECK(jerked[i].y == level[i].y + 10);
        CHECK(jerked[i].y2 == level[i].y2 + 10);
        CHECK(jerked[i].leftX == level[i].leftX);   // vertical only
    }
}

// ---------------------------------------------------------------------------
// The mirror pass: nine rows, drawn after the view's objects.
// ---------------------------------------------------------------------------

TEST_CASE("the far vertical projection is its own, not the near one re-centred") {
    const int camY = 0, depth = 8, offset = 2000;

    // Different centre and a different scale, so the two do not differ by a
    // constant as depth changes.
    const int nearA = srrace::AqueductProjectY(camY, depth, offset, 0);
    const int farA = srrace::AqueductProjectMirrorY(camY, depth, offset);
    const int nearB = srrace::AqueductProjectY(camY, depth * 2, offset, 0);
    const int farB = srrace::AqueductProjectMirrorY(camY, depth * 2, offset);
    CHECK((nearA - farA) != (nearB - farB));

    // A rollover lifts the near half only - the far half has no vjerk term.
    CHECK(srrace::AqueductProjectY(camY, depth, offset, 10) == nearA + 10);

    // Zero depth must not divide by zero.
    CHECK(srrace::AqueductProjectMirrorY(camY, 0, offset) ==
          srrace::AqueductProjectMirrorY(camY, 1, offset));
}

TEST_CASE("the far pass builds nine rows that narrow with distance") {
    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_MIRROR_ROWS];
    srrace::AqueductBuildMirrorRows(flat, 2400, 0, 0, 0, rows);

    for (int i = 1; i < srrace::AQU_MIRROR_ROWS; ++i) {
        const int prev = rows[i - 1].rightX - rows[i - 1].leftX;
        const int here = rows[i].rightX - rows[i].leftX;
        CHECK(here <= prev);
    }
    for (int i = 0; i < srrace::AQU_MIRROR_ROWS; ++i) {
        CHECK(rows[i].midX >= rows[i].leftX);
        CHECK(rows[i].midX <= rows[i].rightX);
    }
}

TEST_CASE("the two passes slide opposite ways") {
    // The near pass subtracts the sub-segment slip and adds it back.
    srrace::AqueductChannel flat;
    srrace::AqueductRow nearA[srrace::AQU_VIEW_ROWS], nearB[srrace::AQU_VIEW_ROWS];
    srrace::AqueductRow farA[srrace::AQU_MIRROR_ROWS], farB[srrace::AQU_MIRROR_ROWS];

    srrace::AqueductBuildViewRows(flat, 2400, 0, 0, 0, 0, nearA);
    srrace::AqueductBuildViewRows(flat, 2400, 0, 0x30, 0, 0, nearB);
    srrace::AqueductBuildMirrorRows(flat, 2400, 0, 0, 0, farA);
    srrace::AqueductBuildMirrorRows(flat, 2400, 0, 0x30, 0, farB);

    // Row index 4 is inside the sliding band of both passes.
    const int nearMoved = (nearB[4].rightX - nearB[4].leftX) -
                          (nearA[4].rightX - nearA[4].leftX);
    const int farMoved = (farB[4].rightX - farB[4].leftX) -
                         (farA[4].rightX - farA[4].leftX);
    CHECK(nearMoved != 0);
    CHECK(farMoved != 0);
    CHECK(((nearMoved > 0) != (farMoved > 0)));
}

TEST_CASE("the far pass projects last frame's channel") {
    // The dispatcher keeps a copy of the channel before zeroing it, and the
    // far pass reads that - so riding a wall skews the two halves by a frame.
    srrace::AqueductChannel flat;
    srrace::AqueductChannel skewed;
    skewed.leftFloor = 40;
    skewed.rightFloor = 40;

    srrace::AqueductRow onFloor[srrace::AQU_MIRROR_ROWS];
    srrace::AqueductRow onWall[srrace::AQU_MIRROR_ROWS];
    srrace::AqueductBuildMirrorRows(flat, 2400, 0, 0, 0, onFloor);
    srrace::AqueductBuildMirrorRows(skewed, 2400, 0, 0, 0, onWall);
    CHECK(onFloor[0].leftX != onWall[0].leftX);
}

// ---------------------------------------------------------------------------
// The frame sequence: view rows, view objects, mirror rows, mirror objects.
// ---------------------------------------------------------------------------

TEST_CASE("a frame draws the channel the last ride left") {
    // The near rows are built before the ride runs.
    const int upTheWall = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 100;

    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, upTheWall, 0, 100, 0, 0, 0);
    const int firstLeft = frame.viewRows[0].leftX;
    CHECK(frame.ride.climb == 100);          // the ride is seen at once

    // ...but what was drawn is the flat channel the frame started with.
    srrace::AqueductRow flat[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(srrace::AqueductChannel{}, upTheWall, 0, 0, 0, 0, flat);
    CHECK(firstLeft == flat[0].leftX);

    // The next frame, same camera, draws the skew the ride left behind.
    srrace::AqueductBuildFrame(frame, upTheWall, 0, 100, 0, 0, 0);
    CHECK(frame.viewRows[0].leftX != firstLeft);

    // And it settles: a third frame at the same place matches the second.
    const int settled = frame.viewRows[0].leftX;
    srrace::AqueductBuildFrame(frame, upTheWall, 0, 100, 0, 0, 0);
    CHECK(frame.viewRows[0].leftX == settled);
}

TEST_CASE("each pass walks its own flag array") {
    srrace::AqueductFrame frame;
    srrace::AqueductTrack track{};
    srrace::BuildAqueductTrack(track);

    const int roadPos = 120;
    srrace::AqueductBuildFrame(frame, 2400, 0, roadPos, 0, 0, 0);

    CHECK(frame.viewFlagIndex == roadPos);
    // The far pass starts from the other end of the mirrored copy.
    CHECK(frame.mirrorFlagIndex == 370 - roadPos);

    // Both indices stay inside the track wherever the car is on it.
    for (int p = 0; p < srrace::AQUEDUCT_SEGMENTS; ++p) {
        srrace::AqueductBuildFrame(frame, 2400, 0, p, 0, 0, 0);
        CHECK(frame.viewFlagIndex >= 0);
        CHECK(frame.viewFlagIndex < srrace::AQUEDUCT_SEGMENTS);
        CHECK(frame.mirrorFlagIndex >= 0);
        CHECK(frame.mirrorFlagIndex < srrace::AQUEDUCT_SEGMENTS);
    }

    // Walking the mirror from that index reaches the same segment the near
    // pass starts at, which is what makes the two halves meet.
    CHECK(track.mirror[370 - roadPos] == track.flags[roadPos]);
}

TEST_CASE("the object passes are one shorter than the row passes") {
    // An object spans a pair of rows, so fifteen rows carry fourteen objects.
    CHECK(srrace::AQU_VIEW_OBJECT_ROWS == srrace::AQU_VIEW_ROWS - 1);
    CHECK(srrace::AQU_MIRROR_OBJECT_ROWS == srrace::AQU_MIRROR_ROWS - 1);
}

// ---------------------------------------------------------------------------
// Banking. The projection does not tilt the channel; every projected point is
// rotated about a pivot afterwards by the bank angle.
// ---------------------------------------------------------------------------

TEST_CASE("the sine table works in whole degrees at 8-bit fixed point") {
    CHECK(srrace::AqueductSine(0) == 0);
    CHECK(srrace::AqueductSine(30) == 128);          // half of 256
    CHECK(srrace::AqueductSine(90) == 255);          // a byte cannot hold 256
    CHECK(srrace::AqueductSine(45) == 181);

    // Quadrant folding: the table only covers 0..90.
    CHECK(srrace::AqueductSine(150) == srrace::AqueductSine(30));
    CHECK(srrace::AqueductSine(210) == -srrace::AqueductSine(30));
    CHECK(srrace::AqueductSine(330) == -srrace::AqueductSine(30));
    CHECK(srrace::AqueductSine(-30) == -srrace::AqueductSine(30));

    // The second coefficient is the angle plus a quarter turn, i.e. cosine.
    CHECK(srrace::AqueductSine(0 + 90) == 255);
    CHECK(srrace::AqueductSine(60 + 90) == srrace::AqueductSine(30));
}

TEST_CASE("a level car is not rotated at all") {
    const srrace::AqueductBank level = srrace::AqueductBankFor(160, 100, 0);
    CHECK(level.sin == 0);
    CHECK(level.cos == 255);

    // With cos 255 rather than 256 the identity is off by a part in 256, so a
    // point is within a pixel of where it started rather than exactly on it.
    int x = 0, y = 0;
    srrace::AqueductBankPoint(level, 200, 140, x, y);
    CHECK(std::abs(x - 200) <= 1);
    CHECK(std::abs(y - 140) <= 1);

    // The pivot itself never moves, whatever the angle.
    for (int deg : {0, 15, 48, -48, 90}) {
        const srrace::AqueductBank b = srrace::AqueductBankFor(160, 100, deg);
        int px = 0, py = 0;
        srrace::AqueductBankPoint(b, 160, 100, px, py);
        CHECK(px == 160);
        CHECK(py == 100);
    }
}

TEST_CASE("riding a wall tilts the channel") {
    // The left wall banks one way and the right the other, so a point on the
    // channel floor swings in opposite directions.
    const int onLeft = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET - 100;
    const int onRight = srrace::AQU_FLOOR_RIGHT - srrace::AQU_CAR_RIGHT_OFFSET + 100;
    const int leftBank = srrace::AqueductRideWall(onLeft).bank;
    const int rightBank = srrace::AqueductRideWall(onRight).bank;
    REQUIRE(leftBank > 0);
    REQUIRE(rightBank < 0);

    int lx = 0, ly = 0, rx = 0, ry = 0;
    srrace::AqueductBankPoint(srrace::AqueductBankFor(160, 100, leftBank), 260, 100, lx, ly);
    srrace::AqueductBankPoint(srrace::AqueductBankFor(160, 100, rightBank), 260, 100, rx, ry);
    CHECK(ly != 100);
    CHECK(ry != 100);
    CHECK(((ly > 100) != (ry > 100)));   // opposite tilts

    // A full-lock bank is a big angle, not a nudge: the left slope reaches
    // 180 units of climb, which is 48 degrees.
    const int atCap = srrace::AQU_FLOOR_LEFT + srrace::AQU_CAR_LEFT_OFFSET
                      - srrace::AQU_LEFT_CLIMB_CAP;
    CHECK(srrace::AqueductRideWall(atCap).bank == srrace::AQU_LEFT_CLIMB_CAP * 0x6c / 400);
    CHECK(srrace::AqueductRideWall(atCap).bank == 48);
}

TEST_CASE("the sine table is read from the executable when it is there") {
    // Too short: the computed fallback stands in, and it agrees with the
    // shipped table closely enough that nothing visibly moves.
    CHECK_FALSE(srrace::AqueductLoadSineTable(std::vector<uint8_t>(16, 0)));
    CHECK(srrace::AqueductSine(30) == 128);

    // A table that does not rise to a peak is not a sine table.
    std::vector<uint8_t> junk(srrace::AQU_SINE_TABLE_OFFSET
                              + srrace::AQU_SINE_TABLE_ENTRIES, 0xAB);
    CHECK_FALSE(srrace::AqueductLoadSineTable(junk));
    CHECK(srrace::AqueductSine(90) == 255);
}

// ---------------------------------------------------------------------------
// The drawable channel: three quads per band of concrete, banked into screen
// space. Everything after this is an ordinary polygon fill.
// ---------------------------------------------------------------------------

TEST_CASE("every band of the channel is three quads") {
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 0, 100, 0, 0, 0);
    const srrace::AqueductBank level = srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 0);

    srrace::AqueductQuad quads[(srrace::AQU_VIEW_ROWS - 1) * 3];
    const int n = srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                                             level, quads, 64);
    // Fifteen rows give fourteen bands, which is the count the object pass
    // walks - the two numbers have to agree or the channel has a gap in it.
    CHECK(n == (srrace::AQU_VIEW_ROWS - 1) * srrace::AqueductSurfaceCount);
    CHECK(n / srrace::AqueductSurfaceCount == srrace::AQU_VIEW_OBJECT_ROWS);

    // Each band carries one of each surface, nearest band first.
    CHECK(quads[0].surface == srrace::AqueductFloor);
    CHECK(quads[1].surface == srrace::AqueductLeftWall);
    CHECK(quads[2].surface == srrace::AqueductRightWall);
    CHECK(quads[0].nearRow == 0);
    CHECK(quads[3].nearRow == 1);
}

TEST_CASE("the channel is a floor with a wall either side") {
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 0, 100, 0, 0, 0);
    const srrace::AqueductBank level = srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 0);
    srrace::AqueductQuad quads[(srrace::AQU_VIEW_ROWS - 1) * 3];
    const int n = srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                                             level, quads, 64);
    REQUIRE(n > 6);

    const srrace::AqueductQuad& floor = quads[0];
    const srrace::AqueductQuad& left = quads[1];
    const srrace::AqueductQuad& right = quads[2];

    // The floor spans the channel, and the walls sit outside it on each side.
    CHECK(floor.x[0] < floor.x[1]);
    CHECK(left.x[0] <= floor.x[0]);     // outer left is left of the channel
    CHECK(right.x[1] >= floor.x[1]);    // outer right is right of it

    // The walls rise: the outer edge is higher up the screen (smaller y) than
    // the channel edge it springs from.
    CHECK(left.y[0] < left.y[1]);
    CHECK(right.y[1] < right.y[0]);

    // The far edge of a band is the near edge of the next, so the concrete has
    // no seams in it.
    CHECK(quads[0].x[3] == quads[3].x[0]);
    CHECK(quads[0].y[3] == quads[3].y[0]);
    CHECK(quads[0].x[2] == quads[3].x[1]);
}

TEST_CASE("banking tilts the whole channel about the near row") {
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 0, 100, 0, 0, 0);

    srrace::AqueductQuad level[(srrace::AQU_VIEW_ROWS - 1) * 3];
    srrace::AqueductQuad leftTilt[(srrace::AQU_VIEW_ROWS - 1) * 3];
    srrace::AqueductQuad rightTilt[(srrace::AQU_VIEW_ROWS - 1) * 3];
    srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                               srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 0), level, 64);
    srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                               srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 48), leftTilt, 64);
    srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                               srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, -48), rightTilt, 64);

    // Level, the nearest floor edge is horizontal.
    CHECK(level[0].y[0] == level[0].y[1]);

    // Banked, it slopes - and the two directions slope opposite ways, which is
    // the whole point of the sign on the bank angle.
    const int leftSlope = leftTilt[0].y[1] - leftTilt[0].y[0];
    const int rightSlope = rightTilt[0].y[1] - rightTilt[0].y[0];
    CHECK(leftSlope != 0);
    CHECK(rightSlope != 0);
    CHECK(((leftSlope > 0) != (rightSlope > 0)));

    // The pivot itself is the one point that does not move, whatever the bank.
    const srrace::AqueductBank b = srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 48);
    int px = 0, py = 0;
    srrace::AqueductBankPoint(b, b.pivotX, b.pivotY, px, py);
    CHECK(px == b.pivotX);
    CHECK(py == b.pivotY);

    // And the bands stay stitched together after banking, so tilting cannot
    // tear the concrete apart.
    CHECK(leftTilt[0].x[3] == leftTilt[3].x[0]);
    CHECK(leftTilt[0].y[3] == leftTilt[3].y[0]);
}

TEST_CASE("the far pass draws the same way with fewer bands") {
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 0, 100, 0, 0, 0);
    const srrace::AqueductBank level = srrace::AqueductBankForRows(frame.mirrorRows, srrace::AQU_MIRROR_ROWS, 0);

    srrace::AqueductQuad quads[(srrace::AQU_MIRROR_ROWS - 1) * 3];
    const int n = srrace::AqueductBuildQuads(frame.mirrorRows, srrace::AQU_MIRROR_ROWS,
                                             level, quads, 64);
    CHECK(n == (srrace::AQU_MIRROR_ROWS - 1) * srrace::AqueductSurfaceCount);
    CHECK(n / srrace::AqueductSurfaceCount == srrace::AQU_MIRROR_OBJECT_ROWS);
}

TEST_CASE("a short row table produces nothing rather than reading past it") {
    srrace::AqueductRow one[1];
    srrace::AqueductQuad quads[8];
    const srrace::AqueductBank level = srrace::AqueductBankFor(0, 0, 0);
    CHECK(srrace::AqueductBuildQuads(one, 1, level, quads, 8) == 0);
    CHECK(srrace::AqueductBuildQuads(nullptr, 15, level, quads, 8) == 0);
    // And it never writes past what it was given room for.
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 0, 100, 0, 0, 0);
    CHECK(srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                                     level, quads, 5) == 5);
}

// ---------------------------------------------------------------------------
// Scan conversion. After this the renderer only has to plot the spans.
// ---------------------------------------------------------------------------

namespace {

srrace::AqueductQuad MakeQuad(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
    srrace::AqueductQuad q;
    q.x[0] = x0; q.y[0] = y0;
    q.x[1] = x1; q.y[1] = y1;
    q.x[2] = x2; q.y[2] = y2;
    q.x[3] = x3; q.y[3] = y3;
    return q;
}

} // namespace

TEST_CASE("an upright rectangle scans to full-width rows") {
    const srrace::AqueductQuad r = MakeQuad(10, 20, 40, 20, 40, 25, 10, 25);
    srrace::AqueductSpan spans[64];
    const int n = srrace::AqueductScanQuad(r, 0, 199, 320, spans, 64);

    CHECK(n == 6);                       // rows 20..25 inclusive
    for (int i = 0; i < n; ++i) {
        CHECK(spans[i].y == 20 + i);
        CHECK(spans[i].x0 == 10);
        CHECK(spans[i].x1 == 40);
    }
}

TEST_CASE("a trapezoid narrows the way the channel does") {
    // Wide at the bottom, narrow at the top - one band of concrete.
    const srrace::AqueductQuad t = MakeQuad(100, 100, 220, 100, 180, 90, 140, 90);
    srrace::AqueductSpan spans[64];
    const int n = srrace::AqueductScanQuad(t, 0, 199, 320, spans, 64);
    REQUIRE(n == 11);

    // Spans are emitted top down and get wider on the way.
    CHECK(spans[0].y == 90);
    CHECK(spans[n - 1].y == 100);
    CHECK(spans[0].x1 - spans[0].x0 < spans[n - 1].x1 - spans[n - 1].x0);
}

TEST_CASE("a banked quad still scans cleanly") {
    // No horizontal edges at all, which is what banking produces.
    const srrace::AqueductQuad q = MakeQuad(100, 100, 200, 110, 190, 80, 90, 70);
    srrace::AqueductSpan spans[128];
    const int n = srrace::AqueductScanQuad(q, 0, 199, 320, spans, 128);
    REQUIRE(n > 0);
    for (int i = 0; i < n; ++i) {
        CHECK(spans[i].x0 <= spans[i].x1);
        if (i > 0) CHECK(spans[i].y == spans[i - 1].y + 1);
    }
    CHECK(spans[0].y == 70);
    CHECK(spans[n - 1].y == 110);
}

TEST_CASE("everything is clipped to the screen") {
    // Hanging off every edge at once.
    const srrace::AqueductQuad huge = MakeQuad(-500, -500, 900, -500, 900, 900, -500, 900);
    srrace::AqueductSpan spans[512];
    const int n = srrace::AqueductScanQuad(huge, 10, 190, 320, spans, 512);
    REQUIRE(n > 0);
    for (int i = 0; i < n; ++i) {
        CHECK(spans[i].y >= 10);
        CHECK(spans[i].y <= 190);
        CHECK(spans[i].x0 >= 0);
        CHECK(spans[i].x1 <= 319);
    }

    // Wholly off to one side: nothing to plot, rather than a wrapped span.
    const srrace::AqueductQuad away = MakeQuad(-200, 50, -150, 50, -150, 60, -200, 60);
    CHECK(srrace::AqueductScanQuad(away, 0, 199, 320, spans, 512) == 0);

    // Degenerate input is not a crash.
    const srrace::AqueductQuad flat = MakeQuad(10, 10, 20, 10, 20, 10, 10, 10);
    CHECK(srrace::AqueductScanQuad(flat, 0, 199, 320, spans, 512) <= 1);
    CHECK(srrace::AqueductScanQuad(flat, 0, 199, 320, spans, 0) == 0);
}

TEST_CASE("a real channel scans to something plottable") {
    // The row offsets are around 2000, so the camera has to sit at about that height for the channel to land on
    // screen at all - at camY 0 every row projects hundreds of pixels below it.
    const int kCameraHeight = 1870;

    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, kCameraHeight, 100, 0, 0, 0);
    // Near to horizon: the first row is low on screen, the last near the top.
    CHECK(frame.viewRows[0].y > frame.viewRows[srrace::AQU_VIEW_ROWS - 1].y);
    CHECK(frame.viewRows[0].y < 200);

    srrace::AqueductQuad quads[(srrace::AQU_VIEW_ROWS - 1) * 3];
    const int q = srrace::AqueductBuildQuads(
        frame.viewRows, srrace::AQU_VIEW_ROWS,
        srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 0), quads, 64);
    REQUIRE(q == (srrace::AQU_VIEW_ROWS - 1) * srrace::AqueductSurfaceCount);

    int plotted = 0;
    int quadsOnScreen = 0;
    srrace::AqueductSpan spans[400];
    for (int i = 0; i < q; ++i) {
        const int n = srrace::AqueductScanQuad(quads[i], 0, 199, 320, spans, 400);
        if (n > 0) ++quadsOnScreen;
        for (int k = 0; k < n; ++k) plotted += spans[k].x1 - spans[k].x0 + 1;
    }
    // Most bands are visible and they cover a good part of a 320x200 screen.
    CHECK(quadsOnScreen > q / 2);
    CHECK(plotted > 20000);

    // Banked, the channel is still drawn - it does not fall off the screen.
    int bankedPixels = 0;
    srrace::AqueductBuildQuads(frame.viewRows, srrace::AQU_VIEW_ROWS,
                               srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 20), quads, 64);
    for (int i = 0; i < q; ++i) {
        const int n = srrace::AqueductScanQuad(quads[i], 0, 199, 320, spans, 400);
        for (int k = 0; k < n; ++k) bankedPixels += spans[k].x1 - spans[k].x0 + 1;
    }
    CHECK(bankedPixels > 20000);
}

TEST_CASE("the bank pivots on the vanishing point") {
    // FUN_00244162 takes the pivot from the LAST entry of the x and y row tables, not the first.
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 1870, 100, 0, 0, 0);

    const srrace::AqueductRow& near = frame.viewRows[0];
    const srrace::AqueductRow& far = frame.viewRows[srrace::AQU_VIEW_ROWS - 1];
    const srrace::AqueductBank bank =
        srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 30);

    CHECK(bank.pivotX == ((far.leftX + far.rightX) >> 1));
    CHECK(bank.pivotY == far.y);
    // On a straight channel every row's midpoint projects to the same x - dead
    // ahead - so it is the height that tells the two ends apart.
    CHECK(bank.pivotY != near.y);

    // So the horizon holds still and the near end is what moves.
    int fx = 0, fy = 0, nx = 0, ny = 0;
    srrace::AqueductBankPoint(bank, (far.leftX + far.rightX) >> 1, far.y, fx, fy);
    srrace::AqueductBankPoint(bank, (near.leftX + near.rightX) >> 1, near.y, nx, ny);
    CHECK(fx == bank.pivotX);
    CHECK(fy == bank.pivotY);
    CHECK(std::abs(ny - near.y) > 1);
}

TEST_CASE("the projections match the running game") {
    // Read out of a live SR2 Aqueduct race, stationary at the start line.
    const int camX = 2560, camY = 1900, subPos = 0, slope = 0, vjerk = 0;
    srrace::AqueductChannel flat;

    srrace::AqueductRow far[srrace::AQU_MIRROR_ROWS];
    srrace::AqueductBuildMirrorRows(flat, camX, camY, subPos, slope, far);
    const int liveFarRightX[3] = {660, 375, 327};
    const int liveFarLeftX[3] = {-260, 145, 213};
    const int liveFarY[3] = {86, 48, 42};
    const int liveFarY2[3] = {-84, 6, 21};
    for (int i = 0; i < 3; ++i) {
        CHECK(far[i].rightX == liveFarRightX[i]);
        CHECK(far[i].leftX == liveFarLeftX[i]);
        CHECK(far[i].y == liveFarY[i]);
        CHECK(far[i].y2 == liveFarY2[i]);
    }

    srrace::AqueductRow near[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, camX, camY, subPos, slope, vjerk, near);
    const int liveNearRightX[12] = {268, 244, 229, 218, 210, 204, 200, 196, 193, 190, 188, 186};
    for (int i = 3; i < srrace::AQU_VIEW_ROWS; ++i) {
        CHECK(near[i].rightX == liveNearRightX[i - 3]);
    }

    // The y values are Amiga screen coordinates, in the same 18..99 band the
    // road viewport clips to - not surface coordinates.
    for (int i = 3; i < srrace::AQU_VIEW_ROWS; ++i) {
        CHECK(near[i].y >= 18);
        CHECK(near[i].y <= 99);
    }
}


TEST_CASE("aqueduct obstacle phase is the bank angle bucket") {
    // jdraw_a.c FUN_00241cc6's ladder. Even phases are positive bank, odd
    // negative, 0 is level.
    CHECK(srrace::AqueductShapePhase(0) == 0);
    CHECK(srrace::AqueductShapePhase(7) == 0);
    CHECK(srrace::AqueductShapePhase(-7) == 0);
    CHECK(srrace::AqueductShapePhase(8) == 2);
    CHECK(srrace::AqueductShapePhase(0x10) == 4);
    CHECK(srrace::AqueductShapePhase(0x1f) == 6);
    CHECK(srrace::AqueductShapePhase(-8) == 1);
    CHECK(srrace::AqueductShapePhase(-0x10) == 3);
    CHECK(srrace::AqueductShapePhase(-0x1f) == 5);
}

TEST_CASE("aqueduct shape table is the nine consecutive lib.p assets") {
    // The records store a one-based number; lib.p is indexed from zero.
    for (int k = 0; k < srrace::AQU_SHAPE_COUNT; ++k) {
        CHECK(srrace::AqueductShapeAsset(k) == srrace::AQU_SHAPE_ASSET_FIRST + k - 1);
    }
    CHECK(srrace::AqueductShapeAsset(0) == 274);
    CHECK(srrace::AqueductShapeAsset(8) == 282);
    // The ladder shrinks monotonically, as a distance series.
    int prev = 1000;
    for (int k = 0; k < srrace::AQU_SHAPE_LADDER_COUNT; ++k) {
        const int shape = srrace::AQU_SHAPE_LADDER_FIRST + k;
        const int w = srrace::AqueductShapeRectFor(shape, 0).w;
        CHECK(w < prev);
        prev = w;
    }
    CHECK(srrace::AqueductShapeRectFor(srrace::AQU_SHAPE_LADDER_FIRST, 0).w == 40);
    CHECK(srrace::AqueductShapeRectFor(8, 0).w == 6);
}

TEST_CASE("aqueduct obstacles sit off a wall on the flagged segments") {
    srrace::AqueductTrack track;
    srrace::BuildAqueductTrack(track);

    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);
    const srrace::AqueductBank bank =
        srrace::AqueductBankForRows(rows, srrace::AQU_VIEW_ROWS, 0);

    srrace::AqueductObstacle out[srrace::AQU_VIEW_OBJECT_ROWS * 2];

    // Segment 5 carries the left bit. With flagBase 0 it lands on row 5, which
    // is row loop i = 9, so z = 4 and the ladder is two rungs up from nearest.
    int n = srrace::AqueductBuildObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                           track.flags, 0, bank, 0, out, 28);
    REQUIRE(n == 1);
    CHECK(out[0].shape == srrace::AQU_SHAPE_LADDER_FIRST + 2);
    CHECK(srrace::AqueductShapeAsset(out[0].shape) == 278);
    CHECK(out[0].phase == 0);
    // A quarter of the channel in from the left edge.
    const int w5 = rows[5].rightX - rows[5].leftX;
    int ex = 0, ey = 0;
    srrace::AqueductBankPoint(bank, rows[5].leftX + (w5 >> 2), rows[5].y, ex, ey);
    CHECK(out[0].x == ex);
    CHECK(out[0].y == ey);

    // Segment 23 carries the right bit; flagBase 10 puts it on row 13, the
    // farthest row the objects reach, so it takes the last rung.
    n = srrace::AqueductBuildObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                       track.flags, 10, bank, 0, out, 28);
    REQUIRE(n == 1);
    CHECK(out[0].shape == srrace::AQU_SHAPE_LADDER_FIRST + 6);
    CHECK(srrace::AqueductShapeAsset(out[0].shape) == 282);
    // An eighth in from the right edge - the channel's walls are not
    // symmetric and neither is this.
    const int w13 = rows[13].rightX - rows[13].leftX;
    srrace::AqueductBankPoint(bank, rows[13].rightX - (w13 >> 3), rows[13].y, ex, ey);
    CHECK(out[0].x == ex);
    CHECK(out[0].y == ey);

    // Nothing is drawn on a clear stretch.
    n = srrace::AqueductBuildObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                       track.flags, 280, bank, 0, out, 28);
    CHECK(n == 0);
}

TEST_CASE("aqueduct obstacle ladder never runs off its seven rungs") {
    srrace::AqueductTrack track;
    srrace::BuildAqueductTrack(track);
    // Every segment carries both bits, so every row that can draw, does.
    for (int s = 0; s < srrace::AQUEDUCT_SEGMENTS; ++s) {
        track.flags[s] |= srrace::AQU_OBSTACLE_DRAW_LEFT | srrace::AQU_OBSTACLE_DRAW_RIGHT;
    }

    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);
    const srrace::AqueductBank bank =
        srrace::AqueductBankForRows(rows, srrace::AQU_VIEW_ROWS, 0);

    srrace::AqueductObstacle out[srrace::AQU_VIEW_OBJECT_ROWS * 2];
    const int n = srrace::AqueductBuildObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                                 track.flags, 0, bank, 0, out, 28);
    // Row 0 is skipped on both sides, so 13 rows x 2 - the original would read
    // past its table if it drew row 0.
    CHECK(n == 26);
    for (int i = 0; i < n; ++i) {
        CHECK(out[i].shape >= srrace::AQU_SHAPE_LADDER_FIRST);
        CHECK(out[i].shape < srrace::AQU_SHAPE_LADDER_FIRST + srrace::AQU_SHAPE_LADDER_COUNT);
    }
}


TEST_CASE("aqueduct road is straight and level") {
    srrace::TrackData road;
    // Fill with non-zero values so the test sees the zeroing.
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        road.curve[i] = 40;
        road.elev[i] = -17;
        road.flags[i] = 0x00e0;
        road.wallLeft[i] = 5;
        road.wallRight[i] = -5;
        road.lane[i] = 9;
    }
    srrace::BuildAqueductRoad(road);
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        CHECK(road.curve[i] == 0);
        CHECK(road.elev[i] == 0);
        CHECK(road.flags[i] == 0);
        CHECK(road.wallLeft[i] == 0);
        CHECK(road.wallRight[i] == 0);
        CHECK(road.lane[i] == 0);
    }
    // Width bits clear is the widest road, which is the channel's own span.
    int16_t l = 0, r = 0;
    srrace::RoadEdges(road.flags[0], l, r);
    CHECK(l == 1500);
    CHECK(r == 3300);
}


TEST_CASE("the sky is 24 banked star pixels above the horizon") {
    srrace::AqueductBank level;
    srrace::AqueductStar stars[srrace::AQU_STARS_TOTAL];
    const int n = srrace::AqueductBuildSky(0, level, 0, stars,
                                           srrace::AQU_STARS_TOTAL);
    CHECK(n == srrace::AQU_STARS_TOTAL);
    CHECK(n == 24);

    int bright = 0, dim = 0;
    for (int i = 0; i < n; ++i) {
        // All of them sit above the horizon the projections centre on.
        CHECK(stars[i].y >= 0);
        CHECK(stars[i].y <= srrace::AQU_SKY_BOTTOM);
        CHECK(stars[i].x >= 0);
        CHECK(stars[i].x < srrace::AQU_SCREEN_WIDTH);
        if (stars[i].color == srrace::AQU_STAR_COLOR_BRIGHT) ++bright;
        if (stars[i].color == srrace::AQU_STAR_COLOR_DIM) ++dim;
    }
    CHECK(bright == 18);
    CHECK(dim == 6);

    // Level, the table is used raw; banked, the sky turns with the channel.
    CHECK(stars[0].x == 16);
    CHECK(stars[0].y == 15);

    srrace::AqueductStar tilted[srrace::AQU_STARS_TOTAL];
    const srrace::AqueductBank bank = srrace::AqueductBankFor(160, 60, 20);
    srrace::AqueductBuildSky(0, bank, 20, tilted, srrace::AQU_STARS_TOTAL);
    CHECK((tilted[0].x != stars[0].x || tilted[0].y != stars[0].y));

    // The base shifts the whole sky sideways.
    srrace::AqueductStar shifted[srrace::AQU_STARS_TOTAL];
    srrace::AqueductBuildSky(20, level, 0, shifted, srrace::AQU_STARS_TOTAL);
    CHECK(shifted[0].x == stars[0].x + 20);
}


TEST_CASE("the roll accumulates into the bank and spins the channel") {
    // Outside a rollover the roll is zero and the bank is just the climb.
    CHECK(srrace::AqueductBankAngle(17, 0) == 17);
    CHECK(srrace::AqueductBankAngle(0, 0) == 0);
    // FUN_00244002 wraps to +/-360, working in whole degrees.
    CHECK(srrace::AqueductBankAngle(300, 100) == 40);
    CHECK(srrace::AqueductBankAngle(-300, -100) == -40);
    CHECK(srrace::AqueductBankAngle(100, 200) == 300);

    // A rolling car turns the whole channel over, which is what makes a flip
    // visible from inside the car: the same rows land somewhere else entirely.
    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);

    const srrace::AqueductBank level =
        srrace::AqueductBankForRows(rows, srrace::AQU_VIEW_ROWS,
                                    srrace::AqueductBankAngle(0, 0));
    const srrace::AqueductBank rolled =
        srrace::AqueductBankForRows(rows, srrace::AQU_VIEW_ROWS,
                                    srrace::AqueductBankAngle(0, 0x20));
    CHECK(level.sin != rolled.sin);

    int lx = 0, ly = 0, rx = 0, ry = 0;
    srrace::AqueductBankPoint(level, rows[1].leftX, rows[1].y, lx, ly);
    srrace::AqueductBankPoint(rolled, rows[1].leftX, rows[1].y, rx, ry);
    CHECK((lx != rx || ly != ry));
}


TEST_CASE("the two obstacle kinds leave opposite ways past") {
    srrace::AqueductObstacleBox boxes[2];

    // 0x10 blocks the middle: the way past is around the outside.
    int n = srrace::AqueductObstacleBoxes(srrace::AQU_OBSTACLE_SPAN, boxes, 2);
    REQUIRE(n == 1);
    CHECK(boxes[0].left > srrace::AQU_FLOOR_LEFT);
    CHECK(boxes[0].right < srrace::AQU_FLOOR_RIGHT);
    CHECK(srrace::AqueductObstacleHits(srrace::AQU_OBSTACLE_SPAN, 2400));
    // Hard against either wall puts both probes outside the block.
    CHECK_FALSE(srrace::AqueductObstacleHits(srrace::AQU_OBSTACLE_SPAN, 1400));
    CHECK_FALSE(srrace::AqueductObstacleHits(srrace::AQU_OBSTACLE_SPAN, 3300));

    // 0x80 blocks both sides: the way past is up the middle.
    n = srrace::AqueductObstacleBoxes(srrace::AQU_OBSTACLE_SIDES, boxes, 2);
    REQUIRE(n == 2);
    CHECK(boxes[0].right < boxes[1].left);       // a gap between them
    CHECK(boxes[0].left > srrace::AQU_FLOOR_LEFT);
    CHECK(boxes[1].right < srrace::AQU_FLOOR_RIGHT);

    // The two are never both set by the track builder, and the original only
    // reaches the 0x80 branch when 0x10 is clear.
    srrace::AqueductTrack t;
    srrace::BuildAqueductTrack(t);
    for (int seg = 0; seg < srrace::AQUEDUCT_SEGMENTS; ++seg) {
        const bool span = (t.flags[seg] & srrace::AQU_OBSTACLE_SPAN) != 0;
        const bool sides = (t.flags[seg] & srrace::AQU_OBSTACLE_SIDES) != 0;
        const bool both = span && sides;
        CHECK_FALSE(both);
    }

    // A clear segment stops nobody.
    CHECK_FALSE(srrace::AqueductObstacleHits(0, 2400));
}


TEST_CASE("obstacle blocks are cuboids on the flagged segments") {
    srrace::AqueductTrack t;
    srrace::BuildAqueductTrack(t);

    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);
    srrace::AqueductBlock blocks[srrace::AQU_VIEW_OBJECT_ROWS * 2];

    // Segment 4 carries 0x10: one low wide block across the middle.
    int n = srrace::AqueductBuildBlocks(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                        t.flags, 0, blocks, 28);
    REQUIRE(n == 1);
    CHECK(blocks[0].nearLeft > rows[3].leftX);
    CHECK(blocks[0].nearRight < rows[3].rightX);
    // Low: a seventh of the wall height, and the top is above the floor.
    CHECK(blocks[0].nearTop < blocks[0].nearBottom);

    // Segment 124 carries 0x80: two full-height pillars, one per wall.
    n = srrace::AqueductBuildBlocks(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                    t.flags, 120, blocks, 28);
    REQUIRE(n == 2);
    CHECK(blocks[0].nearRight < blocks[1].nearLeft);   // a gap up the middle
    // Full wall height, so taller than the 0x10 block at a comparable row.
    const int pillar = blocks[0].nearBottom - blocks[0].nearTop;
    CHECK(pillar > 0);

    // A clear stretch puts nothing up.
    n = srrace::AqueductBuildBlocks(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                    t.flags, 180, blocks, 28);
    CHECK(n == 0);
}


TEST_CASE("the backdrop pans with the heading and wraps at the screen edge") {
    CHECK(srrace::AqueductSkyBase(0) == 0);
    CHECK(srrace::AqueductSkyBase(10) == 20);          // twice the heading
    CHECK(srrace::AqueductSkyBase(160) == 0);          // 320 wraps to 0
    CHECK(srrace::AqueductSkyBase(170) == 20);
    for (int hd = -400; hd <= 400; ++hd) {
        const int b = srrace::AqueductSkyBase(hd);
        CHECK(b > -srrace::AQU_SCREEN_WIDTH);
        CHECK(b < srrace::AQU_SCREEN_WIDTH);
    }

    // Steering slides the stars along with it.
    srrace::AqueductBank level;
    srrace::AqueductStar a[srrace::AQU_STARS_TOTAL];
    srrace::AqueductStar b[srrace::AQU_STARS_TOTAL];
    srrace::AqueductBuildSky(srrace::AqueductSkyBase(0) / 2, level, 0, a,
                             srrace::AQU_STARS_TOTAL);
    srrace::AqueductBuildSky(srrace::AqueductSkyBase(10) / 2, level, 0, b,
                             srrace::AQU_STARS_TOTAL);
    CHECK(b[0].x == a[0].x + 10);
}

TEST_CASE("the horizon carries three fixed sprites off the same base") {
    srrace::AqueductBank level;
    srrace::AqueductObstacle h[srrace::AQU_HORIZON_SPRITES];
    const int n = srrace::AqueductBuildHorizon(0, level, 0, h,
                                               srrace::AQU_HORIZON_SPRITES);
    REQUIRE(n == 3);
    // Records 0 and 1 - the two that are NOT on the distance ladder.
    CHECK(h[0].shape == 0);
    CHECK(h[1].shape == 1);
    CHECK(h[2].shape == 1);
    for (int i = 0; i < n; ++i) {
        CHECK(h[i].shape < srrace::AQU_SHAPE_LADDER_FIRST);
        CHECK(h[i].y <= srrace::AQU_SKY_BOTTOM);
    }
    CHECK(h[0].x == 0x5c);
    CHECK(h[1].x == 0xd2);
    CHECK(h[2].x == 0xb4);

    // They pan with the base, at the full rate rather than the stars' half.
    srrace::AqueductObstacle moved[srrace::AQU_HORIZON_SPRITES];
    srrace::AqueductBuildHorizon(40, level, 0, moved, srrace::AQU_HORIZON_SPRITES);
    CHECK(moved[0].x == h[0].x + 40);
}


TEST_CASE("the channel is outlined and jointed") {
    srrace::AqueductTrack t;
    srrace::BuildAqueductTrack(t);
    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);
    srrace::AqueductBank level;
    srrace::AqueductLine lines[4 + srrace::AQU_VIEW_ROWS * 3];

    // A stretch with no joint segment: just the four long edges.
    int n = srrace::AqueductBuildLines(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                       t.flags, 2, level, 0, lines,
                                       static_cast<int>(std::size(lines)));
    CHECK(n >= 4);
    // Two black floor junctions, then two slope tops.
    CHECK(lines[0].pen == srrace::AQU_PEN_EDGE);
    CHECK(lines[1].pen == srrace::AQU_PEN_EDGE);
    CHECK(lines[2].pen == srrace::AQU_PEN_SLOPE_TOP);
    CHECK(lines[3].pen == srrace::AQU_PEN_SLOPE_TOP);

    // The long edges start at the far row and run PAST the near row, which is
    // what carries them off the bottom of the screen.
    CHECK(lines[0].y0 == rows[srrace::AQU_VIEW_OBJECT_ROWS].y);
    CHECK(lines[0].y1 > rows[0].y);

    // The joints land only on segments carrying bit 0, which the builder lays
    // every 6 from segment 1 - and each joint is three lines.
    int joints = 0;
    for (int r = 0; r <= srrace::AQU_VIEW_OBJECT_ROWS; ++r) {
        if ((t.flags[2 + r] & srrace::AQU_FLAG_JOINT) != 0) ++joints;
    }
    CHECK(n == 4 + joints * 3);
    for (int i = 4; i < n; ++i) {
        CHECK(lines[i].pen == srrace::AQU_PEN_EDGE);
    }

    // Every joint segment is one of the every-6 run.
    for (int seg = 0; seg < srrace::AQUEDUCT_SEGMENTS; ++seg) {
        if ((t.flags[seg] & srrace::AQU_FLAG_JOINT) != 0) {
            CHECK(seg % 6 == 1);
        }
    }
}


TEST_CASE("slope patches sit on the slope, on the flagged side") {
    srrace::AqueductTrack t;
    srrace::BuildAqueductTrack(t);
    srrace::AqueductChannel flat;
    srrace::AqueductRow rows[srrace::AQU_VIEW_ROWS];
    srrace::AqueductBuildViewRows(flat, 2400, 1900, 0, 0, 0, rows);
    srrace::AqueductBank level;
    srrace::AqueductQuad q[srrace::AQU_VIEW_OBJECT_ROWS];

    // The builder lays 0x0004 at s-1 and 0x0008 at s-2 for s = 18, 42, ...
    CHECK((t.flags[17] & srrace::AQU_SLOPE_OBSTACLE_LEFT) != 0);
    CHECK((t.flags[16] & srrace::AQU_SLOPE_OBSTACLE_RIGHT) != 0);
    // ...and 0x0100 / 0x0200 replace them at the two special places.
    CHECK((t.flags[137] & 0x0004) == 0);
    CHECK((t.flags[137] & 0x0100) != 0);
    CHECK((t.flags[137] & srrace::AQU_SLOPE_OBSTACLE_LEFT) != 0);

    // A left patch lies between the floor edge and the slope top, not beyond.
    int n = srrace::AqueductBuildSlopeObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                                t.flags, 10, level, 0, q,
                                                static_cast<int>(std::size(q)));
    REQUIRE(n >= 1);
    bool sawLeft = false;
    for (int i = 0; i < n; ++i) {
        if (q[i].surface != srrace::AqueductLeftWall) continue;
        sawLeft = true;
        for (int k = 0; k < 4; ++k) {
            CHECK(q[i].x[k] <= rows[srrace::AQU_VIEW_OBJECT_ROWS].leftX);
            CHECK(q[i].x[k] >= rows[0].outerLeftX);
        }
    }
    CHECK(sawLeft);

    // A clear stretch puts nothing on either slope.
    n = srrace::AqueductBuildSlopeObstacles(rows, srrace::AQU_VIEW_OBJECT_ROWS,
                                            t.flags, 190, level, 0, q,
                                            static_cast<int>(std::size(q)));
    CHECK(n == 0);
    CHECK_FALSE(srrace::AqueductSlopeObstacle(t.flags[200]));
}


TEST_CASE("a slope patch only bites where it is actually drawn") {
    const uint16_t left = 0x0004;    // inset: a quarter up to three quarters
    const uint16_t leftTall = 0x0100; // from the floor edge to three quarters

    // On the floor, clear of both slopes.
    CHECK_FALSE(srrace::AqueductSlopePatchHits(left, 2400));
    CHECK_FALSE(srrace::AqueductSlopePatchHits(leftTall, 2400));

    // How far up the slope a probe is: the left probe is camX - 80, and the
    // projection reads the clamped camera.
    auto leftProbeUp = [](int camX) {
        return srrace::AQU_FLOOR_LEFT -
               srrace::AqueductProbeX(camX, srrace::AQU_HIT_PROBE_LEFT);
    };
    CHECK(leftProbeUp(1400) == 160);   // well inside the inset patch
    CHECK(leftProbeUp(1500) == 60);    // below it, but inside the tall one

    CHECK(srrace::AqueductSlopePatchHits(left, 1400));
    CHECK(srrace::AqueductSlopePatchHits(leftTall, 1400));
    // Low on the slope: under the inset patch, into the tall one.
    CHECK_FALSE(srrace::AqueductSlopePatchHits(left, 1500));
    CHECK(srrace::AqueductSlopePatchHits(leftTall, 1500));

    // A left patch is no obstacle on the right slope, and vice versa.
    CHECK_FALSE(srrace::AqueductSlopePatchHits(left, 3400));
    CHECK(srrace::AqueductSlopePatchHits(0x0008, 3400));

    // A segment with neither bit never bites.
    CHECK_FALSE(srrace::AqueductSlopePatchHits(0, 1400));
}


TEST_CASE("the hit probes come out of the projection, not a guessed box") {
    // FUN_00242024 probes two fixed screen points on the car's scanline.
    const int camX = 2400;
    CHECK(srrace::AqueductProbeX(camX, srrace::AQU_HIT_PROBE_LEFT) == camX - 80);
    CHECK(srrace::AqueductProbeX(camX, srrace::AQU_HIT_PROBE_RIGHT) == camX + 159);

    // And they really are the inverse of the projection at that depth.
    for (int x = 1500; x <= 3300; x += 150) {
        const int screen = srrace::AqueductProjectViewX(camX, srrace::AQU_NEAR_ROW_DEPTH,
                                                        x, 0);
        CHECK(srrace::AqueductProbeX(camX, screen) == x);
    }

    // Riding a wall must not slide the probes out with the view: the
    // projection reads the clamped camera, so these stop where it does.
    CHECK(srrace::AqueductProbeX(9000, srrace::AQU_HIT_PROBE_LEFT) ==
          srrace::AQU_CAM_CLAMP_MAX - 80);
    CHECK(srrace::AqueductProbeX(-9000, srrace::AQU_HIT_PROBE_LEFT) ==
          srrace::AQU_CAM_CLAMP_MIN - 80);
}


TEST_CASE("the opponent's tilt frame follows its climb and the bank") {
    const srrace::AqueductChannel flat;
    // On the floor, level view: the level frame.
    srrace::AqueductOpponentTilt t = srrace::AqueductOpponentTiltFor(2400, flat, 0);
    CHECK(t.tilt == 3);
    CHECK(t.angle == 0);

    // 100 up the left slope is the full 45, leaning left: the hardest negative frame.
    t = srrace::AqueductOpponentTiltFor(0x5fa - 100, flat, 0);
    CHECK(t.angle == -45);
    CHECK(t.tilt == 0);

    // Half way up the right slope: 22 degrees, which still buckets as the 15 degree frame, mirrored.
    t = srrace::AqueductOpponentTiltFor(0xcc6 + 50, flat, 0);
    CHECK(t.angle == 22);
    CHECK(t.tilt == 4);

    // A negative bank above -90 counts a quarter extra.
    t = srrace::AqueductOpponentTiltFor(2400, flat, -40);
    CHECK(t.angle == -50);
    CHECK(t.tilt == 0);

    // Past 55 there is no frame for it.
    CHECK(srrace::AqueductOpponentTiltFor(2400, flat, 60).tilt == -1);
}


TEST_CASE("an opponent up the slope stands at the slope's height") {
    const srrace::AqueductChannel flat;
    CHECK(srrace::AqueductOpponentGroundY(2400, flat) == 2000);
    // At the outer lip it stands where the slope top is projected from, 0x67c.
    CHECK(srrace::AqueductOpponentGroundY(0x5a0 - 400, flat) == 0x67c);
    CHECK(srrace::AqueductOpponentGroundY(srrace::AQU_FLOOR_RIGHT + 400, flat) == 0x67c);
}


TEST_CASE("a tilted frame's anchor stays inside its turned box") {
    int xo = 0, yo = 0;
    // Level, the anchor is the bottom centre (the sine table peaks at 255, so a pixel short).
    srrace::AqueductTiltAnchor(40, 20, 0, 20, xo, yo);
    CHECK(xo == 19);
    CHECK(yo == 1);
    for (int a : srrace::AQU_OPP_TILT_ANGLE) {
        srrace::AqueductTiltAnchor(40, 20, a, 20, xo, yo);
        CHECK(xo >= 0);
        CHECK(xo <= 45);
        CHECK(yo >= 0);
        CHECK(yo <= 45);
    }
}


TEST_CASE("a block shows only the faces that face the camera, near end last") {
    const srrace::AqueductBank level = srrace::AqueductBankFor(0, 0, 0);
    // Dead ahead: the near end is wider than the far, so neither side shows, the top does.
    srrace::AqueductBlock b;
    b.farLeft = 140; b.farRight = 180; b.farBottom = 70; b.farTop = 66;
    b.nearLeft = 120; b.nearRight = 200; b.nearBottom = 90; b.nearTop = 80;
    srrace::AqueductFace faces[4];
    int n = srrace::AqueductBlockFaces(b, level, faces);
    REQUIRE(n == 2);
    CHECK(faces[0].pen == srrace::AQU_PEN_BLOCK_TOP);
    CHECK(faces[1].pen == srrace::AQU_PEN_BLOCK_SIDE);
    // The near end is the last face, on the near corners.
    CHECK(faces[1].quad.y[0] == (90 * 255) >> 8);

    // Off to the right, its left side faces us.
    b.farLeft = 240; b.farRight = 260; b.nearLeft = 280; b.nearRight = 310;
    n = srrace::AqueductBlockFaces(b, level, faces);
    REQUIRE(n == 3);
    CHECK(faces[0].pen == srrace::AQU_PEN_BLOCK_SIDE);
}


TEST_CASE("a block hits the car when it reaches the bottom scanline right of x 80") {
    srrace::AqueductQuad q;
    const int xs[4] = {60, 200, 200, 60};
    const int ys[4] = {70, 70, 120, 120};
    for (int k = 0; k < 4; ++k) { q.x[k] = xs[k]; q.y[k] = ys[k]; }
    CHECK(srrace::AqueductQuadHitsCar(q, 4, 85, 320));
    // All of it left of 80 misses.
    for (int k = 0; k < 4; ++k) q.x[k] = xs[k] - 150;
    CHECK_FALSE(srrace::AqueductQuadHitsCar(q, 4, 85, 320));
    // Not down to the scanline yet.
    for (int k = 0; k < 4; ++k) { q.x[k] = xs[k]; q.y[k] = ys[k] - 40; }
    CHECK_FALSE(srrace::AqueductQuadHitsCar(q, 4, 85, 320));
}


TEST_CASE("the opponent swerves for a block, harder the closer it is") {
    srrace::AqueductTrack track{};
    const srrace::AqueductChannel flat;
    // No block: level with us and on our right, it just follows the banding's wish, a seventh at a time.
    CHECK(srrace::AqueductOpponentStep(2600, 70, 100, 2400, 100, flat, track.flags) == 10);

    // A block across the middle three segments ahead: on our right it takes the right slope lane.
    track.flags[103] = srrace::AQU_OBSTACLE_SPAN;
    const int laneRight = (srrace::AQU_FLOOR_RIGHT + srrace::AQU_OUTER_RIGHT) / 2;
    const int far = srrace::AqueductOpponentStep(2600, 0, 100, 2400, 100, flat, track.flags);
    CHECK(far == (laneRight - 2600) / 2);
    // On our left, the left slope lane.
    CHECK(srrace::AqueductOpponentStep(2200, 0, 100, 2400, 100, flat, track.flags) < 0);
    // One segment off, it moves the whole way at once.
    CHECK(srrace::AqueductOpponentStep(2600, 0, 102, 2400, 100, flat, track.flags) == laneRight - 2600);

    // Pillars, well ahead of us: up the middle.
    track.flags[103] = srrace::AQU_OBSTACLE_SIDES;
    const int middle = (srrace::AQU_FLOOR_LEFT + srrace::AQU_FLOOR_RIGHT) / 2 - 0x1e;
    CHECK(srrace::AqueductOpponentStep(2600, 0, 103, 2400, 90, flat, track.flags) == middle - 2600);
}


TEST_CASE("a turned frame fills exactly the box its anchor is measured in") {
    const int w = 20, h = 10;
    std::vector<int> px(static_cast<size_t>(w) * h, 7);
    int rw = 0, rh = 0;
    // Level is the frame itself, near enough (the sine table peaks at 255).
    std::vector<int> out = srrace::AqueductRotateSprite(px, w, h, 0, 0, rw, rh);
    CHECK(std::abs(rw - w) <= 1);
    CHECK(std::abs(rh - h) <= 1);
    for (int a : srrace::AQU_OPP_TILT_ANGLE) {
        out = srrace::AqueductRotateSprite(px, w, h, a, 0, rw, rh);
        int xo = 0, yo = 0;
        srrace::AqueductTiltAnchor(w, h, a, w / 2, xo, yo);
        // The anchor lands on a painted pixel of the turned frame, one row up from where yOff counts.
        CHECK(xo >= 0);
        CHECK(xo < rw);
        CHECK(yo >= 0);
        CHECK(yo <= rh);
        const int ay = std::clamp(rh - yo, 0, rh - 1);
        int painted = 0;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                const int x = std::clamp(xo + dx, 0, rw - 1), y = std::clamp(ay + dy, 0, rh - 1);
                painted += out[static_cast<size_t>(y) * rw + x] == 7;
            }
        }
        CHECK(painted > 0);
    }
}


TEST_CASE("the midline is one strip, cut where its first gap in view lies") {
    srrace::AqueductFrame frame;
    srrace::AqueductBuildFrame(frame, 2400, 1900, 100, 0, 0, 0);
    const srrace::AqueductBank level = srrace::AqueductBankForRows(frame.viewRows, srrace::AQU_VIEW_ROWS, 0);
    srrace::AqueductTrack track{};
    srrace::AqueductQuad q[2];
    // No gap in view: one strip end to end, centred.
    REQUIRE(srrace::AqueductBuildMidline(frame.viewRows, 14, track.flags, 100, level, q) == 1);
    CHECK(std::abs((q[0].x[1] + q[0].x[2]) / 2 - 160) <= 2);

    // A gap of four segments in the middle of the view: a far piece and a near piece.
    for (int s = 106; s < 110; ++s) track.flags[s] = srrace::AQU_FLAG_MIDLINE_GAP;
    REQUIRE(srrace::AqueductBuildMidline(frame.viewRows, 14, track.flags, 100, level, q) == 2);
    // The far piece stops above where the near one starts.
    CHECK(std::max(q[0].y[1], q[0].y[2]) < std::min(q[1].y[0], q[1].y[3]));

    // Only at the far end: ignored, still one strip.
    srrace::AqueductTrack farGap{};
    farGap.flags[114] = srrace::AQU_FLAG_MIDLINE_GAP;
    CHECK(srrace::AqueductBuildMidline(frame.viewRows, 14, farGap.flags, 100, level, q) == 1);

    // The track lays the gaps: four segments every 24.
    srrace::AqueductTrack built;
    srrace::BuildAqueductTrack(built);
    CHECK((built.flags[3] & srrace::AQU_FLAG_MIDLINE_GAP) != 0);
    CHECK((built.flags[6] & srrace::AQU_FLAG_MIDLINE_GAP) != 0);
    CHECK((built.flags[7] & srrace::AQU_FLAG_MIDLINE_GAP) == 0);
}


TEST_CASE("the mirror has its own stars, panning a quarter of the view's way back") {
    srrace::AqueductStar s[12];
    const srrace::AqueductBank level = srrace::AqueductBankFor(0, 0, 0);
    REQUIRE(srrace::AqueductBuildMirrorSky(0, level, 0, s, 12) == 12);
    CHECK(s[0].x == 246);
    CHECK(s[0].y == 27);
    CHECK(s[0].color == srrace::AQU_STAR_COLOR_BRIGHT);
    CHECK(s[9].color == srrace::AQU_STAR_COLOR_DIM);
    srrace::AqueductBuildMirrorSky(40, level, 0, s, 12);
    CHECK(s[0].x == 236);
}
