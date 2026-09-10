#include "doctest/doctest.h"
#include "audio/engine_sound.h"

TEST_CASE("engine period follows the Amiga's rpm formula") {
    // RoadRaceMainLoop 0x236278 computes 240 - rpm/32; the setter at 0x227c8c
    // adds 400. So period = 640 - rpm/32.
    CHECK(EngineSound::RPMToPeriod(0) == 640);
    CHECK(EngineSound::RPMToPeriod(1500) == 640 - 46);   // idle  -> 594
    CHECK(EngineSound::RPMToPeriod(5700) == 640 - 178);  // redline -> 462

    // Negative rpm cannot raise the period above the idle stop.
    CHECK(EngineSound::RPMToPeriod(-100) == 640);
}

TEST_CASE("engine volume follows the Amiga's rpm formula") {
    // 0x236286: (rpm + 14) / 256, and the setter adds 6.
    CHECK(EngineSound::RPMToPaulaVolume(1500) == 11);
    CHECK(EngineSound::RPMToPaulaVolume(5700) == 28);

    // It has to actually climb - a flat drone is the bug this replaces.
    CHECK(EngineSound::RPMToPaulaVolume(5700) > EngineSound::RPMToPaulaVolume(1500));
    int prev = EngineSound::RPMToPaulaVolume(0);
    for (int rpm = 0; rpm <= 5700; rpm += 50) {
        const int v = EngineSound::RPMToPaulaVolume(static_cast<int16_t>(rpm));
        CHECK(v >= prev);
        prev = v;
    }
}

TEST_CASE("engine pitch rises with rpm and stays near the garage idle") {
    auto rate = [](int rpm) {
        return EngineSound::RPMToFrequencyMultiplier(static_cast<int16_t>(rpm)) * 16000.0f;
    };

    float prev = rate(0);
    for (int rpm = 0; rpm <= 5700; rpm += 50) {
        CHECK(rate(rpm) >= prev);
        prev = rate(rpm);
    }

    // Same car and same 512-byte loop as the garage drone (period 605), so the
    // race must land in the same register rather than an octave above it.
    const float garage = 3579545.0f / 605.0f;
    CHECK(rate(1500) / garage == doctest::Approx(605.0f / 594.0f).epsilon(0.01));
    CHECK(rate(5700) / garage < 1.5f);
}
