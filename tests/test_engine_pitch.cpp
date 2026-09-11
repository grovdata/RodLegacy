#include "doctest/doctest.h"
#include "audio/engine_sound.h"
#include "parsers/libp.h"
#include <cmath>

TEST_CASE("engine period follows the Amiga's rpm formula") {
    // RoadRaceMainLoop 0x2363ae computes 240 - rpm/32; the setter at 0x227c8c
    // adds 400. So period = 640 - rpm/32 below the over-rev threshold.
    CHECK(EngineSound::RPMToPeriod(0) == 640);
    CHECK(EngineSound::RPMToPeriod(1500) == 640 - 46);   // idle -> 594
    CHECK(EngineSound::RPMToPeriod(5000) == 640 - 156);  // -> 484

    // Negative rpm cannot raise the period above the idle stop.
    CHECK(EngineSound::RPMToPeriod(-100) == 640);
}

TEST_CASE("crossing 5500 rpm jumps the drone up, and dropping back undoes it") {
    // 0x2363d4 swaps the base 240 for 180, so the period falls 60 in one tick.
    const int below = EngineSound::RPMToPeriod(5499);
    const int above = EngineSound::RPMToPeriod(5500);
    CHECK(below == 640 - 171);   // 469
    CHECK(above == 580 - 171);   // 409
    CHECK(below - above == 60);

    // That is an audible step, not a ramp: about 2.4 semitones, arriving 200
    // rpm before the limiter so there is time to lift.
    const float jump = static_cast<float>(below) / static_cast<float>(above);
    CHECK(jump > 1.13f);
    CHECK(12.0f * std::log2(jump) == doctest::Approx(2.4f).epsilon(0.1));

    // At the limiter it stays in the warning state.
    CHECK(EngineSound::RPMToPeriod(5700) == 580 - 178);  // 402

    // Lifting off below the threshold restores the normal drone.
    CHECK(EngineSound::RPMToPeriod(5499) == below);

    // The warning must not be reachable by the ramp alone - the normal branch
    // never gets down to where the over-rev branch starts.
    CHECK(EngineSound::RPMToPeriod(5499) > EngineSound::RPMToPeriod(5500));
}

TEST_CASE("the bay drone detunes with mistune and goes clean at perfect tune") {
    // Measured live in Copperline off the period array at A4-0x44ec: both pairs
    // sit at 670 at perfect tune, and a knob press moves channels 0/1 by 10
    // and channels 2/3 by 11.
    auto pairA = [](int off) { return BayDronePeriodA(off); };
    auto pairB = [](int off) { return BayDronePeriodB(off); };

    // Perfect tune: identical, so no beating at all. This is the whole point.
    CHECK(pairA(0) == 670);
    CHECK(pairB(0) == 670);
    CHECK(pairA(0) == pairB(0));

    // Advancing shortens the period (higher pitch), retarding lengthens it.
    CHECK(pairA(+3) == 670 - 39);
    CHECK(pairA(-3) == 670 + 30);
    CHECK(pairA(+1) < pairA(0));
    CHECK(pairA(-1) > pairA(0));

    // The detune grows by one period a notch, so the phasing speeds up the
    // worse the tune is - in both directions.
    for (int off = -8; off < 0; ++off) {
        CHECK(pairB(off) - pairA(off) == -off);
    }
    for (int off = 1; off <= 4; ++off) {
        CHECK(pairB(off) - pairA(off) == off);
    }
}

TEST_CASE("advancing the tune detunes faster than retarding") {
    // 0x2419fa (Advance) multiplies by 13/12; 0x241a56 (Retard) by 10/11. The
    // two directions are deliberately not mirror images.
    CHECK(SRL0_BAY_DRONE_STEP_A_RETARD == 10);
    CHECK(SRL0_BAY_DRONE_STEP_B_RETARD == 11);
    CHECK(SRL0_BAY_DRONE_STEP_A_ADVANCE == 13);
    CHECK(SRL0_BAY_DRONE_STEP_B_ADVANCE == 12);

    // Retarding: pair B falls behind pair A. Advancing: pair A runs away from
    // pair B. Either way the two split apart, so the beating returns - but a
    // notch of advance moves pair A further than a notch of retard.
    CHECK(SRL0_BAY_DRONE_STEP_A_ADVANCE > SRL0_BAY_DRONE_STEP_A_RETARD);

    // The detune is one period a notch each way, opposite in sign.
    const int advDetune = SRL0_BAY_DRONE_STEP_B_ADVANCE - SRL0_BAY_DRONE_STEP_A_ADVANCE;
    const int retDetune = SRL0_BAY_DRONE_STEP_B_RETARD - SRL0_BAY_DRONE_STEP_A_RETARD;
    CHECK(advDetune == -1);
    CHECK(retDetune == 1);

    // The tune byte at car+0x2b clamps to -8..+4, so these are the extremes.
    CHECK(BayDronePeriodA(-8) == 750);   // fully retarded: slowest
    CHECK(BayDronePeriodA(+4) == 618);   // fully advanced: fastest
}

TEST_CASE("perfect tune drops the bay drone's volume too") {
    // 0x241a1a: tst.b the tune byte, 16 when zero and 25 otherwise. Perfect
    // tune is marked twice - the phasing stops AND the drone quietens.
    CHECK(BayDroneVolume(0) == SRL0_BAY_DRONE_VOLUME_TUNED);
    CHECK(BayDroneVolume(-1) == SRL0_BAY_DRONE_VOLUME);
    CHECK(BayDroneVolume(+1) == SRL0_BAY_DRONE_VOLUME);
    CHECK(BayDroneVolume(0) < BayDroneVolume(+3));
}

TEST_CASE("the bay ignition rolls its two channels independently") {
    // 0x22818e calls PRNG(50) twice, once per channel, so the two sides end up
    // at different rates and phase against each other. It plays ONCE
    // (ioa_Cycles = 1 at 0x2281b4) - looping it means the ignition never ends,
    // which is what it sounded like.
    const int lo = SRL0_BAY_IGNITION_PERIOD_BASE;
    const int hi = SRL0_BAY_IGNITION_PERIOD_BASE + SRL0_BAY_IGNITION_PERIOD_SPREAD - 1;
    CHECK(lo == 320);
    CHECK(hi == 369);

    // The widest detune the roll can produce has to be audible as phasing but
    // not as two separate notes: a few percent, well under a semitone.
    const double ratio = static_cast<double>(hi) / lo;
    CHECK(ratio > 1.10);
    CHECK(12.0 * std::log2(ratio) < 3.0);

    // And it has to be a long sample, not the 512-byte racing loop.
    CHECK(SRL0_BAY_IGNITION_LENGTH == 6912);
    CHECK(SRL0_BAY_IGNITION_OFFSET != SRL0_ENGINE_LOOP_OFFSET);
}

TEST_CASE("the part-mount sound is detuned, and drifts slowly") {
    // 0x227de8 plays the racing loop twice at 3550 and 3650. The point is the
    // gap: equal periods would be plain mono and there would be no phasing.
    CHECK(EngineSound::kPartMountPeriodLeft == 3550);
    CHECK(EngineSound::kPartMountPeriodRight == 3650);
    CHECK(EngineSound::kPartMountPeriodLeft != EngineSound::kPartMountPeriodRight);

    // The two copies drift a full loop apart roughly every 18-19 seconds, which
    // is what makes it a slow sweep rather than an audible beat note.
    const double rateL = 3579545.0 / EngineSound::kPartMountPeriodLeft / 512.0;
    const double rateR = 3579545.0 / EngineSound::kPartMountPeriodRight / 512.0;
    const double sweepSeconds = 1.0 / std::abs(rateL - rateR);
    CHECK(sweepSeconds > 15.0);
    CHECK(sweepSeconds < 25.0);

    // Volume 28 is the same level the rev ramp treats as full scale.
    CHECK(EngineSound::kPartMountPaulaVolume == EngineSound::RPMToPaulaVolume(5700));
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
    // race must land in the same register rather than an octave above it. Check
    // that below the over-rev threshold - above it the drone is meant to jump.
    const float garage = 3579545.0f / 605.0f;
    CHECK(rate(1500) / garage == doctest::Approx(605.0f / 594.0f).epsilon(0.01));
    CHECK(rate(EngineSound::kOverRevRpm - 1) / garage < 1.35f);

    // And the warning state sits above everything the ramp can reach.
    CHECK(rate(EngineSound::kOverRevRpm) > rate(EngineSound::kOverRevRpm - 1));
}
