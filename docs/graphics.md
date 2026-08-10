# Graphics port notes

Research on the original format: `RodLegacyResearch/docs/formats/lib_p.md`.

## Palette selection is approximated, and it is fragile

`lib.p` stores one palette in the executable, but the Amiga original also
reprograms the hardware colour registers per screen via copper lists, so the
same bitmap art can appear under different active colours depending on which
scene is showing it.

`BitmapParser::GetPalette()` in `src/parsers/libp.cpp` approximates that with a
set of hand-derived stock palettes (`PAL1`, `PAL_GAS`, `PAL_DRIVING`,
`PAL_DRIVING_LOW`, `PAL_DRIVING_HOOD` in `libp.h`) selected by hardcoded
asset-index ranges, rather than reading a per-screen palette from the binary.

The weakness is structural: index ranges group assets by numeric proximity, not
by which screen they belong to, so unrelated assets end up sharing a bucket.

**Confirmed case.** The generic `index < 45` bucket was intended for
driving-backdrop assets - `RACING_CLOUDS`, `RACING_MOUNTAINS`, sign posts,
`RACING_DRIVE_IN`, `RACING_CEILING`. It also contains `NEWSPAPER_MENU`
through `NEWSPAPER_NO_SOLDIERS_IN_VIETNAM` (indices 5-10), a menu screen with
nothing to do with driving. Commit `2c95ae4` ("Updates", 2026-07-08) changed
that bucket from `PAL1` to `PAL_DRIVING_LOW`, almost certainly to fix the
driving backdrops, and dragged the newspaper assets onto the olive-toned
driving palette, corrupting the newspaper scene.

Fixed by special-casing the newspaper index range back to `PAL1` ahead of the
`< 45` bucket. See `GetPalette()` and the regression test in
`tests/test_bitmapparser.cpp` ("newspaper assets (5-10) return PAL1, not
driving-low").

**Generalise from this.** Any asset that looks wrong on one screen but right on
another is a candidate for the same class of bug. Check whether its index falls
inside one of these ranges alongside assets from an unrelated scene.
