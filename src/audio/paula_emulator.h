#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <cstdint>
#include <array>

/**
 * Paula chip emulator for Amiga-style 4-channel audio
 * 
 * Emulates the Amiga Paula chip's core audio features:
 * - 4 independent 8-bit PCM channels
 * - Period-based pitch control (Amiga hardware periods)
 * - Per-channel volume control (0-64)
 * - Sample looping with configurable loop points
 * - Hardware-style DMA playback
 */
class PaulaEmulator {
public:
    static constexpr int NUM_CHANNELS = 4;
    static constexpr int PAULA_BASE_FREQ = 3546895; // PAL Amiga clock frequency
    
    struct AudioChannel {
        std::vector<int8_t> sampleData;  // 8-bit signed PCM data
        uint32_t samplePos;              // Current position in sample (fixed point 16.16)
        uint32_t period;                 // Amiga period value (affects pitch)
        uint16_t volume;                 // 0-64 volume (Amiga range)
        uint32_t loopStart;              // Loop start position (samples)
        uint32_t loopLength;             // Loop length (samples), 0 = no loop
        bool enabled;                    // Channel enabled flag
        bool loop;                       // Loop enabled flag
        
        AudioChannel() 
            : samplePos(0), period(320), volume(64), 
              loopStart(0), loopLength(0), enabled(false), loop(false) {}
    };
    
    PaulaEmulator();
    ~PaulaEmulator();
    
    // Initialize Paula emulator with SDL audio
    bool Initialize(int outputFreq = 44100);
    
    // Shutdown audio system
    void Shutdown();
    
    // Channel control
    void SetChannelSample(int channel, const std::vector<int8_t>& data);
    void SetChannelPeriod(int channel, uint16_t period);
    void SetChannelVolume(int channel, uint16_t volume);  // 0-64
    void SetChannelLoop(int channel, uint32_t loopStart, uint32_t loopLength);
    void EnableChannel(int channel, bool enable);
    void RestartChannel(int channel);  // Restart from beginning
    
    // Get channel info
    bool IsChannelPlaying(int channel) const;
    const AudioChannel& GetChannel(int channel) const;
    
    // Start/stop audio processing
    void Start();
    void Stop();
    
private:
    std::array<AudioChannel, NUM_CHANNELS> channels;
    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;
    int outputFrequency;
    bool playing;
    
    // Audio callback (static wrapper)
    static void AudioCallback(void* userdata, Uint8* stream, int len);
    
    // Instance audio callback - mix all channels
    void MixAudio(int16_t* output, int numSamples);
    
    // Convert Amiga period to sample step (fixed point 16.16)
    uint32_t PeriodToStep(uint16_t period) const;
};

