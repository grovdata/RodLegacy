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