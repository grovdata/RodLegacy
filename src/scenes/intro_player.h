/*
 * RodLegacy - The Amiga intro as a tick machine, SDL-free: IntroPlayer drives two indexed buffers and a
 * palette.
 *
 * Created: 2026-09-19
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

// The Amiga intro as a 50 Hz tick machine over two indexed 320x200 buffers. SDL-free for tests.

enum class IntroSound {
    ChimeA,
    ChimeB,
    ChimeC,
    Ignition,
    Hit100,
    Hit200,
    Hit150,
    Screech,
    StartMusic,
};

struct IntroImage {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px;  // palette indices, w * h
};

class IntroPlayer {
public:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 200;

    // images[i] is libstart.p index i.
    void Start(const std::array<IntroImage, 6>& images);

    // Street Rod 2: every libstart.p picture by index, and the nine palettes from its executable.
    void StartSr2(const std::vector<IntroImage>& libstart,
                  const std::vector<std::array<uint16_t, 32>>& palettes);

    // One PAL vblank. Sounds due on this tick are appended.
    void Tick(std::vector<IntroSound>& sounds);

    bool Done() const { return pc >= ops.size(); }

    const uint8_t* Front() const { return buffers[front].data(); }
    const std::array<uint16_t, 32>& Palette() const { return palette; }

private:
    // The intro is a flat list of ops run one at a time. What a, b and c mean depends on the kind:
    //   Clear        both buffers to colour 0
    //   DrawImage    a = image index, into both buffers
    //   Fade         a = from palette (-1 = black), b = to palette, c = extra ticks per step; 16 steps
    //   FadeToBlack  one step down per tick until black
    //   SetPalette   a = palette, immediately
    //   Wait         a = ticks
    //   Sound        a = IntroSound
    //   LogoCels     a = logo step
    //   TitleSetup   SR1: keep only the car of the photo
    //   TitleCels    a = SR1 title step
    //   Sr2Cels      a = SR2 title step
    //   Flip         show the back buffer, one tick
    enum class OpKind {
        Clear, DrawImage, Fade, FadeToBlack, SetPalette, Wait, Sound,
        LogoCels, TitleSetup, TitleCels, Sr2Cels, Flip,
    };
    struct Op {
        OpKind kind;
        int a = 0;
        int b = 0;
        int c = 0;
    };

    void Reset();
    void Emit(OpKind kind, int a = 0, int b = 0, int c = 0) { ops.push_back({kind, a, b, c}); }
    void EmitSound(IntroSound s) { Emit(OpKind::Sound, static_cast<int>(s)); }
    // Both games open with the same logo animation, chimes included.
    void BuildLogo();
    void Build();
    void BuildSr2();
    const IntroImage& Image(int i) const;
    std::array<uint16_t, 32> PaletteOf(int id) const;
    // Runs the op at pc. Returns true when it used up this tick.
    bool Step(std::vector<IntroSound>& sounds);
    void Blit(const IntroImage& src, int sx, int sy, int w, int h,
              int dx, int dy, bool opaque);
    void BlitBoth(const IntroImage& src, int w, int h, int dx, int dy, bool xorBlit);

    // Street Rod's six pictures, or Street Rod 2's whole libstart.p. Only one is set.
    const std::array<IntroImage, 6>* images = nullptr;
    const std::vector<IntroImage>* sr2Images = nullptr;
    std::vector<std::array<uint16_t, 32>> sr2Palettes;
    int logoImage = 0;
    std::vector<Op> ops;
    size_t pc = 0;
    int wait = 0;

    // Steps done in the running FadePalette, -1 when none is running.
    int fadeStep = -1;
    std::array<uint16_t, 32> fadeTarget = {};

    std::array<std::vector<uint8_t>, 2> buffers;
    int front = 0;
    std::array<uint16_t, 32> palette = {};
};
