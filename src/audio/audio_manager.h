#pragma once

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

#include "pc_speaker.h"

// Forward declaration
class EngineSound;

// Audio sample types
enum class AudioSampleType {
    ENGINE_LOOP,        // Racing engine sound (srs0.wave offset 0x0000, 480 bytes)
    ENGINE_START,       // Engine startup sound (srl0.wave offset 0x0000, 2000 bytes)
    IGNITION,          // Startup sound (srl0.wave)
    TIRE_SCREECH,      // Tire skid sound (srl0.wave)
    HONK,              // Car horn (srl0.wave)
    CRASH,             // Collision sound (srl0.wave)
    COP_SIREN,         // Police siren (srl0.wave)
    CAR_BASH,          // Car bash/impact sound (srl0.wave)
    CAR_JACK,          // Car jack sound (srl0.wave)
    MUSIC_TONE,        // Music samples (srl1.wave)
    MUSIC_BASS,
    MUSIC_BASSDRUM,
    MUSIC_HIHAT,
    MUSIC_BUZZ
};

// IFF 8SVX chunk header
struct IFF8SVXHeader {
    uint32_t oneShotHiSamples;   // # samples in high octave 1-shot part
    uint32_t repeatHiSamples;    // # samples in high octave repeat part
    uint32_t samplesPerHiCycle;  // # samples/cycle in high octave, else 0
    uint16_t samplesPerSec;      // Data sampling rate
    uint8_t  ctOctave;           // # octaves of waveforms
    uint8_t  sCompression;       // Data compression technique used
    uint32_t volume;             // Playback volume from 0 to 0x10000
};

// Audio sample data
struct AudioSample {
    std::vector<int8_t> data;    // Signed 8-bit audio
    uint32_t sampleRate;
    uint32_t loopStart;          // Loop start position (samples)
    uint32_t loopLength;         // Loop length (samples), 0 = no loop
    bool isLooped;

    AudioSample() : sampleRate(22050), loopStart(0), loopLength(0), isLooped(false) {}
};

/**
 * Central audio management system for Street Rod
 * Handles loading audio from ADF files and playback of samples
 */
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Initialize audio system and load samples from ADF
    bool Initialize(const char* disk1AdfPath);

    // Shutdown audio system
    void Shutdown();

    // Load specific audio files from ADF
    bool LoadAudioFromADF(const char* adfPath, const char* filename);

    // Get a loaded sample by type
    AudioSample* GetSample(AudioSampleType type);

    // Play a one-shot sound effect
    void PlaySound(AudioSampleType type, float volume = 1.0f);

    // Start looping sound (returns channel ID)
    int StartLoop(AudioSampleType type, float volume = 1.0f);

    // Stop a looping sound
    void StopLoop(int channelId);

    // Update engine sound pitch based on RPM
    void UpdateEnginePitch(int16_t rpm);
    void UpdateEnginePeriod(int16_t period);  // For car animations using Amiga periods

    // Set volume for a specific channel
    void SetChannelVolume(int channelId, float volume);

    // Initialize PC speaker music from a DOS SR.EXE (optional; called after Initialize()).
    void InitPCSpeaker(const std::string& dosExePath);

    // Same, from an already-unpacked DOS MZ image in memory (e.g. after EXEPACK unpack).
    void InitPCSpeakerFromExeBytes(const uint8_t* data, size_t len);

    // SFX volume: 0-10, default 8. Affects engine sound and one-shot sounds.
    int  GetSfxVolume() const   { return sfxVolume_; }
    void SetSfxVolume(int v);

    // Music volume: 0-10, default 8. Affects PC speaker output amplitude.
    int  GetMusicVolume() const { return musicVolume_; }
    void SetMusicVolume(int v);

    bool HasPCSpeaker() const { return pcSpeaker_ != nullptr; }

    // Mute/unmute PC speaker music (the "Squelch it" toggle).
    void SetMusicMuted(bool muted);
    bool IsMusicMuted() const { return musicMuted_; }

    // Get engine sound for external management
    AudioSample* GetEngineSound() { return GetSample(AudioSampleType::ENGINE_LOOP); }

private:
    std::map<AudioSampleType, AudioSample> samples;
    SDL_AudioDeviceID audioDevice = 0;

    // PC speaker music (optional -- requires DOS SR.EXE)
    std::unique_ptr<PCSpeaker> pcSpeaker_;

    // Volume levels (0-10)
    int  sfxVolume_   = 8;
    int  musicVolume_ = 8;
    bool musicMuted_  = false;

    // Engine sound system (uses EngineSound class for real-time pitch modulation)
    std::unique_ptr<EngineSound> engineSound;
    int engineChannelId = -1;
    int16_t currentRPM = 1500;

    // Load raw audio file (unsigned 8-bit)
    bool LoadRawAudio(const uint8_t* data, size_t size, AudioSample& sample, uint32_t sampleRate = 22050);

    // Load IFF 8SVX format
    bool Load8SVX(const uint8_t* data, size_t size, AudioSample& sample);

    // Parse IFF chunks to find 8SVX data
    bool Parse8SVXChunks(const uint8_t* data, size_t size, AudioSample& sample);

    // Convert sample to SDL_AudioStream for playback
    SDL_AudioStream* CreateAudioStream(const AudioSample& sample, float pitchMultiplier = 1.0f);

    // Extract specific samples from srs0.wave (racing engine)
    void ExtractSRS0Samples(const uint8_t* data, size_t size);

    // Extract specific samples from srl0.wave
    void ExtractSRL0Samples(const uint8_t* data, size_t size);

    // Extract specific samples from srl1.wave
    void ExtractSRL1Samples(const uint8_t* data, size_t size);

    // Convert RPM to pitch multiplier
    float RPMToPitchMultiplier(int16_t rpm);
};
