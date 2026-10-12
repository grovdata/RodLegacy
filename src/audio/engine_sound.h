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

    // paulaVolume 0..64 on the race's scale (28 = full); 0 keeps full.
    void UpdatePeriod(int16_t period, int paulaVolume = 0);

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
    SDL_AudioDeviceID Device() const { return audioDevice; }

    // Racing drone: period = 640 - rpm/32, volume = (rpm+14)/256 + 6.
    // At 5500 rpm the base drops by 60 in one step, a ~2.4 semitone jump: the "about to let go" warning,
    // ~200 rpm before the 5700 limiter.
    static constexpr int kOverRevRpm = 5500;

    static int RPMToPeriod(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r >= kOverRevRpm ? 580 : 640) - r / 32;
    }

    // The exact inverse, for callers that only have a period but need to drive a back end that speaks rpm.
    static int PeriodToRPM(int period) {
        if (period <= 0) {
            return 0;
        }
        const int low = (640 - period) * 32;
        if (low >= kOverRevRpm) {
            return (580 - period) * 32;
        }
        return low < 0 ? 0 : low;
    }

    static float RPMToFrequencyMultiplier(int16_t rpm) {
        return (3579545.0f / static_cast<float>(RPMToPeriod(rpm))) / 16000.0f;
    }

    // Paula's own 0..64 volume: 11 at idle, 28 at the redline.
    static int RPMToPaulaVolume(int16_t rpm) {
        const int r = rpm > 0 ? static_cast<int>(rpm) : 0;
        return (r + (r >= kOverRevRpm ? 19 : 14)) / 256 + 6;
    }

    // The car animations (docs/audio.md "Engine sound in the scene screens"): one note per phase, the second phase
    // being the slow end of a drive-in or the speed-up of a drive-out. base = 600 + rand % 10 per animation.
    struct Note { int16_t period; int paulaVolume; };
    static Note AnimationNote(int base, bool drivingIn, bool secondPhase) {
        if (drivingIn) {
            return secondPhase ? Note{800, 18} : Note{static_cast<int16_t>(base + 6), 19};
        }
        return secondPhase ? Note{595, 26} : Note{static_cast<int16_t>(base - 3), 24};
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
