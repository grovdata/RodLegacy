#pragma once

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

#include "audio/pc_speaker.h"

class EngineSound;
class AmigaMusic;

enum class AudioSampleType {
    ENGINE_LOOP,        // Racing engine sound (srl0.wave offset 0x1D8, 512 bytes)
    ENGINE_START,       // Engine startup sound (srl0.wave offset 0x0000, 2000 bytes)
    IGNITION,
    TIRE_SCREECH,
    HONK,
    CRASH,
    COP_SIREN,
    CAR_BASH,
    CAR_JACK,
    BAY_IGNITION,          // Engine bay engine sound (srl0.wave 0x408, 6912 bytes, looped)
    PART_MOUNT,         // Part mounted into engine, re-pitched per use (srl0.wave 0x5DC8, 800 bytes)
    CUTSCENE_HIT,       // Intro bangs, re-pitched per use (srl0.wave 0x3108, 1920 bytes)
    MUSIC_TONE,
    MUSIC_BASS,
    MUSIC_BASSDRUM,
    MUSIC_HIHAT,
    MUSIC_BUZZ
};

struct IFF8SVXHeader {
    uint32_t oneShotHiSamples;   // # samples in high octave 1-shot part
    uint32_t repeatHiSamples;    // # samples in high octave repeat part
    uint32_t samplesPerHiCycle;  // # samples/cycle in high octave, else 0
    uint16_t samplesPerSec;      // Data sampling rate
    uint8_t  ctOctave;           // # octaves of waveforms
    uint8_t  sCompression;       // Data compression technique used
    uint32_t volume;             // Playback volume from 0 to 0x10000
};

struct AudioSample {
    std::vector<int8_t> data;
    uint32_t sampleRate;
    uint32_t loopStart;
    uint32_t loopLength;         // Loop length (samples), 0 = no loop
    bool isLooped;

    AudioSample() : sampleRate(22050), loopStart(0), loopLength(0), isLooped(false) {}
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool Initialize(const uint8_t* disk1AdfData, size_t disk1AdfSize);

    void Shutdown();

    bool LoadAudioFromADF(const uint8_t* adfData, size_t adfSize, const char* filename);

    AudioSample* GetSample(AudioSampleType type);

    // sampleRateOverride replaces the sample's own rate, for effects the original re-pitches per shot.
    void PlaySound(AudioSampleType type, float volume = 1.0f, int sampleRateOverride = 0);

    int StartLoop(AudioSampleType type, float volume = 1.0f);

    void StopLoop(int channelId);

    void UpdateEnginePitch(int16_t rpm);
    void UpdateEnginePeriod(int16_t period);  // For car animations using Amiga periods

    void PlayBayIgnition();     // one-shot, re-pitched
    // tuneOffset is knob position minus perfect; 0 means no phasing.
    void StartEngineBayDrone(int tuneOffset);
    void SetEngineBayTune(int tuneOffset);
    void StopEngineBayDrone();

    void StartGasPumpLoop();
    void StopGasPumpLoop();

    // One Amiga instrument note at a Paula period, outside any song. SFX
    // volume. No-op without Amiga music data.
    void PlayInstrumentNote(int channel, int instrument, int period, int volume);

    // Starts again from a fresh random arrangement.
    void StopMusicPlayback();
    void RestartMusic() { SetMusicSource(musicSource_); }

    void SetChannelVolume(int channelId, float volume);

    enum class MusicSource { NONE, DOS, AMIGA };

    // Does not start playback; call SetMusicSource() afterwards.
    void InitAmigaMusic(const uint8_t* disk1AdfData, size_t disk1AdfSize);

    // start = false only selects it; RestartMusic starts it later.
    void SetMusicSource(MusicSource src, bool start = true);
    MusicSource GetMusicSource() const;

    bool HasAmigaMusic() const;

    // Arrangement step 3 at CIA period 42000. Amiga music only.
    void PlayVictoryMusic();

    void InitPCSpeaker(const std::string& dosExePath);

    void InitPCSpeakerFromExeBytes(const uint8_t* data, size_t len);

    // SFX volume: 0-10, default 8. Affects engine sound and one-shot sounds.
    int  GetSfxVolume() const   { return sfxVolume_; }
    void SetSfxVolume(int v);

    // Music volume: 0-10, default 8. Affects PC speaker output amplitude.
    int  GetMusicVolume() const { return musicVolume_; }
    void SetMusicVolume(int v);

    bool HasPCSpeaker() const { return pcSpeaker_ != nullptr; }

    void SetMusicMuted(bool muted);
    bool IsMusicMuted() const { return musicMuted_; }

    // Call once per frame. Advances to a different DOS song when the current one loops.
    void UpdateMusic();

    void NextPCSong();
    void PrevPCSong();
    void PausePCSpeaker();
    void ResumePCSpeaker();
    bool IsPCSpeakerPaused() const;
    int  GetCurrentPCSongIndex() const;
    int  GetPCSongCount() const;
    std::string GetCurrentPCSongName() const;

    bool HasActiveMusic() const;
    void NextSong();
    void PrevSong();
    void PauseMusic();
    void ResumeMusic();
    bool IsMusicPaused() const;
    std::string GetCurrentSongName() const;
    int  GetCurrentSongIndex() const;
    int  GetSongCount() const;

    // Resume brings back only what was playing when paused.
    void PauseAll();
    void ResumeAll();

    AudioSample* GetEngineSound() { return GetSample(AudioSampleType::ENGINE_LOOP); }

private:
    std::map<AudioSampleType, AudioSample> samples;
    SDL_AudioDeviceID audioDevice = 0;

    std::unique_ptr<AmigaMusic> amigaMusic_;
    MusicSource musicSource_ = MusicSource::NONE;

    std::unique_ptr<PCSpeaker> pcSpeaker_;
    std::vector<std::vector<SrNote>> pcSongs_;
    std::vector<std::string>         pcSongNames_;
    int                              currentPCSong_ = 0;

    int  sfxVolume_   = 8;
    int  musicVolume_ = 8;
    bool musicMuted_  = false;

    std::unique_ptr<EngineSound> engineSound;
    int engineChannelId = -1;
    int16_t currentRPM = 1500;

    bool allPaused_ = false;
    bool musicWasPlaying_ = false;
    bool engineWasPlaying_ = false;

    bool LoadRawAudio(const uint8_t* data, size_t size, AudioSample& sample, uint32_t sampleRate = 22050);

    bool Load8SVX(const uint8_t* data, size_t size, AudioSample& sample);

    bool Parse8SVXChunks(const uint8_t* data, size_t size, AudioSample& sample);

    SDL_AudioStream* CreateAudioStream(const AudioSample& sample, float pitchMultiplier = 1.0f);

    void ExtractSRL0Samples(const uint8_t* data, size_t size);

    void ExtractSRL1Samples(const uint8_t* data, size_t size);

    float RPMToPitchMultiplier(int16_t rpm);
};
