# To do in dissassembly / reversing

- [ ] Understand delays/timings between scene transitions, etc. so it looks better

# Current progress

- [x] Garage
  - [ ] Selling cars
    - [ ] Add asking price dialog and the price "haggling" flow
    - [ ] Add original game car selling cheat (see RodLegacyResearc/docs/gameplay.md)
  - [ ] Selling parts
  - [x] Engine dialog
    - [x] Tuning engine
    - [ ] Swapping parts
    - [ ] Dismantling and assembling the engine
    - [ ] Transmission swapping
    - [ ] Animating screws and parts
    - [ ] Tune animation without glitches
    - [x] Menu dialog
        - [ ] New game
        - [ ] Old game
        - [ ] Save game
        - [x] Quitting
    - [ ] Calendar
        - [ ] Highlight the date in calendar
        - [x] Figure out game time progression
        - [x] Figure out how time relates to final score
    - [ ] Car painting
        - [x] Mask spray can with car color
        - [x] Implement painting with stock colors
        - [ ] Custom color?
    - [ ] Newspaper
        - [x] Buying cars with dialogs
        - [ ] Buying parts
        - [ ] Updating headlines from the current date
        - [ ] Close newspaper by clicking in the bottom-right area
        - [ ] Improvement: pull more historic headlines to use randomly?
    - [x] Changing tires
    - [x] Radio mute/unmute

- [ ] Diner
    - [ ] Fix actor animations
    - [ ] Add jail state

- [ ] Gas station
    - [ ] Fix actor animation
    - [ ] Implement filling up gas
  

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
