#include "core/log.h"
#include "audio/engine_sound.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

// Tuning knobs for how close the engine drone sits to the Amiga. None of these
// are confirmed against the binary - they are ear-matched, so expect to nudge
// them.
//
// kLowpassHz: stands in for Paula's output RC filter, whose fixed stage on an
// A500 is around 4.4 kHz.
//
// This sat at 700 Hz for a while, because 4.4 kHz was tried once and judged too
// bright. That test was confounded: at the time the race period was 258 instead
// of 594, so the drone was running about fifteen semitones sharp and the extra
// harmonics made an already-wrong pitch sound worse. With the period fixed, the
// 700 Hz corner is the problem instead - the engine tone is only 140 Hz at idle
// and 180 Hz at the redline, so a 700 Hz roof leaves about five harmonics and
// the rev change happens in a dull hum you have to strain to hear. Paula's real
// corner keeps roughly thirty, which is what makes the revs obvious.
//
// kStereoOffsetFraction: the engine loop runs on two Paula channels, one
// panned left and one right. Playing both from the same position is just mono;
// offsetting the right voice within the loop is what gives the drone width.
static constexpr float kLowpassHz = 4400.0f;
static constexpr float kStereoOffsetFraction = 0.25f;

// M_PI is POSIX, not standard C++, and MinGW/MSVC only expose it when
// _USE_MATH_DEFINES is defined before <cmath> - which breaks on whichever
// header pulls <cmath> in first.
static constexpr float kPi = 3.14159265358979323846f;

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
    // Both Paula voices run the same loop at the same period, so they hold
    // whatever offset they start with. A one-sample skew is inaudible - the two
    // sides stay near-identical and the drone collapses to mono - so start the
    // right voice a real fraction of the loop in.
    samplePositionR = static_cast<float>(baseSample.size()) * kStereoOffsetFraction;
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
    // Racing runs both player voices at one period (0x227c8c writes the same
    // value to channels 0 and 1); only the bay detunes them.
    currentFreqMultiplierR = currentFreqMultiplier;
    // Paula 11 at idle, 28 at the redline. Normalise against 28 rather than
    // Paula's full 0..64 so the loudest point stays at the caller's level and
    // only the ramp below it is added.
    constexpr float kRedlinePaulaVolume = 28.0f;
    const float v = static_cast<float>(RPMToPaulaVolume(rpm)) / kRedlinePaulaVolume;
    rpmVolumeScale = std::min(1.0f, v);
}

void EngineSound::UpdatePeriod(int16_t period) {
    if (period <= 0) return;

    // This used to add 6 to the period. The +6 in the original (0x227c8c,
    // `addq.w #$6, d1`) is on the VOLUME argument, not the period - the mute
    // flag at 0x22c30e zeroes that same argument, which is what identifies it.
    // The period argument gets `addi.w #$190` = +400 instead, and callers here
    // already pass final periods.
    float frequency = 3579545.0f / static_cast<float>(period);
    // Callers on this path (garage, diner, gas station) set an absolute level,
    // so clear any rev-dependent gain left over from a race - otherwise the
    // scene drone inherits the quiet end of the racing ramp.
    rpmVolumeScale = 1.0f;
    currentFreqMultiplierR = frequency / 16000.0f;
    currentFreqMultiplier = frequency / 16000.0f;
    // LOG_INFO("EngineSound::UpdatePeriod - period=%d, freq=%.1f Hz, mult=%.3f", period, frequency, currentFreqMultiplier);
}

void EngineSound::SetSample(const std::vector<int8_t>& sampleData) {
    if (sampleData.empty() || sampleData == baseSample) {
        return;
    }
    baseSample = sampleData;
    // Restart both voices inside the new sample rather than leaving positions
    // that may be past its end.
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

    // Output directly as 16-bit samples.
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    const int channels = audioSpec.channels > 0 ? audioSpec.channels : 1;
    const int numFrames = len / (static_cast<int>(sizeof(int16_t)) * channels);

    // Sample step accounts for:
    // - The pitch multiplier (set by UpdateRPM or UpdatePeriod)
    // - The ratio between sample rate (16000 Hz) and device rate (22050 Hz)
    // One step per voice: equal everywhere except the engine bay, which detunes
    // them so the two copies drift through phase and sweep a comb filter.
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

    // One-pole low-pass standing in for Paula's output filter. Expressed in Hz
    // so it stays put if the device rate changes; see kLowpassHz.
    const float lowpassAlpha = 1.0f - std::exp(-2.0f * kPi * kLowpassHz /
                                               static_cast<float>(audioSpec.freq));

    for (int frame = 0; frame < numFrames; frame++) {
        // Linear interpolation reduces aliasing compared to nearest-neighbor lookup.
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

        // Advance through sample with pitch shifting.
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

