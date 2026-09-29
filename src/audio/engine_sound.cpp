#include "core/log.h"
#include "audio/engine_sound.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

// Ear-matched, not from the binary.
// kLowpassHz: Paula's A500 output filter is around 4.4 kHz; a lower corner makes revs hard to hear.
// kStereoOffsetFraction: offsetting the right voice gives the two-channel drone its width.
static constexpr float kLowpassHz = 4400.0f;
static constexpr float kStereoOffsetFraction = 0.25f;

// M_PI is not standard C++.
static constexpr float kPi = 3.14159265358979323846f;

EngineSound::EngineSound() {
}

EngineSound::~EngineSound() {
    Shutdown();
}

bool EngineSound::Initialize(const std::vector<int8_t>& sampleData) {
    if (sampleData.empty()) {
        LOG_INFO("EngineSound::Initialize - Empty sample data provided");
        return false;
    }

    baseSample = sampleData;
    LOG_INFO("EngineSound::Initialize - Loaded %zu bytes of engine sound", baseSample.size());

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

    audioSpec = obtained;

    // A safe default so the callback cannot glitch before UpdateEnginePeriod.
    currentFreqMultiplier = 0.699f;  // 11186 Hz / 16000 Hz

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
    // Both voices run at one period and hold their start offset, so start the right one well into the loop.
    samplePositionR = static_cast<float>(baseSample.size()) * kStereoOffsetFraction;
    filterStateL = 0.0f;
    filterStateR = 0.0f;
    SDL_PauseAudioDevice(audioDevice, 0);
}

void EngineSound::Stop() {
    if (!playing) return;

    playing = false;
    if (audioDevice) {
        SDL_PauseAudioDevice(audioDevice, 1);
    }
}

void EngineSound::UpdateRPM(int16_t rpm) {
    currentRPM = rpm;
    currentFreqMultiplier = RPMToFrequencyMultiplier(rpm);
    // Racing runs both player voices at one period; only the bay detunes them.
    currentFreqMultiplierR = currentFreqMultiplier;
    // Normalised against the redline's 28 so the loudest point is the caller's level.
    constexpr float kRedlinePaulaVolume = 28.0f;
    const float v = static_cast<float>(RPMToPaulaVolume(rpm)) / kRedlinePaulaVolume;
    rpmVolumeScale = std::min(1.0f, v);
}

void EngineSound::UpdatePeriod(int16_t period) {
    if (period <= 0) return;

    // Callers pass final periods; the original's +6 is on the volume argument.
    float frequency = 3579545.0f / static_cast<float>(period);
    // Clear any rev gain left over from a race.
    rpmVolumeScale = 1.0f;
    currentFreqMultiplierR = frequency / 16000.0f;
    currentFreqMultiplier = frequency / 16000.0f;
}

void EngineSound::SetSample(const std::vector<int8_t>& sampleData) {
    if (sampleData.empty() || sampleData == baseSample) {
        return;
    }
    baseSample = sampleData;
    // Positions may be past the new sample's end.
    samplePositionL = 0.0f;
    samplePositionR = 0.0f;
}

void EngineSound::UpdatePeriodPair(int16_t leftPeriod, int16_t rightPeriod) {
    if (leftPeriod <= 0 || rightPeriod <= 0) return;
    currentFreqMultiplier  = (3579545.0f / static_cast<float>(leftPeriod)) / 16000.0f;
    currentFreqMultiplierR = (3579545.0f / static_cast<float>(rightPeriod)) / 16000.0f;
    // The bay runs at Paula 28, the same level the rev ramp treats as full.
    rpmVolumeScale = 1.0f;
}

void EngineSound::SetVolume(float volume) {
    currentVolume = std::max(0.0f, std::min(1.0f, volume));
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

    int16_t* output = reinterpret_cast<int16_t*>(stream);
    const int channels = audioSpec.channels > 0 ? audioSpec.channels : 1;
    const int numFrames = len / (static_cast<int>(sizeof(int16_t)) * channels);

    // Per voice: equal except in the engine bay, which detunes them into a phasing sweep.
    const float sampleStepL = currentFreqMultiplier * (16000.0f / audioSpec.freq);
    const float sampleStepR = currentFreqMultiplierR * (16000.0f / audioSpec.freq);

    const float sampleCount = static_cast<float>(baseSample.size());
    auto sampleAt = [&](float pos) -> float {
        const size_t idx0 = static_cast<size_t>(pos);
        const size_t idx1 = (idx0 + 1U) % baseSample.size();
        const float frac = pos - static_cast<float>(idx0);
        const float s0 = static_cast<float>(baseSample[idx0]);
        const float s1 = static_cast<float>(baseSample[idx1]);
        return s0 + (s1 - s0) * frac;
    };

    // Expressed in Hz so it holds if the device rate changes.
    const float lowpassAlpha = 1.0f - std::exp(-2.0f * kPi * kLowpassHz /
                                               static_cast<float>(audioSpec.freq));

    for (int frame = 0; frame < numFrames; frame++) {
        const float leftSample = sampleAt(samplePositionL);
        const float rightSample = sampleAt(samplePositionR);

        filterStateL += lowpassAlpha * (leftSample - filterStateL);
        filterStateR += lowpassAlpha * (rightSample - filterStateR);

        const float gain = 256.0f * currentVolume * rpmVolumeScale;
        const int16_t left16 = static_cast<int16_t>(filterStateL * gain);
        const int16_t right16 = static_cast<int16_t>(filterStateR * gain);

        if (channels >= 2) {
            output[frame * channels + 0] = left16;
            output[frame * channels + 1] = right16;
            for (int ch = 2; ch < channels; ch++) {
                output[frame * channels + ch] = left16;
            }
        } else {
            output[frame] = static_cast<int16_t>((left16 + right16) / 2);
        }

        samplePositionL += sampleStepL;
        samplePositionR += sampleStepR;
        if (samplePositionL >= sampleCount) {
            samplePositionL = std::fmod(samplePositionL, sampleCount);
        }
        if (samplePositionR >= sampleCount) {
            samplePositionR = std::fmod(samplePositionR, sampleCount);
        }
    }
}

