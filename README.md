# Street Rod Legacy

Street Rod Legacy is a reimplementation of the classic Street Rod game for modern systems.

# Progress

- [x] Garage
  - [x] Engine dialog
    - [x] Tuning engine
    - [ ] Swapping parts
    - [ ] Dismantling and assembling the engine
    - [ ] Animating parts
    - [ ] Tune animation without glitches

# Building

The game is built using [Meson build system](https://mesonbuild.com/).

```
mkdir build && cd build && ninja
```

# Running the game

The Amiga disk images are required to run the game. They can be found for free on [streetrodonline.com](https://www.streetrodonline.com/downloads/street-rod-legacy-amiga-disk-images/).

The images must be named `SR1AMIGA_DISK1.adf` and `SR1AMIGA_DISK2.adf`.

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