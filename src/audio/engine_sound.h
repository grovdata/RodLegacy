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

private:
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioStream* audioStream = nullptr;
    SDL_AudioSpec audioSpec;  // Obtained audio spec from device
    std::vector<int8_t> baseSample;  // Original engine sample (signed 8-bit)

    bool playing = false;
    int16_t currentRPM = 1500;
    float currentFreqMultiplier = 1.0f;  // Current frequency multiplier (updated by RPM or period)
    float currentVolume = 0.5f;
    float samplePosition = 0.0f;  // Current position in base sample for looping (float for smooth pitch)

    // Audio callback (static wrapper)
    static void AudioCallback(void* userdata, Uint8* stream, int len);

    // Instance audio callback
    void FillAudioBuffer(Uint8* stream, int len);

    // Convert RPM to playback frequency multiplier
    float RPMToFrequencyMultiplier(int16_t rpm);
};
