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
    // This replaces a 17-entry period table that exists nowhere in the Amiga
    // binary, plus a velocity ladder that fed it. Header-inline so the SDL-free
    // unit tests can reach it.
    static int RPMToPeriod(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return 640 - r / 32;
    }

    static float RPMToFrequencyMultiplier(int16_t rpm) {
        return (3579545.0f / static_cast<float>(RPMToPeriod(rpm))) / 16000.0f;
    }

    // Paula's own 0..64 volume: 11 at idle, 28 at the redline. The engine
    // getting louder as it revs is half of the over-rev cue, and we had none
    // of it - the drone played at one flat level whatever the rpm.
    static int RPMToPaulaVolume(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r + 14) / 256 + 6;
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
