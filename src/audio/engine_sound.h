#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <cstdint>

/**
 * Engine sound playback with period modulation (pitch shifting)
 * Based on Amiga Street Rod implementation where RPM changes playback speed
 *
 * Uses SDL_AudioStream for real-time resampling/pitch shifting
 */
class EngineSound {
public:
    EngineSound();
    ~EngineSound();

    // Initialize audio system with provided sample data
    bool Initialize(const std::vector<int8_t>& sampleData);

    // Cleanup
    void Shutdown();

    // Start/stop engine sound loop
    void Start();
    void Stop();

    // Update engine pitch based on RPM (1500-7000 range) - for racing
    void UpdateRPM(int16_t rpm);

    // Update engine pitch directly using Amiga period (for car animations)
    void UpdatePeriod(int16_t period);

    // Set volume (0.0-1.0)
    void SetVolume(float volume);

    bool IsPlaying() const { return playing; }

    // Game RPM (idle 1500 -> redline 5700) to playback rate / 16000 Hz, via the
    // Amiga velocity -> audio level -> Paula period table. Header-inline so the
    // SDL-free unit tests can reach it.
    static float RPMToFrequencyMultiplier(int16_t rpm) {
        // Paula periods, idle -> redline.
        //
        // Idle is anchored on the binary: EngineSoundTick (0x227c2e) starts the
        // drone with `move.w #$226, d2` = period 550, and the per-tick setter
        // (0x227c8c) computes the live period as `addi.w #$190` = param + 400,
        // so 550 is the idle pitch, not a one-off.
        //
        // These used to run 252..193, which is neither the start period nor
        // anything +400 lands on - and no such table exists anywhere in the
        // Amiga binary, as a literal or a byte run. Whatever its provenance, it
        // played the race an octave and a fifth above the garage idle (605),
        // off the same sample and the same car.
        //
        // The ramp SHAPE below is the old table rescaled so level 0 = 550. The
        // top end is the one piece still unverified: 421 keeps the old table's
        // 1.31 idle:redline ratio, which is about 5 semitones of rev.
        static const int periods[17] = {
            550, 543, 535, 526, 519, 511, 502, 495, 487,
            478, 471, 463, 454, 447, 439, 430, 421
        };

        // The upper bound is the redline PlayerCarPhysics actually caps rpm at.
        // With the old 7000 here the mapping never got past velocity 99, so
        // levels 9-16 - the finely stepped top of the table, i.e. the redline
        // cue - were unreachable and the engine barely changed pitch at all.
        const float MIN_RPM = 1500.0f;
        const float MAX_RPM = 5700.0f;
        float clampedRPM = rpm < MIN_RPM ? MIN_RPM : (rpm > MAX_RPM ? MAX_RPM : static_cast<float>(rpm));
        int velocity = static_cast<int>((clampedRPM - MIN_RPM) / (MAX_RPM - MIN_RPM) * 130.0f);

        // Velocity to audio level (0-16), Amiga thresholds from LAB_1392.
        // >= 130, not > 130: the clamp above lands exactly on 130 at redline,
        // so a strict test would leave the top level unreachable.
        int level = 0;
        if (velocity >= 130) level = 16;
        else if (velocity > 125) level = 15;
        else if (velocity > 121) level = 14;
        else if (velocity > 117) level = 13;
        else if (velocity > 113) level = 12;
        else if (velocity > 109) level = 11;
        else if (velocity > 105) level = 10;
        else if (velocity > 101) level = 9;
        else if (velocity > 97) level = 8;
        else if (velocity > 83) level = 7;
        else if (velocity > 69) level = 6;
        else if (velocity > 55) level = 5;
        else if (velocity > 41) level = 4;
        else if (velocity > 27) level = 3;
        else if (velocity > 13) level = 2;
        else if (velocity > 0) level = 1;

        return (3579545.0f / static_cast<float>(periods[level])) / 16000.0f;
    }

private:
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioStream* audioStream = nullptr;
    SDL_AudioSpec audioSpec;  // Obtained audio spec from device
    std::vector<int8_t> baseSample;  // Original engine sample (signed 8-bit)

    bool playing = false;
    int16_t currentRPM = 1500;
    float currentFreqMultiplier = 1.0f;  // Current frequency multiplier (updated by RPM or period)
    float currentVolume = 0.5f;
    float samplePositionL = 0.0f;  // Left voice position
    float samplePositionR = 0.0f;  // Right voice position
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;

    // Audio callback (static wrapper)
    static void AudioCallback(void* userdata, Uint8* stream, int len);

    // Instance audio callback
    void FillAudioBuffer(Uint8* stream, int len);
};
