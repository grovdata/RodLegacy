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
        SDL_Log("EngineSound::Initialize - Empty sample data provided");
        return false;
    }
    
    baseSample = sampleData;
    SDL_Log("EngineSound::Initialize - Loaded %zu bytes of engine sound", baseSample.size());
    
    // Set up audio device with callback for real-time processing
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    desired.channels = 1;  // Mono
    desired.samples = 2048;
    desired.callback = AudioCallback;
    desired.userdata = this;
    
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audioDevice == 0) {
        SDL_Log("Failed to open audio device: %s", SDL_GetError());
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
        SDL_Log("Failed to create audio stream: %s", SDL_GetError());
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
        return false;
    }
    
    SDL_Log("Engine sound initialized: %zu bytes, device freq=%d, initial mult=%.3f", 
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
    
    SDL_Log("EngineSound::Start - freqMult=%.3f", currentFreqMultiplier);
    playing = true;
    samplePosition = 0;
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
    // Calculate frequency from Amiga period: freq = 3579545 / period
    // Then calculate multiplier relative to base sample rate (16000 Hz)
    float frequency = 3579545.0f / period;
    currentFreqMultiplier = frequency / 16000.0f;
    // SDL_Log("EngineSound::UpdatePeriod - period=%d, freq=%.1f Hz, mult=%.3f", period, frequency, currentFreqMultiplier);
}

void EngineSound::SetVolume(float volume) {
    currentVolume = std::max(0.0f, std::min(1.0f, volume));
}

float EngineSound::RPMToFrequencyMultiplier(int16_t rpm) {
    // Amiga period table for engine sound (17 levels, 0-16)
    // Calculated from 3579545 / period
    static const float freqs[17] = {14204.5f, 14375.7f, 14610.4f, 14852.9f, 15040.1f, 15297.2f, 15563.2f, 15768.9f, 
                                     16051.8f, 16345.0f, 16572.0f, 16884.6f, 17209.4f, 17461.2f, 17808.7f, 18170.3f, 18546.9f};
    
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
    
    // Return frequency multiplier relative to 16000 Hz base
    return freqs[level] / 16000.0f;
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
    
    // Simple approach: output directly as 16-bit samples
    // The device expects 16-bit at audioSpec.freq Hz
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    int numSamples = len / 2;  // 16-bit = 2 bytes per sample
    
    // Sample step accounts for:
    // - The pitch multiplier (set by UpdateRPM or UpdatePeriod)
    // - The ratio between sample rate (16000 Hz) and device rate (22050 Hz)
    float sampleStep = currentFreqMultiplier * (16000.0f / audioSpec.freq);
    
    for (int i = 0; i < numSamples; i++) {
        // Get sample from base (signed 8-bit)
        int8_t sample8 = baseSample[static_cast<size_t>(samplePosition) % baseSample.size()];
        
        // Convert to 16-bit and apply volume
        int16_t sample16 = static_cast<int16_t>(sample8) * 256;
        output[i] = static_cast<int16_t>(sample16 * currentVolume);
        
        // Advance through sample with pitch shifting
        samplePosition += sampleStep;
        if (samplePosition >= baseSample.size()) {
            samplePosition = std::fmod(samplePosition, static_cast<float>(baseSample.size()));
        }
    }
}

