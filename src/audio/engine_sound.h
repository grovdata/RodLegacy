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

    // Run the two voices at different Paula periods. 0x227de8 does this - the
    // same loop on channels 0 and 1 at 3550 and 3650, so the copies drift
    // through phase and sweep a comb filter, one full pass every ~18.5s.
    //
    // That routine is NOT the engine bay drone, which is the ordinary loop at
    // period 600 (see EngineBay). It accompanies part mounting, and is toggled
    // on and off around a single animated action rather than held - at period
    // 3550 the loop repeats about twice a second, which is a brief low chug
    // under an animation and pure boom if you sustain it.
    // ponytail: unused until the part install/remove path wants it.
    void UpdatePeriodPair(int16_t leftPeriod, int16_t rightPeriod);

    // Swap the looped sample. The racing drone and the engine bay use different
    // slices of srl0, so whoever starts a loop sets the one it wants.
    void SetSample(const std::vector<int8_t>& sampleData);

    static constexpr int16_t kPartMountPeriodLeft = 3550;
    static constexpr int16_t kPartMountPeriodRight = 3650;
    static constexpr int kPartMountPaulaVolume = 28;

    // Set volume (0.0-1.0)
    void SetVolume(float volume);

    bool IsPlaying() const { return playing; }

    // The racing drone's period and volume, both straight out of
    // RoadRaceMainLoop 0x236278:
    //
    //     move.w  $4b18(a4), d0   ; rpm
    //     asr.w   #$5, d1         ; rpm/32
    //     sub.w   d1, d2          ; arg1 = 240 - rpm/32
    //     addi.w  #$e, d0
    //     asr.w   #$8, d0         ; arg2 = (rpm+14)/256
    //
    // and the setter at 0x227c8c adds 400 to arg1 (period) and 6 to arg2
    // (volume). So the period runs 594 at idle down to 462 at the redline,
    // which sits right beside the garage idle of 605.
    //
    // The version above is the plain path. The one that actually runs during a
    // live race is 0x2363ae, which has a second branch:
    //
    //     cmpi.w  #$157c, d0      ; rpm >= 5500 ?
    //     bge.b   $2363d4
    //     move.w  #$f0, d2        ; normal:   240 - rpm/32,  (rpm+14)/256
    //     ...
    // $2363d4:
    //     move.w  #$b4, d2        ; over-rev: 180 - rpm/32,  (rpm+19)/256
    //
    // Crossing 5,500 drops the base from 240 to 180, so the period falls by 60
    // in a single step and the drone jumps about 2.4 semitones with no ramp.
    // THAT is the "engine is about to let go" warning: it arrives ~200 rpm
    // before the 5,700 limiter, and lifting off drops back under 5,500 and
    // takes it away. It is a discrete state, not the top of a ramp, which is
    // why rescaling the ramp never produced it.
    //
    // Applied unconditionally here. The original picks this path over the plain
    // one on A4+0x4b08, which is set at race init (0x2442ac) and cleared at
    // teardown (0x23c5c0) and is only ever tst.b'd elsewhere - i.e. "a race is
    // live". The scene drive-in/out cutscenes take the plain path, and they
    // reach us through UpdateEnginePeriod with literal periods rather than
    // through here, so they are unaffected.
    //
    // This replaces a 17-entry period table that exists nowhere in the Amiga
    // binary, plus a velocity ladder that fed it. Header-inline so the SDL-free
    // unit tests can reach it.
    static constexpr int kOverRevRpm = 5500;

    static int RPMToPeriod(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r >= kOverRevRpm ? 580 : 640) - r / 32;
    }

    static float RPMToFrequencyMultiplier(int16_t rpm) {
        return (3579545.0f / static_cast<float>(RPMToPeriod(rpm))) / 16000.0f;
    }

    // Paula's own 0..64 volume: 11 at idle, 28 at the redline. The engine
    // getting louder as it revs is the other half of the cue, and we had none
    // of it - the drone played at one flat level whatever the rpm.
    static int RPMToPaulaVolume(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r + (r >= kOverRevRpm ? 19 : 14)) / 256 + 6;
    }

private:
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioStream* audioStream = nullptr;
    SDL_AudioSpec audioSpec;  // Obtained audio spec from device
    std::vector<int8_t> baseSample;  // Original engine sample (signed 8-bit)

    bool playing = false;
    int16_t currentRPM = 1500;
    float currentFreqMultiplier = 1.0f;   // left voice (updated by RPM or period)
    float currentFreqMultiplierR = 1.0f;  // right voice; equal unless a pair was set
    float currentVolume = 0.5f;
    // Rev-dependent gain, normalised so the redline (Paula 28) plays at the
    // volume the caller asked for and idle sits proportionally below it.
    float rpmVolumeScale = 1.0f;
    float samplePositionL = 0.0f;  // Left voice position
    float samplePositionR = 0.0f;  // Right voice position
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;

    // Audio callback (static wrapper)
    static void AudioCallback(void* userdata, Uint8* stream, int len);

    // Instance audio callback
    void FillAudioBuffer(Uint8* stream, int len);
};
