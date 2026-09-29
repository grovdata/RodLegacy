/*
 * RodLegacy - Engine drone: EngineSound resamples the engine loop in real time from RPM or an Amiga period.
 *
 * Created: 2025-10-08
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <cstdint>

class EngineSound {
public:
    EngineSound();
    ~EngineSound();

    bool Initialize(const std::vector<int8_t>& sampleData);

    void Shutdown();

    void Start();
    void Stop();

    void UpdateRPM(int16_t rpm);

    void UpdatePeriod(int16_t period);

    // Two voices at different Paula periods, so they drift through phase. Accompanies part mounting, toggled
    // around one action; sustained it booms.
    // ponytail: unused until the part install/remove path wants it.
    void UpdatePeriodPair(int16_t leftPeriod, int16_t rightPeriod);

    // The racing drone and the engine bay use different slices of srl0.
    void SetSample(const std::vector<int8_t>& sampleData);

    static constexpr int16_t kPartMountPeriodLeft = 3550;
    static constexpr int16_t kPartMountPeriodRight = 3650;
    static constexpr int kPartMountPaulaVolume = 28;

    void SetVolume(float volume);

    bool IsPlaying() const { return playing; }

    // Racing drone: period = 640 - rpm/32, volume = (rpm+14)/256 + 6.
    // At 5500 rpm the base drops by 60 in one step, a ~2.4 semitone jump: the "about to let go" warning,
    // ~200 rpm before the 5700 limiter.
    static constexpr int kOverRevRpm = 5500;

    static int RPMToPeriod(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r >= kOverRevRpm ? 580 : 640) - r / 32;
    }

    static float RPMToFrequencyMultiplier(int16_t rpm) {
        return (3579545.0f / static_cast<float>(RPMToPeriod(rpm))) / 16000.0f;
    }

    // Paula's own 0..64 volume: 11 at idle, 28 at the redline.
    static int RPMToPaulaVolume(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r + (r >= kOverRevRpm ? 19 : 14)) / 256 + 6;
    }

private:
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioStream* audioStream = nullptr;
    SDL_AudioSpec audioSpec;
    std::vector<int8_t> baseSample;

    bool playing = false;
    int16_t currentRPM = 1500;
    float currentFreqMultiplier = 1.0f;
    float currentFreqMultiplierR = 1.0f;
    float currentVolume = 0.5f;
    // Normalised so the redline plays at the caller's volume.
    float rpmVolumeScale = 1.0f;
    float samplePositionL = 0.0f;  // Left voice position
    float samplePositionR = 0.0f;  // Right voice position
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;

    static void AudioCallback(void* userdata, Uint8* stream, int len);

    void FillAudioBuffer(Uint8* stream, int len);
};
