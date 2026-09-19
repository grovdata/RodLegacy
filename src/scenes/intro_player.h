#pragma once

#include <array>
#include <cstdint>
#include <vector>

// The Amiga intro (IntroSequence) as a 50 Hz tick machine over two indexed
// 320x200 buffers and a 32-register RGB4 palette. No SDL, so it is testable;
// the Intro scene renders it and plays the sounds.
// See RodLegacyResearch/docs/intro.md.

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

    // One PAL vblank. Sounds due on this tick are appended.
    void Tick(std::vector<IntroSound>& sounds);

    // Honoured only while the title animation runs, as in the original.
    void RequestSkip() { skipRequested = true; }

    // The credits plate is up and fully faded in.
    bool Done() const { return pc >= ops.size(); }

    const uint8_t* Front() const { return buffers[front].data(); }
    const std::array<uint16_t, 32>& Palette() const { return palette; }

private:
    enum class OpKind {
        Clear, DrawImage, Fade, FadeToBlack, Wait, Sound,
        LogoCels, TitleSetup, TitleCels, Flip, SkipCheck,
    };
    struct Op {
        OpKind kind;
        int a = 0;
        int b = 0;
        int c = 0;
    };

    void Build();
    // Runs the op at pc. Returns true when it used up this tick.
    bool Step(std::vector<IntroSound>& sounds);
    void Blit(const IntroImage& src, int sx, int sy, int w, int h,
              int dx, int dy, bool opaque);

    const std::array<IntroImage, 6>* images = nullptr;
    std::vector<Op> ops;
    size_t pc = 0;
    size_t afterTitle = 0;
    int wait = 0;

    // Steps done in the running FadePalette, -1 when none is running.
    int fadeStep = -1;
    std::array<uint16_t, 32> fadeTarget = {};

    std::array<std::vector<uint8_t>, 2> buffers;
    int front = 0;
    std::array<uint16_t, 32> palette = {};
    bool skipRequested = false;
};
