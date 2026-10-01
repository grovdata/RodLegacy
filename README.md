# RodLegacy

RodLegacy is a reimplementation of the classic Street Rod game for modern systems.
It aims to stay true to the original, with some enhancements:

- Play the Amiga version but with glorious framerate and instant loading times!
- Add cars from Street Rod SE to get twice the car selection
- Enjoy both Amiga and beepy DOS soundtrack
- Load original save files
- Gamepad controls
- Hill occlusion in races
- Extra animations and UI improvements
- Pause button
- Windows, Linux and macOS (26+) builds

# Status

RodLegacy is in development, so some bugs are expected but feel free to report them.
The game should be playable up to the end, including races against The King.
Support for Street Rod 2 is planned.

# Playing the game

Download the latest RodLegacy binaries from the [releases](../../releases) page.

The Amiga disk images are *required* to run the game. They can be found for free on [streetrodonline.com](https://www.streetrodonline.com/downloads/).

Start RodLegacy and point it to the folder where `sr1amiga.zip` sits.
Add `srsepc.zip` in the same folder, if you want the DOS SE cars, music, and stickers.

## Config

| OS | Path |
| --- | --- |
| macOS | `~/Library/Application Support/se.grovdata/rodlegacy/config.txt` |
| Linux | `~/.local/share/se.grovdata/rodlegacy/config.txt` |
| Windows | `%APPDATA%\se.grovdata\rodlegacy\config.txt` |

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
| LB / Y | Straight to the garage / the diner |
| RB | Enter |

Anywhere, driving included:

| Gamepad | Keyboard | Action |
| --- | --- | --- |
| Start (Menu) | P | Pause |

Pausing freezes the scene and the audio.

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

# License

RodLegacy is licensed under the GNU General Public License v3.0 or later, see [LICENSE](LICENSE).
Bundled third-party code keeps its own license.

# Legal

RodLegacy makes no claim for Street Rod copyright, it contains no assets or original code from the game.
The copyright sits solely with the owner of [streetrodonline.com](https://www.streetrodonline.com/about/).
