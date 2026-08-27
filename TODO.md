# To do in dissassembly / reversing

- [ ] Understand delays/timings between scene transitions, etc. so it looks better. Reverse+Implement Amiga palette fade.

# Legal TODO

  - [x] src/racing.cpp:58 kSpriteScale pixel-scale data lifted from the original Amiga binary, hardcoded in source. Read it from the
  loaded Amiga assets at runtime instead of shipping it as a source constant (same pattern already used for graphics/audio: user supplies the disk images).
  - [x] src/bet_acceptance.cpp:6 kThreshold table - read from binary. Found live via Copperline (breakpoint at
  FUN_0023d718 showed `LEA (-$2B64,A4),A0` indexing a 4x3 word table); the table itself lives in the SR_data
  asset (disk2), not the Street_Rod exe -- confirmed at file offset 0x74C. Now read at runtime via
  LoadBetThresholdTable(assets.GetSrData()), called from State::Init.
  - [x] data/drivers/guybrush.png, Homer.png, mario.png, zak.png — third-party copyrighted characters (LucasArts/Disney, Fox/Disney, Nintendo), unrelated to Street Rod. Remove or replace before any public release.
  - [ ] Remove before release: data/snd/sr1_ost.ogg, s1.aiff/wav, srl0/1.aiff, srs0/1.aiff, data/font/sr_font*.png.
  - [ ] No LICENSE file in RodLegacy yet — add one (see below).

  License

  ADFlib is vendored in subprojects/ADFlib and is GPL-2.0-or-later (COPYING-GPL2, confirmed in the header). Since RodLegacy links it to read Amiga disk images,
  that forces the whole binary to be GPL-2.0-or-later-compatible on distribution — this isn't really a choice, it's inherited.

  So: GPL-2.0-or-later, or GPLv3 if you want the newer terms (both satisfy ADFlib's "or later" clause). This also matches the norm for this exact genre —  OpenTTD is GPLv2, OpenRCT2 and OpenMW are GPLv3, ScummVM is LGPL/GPL mixed. Add a LICENSE file at the RodLegacy root and an SPDX header convention if you  want one; not required, just common practice.

# Current progress

- [ ] Intro sequence
- [ ] GameOver sequence
- [x] Garage
  - [ ] Selling cars
    - [ ] Add asking price dialog and the price "haggling" flow
    - [ ] Add original game car selling cheat (see RodLegacyResearc/docs/gameplay.md)
  - [ ] Selling parts
  - [x] Engine dialog
    - [x] Tuning engine
    - [ ] Swapping parts
    - [ ] Dismantling and assembling the engine
    - [x] Transmission swapping
    - [x] Animating screws and parts
    - [ ] Tune animation without glitches
    - [x] Menu dialog
        - [x] New game
        - [x] Old game
        - [ ] Save game
        - [x] Quitting
    - [ ] Calendar
        - [ ] Highlight the date in calendar
        - [x] Figure out game time progression
        - [x] Figure out how time relates to final score
    - [ ] Car painting
        - [x] Mask spray can with car color
        - [x] Implement painting with stock colors

    - [ ] Newspaper
        - [x] Buying cars with dialogs
        - [x] Buying parts
        - [x] Updating headlines from the current date
        - [x] Close newspaper by clicking in the bottom-right area
    - [x] Changing tires
    - [x] Radio mute/unmute

- [ ] Diner
    - [ ] Fix actor animations
    - [ ] Add jail state with GameOver flow when caught by police

- [ ] Gas station
    - [ ] Fix actor animation
    - [ ] Implement filling up gas

- [ ] Possible enhancements
    - [ ] Paint with custom color?
    - [ ] Newspaper: pull historic headlines to use randomly?

# Chores

## Known bugs (unfixed)

- Bug: MoveToScene(SCENE_GARAGE) called twice (lines 318 and 391 in main.cpp); one is likely leftover
- Bug: Missing return 0; at end of main (line 489)
- Bug: SDL_HINT_RENDER_SCALE_QUALITY set after renderer creation (line 260 vs 238) -- has no effect
- Bug: SDL_MOUSEBUTTONDOWN not handled in event loop
- Bug: Mix_OpenAudio failure path returns 0 (success) and skips SDL cleanup

## Checklist

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
- [ ] Medium: centralize input handling; scenes consume events first; debounce via timestamps

- [ ] Low: upgrade adflib
- [ ] Low: build hygiene: debug sanitizers, release profile (O2/O3, LTO)
- [ ] Low: replace remaining C-style casts with static_cast/dynamic_cast
- [ ] Low: add const correctness; prefer string_view where feasible
- [ ] Low: document scene responsibilities and transitions in README.md

- [ ] Nice-to-have: reduce globals; pass render/context object or store on State
- [ ] Nice-to-have: assert and bounds-check indices/palettes in asset loading
- [ ] Nice-to-have: test utility to dump assets to PNG and validate palette/alpha
- [ ] Nice-to-have: add FPS/frametime display in debug overlay
