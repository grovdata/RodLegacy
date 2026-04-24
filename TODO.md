# To do in dissassembly / reversing

## Investigate how cheating works
There are money cheats in the game.

### MS-DOS

[Link](https://www.streetrodonline.com/2013/11/22/how-to-get-more-money-in-street-rod/)

- Buy the 1949 Chevrolet 2dr Styleline for $475
- Remove the transmission
- Completely remove the engine
- Sell the car for $30000

### Amiga

[Link](https://eab.abime.net/showthread.php?t=16819&page=6&styleid=4)

- Buy any car or buy the cheapest one, and try to sell it for the same price you bought it for
- They will offer a lower price. Refuse it, then try it again. They will offer a higher price. Refuse it, then sell the car for $22,001.

# Current progress

- [x] Garage
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
    - [x] Newspaper
    - [x] Changing tires
    - [x] Radio

- [x] Diner

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
