Open work lives in Forgejo: http://registry.example:3000/RodLegacy/RodLegacy/issues

This file is now the done-list plus the notes that did not fit in an issue.
Migrated 2026-08-28: #11 license, #23 intro, #24 game over, #25 diner, #26 gas
station, #27 scene transitions and palette fade, #28 selling cars, #29 selling
parts, #30 engine part swap, #31 engine assembly, #32 tune animation, #33 save
game, #34 calendar highlight, #35 custom paint, #36 non-free assets, #37
main.cpp bugs, #38 input handling, #39 code hygiene, #40 nice-to-haves.

# Legal

- [x] src/racing.cpp:58 kSpriteScale pixel-scale data lifted from the original Amiga binary, hardcoded in source. Read it from the
  loaded Amiga assets at runtime instead of shipping it as a source constant (same pattern already used for graphics/audio: user supplies the disk images).
- [x] src/bet_acceptance.cpp:6 kThreshold table - read from binary. Found live via Copperline (breakpoint at
  FUN_0023d718 showed `LEA (-$2B64,A4),A0` indexing a 4x3 word table); the table itself lives in the SR_data
  asset (disk2), not the Street_Rod exe -- confirmed at file offset 0x74C. Now read at runtime via
  LoadBetThresholdTable(assets.GetSrData()), called from State::Init.
- [x] data/drivers/guybrush.png, Homer.png, mario.png, zak.png — third-party copyrighted characters (LucasArts/Disney, Fox/Disney, Nintendo), unrelated to Street Rod. Remove or replace before any public release.

## License background (for #11)

ADFlib is vendored in subprojects/ADFlib and is GPL-2.0-or-later (COPYING-GPL2, confirmed in the header). Since RodLegacy links it to read Amiga disk images,
that forces the whole binary to be GPL-2.0-or-later-compatible on distribution — this isn't really a choice, it's inherited.

So: GPL-2.0-or-later, or GPLv3 if you want the newer terms (both satisfy ADFlib's "or later" clause). This also matches the norm for this exact genre —
OpenTTD is GPLv2, OpenRCT2 and OpenMW are GPLv3, ScummVM is LGPL/GPL mixed. Add a LICENSE file at the RodLegacy root and an SPDX header convention if you
want one; not required, just common practice.

# Done

## Garage

- [x] Engine dialog
  - [x] Tuning engine
  - [x] Transmission swapping
  - [x] Animating screws and parts
- [x] Menu dialog: new game, old game, quitting
- [x] Figure out game time progression
- [x] Figure out how time relates to final score
- [x] Car painting: mask spray can with car color, paint with stock colors
- [x] Newspaper: buying cars with dialogs, buying parts, headlines from the
      current date, close by clicking the bottom-right area
- [x] Changing tires
- [x] Radio mute/unmute

## Racing

See RACING_TODO.md for the full research log.

## Chores

- [x] Critical: crash on exit
- [x] Critical: create SDL_Window/SDL_Renderer after SDL_Init; robust error handling
- [x] Critical: fix delta-time in main loop; update state.SetElapsedTime(delta)
- [x] Critical: remove C-style cast for DrawMaskAfterCar; now virtual via DrawExtra()
- [x] High: update mouse pointer on scene change instead of every frame
- [x] High: Scene memory management: unique_ptr for HUD, dialogs, bubbles
- [x] High: replace SDL_AddTimer fade with per-frame fade on main thread
- [x] High: remove strdup in Bitmap::LoadBitmap
- [x] High: RAII wrappers for SDL_Surface*/SDL_Texture*
- [x] Medium: AssetManager palette variant caching by {asset.id, palette}
- [x] Medium: move DrawMaskAfterCar into scene rendering via DrawExtra()

## Highscore

- [x] King of County screen, hall_dat load/save, game clock (was PLAN_highscore.md, #22)
