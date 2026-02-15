# Current progress

- [x] Garage
  - [x] Engine dialog
    - [x] Tuning engine
    - [ ] Swapping parts
    - [ ] Dismantling and assembling the engine
    - [ ] Animating parts
    - [ ] Tune animation without glitches
    - [x] Menu dialog
        - [ ] New game
        - [ ] Old game
        - [ ] Save game
        - [x] Quitting
    - [ ] Calendar
        - [ ] Highlight the date in calendar
        - [ ] Figure out game time progression
        - [ ] Figure out how time relates to final score
    - [ ] Car painting
        - [ ] Mask spray can with car color
        - [ ] Implement painting with stock colors
        - [ ] Custom color?
    - [ ] Newspaper
    - [ ] Changing tires
    - [ ] Transmission dialog
    - [ ] Radio

# Chores

  Issues

  Bug: MoveToScene(SCENE_GARAGE) called 3 times

  Lines 316, 329, and 345 all call state.MoveToScene(SCENE_GARAGE). The first call is in the init block (line 316), the second is a redundant guard (line 328-329), and the third (line 345) appears to be a leftover. This likely causes the garage scene to be initialized 2-3 times on startup.

  Bug: Missing return in main

  Line 489 — main falls off the end without return 0;. While some compilers implicitly return 0 from main, the WinMain variant on line 163 shares the same code path and WinMain does not have that implicit guarantee. It's better to be explicit.

  Bug: Render hint set after renderer creation

  Line 247 sets SDL_HINT_RENDER_SCALE_QUALITY after the renderer is created on line 225. SDL hints need to be set before the renderer/texture they affect is created. Similarly, line 250 sets SDL_HINT_RENDER_DRIVER to "software" after the renderer already exists — this has no effect.

  Bug: SDL_MOUSEBUTTONDOWN not handled

  Line 425-428 handles SDL_MOUSEBUTTONUP and SDL_MOUSEMOTION but not SDL_MOUSEBUTTONDOWN. Mouse clicks may not register properly depending on how mouse.Update works.

  Resource leak: Mix_OpenAudio failure path

  Lines 257-263 — if Mix_OpenAudio fails, the function returns 0 (success) without cleaning up the window, renderer, or calling SDL_Quit(). Should return 1 and clean up, consistent with other error paths.

  Duplicated cleanup code

  The cleanup sequence (SDL_DestroyRenderer / SDL_DestroyWindow / SDL_JoystickClose / Mix_CloseAudio / SDL_Quit) is repeated 4 times (lines 289-295, 301-306, 319-324, 483-487). If controller is NULL (no joystick), SDL_JoystickClose(NULL) is also called in the error paths, which is documented as safe but sloppy.
  Consider an RAII wrapper or a goto cleanup pattern to centralize this.

  Global mutable state

  win, renderer, controller, mouse, isFullScreen, useOriginalSize, scaleX, scaleY, screenshotCounter, lastScreenshotTime are all global variables (lines 38-51). scaleX/scaleY are declared but never used anywhere.

  IsSteamDeck() is over-engineered

  Lines 53-104 — Methods 2-5 check environment variables (HARDWARE, PLATFORM, DEVICE_MODEL, HTTP_USER_AGENT) that are not set by SteamOS/Valve. The only reliable methods are checking SteamAppRunningOnDeck() via the Steam API, the STEAM_DECK env var (method 1), and the DMI product name (method 6). The extra checks add
  dead code.

  Delta-time cast truncation

  Line 472 — delta is Uint64 but (1000 / GAME_FPS) is integer division. If GAME_FPS doesn't evenly divide 1000, you get truncation. Also, SDL_Delay takes Uint32, so the subtraction result should be cast.

  Minor / Style

  - Line 40: Mixes NULL and nullptr (controller = NULL vs nullptr everywhere else).
  - Lines 258-259: Uses printf for the mixer error but SDL_Log elsewhere. Inconsistent logging.
  - Lines 350-355: SDL_Rect dest and SDL_QueryTexture results are set but dest is never used after that.
  - Lines 264-265: Dead commented-out music code.
  - Lines 441-446: Large block of commented-out debug code.
  - Line 48: SCREENSHOT_COOLDOWN_MS is const global — could be constexpr.

  Summary

  The most impactful bugs are the triple MoveToScene call, render hints set too late, and the Mix_OpenAudio error path. The duplicated cleanup is a maintenance hazard worth addressing.

- [x] Critical: crash on exit.
- [x] Critical: create `SDL_Window`/`SDL_Renderer` after `SDL_Init` in `src/main.cpp`; keep globals as `nullptr`; use `SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC`; robust error handling.
- [x] Critical: fix delta-time in main loop (`prevTicks`/`now`), update `state.SetElapsedTime(delta)`, and rely on vsync or `SDL_Delay(1000 / GAME_FPS)` consistently.
- [x] Critical: remove C-style cast in `main` when calling `DrawMaskAfterCar`; either use `dynamic_cast<CarScene*>` or add `virtual DrawAfter()` to `Scene` and implement in `CarScene`, then call from `main`.

- [x] High: update mouse pointer on scene change (in `State::MoveToScene` or `Scene::OnFadeComplete`) instead of every frame in `main`.
- [x] High: `Scene` memory management: delete `hud` in `Scene::~Scene` or convert `HUD* hud` to `std::unique_ptr<HUD>`. Consider smart pointers for dialogs/bubbles too.
- [x] High: replace `SDL_AddTimer`-based fade with per-frame fade driven by elapsed time on the main thread.
- [x] High: `Bitmap::LoadBitmap` leak: remove `strdup` and pass `filename.c_str()` directly.
- [x] High: wrap `SDL_Surface*`/`SDL_Texture*` with RAII (custom deleters) across `Bitmap`, `Scene`, and `main` to avoid leaks on error paths.

- [x] Medium: `AssetManager` palette variants: cache by `{asset.id, palette}` to prevent unbounded `imageAssets` growth; provide eviction or reuse.
- [ ] Medium: centralize input handling: let scenes consume events first; avoid duplicated key logic in `main`; use event timestamps for debouncing.
- [ ] Medium: move `DrawMaskAfterCar` ordering into scene rendering (e.g., `CarScene::Draw`/`DrawAfter`) to remove special-case logic from `main`.

- [ ] Low: Upgrade adflib

- [ ] Low: build hygiene: remove duplicate `-std=c++17` (Meson already sets `cpp_std=c++17`); add debug sanitizers and a proper `release` profile (O2/O3, LTO).
- [ ] Low: replace remaining C-style casts with `static_cast`/`dynamic_cast`.
- [ ] Low: add `const` correctness and prefer `string_view` where feasible (not for SDL C APIs).
- [ ] Low: document scene responsibilities and transitions in `README.md`.

- [ ] Nice-to-have: reduce globals (`extern SDL_Renderer* renderer`); pass a render/context object or store on `State` and inject into scenes.

- [ ] Nice-to-have: assert and bounds-check indices/palettes in asset loading; unify logging levels and categories.

- [ ] Nice-to-have: test utility to dump a few representative assets to PNG and validate palette/alpha correctness.

- [ ] Nice-to-have: ensure all early-return error paths in `main` destroy created SDL resources.

- [ ] Nice-to-have: add a simple FPS/frametime display in the debug overlay to verify pacing.

Summary: A focused checklist to fix main-loop timing and safety, improve resource/RAII, remove fragile casts, cache assets correctly, and tidy build/config for stability and maintainability.


- [x] Diner 