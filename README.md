# Street Rod Legacy

Street Rod Legacy is a reimplementation of the classic Street Rod game for modern systems.
It aims to stay true to the original, with some light enhancements.

# Building

The game is built with [Meson build system](https://mesonbuild.com/).

```
meson setup build
ninja -C build
```

# Tests

```
meson test -C build
```

To run with verbose output:

```
./build/tests/test_libp
```

# Running the game

The Amiga disk images are required to run the game. They can be found for free on [streetrodonline.com](https://www.streetrodonline.com/downloads/).

The images must be named `SR1AMIGA_DISK1.adf` and `SR1AMIGA_DISK2.adf`.

`LIB2` file from the MS-DOS [Special Edition](https://www.streetrodonline.com/2014/07/01/street-rod-se-released/) (Data Disk add-on) will get you some new stickers
to add to your cars.

# Credits

StreetRod Legacy use these fine libraries:

 - [SDL2](https://github.com/libsdl-org/SDL)
 - [SDL2_image](https://github.com/libsdl-org/SDL_image)
 - [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer)
 - [ADFlib](https://github.com/michalp/adflib)
 - [SFont](https://github.com/karlb/sfont)
 - [toml11](https://github.com/ToruNiina/toml11)
 - [lemon font](https://github.com/cmvnd/fonts)
 - [ISO88591-8x16 font](https://github.com/epto/epto-fonts)
 - [tinyfiledialogs](http://tinyfiledialogs.sourceforge.net)
 - [doctest](https://github.com/doctest/doctest)
 - [miniz](https://github.com/richgel999/miniz)
