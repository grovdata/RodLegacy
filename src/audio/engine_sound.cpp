#include "../log.h"
#include "engine_sound.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

EngineSound::EngineSound() {
}

EngineSound::~EngineSound() {
    Shutdown();
}

bool EngineSound::Initialize(const std::vector<int8_t>& sampleData) {
    // Store the provided sample data
    if (sampleData.empty()) {
        LOG_INFO("EngineSound::Initialize - Empty sample data provided");
        return false;
    }

    baseSample = sampleData;
    LOG_INFO("EngineSound::Initialize - Loaded %zu bytes of engine sound", baseSample.size());

    // Set up audio device with callback for real-time processing
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    desired.channels = 2;  // Stereo (Amiga uses paired Paula channels)
    desired.samples = 2048;
    desired.callback = AudioCallback;
    desired.userdata = this;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audioDevice == 0) {
        LOG_INFO("Failed to open audio device: %s", SDL_GetError());
        return false;
    }

    // Store the obtained audio spec for later use
    audioSpec = obtained;

    // Initialize to a safe default frequency (period 320 = 11186 Hz)
    // This prevents high-pitched glitch if callback runs before UpdateEnginePeriod is called
    currentFreqMultiplier = 0.699f;  // 11186 Hz / 16000 Hz

    // Create audio stream for resampling (pitch shifting)
    audioStream = SDL_NewAudioStream(
        AUDIO_S8, 1, 16000,           // Source: 8-bit signed mono at 16000Hz (our sample rate)
        obtained.format, obtained.channels, obtained.freq  // Dest: device format
    );

    if (!audioStream) {
        LOG_INFO("Failed to create audio stream: %s", SDL_GetError());
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
        return false;
    }

    LOG_INFO("Engine sound initialized: %zu bytes, device freq=%d, initial mult=%.3f",
            baseSample.size(), obtained.freq, currentFreqMultiplier);
    return true;
}

void EngineSound::Shutdown() {
    Stop();
    if (audioStream) {
        SDL_FreeAudioStream(audioStream);
        audioStream = nullptr;
    }
    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
}

void EngineSound::Start() {
    if (playing || audioDevice == 0) return;

    LOG_INFO("EngineSound::Start - freqMult=%.3f", currentFreqMultiplier);
    playing = true;
    samplePositionL = 0.0f;
    // Tiny initial skew between channels better matches sequential channel trigger timing.
    samplePositionR = baseSample.size() > 1 ? 1.0f : 0.0f;
    filterStateL = 0.0f;
    filterStateR = 0.0f;
    SDL_PauseAudioDevice(audioDevice, 0);  // Unpause
}

void EngineSound::Stop() {
    if (!playing) return;

    playing = false;
    if (audioDevice) {
        SDL_PauseAudioDevice(audioDevice, 1);  // Pause
    }
}

void EngineSound::UpdateRPM(int16_t rpm) {
    currentRPM = rpm;
    currentFreqMultiplier = RPMToFrequencyMultiplier(rpm);
}

void EngineSound::UpdatePeriod(int16_t period) {
    if (period <= 0) return;

    // Decompiled Amiga engine path applies a small positive offset here.
    const int16_t amigaAdjustedPeriod = static_cast<int16_t>(period + 6);

    // Calculate frequency from Amiga period: freq = 3579545 / period
    // Then calculate multiplier relative to base sample rate (16000 Hz)
    float frequency = 3579545.0f / static_cast<float>(amigaAdjustedPeriod);
    currentFreqMultiplier = frequency / 16000.0f;
    // LOG_INFO("EngineSound::UpdatePeriod - period=%d, freq=%.1f Hz, mult=%.3f", period, frequency, currentFreqMultiplier);
}

void EngineSound::SetVolume(float volume) {
    currentVolume = std::max(0.0f, std::min(1.0f, volume));
}

float EngineSound::RPMToFrequencyMultiplier(int16_t rpm) {
    // Amiga engine levels mapped to Paula periods (idle->redline).
    static const int periods[17] = {
        252, 249, 245, 241, 238, 234, 230, 227, 223,
        219, 216, 212, 208, 205, 201, 197, 193
    };

    // Convert game RPM (1500-7000) to Amiga velocity (0-130)
    // Linear mapping
    const float MIN_RPM = 1500.0f;
    const float MAX_RPM = 7000.0f;
    float clampedRPM = std::max(MIN_RPM, std::min(MAX_RPM, static_cast<float>(rpm)));
    int velocity = static_cast<int>((clampedRPM - MIN_RPM) / (MAX_RPM - MIN_RPM) * 130.0f);

    // Map velocity to audio level (0-16) using Amiga's thresholds from LAB_1392
    int level = 0;
    if (velocity > 130) level = 16;
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

    // Match the decompiled path that applies a +6 period bias.
    const int adjustedPeriod = periods[level] + 6;
    const float frequency = 3579545.0f / static_cast<float>(adjustedPeriod);
    return frequency / 16000.0f;
}

void EngineSound::AudioCallback(void* userdata, Uint8* stream, int len) {
    EngineSound* engine = static_cast<EngineSound*>(userdata);
    engine->FillAudioBuffer(stream, len);
}

void EngineSound::FillAudioBuffer(Uint8* stream, int len) {
    if (!playing || baseSample.empty()) {
        SDL_memset(stream, 0, len);
        return;
    }

    // Output directly as 16-bit samples.
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    const int channels = audioSpec.channels > 0 ? audioSpec.channels : 1;
    const int numFrames = len / (static_cast<int>(sizeof(int16_t)) * channels);

    // Sample step accounts for:
    // - The pitch multiplier (set by UpdateRPM or UpdatePeriod)
    // - The ratio between sample rate (16000 Hz) and device rate (22050 Hz)
    float sampleStep = currentFreqMultiplier * (16000.0f / audioSpec.freq);

    const float sampleCount = static_cast<float>(baseSample.size());
    auto sampleAt = [&](float pos) -> float {
        const size_t idx0 = static_cast<size_t>(pos);
        const size_t idx1 = (idx0 + 1U) % baseSample.size();
        const float frac = pos - static_cast<float>(idx0);
        const float s0 = static_cast<float>(baseSample[idx0]);
        const float s1 = static_cast<float>(baseSample[idx1]);
        return s0 + (s1 - s0) * frac;
    };

    // Mild low-pass to better match Paula's less-bright output.
    const float lowpassAlpha = 0.18f;

    for (int frame = 0; frame < numFrames; frame++) {
        // Linear interpolation reduces aliasing compared to nearest-neighbor lookup.
        const float leftSample = sampleAt(samplePositionL);
        const float rightSample = sampleAt(samplePositionR);

        filterStateL += lowpassAlpha * (leftSample - filterStateL);
        filterStateR += lowpassAlpha * (rightSample - filterStateR);

        const int16_t left16 = static_cast<int16_t>(filterStateL * 256.0f * currentVolume);
        const int16_t right16 = static_cast<int16_t>(filterStateR * 256.0f * currentVolume);

        if (channels >= 2) {
            output[frame * channels + 0] = left16;
            output[frame * channels + 1] = right16;
            for (int ch = 2; ch < channels; ch++) {
                output[frame * channels + ch] = left16;
            }
        } else {
            output[frame] = static_cast<int16_t>((left16 + right16) / 2);
        }

        // Advance through sample with pitch shifting.
        samplePositionL += sampleStep;
        samplePositionR += sampleStep;
        if (samplePositionL >= sampleCount) {
            samplePositionL = std::fmod(samplePositionL, sampleCount);
        }
        if (samplePositionR >= sampleCount) {
            samplePositionR = std::fmod(samplePositionR, sampleCount);
        }
    }
}

