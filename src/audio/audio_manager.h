/*
 * RodLegacy - Audio front end: AudioManager loads the srl0/srl1 samples, plays effects and loops, and
 * switches music sources.
 *
 * Created: 2025-10-08
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

#include "audio/pc_speaker.h"
#include "audio/pc_speaker_fx.h"
#include "audio/sr2_mus.h"

class AdlibEngine;
class PcSpeakerSfx;
class AdlibSfx;
class AdlibMusic;
class C64Music;
class C64Sfx;
class C64Engine;
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
    HISS,               // srl0.wave 0x60F4, 0xC80 bytes: SR2 bay click, ratchet and slide hiss, re-pitched per use
    PART_MOUNT,         // Part mounted into engine, re-pitched per use (srl0.wave 0x5DC8, 800 bytes)
    CUTSCENE_HIT,       // Intro bangs, re-pitched per use (srl0.wave 0x3108, 1920 bytes)
    MUSIC_TONE,
    MUSIC_BASS,
    MUSIC_BASSDRUM,
    MUSIC_HIHAT,
    MUSIC_BUZZ,
    PAINT_SPRAY,        // no Amiga sample; the PC speaker's spray loop
    // The DOS games' tyre change, text-field and race-result sounds. The Amiga plays only the last.
    TIRE_AIR_WRENCH,
    TIRE_PUTT,
    TIRE_LUG_NUT,
    REJECT_BEEP,
    DRIVETRAIN_BLOWN,
    STICKER,            // the sticker wipe; Amiga only
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
    // The C64 off-road loop (me.o $8A12); other sources have no off-road sound.
    void SetOffRoad(bool offRoad);

    // Which engine sound plays.
    enum class EngineSource { AMIGA, ADLIB, PCSPEAKER, C64 };
    bool HasC64Engine() const { return !c64SfxPrg_.empty(); }
    void SetEngineSource(EngineSource src);
    EngineSource GetEngineSource() const { return engineSource_; }
    // Which one-shots, siren and squeal play: the Amiga samples or the DOS PC speaker's.
    enum class SfxSource { AMIGA, PCSPEAKER, ADLIB, C64 };
    // Street Rod 2's AdLib effects need its MSR.SND bank.
    bool HasAdlibSfx() const { return !adlibBank_.empty(); }
    bool HasC64Sfx() const { return !c64SfxPrg_.empty(); }
    // The PC speaker's engine follows the game being played.
    void SetSr2Game(bool sr2) { sr2Game_ = sr2; }
    // One sound through the given source, whatever the setting (the sound debug scene).
    void PlaySoundVia(SfxSource src, AudioSampleType type);
    void SetSfxSource(SfxSource src);
    SfxSource GetSfxSource() const { return sfxSource_; }
    // A car with no muffler runs the AdLib engine at full volume instead of the quiet-at-idle ramp.
    void SetMufflerRemoved(bool removed);
    void UpdateEnginePeriod(int16_t period, int paulaVolume = 0);  // For car animations using Amiga periods
    void UpdateEngineDistance(int px);        // The same animations for the DOS engines: pixels from the parked spot

    void PlayBayIgnition();     // one-shot, re-pitched
    // tuneOffset is knob position minus perfect; 0 means no phasing.
    void StartEngineBayDrone(int tuneOffset);
    void SetEngineBayTune(int tuneOffset);
    void StopEngineBayDrone();

    void StartTireRollLoop();
    void StopTireRollLoop();

    void StartGasPumpLoop();
    void StopGasPumpLoop();

    // One Amiga instrument note at a Paula period, outside any song. SFX
    // volume. No-op without Amiga music data.
    void PlayInstrumentNote(int channel, int instrument, int period, int volume);

    // The intro logo chime: instrument 6 on a detuned channel pair, volume 0x32.
    void PlayChime(int chA, int period, int chB);

    // Starts again from a fresh random arrangement.
    void StopMusicPlayback();
    void RestartMusic() { SetMusicSource(musicSource_); }

    void SetChannelVolume(int channelId, float volume);

    // Which music source is currently active.
    enum class MusicSource { NONE, DOS, AMIGA, ADLIB, C64 };

    // Does not start playback; call SetMusicSource() afterwards.
    void InitAmigaMusic(const uint8_t* disk1AdfData, size_t disk1AdfSize, bool sr2 = false);

    // start = false only selects it; RestartMusic starts it later.
    void SetMusicSource(MusicSource src, bool start = true);
    MusicSource GetMusicSource() const;

    bool HasAmigaMusic() const;

    // Street Rod 2's AdLib soundtrack: the MSR.SND instrument bank first, then one call per .MUS song.
    bool InitAdlibMusic(const uint8_t* sndData, size_t sndSize);
    bool AddAdlibSong(const std::string& name, const uint8_t* musData, size_t musSize);
    // MSR21, played when the King's road race is won rather than in rotation.
    bool SetAdlibVictorySong(const std::string& name, const uint8_t* musData, size_t musSize);
    bool HasAdlibMusic() const;

    // The C64 soundtrack, its driver PRGs read out of STREETR1.D64.
    bool InitC64Music(const std::vector<uint8_t>& d64);
    bool HasC64Music() const;

    void PlayVictoryMusic();

    void InitPCSpeaker(const std::string& dosExePath);

    void InitPCSpeakerFromExeBytes(const uint8_t* data, size_t len);
    // Street Rod 2's own speaker loops, which replace SR1's.
    void SetPcSpeakerLoops(std::vector<pcspk::LoopSong> loops);
    // Street Rod 2's own speaker tune, which replaces SR1's songs.
    void SetPcSpeakerSongs(std::vector<std::vector<SrNote>> songs, std::vector<std::string> names);

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
    // SR2 AdLib music (optional -- requires MSR.SND and the .MUS files)
    std::unique_ptr<AdlibMusic> adlibMusic_;
    std::unique_ptr<C64Music> c64Music_;
    MusicSource musicSource_ = MusicSource::NONE;

    std::unique_ptr<PCSpeaker> pcSpeaker_;
    void StartPcSpeakerMusic();
    std::vector<std::vector<SrNote>> pcSongs_;
    std::vector<std::string>         pcSongNames_;
    int                              currentPCSong_ = 0;

    int  sfxVolume_   = 8;
    int  musicVolume_ = 8;
    bool musicMuted_  = false;

    std::unique_ptr<EngineSound> engineSound;
    std::unique_ptr<AdlibEngine> adlibEngine;
    EngineSource engineSource_ = EngineSource::AMIGA;
    SfxSource sfxSource_ = SfxSource::AMIGA;
    bool sr2Game_ = false;
    std::unique_ptr<PcSpeakerSfx> pcSpeakerSfx_;
    PcSpeakerSfx* Speaker();
    bool PlayOnSpeaker(AudioSampleType type, double seconds);
    std::vector<sr2mus::Patch> adlibBank_;
    std::unique_ptr<AdlibSfx> adlibSfx_;
    AdlibSfx* AdlibEffects();
    bool PlayOnAdlib(AudioSampleType type, double seconds);
    std::vector<uint8_t> c64SfxPrg_;   // seqgar1, whose bank holds the effects
    std::unique_ptr<C64Sfx> c64Sfx_;
    std::unique_ptr<C64Engine> c64Engine_;
    bool c64ScenePending_ = false;   // a scene loop started; its first distance says driving in or out
    bool c64Scene_ = false;          // the drive in / out effect runs to its own end
    bool c64OffRoad_ = false;        // SFX 18 looping; it holds the effect voice over the siren
    C64Sfx* C64Effects();
    void PlayOnC64(AudioSampleType type, double seconds);
    std::vector<pcspk::LoopSong> pcLoops_;
    // The Amiga engine sample re-pitched as another machine (gas pump, tyre roll), whatever the engine source.
    void StartSampleLoop(int16_t left, int16_t right, float volume);
    // The scene tone currently held, or 0 while the rpm ladder owns the voice.
    int16_t lastEnginePeriod_ = 0;
    int lastEngineVolume_ = 0;
    int sceneDistance_ = 0;
    bool mufflerRemoved_ = false;
    // Whether a loop is wanted at all, so switching source mid-race hands the
    // running state over instead of going silent.
    bool engineLoopWanted_ = false;
    int engineChannelId = -1;
    int16_t currentRPM = 1500;

    bool allPaused_ = false;
    bool musicWasPlaying_ = false;
    std::vector<SDL_AudioDeviceID> pausedDevices_;

    bool LoadRawAudio(const uint8_t* data, size_t size, AudioSample& sample, uint32_t sampleRate = 22050);

    bool Load8SVX(const uint8_t* data, size_t size, AudioSample& sample);

    bool Parse8SVXChunks(const uint8_t* data, size_t size, AudioSample& sample);

    SDL_AudioStream* CreateAudioStream(const AudioSample& sample, float pitchMultiplier = 1.0f);

    void ExtractSRL0Samples(const uint8_t* data, size_t size);

    void ExtractSRL1Samples(const uint8_t* data, size_t size);

    float RPMToPitchMultiplier(int16_t rpm);
};
