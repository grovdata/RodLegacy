#include <doctest/doctest.h>
#include "audio/pc_speaker_fx.h"

using namespace pcspk;

namespace {
int Zero(int) { return 0; }
}

TEST_SUITE("PC speaker effects") {

TEST_CASE("the engine note follows the square of the revs") {
    CHECK(EngineDivisor(1500) == 47803);   // 25 Hz at idle
    CHECK(EngineDivisor(5699) > EngineDivisor(5700) + 9000);   // the limiter rev jumps up
    CHECK(EngineDivisor(3000) < EngineDivisor(1500));
}

TEST_CASE("Street Rod 2's engine note, the spec's worked values") {
    CHECK(EngineDivisor(1500, true) == 56980);
    CHECK(EngineDivisor(3000, true) == 47916);
    CHECK(EngineDivisor(5700, true) == 10374);
}

TEST_CASE("Street Rod 2 without a muffler drops every other engine note to the 0xF0 rumble") {
    int count = 0;
    CHECK(MufflerPulse(56980, count, 1) == 0xF094);
    CHECK(MufflerPulse(56980, count, 1) == 56980);
    CHECK(MufflerPulse(56980, count, 1) == 0xF094);
}

TEST_CASE("the car animations rumble on two engine notes of three") {
    int count = 0;
    CHECK(MufflerPulse(56980, count, 2) == 0xF094);
    CHECK(MufflerPulse(56980, count, 2) == 0xF094);
    CHECK(MufflerPulse(56980, count, 2) == 56980);
    CHECK(MufflerPulse(56980, count, 2) == 0xF094);
}

TEST_CASE("the tune-up tone drops with the setting") {
    CHECK(TuneDivisor(0) == 30000);
    CHECK(TuneDivisor(-8) == 30000 + 8 * 1024);
    CHECK(TuneDivisor(4) == TuneDivisor(-4));
}

TEST_CASE("the animation note climbs with the car's distance from its parking spot") {
    CHECK(AnimationTone(0) == 0);
    CHECK(AnimationTone(100) == 5000);
    CHECK(AnimationTone(-100) == 5000);
    CHECK(AnimationTone(200) == 20000);
    CHECK(AnimationTone(450) == 20000);
}

TEST_CASE("effect lengths at the DOSBox delay unit") {
    // Click: 9 x 4 tones of 60 units; thud: 11 tones of 1000.
    CHECK(DurationUs(BuildEffect(Effect::Click, Zero)) == doctest::Approx(9 * 4 * (20 + 60 * 4.47)));
    CHECK(DurationUs(BuildEffect(Effect::Thud, Zero)) == doctest::Approx(11 * (20 + 1000 * 4.47)));
}

TEST_CASE("every effect ends with the speaker off") {
    for (Effect fx : {Effect::Click, Effect::ChirpHigh, Effect::ChirpLow, Effect::Clank, Effect::Crash, Effect::Hit,
                      Effect::Thud}) {
        const auto segs = BuildEffect(fx, Zero);
        REQUIRE(!segs.empty());
        CHECK((segs.back().port61 & 3) == 0);
    }
}

TEST_CASE("noise runs with the timer gate off, toggling the speaker bit") {
    const auto segs = BuildEffect(Effect::Clank, Zero);
    int toggles = 0;
    for (size_t i = 1; i < segs.size(); ++i) {
        CHECK((segs[i].port61 & 1) == 0);
        if ((segs[i].port61 & 2) != (segs[i - 1].port61 & 2)) ++toggles;
    }
    CHECK(toggles >= 59);
}

TEST_CASE("loop songs come out of the unpacked SR.EXE's song block") {
    const size_t image = 32;
    std::vector<uint8_t> exe(image + 0x3e960 + 0x57f6 + 48, 0);
    exe[8] = 2;   // header paragraphs
    auto put = [&](size_t at, uint16_t v) { exe[at] = uint8_t(v); exe[at + 1] = uint8_t(v >> 8); };
    const size_t spray = image + 0x2f930 + 0xa0 + 4;
    put(spray, 36); put(spray + 2, 248); put(spray + 6, 1);   // one note, then the section marker
    put(image + 0x3e960 + 0x57f6 + 2, 1);                     // row 0, second step
    const auto loops = ReadLoops(exe.data(), exe.size());
    const LoopSong& s = loops[static_cast<size_t>(Loop::Spray)];
    REQUIRE(s.events.size() == 1);
    CHECK(s.events[0].first == 36);
    CHECK(s.events[0].second == 248);
    CHECK(s.vibrato);
    CHECK(s.depth == 10);
    CHECK(s.wave[1] == 1);
    CHECK_FALSE(loops[static_cast<size_t>(Loop::IdlePutt)].vibrato);
    CHECK(ReadLoops(exe.data(), 16)[0].events.empty());
}

TEST_CASE("Street Rod 2's loops and vibrato come out of its own data segment") {
    const size_t ds = 32 + 0x3e080;
    std::vector<uint8_t> exe(ds + 0x5600, 0);
    exe[8] = 2;   // header paragraphs
    auto put = [&](size_t at, uint16_t v) { exe[at] = uint8_t(v); exe[at + 1] = uint8_t(v >> 8); };
    put(ds + 0x5534 + 4, 7); put(ds + 0x5534 + 6, 7977);      // the paint job's 150 Hz buzz
    put(ds + 0x548c, 10);                                      // its vibrato table: record 10
    const size_t rec = ds + 0x539e + 100;
    put(rec + 2, 2); put(rec + 4, 25); put(rec + 6, 2);        // row 2, depth 25, period 2
    put(ds + 0x536e + 2 * 16 + 2, 1);                          // row 2: 0 1 0 0 ...
    CHECK(ReadLoopsSr2(exe.data(), exe.size())[0].events.empty());   // not SR2's exe without its putt
    put(ds + 0x5544 + 4, 1); put(ds + 0x5544 + 6, 36488);

    const auto loops = ReadLoopsSr2(exe.data(), exe.size());
    const LoopSong& paint = loops[static_cast<size_t>(Loop::Spray)];
    REQUIRE(paint.events.size() == 1);
    CHECK(paint.events[0].second == 7977);
    CHECK(paint.vibrato);
    CHECK(paint.period == 2);
    CHECK(paint.wave[1] == 1);
    CHECK(VibratoAmount(paint, 7977) == 15 * 25);              // (note / 500) * depth
    CHECK_FALSE(loops[static_cast<size_t>(Loop::IdlePutt)].vibrato);   // record 0, depth 0

    LoopSong sr1;
    sr1.depth = 10;
    CHECK(VibratoAmount(sr1, 248) == 2);                       // ((depth * note) & 0xFFFF) / 1000
}

}
