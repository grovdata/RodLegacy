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

int RunToEnd(IntroPlayer& p, std::vector<IntroSound>& sounds) {
    int ticks = 0;
    while (!p.Done() && ticks < 5000) {
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
    // logo 96 + 150 + 28*3, fade out 15, sepia 96, colour 96, title 169*3,
    // fade out 15, plate 16
    CHECK(RunToEnd(p, sounds) == 1075);
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

#include "audio/amiga_envelope.h"

TEST_CASE("srl.env entry 3 under the intro chime: volume per CIA tick") {
    // Levels then rates, as in srl.env. Entry 2's last word (0) precedes it.
    const int16_t env3[16] = {64, 48, 53, 42, 48, 0, 0, 0,
                              14336, 2048, 1024, 1792, 2048, 1024, 0, 0};
    AmigaEnvelope e;
    e.Start(env3, 0, 0x32, 15);
    CHECK(e.volume == 43);
    const int expected[] = {50, 43, 37, 40, 41, 36, 32, 37, 34, 31, 28, 25, 22,
                            19, 16, 13, 10, 7, 4, 1, 0};
    for (int v : expected) {
        REQUIRE(e.Active());
        e.Tick();
        CHECK(e.volume == v);
    }
    CHECK_FALSE(e.Active());
}

TEST_CASE("music note on srl.env entry 0: up in one tick, then down 1 per tick") {
    const int16_t env0[16] = {64, 0, 0, 0, 0, 0, 0, 0, 16384, 256, 0, 0, 0, 0, 0, 0};
    AmigaEnvelope e;
    e.Start(env0, 0, 64, 9);
    e.volume = 0;   // MusicTick zeroes it after MusicPlayNote
    e.Tick();
    CHECK(e.volume == 38);   // 64 * 9 / 15
    int ticks = 1;
    while (e.Active() && ticks < 100) {
        e.Tick();
        ticks++;
    }
    CHECK(e.volume == 0);
    CHECK(ticks == 39);
}
