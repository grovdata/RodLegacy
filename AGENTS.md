# CLAUDE.md

This file provides guidance to coding agents when working with code in this repository.

## Project Overview

Street Rod Legacy is a C++17 reimplementation of the classic Street Rod game using SDL2. It reads original Amiga disk images (.adf) for game assets. The executable is `rodlegacy`.

## Build Commands

```bash
# Initial setup
meson setup build

# Build
ninja -C build

# Rebuild after changes
ninja -C build

# Run (requires SR1AMIGA_DISK1.adf and SR1AMIGA_DISK2.adf in working directory)
./build/rodlegacy
```

Build system: Meson + Ninja. Dependencies are vendored as Meson subprojects in `subprojects/`.

## Architecture

**Scene-based game loop.** The main loop in `main.cpp` runs at 60 FPS with delta-time updates. All game locations are `Scene` subclasses:

- `Scene` (base) - Background rendering, fade transitions, zone-based click interaction, dialog management
- `CarScene` (extends Scene) - Adds car animation with Amiga period-based timing and engine sounds
- Concrete scenes: `Garage`, `Diner`, `GasStation`, `Driving`, `Racing`, `Newspaper`, `EngineBay`

**Key components:**

- **State** (`state.h/cpp`) - Global game state. Owns the `AssetManager`, manages scene transitions via `MoveToScene()`, tracks bankroll, car roster, drivers, elapsed time.
- **AssetManager** (`assets.h/cpp`) - Loads and caches all assets from Amiga disk images. Manages bitmaps with palette variants, audio, and fonts (SFont format).
- **Car system** - `Car` (instance with modifications), `CarBitmap` (rendering with parts), `CarData` (data loading). Car definitions live in `data/cars.toml`.
- **Parsers** (`parsers/`) - `libp.h/cpp` parses the game's `.lib` asset format, `adf.c/h` reads Amiga Disk Format. **libp must not depend on SDL.**
- **UI** (`ui/`) - Zone (clickable areas), Button, Bubble (NPC speech), HUD, Label, Rectangle.
- **Dialogs** (`dialogs/`) - Modal Dialog, ListDialog (scrollable lists), ColorPicker.
- **Audio** (`audio/`) - AudioManager for music/SFX via SDL_mixer, EngineSound for real-time RPM-based engine sound generation.

**Rendering:** Original resolution is 320x200, rendered at 4x scale (1280x800). Uses 12-bit Amiga color palettes. Bitmap class wraps SDL textures with palette support.

## Code Conventions

- Use SDL2 logging (`SDL_Log`, etc.) and stdlib equivalents in game code
- `parsers/libp.h` and `parsers/libp.cpp` must not require SDL
- Avoid using C++ templates whenever possible
- No emojis in logging messages
- C++17 standard with smart pointers for ownership
