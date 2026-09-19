#include <doctest/doctest.h>
#include "parsers/libp.h"
#include "scenes/intro_player.h"

namespace {

std::array<IntroImage, 6> BlankImages() {
    const int sizes[6][2] = {{288, 190}, {320, 200}, {320, 180}, {320, 200}, {320, 200}, {320, 200}};
    std::array<IntroImage, 6> imgs;
    for (int i = 0; i < 6; i++) {
        imgs[i].w = sizes[i][0];
        imgs[i].h = sizes[i][1];
        imgs[i].px.assign(imgs[i].w * imgs[i].h, 1);
    }
    return imgs;
}

int RunToEnd(IntroPlayer& p, std::vector<IntroSound>& sounds, int skipAtTick = -1) {
    int ticks = 0;
    while (!p.Done() && ticks < 5000) {
        if (ticks == skipAtTick) p.RequestSkip();
        p.Tick(sounds);
        ticks++;
    }
    return ticks;
}

}  // namespace

TEST_CASE("PlanarToIndices reads plane-sequential bitplanes") {
    BitmapEntry_t e = {};
    e.width = 8;
    e.height = 1;
    e.depth = 2;
    const std::vector<uint8_t> planar = {0xA0, 0xC0};
    const auto idx = BitmapParser().PlanarToIndices(planar, e);
    REQUIRE(idx.size() == 8);
    CHECK(idx[0] == 3);
    CHECK(idx[1] == 2);
    CHECK(idx[2] == 1);
    CHECK(idx[3] == 0);
}

TEST_CASE("intro runs its full length and ends on the credits plate palette") {
    const auto imgs = BlankImages();
    IntroPlayer p;
    p.Start(imgs);
    std::vector<IntroSound> sounds;
    // logo 96 + 150 + 28*3, fade out 15, sepia 96, colour 96, title 169*2,
    // fade out 15, plate 16
    CHECK(RunToEnd(p, sounds) == 906);
    const std::vector<IntroSound> expected = {
        IntroSound::ChimeA, IntroSound::ChimeB, IntroSound::ChimeC,
        IntroSound::ChimeB, IntroSound::ChimeC, IntroSound::ChimeA,
        IntroSound::Ignition, IntroSound::Ignition,
        IntroSound::Hit100, IntroSound::Hit200, IntroSound::Hit150,
        IntroSound::Screech, IntroSound::StartMusic};
    CHECK(sounds == expected);
    CHECK(p.Palette()[1] == 0x810);
    CHECK(p.Front()[0] == 1);           // the plate is drawn
    CHECK(p.Front()[199 * 320] == 0);   // and is only 180 rows high
}

TEST_CASE("skip is ignored before the title animation") {
    const auto imgs = BlankImages();
    IntroPlayer p;
    p.Start(imgs);
    std::vector<IntroSound> sounds;
    CHECK(RunToEnd(p, sounds, 10) == 906);
}

TEST_CASE("skipping the title animation still starts the music once") {
    const auto imgs = BlankImages();
    IntroPlayer p;
    p.Start(imgs);
    std::vector<IntroSound> sounds;
    // Title starts at tick 330 + 15 + 96 + 96 = 537; skip on its 3rd step.
    const int ticks = RunToEnd(p, sounds, 541);
    CHECK(ticks < 906);
    CHECK(std::count(sounds.begin(), sounds.end(), IntroSound::StartMusic) == 1);
    CHECK(std::count(sounds.begin(), sounds.end(), IntroSound::Ignition) == 0);
}
