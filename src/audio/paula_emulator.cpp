#include "paula_emulator.h"
#include <cmath>
#include <algorithm>
#include <SDL2/SDL_log.h>

PaulaEmulator::PaulaEmulator() 
    : audioDevice(0), outputFrequency(44100), playing(false) {
}

PaulaEmulator::~PaulaEmulator() {
    Shutdown();
}

bool PaulaEmulator::Initialize(int outputFreq) {
    outputFrequency = outputFreq;
    
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = outputFrequency;
    desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    desired.channels = 2;           // Stereo (we'll do panning later if needed)
    desired.samples = 1024;
    desired.callback = AudioCallback;
    desired.userdata = this;
    
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpec, 0);
    if (audioDevice == 0) {
        SDL_Log("PaulaEmulator: Failed to open audio device: %s", SDL_GetError());
        return false;
    }
    
    SDL_Log("PaulaEmulator: Initialized at %d Hz, %d channels, %d samples buffer", 
            audioSpec.freq, audioSpec.channels, audioSpec.samples);
    
    return true;
}

void PaulaEmulator::Shutdown() {
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
}

void PaulaEmulator::SetChannelSample(int channel, const std::vector<int8_t>& data) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    
    SDL_LockAudioDevice(audioDevice);
    channels[channel].sampleData = data;
    channels[channel].samplePos = 0;
    SDL_UnlockAudioDevice(audioDevice);
}

void PaulaEmulator::SetChannelPeriod(int channel, uint16_t period) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    if (period == 0) period = 1;  // Prevent division by zero
    
    channels[channel].period = period;
}

void PaulaEmulator::SetChannelVolume(int channel, uint16_t volume) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    
    // Clamp to Amiga volume range (0-64)
    channels[channel].volume = std::min(volume, static_cast<uint16_t>(64));
}

void PaulaEmulator::SetChannelLoop(int channel, uint32_t loopStart, uint32_t loopLength) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    
    SDL_LockAudioDevice(audioDevice);
    channels[channel].loopStart = loopStart;
    channels[channel].loopLength = loopLength;
    channels[channel].loop = (loopLength > 0);
    SDL_UnlockAudioDevice(audioDevice);
}

void PaulaEmulator::EnableChannel(int channel, bool enable) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    
    channels[channel].enabled = enable;
}

void PaulaEmulator::RestartChannel(int channel) {
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    
    SDL_LockAudioDevice(audioDevice);
    channels[channel].samplePos = 0;
    SDL_UnlockAudioDevice(audioDevice);
}

bool PaulaEmulator::IsChannelPlaying(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS) return false;
    return channels[channel].enabled && !channels[channel].sampleData.empty();
}

const PaulaEmulator::AudioChannel& PaulaEmulator::GetChannel(int channel) const {
    static AudioChannel dummy;
    if (channel < 0 || channel >= NUM_CHANNELS) return dummy;
    return channels[channel];
}

void PaulaEmulator::Start() {
    if (audioDevice != 0) {
        SDL_PauseAudioDevice(audioDevice, 0);
        playing = true;
        SDL_Log("PaulaEmulator: Audio started");
    }
}

void PaulaEmulator::Stop() {
    if (audioDevice != 0) {
        SDL_PauseAudioDevice(audioDevice, 1);
        playing = false;
        SDL_Log("PaulaEmulator: Audio stopped");
    }
}

void PaulaEmulator::AudioCallback(void* userdata, Uint8* stream, int len) {
    PaulaEmulator* paula = static_cast<PaulaEmulator*>(userdata);
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    int numSamples = len / (2 * paula->audioSpec.channels);  // 16-bit stereo
    
    paula->MixAudio(output, numSamples);
}

uint32_t PaulaEmulator::PeriodToStep(uint16_t period) const {
    if (period == 0) period = 1;
    
    // Calculate actual playback frequency from Amiga period
    // freq = PAULA_BASE_FREQ / period
    double playbackFreq = static_cast<double>(PAULA_BASE_FREQ) / period;
    
    // Calculate how much to advance through the sample per output sample
    // step = playbackFreq / outputFreq (in fixed point 16.16)
    double step = playbackFreq / outputFrequency;
    
    return static_cast<uint32_t>(step * 65536.0);
}

void PaulaEmulator::MixAudio(int16_t* output, int numSamples) {
    // Clear output buffer
    for (int i = 0; i < numSamples * audioSpec.channels; i++) {
        output[i] = 0;
    }
    
    // Mix all enabled channels
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        AudioChannel& channel = channels[ch];
        
        if (!channel.enabled || channel.sampleData.empty()) {
            continue;
        }
        
        uint32_t step = PeriodToStep(channel.period);
        uint32_t sampleLen = channel.sampleData.size();
        
        for (int i = 0; i < numSamples; i++) {
            // Get current sample position (integer part of fixed point)
            uint32_t pos = channel.samplePos >> 16;
            
            // Check if we've reached the end
            if (pos >= sampleLen) {
                if (channel.loop && channel.loopLength > 0) {
                    // Loop back
                    uint32_t loopEnd = channel.loopStart + channel.loopLength;
                    if (loopEnd > sampleLen) loopEnd = sampleLen;
                    
                    // Wrap position within loop
                    pos = channel.loopStart + ((pos - channel.loopStart) % channel.loopLength);
                    channel.samplePos = pos << 16;
                } else {
                    // End of sample, disable channel
                    channel.enabled = false;
                    break;
                }
            }
            
            // Get 8-bit sample and convert to 16-bit
            int8_t sample8 = channel.sampleData[pos];
            int32_t sample16 = static_cast<int32_t>(sample8) * 256;
            
            // Apply volume (0-64 range, scale to 0-1)
            sample16 = (sample16 * channel.volume) / 64;
            
            // Mix to stereo output (mono for now, just duplicate to both channels)
            if (audioSpec.channels == 2) {
                output[i * 2 + 0] += static_cast<int16_t>(sample16 / NUM_CHANNELS);  // Left
                output[i * 2 + 1] += static_cast<int16_t>(sample16 / NUM_CHANNELS);  // Right
            } else {
                output[i] += static_cast<int16_t>(sample16 / NUM_CHANNELS);
            }
            
            // Advance sample position
            channel.samplePos += step;
        }
    }
}


