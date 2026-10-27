# RodLegacy

RodLegacy is a reimplementation of the classic Street Rod and Street Rod 2 games for modern systems.
It aims to stay true to the originals, with some enhancements:

- Play the Amiga versions but with glorious framerate and instant loading times!
- Mix the car sets: play Street Rod 2 with the Street Rod and Street Rod SE cars too, or Street Rod with the SE cars
- Pick a soundtrack: Amiga, AdLib, C64 or PC speaker
- Load original save files
- Gamepad controls
- Hill occlusion in races
- Extra animations and UI improvements
- Pause button
- Video capture to mp4 (needs ffmpeg installed)
- Windows, Linux and macOS (26+) builds

# Status

RodLegacy is in development, so some bugs are expected but feel free to report them.
The game should be playable up to the end, including races against The King.
Street Rod 2 is playable, including Mulholland, the Aqueduct, Grudge Night and The King.

# Playing the game

Download the latest RodLegacy binaries from the [releases](../../releases) page.

The Amiga disk images are *required* to run the game. They can be found for free on [streetrodonline.com](https://www.streetrodonline.com/downloads/).

Start RodLegacy and point it to the folder holding the zips. You need at least one of the games:

| File | What it gives you |
| --- | --- |
| `sr1amiga.zip` | Street Rod |
| `sr2amiga.zip` | Street Rod 2 |
| `sr1pc.zip` | Optional: Street Rod's DOS PC speaker music |
| `sr2pc.zip` | Optional: Street Rod 2's AdLib music and sounds, and its PC speaker sounds |
| `srsepc.zip` | Optional: the DOS SE cars and stickers |
| `sr1commodore.zip` | Optional: Commodore 64 SID music and sound effects |

If both games are present, RodLegacy asks which to play at startup.

Extra car sets are picked on the name screen when starting a new game. Street Rod 2 can use
the Street Rod and SE cars alongside its own, Street Rod can add the SE cars.

# Controls

Also listed in the game under Options -> Key bindings.

Keyboard and gamepad both work at any time; whichever you touch last takes over
the cursor. Any pad SDL recognises as a game controller is picked up
automatically.

Menus and scenes:

| Gamepad | Action |
| --- | --- |
| Left stick | Move the cursor |
| D-pad | Jump to the next button or hotspot in that direction |
| A | Click |
| B | Back (closes a dialog, leaves a screen) |
| X | Snap the cursor to the nearest button or hotspot |
| LB / Y / A | Skip the cruising cutscene |
| RB | Enter |

On-screen keyboard (while a text field is active):

| Gamepad | Action |
| --- | --- |
| Back (Select) / LB | Show / hide the keyboard |
| D-pad | Move over the keys (Up from the top row hides it) |
| A | Type the key |
| X | Delete |
| Y / RS | Shift |
| RB | OK |
| B | Esc |

Anywhere, driving included:

| Gamepad | Keyboard | Action |
| --- | --- | --- |
| Start (Menu) | P | Pause |

Pausing freezes the scene and the audio.

| Keyboard | Action |
| --- | --- |
| Esc | Quit (asks first) |
| F11 or Ctrl+F | Toggle fullscreen |
| F5 | Screenshot |
| F6 | Start / stop video capture (needs ffmpeg on the PATH) |

Window scale and fullscreen are also under Options -> Video.

Driving:

| Gamepad | Keyboard | Action |
| --- | --- | --- |
| Right trigger | Up | Throttle |
| Left trigger | Down | Brake |
| Left stick or D-pad | Left / Right | Steer |
| A or RB | Space | Shift |

# Building

The game is built using [Meson build system](https://mesonbuild.com/).

```
meson setup build
ninja -C build
```

## macOS app bundle

```
./scripts/build_macos_app.sh
```

Builds a release `RodLegacy.app` and a `.dmg` in `build-macos/`, with the
Homebrew dylibs copied in and ad-hoc signed, so it runs on a Mac without
Homebrew. Needs `brew install dylibbundler`. Homebrew's `sdl2` is sdl2-compat,
which loads SDL3 at run time, so the bundle carries `libSDL3.dylib` too.

## Debug UI

An optional Dear ImGui debug UI docks the game alongside inspector panels. It is
built into debug builds only (release builds link no ImGui at all) and opened
at run time with `--debug`. `-Ddebug_ui=enabled` or `-Ddebug_ui=disabled`
overrides the build type.

```
ninja -C build
./build/rodlegacy --debug
```

## Command line

| Flag | Effect |
| --- | --- |
| `--sr1` / `--sr2` | Start that game without asking |
| `--scale N` | Window scale, 320x200 times N (default 4, or the largest that fits) |
| `--original-size` | Same as `--scale 1` |
| `--fullscreen` / `--windowed` | Start fullscreen or in a window |
| `--mute` | Mute all audio |
| `-config` | Show the setup screen |
| `--debug` | Debug UI (debug builds) |
| `--version` | Print the version and exit |

# Tests

```
meson test -C build --verbose
```

# Credits

RodLegacy uses these fine libraries:

 - [SDL2](https://github.com/libsdl-org/SDL)
 - [SDL2_image](https://github.com/libsdl-org/SDL_image)
 - [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer)
 - [ADFlib](https://github.com/adflib/ADFlib)
 - [exepack](https://www.bamsoftware.com/software/exepack/)
 - [SFont](https://github.com/karlb/sfont)
 - [toml11](https://github.com/ToruNiina/toml11)
 - [lemon font](https://github.com/cmvnd/fonts)
 - [ISO88591-8x16, g1-7x8 fonts](https://github.com/epto/epto-fonts)
 - [doctest](https://github.com/doctest/doctest)
 - [miniz](https://github.com/richgel999/miniz)
 - [1-bit keyboard/gamepad button icons by ansdor](https://ansdor.itch.io/button-icons)
 - [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3)
 - [reSID](https://vice-emu.sourceforge.io/) (from VICE)
 - [mos6502](https://github.com/gianlucag/mos6502)

# License

RodLegacy is licensed under GPLv3 (see LICENSE). The license covers only the code in this repository, not the original game or its data. Bundled third-party code keeps its own license.

# Legal

 RodLegacy is a fan-made reimplementation of Street Rod and Street Rod 2. It contains no original code or assets. Street Rod remains the property of its copyright holder, the owner of  [streetrodonline.com](https://www.streetrodonline.com/about/).
