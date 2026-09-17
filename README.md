# RodLegacy

RodLegacy is a reimplementation of the classic Street Rod game for modern systems.
It aims to stay true to the original, with some light enhancements.

# Running the game

Download the latest RodLegacy binaries from the release page.

The Amiga disk images are *required* to run the game. They can be found for free on [streetrodonline.com](https://www.streetrodonline.com/downloads/).

Start RodLegacy and point it to the folder where `sr1amiga.zip` sits.

If you add the MS-DOS [Special Edition](https://www.streetrodonline.com/2014/07/01/street-rod-se-released/) (Data Disk add-on), you can play with 25 additional cars and extra stickers. Just put `srsepc.zip` in the same folder.

## Street Rod SE cars

The SE cars are off by default and there is no in-game toggle yet. Turn them on
by adding this line to `config.txt` and restarting:

```
car_set_srse=1
```

`config.txt` sits in RodLegacy's settings folder:

| OS | Path |
| --- | --- |
| macOS | `~/Library/Application Support/se.grovdata/rodlegacy/config.txt` |
| Linux | `~/.local/share/se.grovdata/rodlegacy/config.txt` |
| Windows | `%APPDATA%\se.grovdata\rodlegacy\config.txt` |

With it on, the SE cars are fully playable: they show up in the classifieds, on
the used car lot, and as opponents. The stock Amiga cars keep their original
indices, so existing `.SAV` files still load. See
[docs/cars.md](docs/cars.md) for the remaining gaps (gas cap sprite, per-scene
car height).

## Other config.txt keys

Additions and deviations, all off by default, all in the same `config.txt`:

| key | what it does |
| --- | --- |
| `extra_animations=1` | Small scene animations the original does not have. Right now: the diner waitress stops halfway in and stands for three seconds before carrying on. |
| `hill_occlusion=1` | Hills hide the opponent's car instead of the original's clamp-to-the-ridge. |

# Controls

Keyboard and gamepad both work at any time; whichever you touch last takes over
the cursor. Any pad SDL recognises as a game controller is picked up
automatically, hotplug included.

Menus and scenes:

| Gamepad | Action |
| --- | --- |
| Left stick | Move the cursor |
| D-pad | Jump to the next button or hotspot in that direction |
| A | Click |
| B | Close the dialog, otherwise right click |
| X | Snap the cursor to the nearest button or hotspot |
| LB / RB | Straight to the garage / the diner |
| Start | Enter |

Driving:

| Gamepad | Keyboard | Action |
| --- | --- | --- |
| Right trigger or A | Up | Throttle |
| Left trigger or B | Down | Brake |
| Left stick or D-pad | Left / Right | Steer |
| X or RB | Space | Shift |

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
off by default, so release builds link no ImGui at all. Enable it at build time
and switch it on at run time:

```
meson setup build -Ddebug_ui=true    # or: meson configure build -Ddebug_ui=true
ninja -C build
./build/rodlegacy --debug
```

Both are required: `-Ddebug_ui=true` compiles it in, `--debug` opens the panels.

# Tests

```
meson test -C build
```

To run with verbose output:

```
./build/tests/test_libp
```

# Notes

Per-subsystem notes on how this port differs from the original, what is
approximated, and what is still missing:

- [docs/audio.md](docs/audio.md) - engine sound mismatch, music calibration and gaps
- [docs/cars.md](docs/cars.md) - wheel offsets, driver portrait, the car-set registry
- [docs/graphics.md](docs/graphics.md) - palette selection and its failure mode
- [docs/gameplay.md](docs/gameplay.md) - challenge retry

Research on the original game and its file formats lives in the sibling
`RodLegacyResearch` repository.

# Credits

RodLegacy use these fine libraries:

 - [SDL2](https://github.com/libsdl-org/SDL)
 - [SDL2_image](https://github.com/libsdl-org/SDL_image)
 - [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer)
 - [ADFlib](https://github.com/adflib/ADFlib)
 - [exepack](https://www.bamsoftware.com/software/exepack/)
 - [SFont](https://github.com/karlb/sfont)
 - [toml11](https://github.com/ToruNiina/toml11)
 - [lemon font](https://github.com/cmvnd/fonts)
 - [ISO88591-8x16, g1-7x8 font](https://github.com/epto/epto-fonts)
 - [tinyfiledialogs](http://tinyfiledialogs.sourceforge.net)
 - [doctest](https://github.com/doctest/doctest)
 - [miniz](https://github.com/richgel999/miniz)

 # Legal
 
RodLegacy make no claim for Street Rod copyright, it contains no assets or original code from the game.
The copyright sits solely with the owner of [streetrodonline.com](https://www.streetrodonline.com/about/).
