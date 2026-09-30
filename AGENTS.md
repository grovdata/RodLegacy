# AGENTS.md

C++17 SDL2 reimplementation of Street Rod. Assets come from Amiga .adf images.

## Build
meson setup build && ninja -C build
Run ./build/rodlegacy from a directory containing SR1AMIGA_DISK1.adf and SR1AMIGA_DISK2.adf.
Dependencies are vendored in subprojects/.

## Conventions
- Native res 320x200, rendered at 4x.
- Use SDL_Log and stdlib in game code. parsers/libp.* must not depend on SDL.
- Avoid C++ templates.
- No emojis in log messages.
- Never launch the app to verify visually. The user does that.
