#include "doctest/doctest.h"
#include "audio/engine_sound.h"

// Playback rate in Hz that the engine loop is asked for at a given rpm.
static float RateAt(int rpm) {
    return EngineSound::RPMToFrequencyMultiplier(static_cast<int16_t>(rpm)) * 16000.0f;
}

TEST_CASE("engine idles at the period the Amiga starts the drone with") {
    // EngineSoundTick (0x227c2e) starts the loop with `move.w #$226, d2`.
    CHECK(RateAt(1500) == doctest::Approx(3579545.0f / 550.0f).epsilon(0.001));
    CHECK(RateAt(5700) == doctest::Approx(3579545.0f / 421.0f).epsilon(0.001));

    // Idle and below clamp to the same level, as does anything past redline.
    CHECK(RateAt(0) == doctest::Approx(RateAt(1500)));
    CHECK(RateAt(9000) == doctest::Approx(RateAt(5700)));
}

TEST_CASE("race and garage engines sit in the same register") {
    // Same car, same 512-byte loop: the race must not be an octave off the
    // garage idle (period 605), which is the bug this pins down. Anything
    // inside a fifth of it is fine; 2x is not.
    const float garage = 3579545.0f / 605.0f;
    CHECK(RateAt(1500) / garage < 1.5f);
    CHECK(RateAt(5700) / garage < 1.5f);
    CHECK(RateAt(1500) / garage > 0.67f);
}

TEST_CASE("engine pitch rises monotonically with rpm") {
    float prev = RateAt(1500);
    for (int rpm = 1500; rpm <= 5700; rpm += 50) {
        const float rate = RateAt(rpm);
        CHECK(rate >= prev);
        prev = rate;
    }
}

TEST_CASE("the redline band gets most of the pitch steps") {
    // The Amiga table steps every 14 velocity units up to 97 and every 4 above
    // it, so the top quarter of the rev range is where the pitch audibly runs
    // away. That is the over-rev cue; without it the engine is nearly flat.
    auto levels = [](int lo, int hi) {
        int count = 0;
        float prev = RateAt(lo);
        for (int rpm = lo; rpm <= hi; rpm += 10) {
            const float rate = RateAt(rpm);
            if (rate > prev) count++;
            prev = rate;
        }
        return count;
    };

    const int lowSteps = levels(1500, 4600);
    const int redlineSteps = levels(4600, 5700);
    CHECK(lowSteps == 7);
    CHECK(redlineSteps == 9);
    CHECK(lowSteps + redlineSteps == 16);
}
