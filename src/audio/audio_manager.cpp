#include "core/log.h"
#include "audio/audio_manager.h"
#include "audio/record_tap.h"

#include "audio/adlib_engine.h"
#include "audio/adlib_music.h"
#include "audio/c64_engine.h"
#include "audio/c64_music.h"
#include "audio/amiga_music.h"
#include "audio/engine_sound.h"
#include "audio/pc_speaker.h"
#include "audio/pc_speaker_sfx.h"
#include "audio/adlib_sfx.h"
#include "parsers/adf.h"
#include "parsers/libp.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <random>
#include <set>

static uint32_t ReadBE32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint16_t ReadBE16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize(const uint8_t* disk1AdfData, size_t disk1AdfSize) {
    LOG_INFO("AudioManager: Initializing...");

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            LOG_INFO("Failed to initialize SDL audio: %s", SDL_GetError());
            return false;
        }
    }

    if (disk1AdfData && disk1AdfSize > 0) {
        if (!LoadAudioFromADF(disk1AdfData, disk1AdfSize, "srl0.wave")) {
            LOG_INFO("Warning: Failed to load srl0.wave (engine loop and effects)");
        }
        if (!LoadAudioFromADF(disk1AdfData, disk1AdfSize, "srl1.wave")) {
            LOG_INFO("Warning: Failed to load srl1.wave (music samples)");
        }
    }

    engineSound = std::make_unique<EngineSound>();
    AudioSample* engineSample = GetSample(AudioSampleType::ENGINE_LOOP);
    if (engineSample && !engineSample->data.empty()) {
        if (!engineSound->Initialize(engineSample->data)) {
            LOG_INFO("Warning: Failed to initialize engine sound");
            engineSound.reset();
        } else {
            LOG_INFO("Engine sound initialized successfully from ADF data");
        }
    } else {
        LOG_INFO("Warning: No engine sample data found in ADF");
        engineSound.reset();
    }

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16SYS;  // 16-bit signed
    desired.channels = 2;  // Stereo
    desired.samples = 4096;
    desired.callback = nullptr;  // Use queue mode for one-shots

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audioDevice == 0) {
        LOG_INFO("Warning: Failed to open audio device for one-shots: %s", SDL_GetError());
    } else {
        LOG_INFO("AudioManager: One-shot audio device opened successfully");
        SDL_PauseAudioDevice(audioDevice, 0);
    }

    LOG_INFO("AudioManager: Loaded %zu audio samples", samples.size());
    return true;  // True even without samples: the engine sound may still work.
}

void AudioManager::Shutdown() {
    if (pcSpeaker_) {
        pcSpeaker_->Stop();
        pcSpeaker_->Shutdown();
        pcSpeaker_.reset();
    }

    if (amigaMusic_) {
        amigaMusic_->Stop();
        amigaMusic_->Shutdown();
        amigaMusic_.reset();
    }

    if (adlibMusic_) {
        adlibMusic_->Stop();
        adlibMusic_->Shutdown();
        adlibMusic_.reset();
    }

    if (c64Music_) {
        c64Music_->Stop();
        c64Music_->Shutdown();
        c64Music_.reset();
    }

    if (engineSound) {
        engineSound->Shutdown();
        engineSound.reset();
    }

    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }

    samples.clear();
}

bool AudioManager::HasAmigaMusic() const { return amigaMusic_ != nullptr; }

bool AudioManager::HasAdlibMusic() const {
    return adlibMusic_ && adlibMusic_->SongCount() > 0;
}

bool AudioManager::HasC64Music() const {
    return c64Music_ && c64Music_->SongCount() > 0;
}

bool AudioManager::InitC64Music(const std::vector<uint8_t>& d64) {
    auto music = std::make_unique<C64Music>();
    struct Prg { const char* file; const char* label; };
    static const Prg kSongs[] = {{"seqbeg", "Intro"}, {"seqgar1", "Song 1"}, {"seqgar2", "Song 2"}};
    for (const Prg& song : kSongs) {
        if (!music->AddSong(song.label, c64::D64ExtractPrg(d64.data(), d64.size(), song.file)))
            LOG_INFO("C64Music: %s missing or not recognized, skipped", song.file);
    }
    if (!music->SetVictorySong("Victory", c64::D64ExtractPrg(d64.data(), d64.size(), "wseq.o")))
        LOG_INFO("C64Music: wseq.o missing or not recognized, no win piece");
    if (music->SongCount() == 0 || !music->Init()) {
        LOG_INFO("C64Music: no C64 music");
        return false;
    }
    c64SfxPrg_ = c64::D64ExtractPrg(d64.data(), d64.size(), "seqgar1");
    music->SetVolume(0.0f);   // volume applied when SetMusicSource selects C64
    c64Music_ = std::move(music);
    LOG_INFO("C64Music: %d songs loaded", c64Music_->SongCount());
    return true;
}

bool AudioManager::InitAdlibMusic(const uint8_t* sndData, size_t sndSize) {
    if (!sndData || sndSize == 0) return false;
    auto music = std::make_unique<AdlibMusic>();
    if (!music->LoadBank(sndData, sndSize)) {
        LOG_INFO("AdlibMusic: MSR.SND not recognized, no AdLib music");
        return false;
    }
    if (!music->Init()) {
        LOG_INFO("AdlibMusic: audio init failed");
        return false;
    }
    music->SetVolume(0.0f);   // volume applied when SetMusicSource selects ADLIB
    adlibMusic_ = std::move(music);
    sr2mus::LoadPatches(sndData, sndSize, adlibBank_);
    return true;
}

bool AudioManager::AddAdlibSong(const std::string& name, const uint8_t* musData, size_t musSize) {
    if (!adlibMusic_ || !musData || musSize == 0) return false;
    if (!adlibMusic_->AddSong(name, musData, musSize)) {
        LOG_INFO("AdlibMusic: %s did not parse, skipped", name.c_str());
        return false;
    }
    LOG_INFO("AdlibMusic: loaded %s", name.c_str());
    return true;
}

bool AudioManager::SetAdlibVictorySong(const std::string& name, const uint8_t* musData,
                                      size_t musSize) {
    if (!adlibMusic_ || !musData || musSize == 0) return false;
    if (!adlibMusic_->SetVictorySong(name, musData, musSize)) {
        LOG_INFO("AdlibMusic: victory song %s did not parse, skipped", name.c_str());
        return false;
    }
    LOG_INFO("AdlibMusic: loaded %s as the win piece", name.c_str());
    return true;
}

void AudioManager::PlayVictoryMusic() {
    victoryPlaying_ = true;
    // On the AdLib source the win piece is MSR21, so the source does not change.
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_ && adlibMusic_->HasVictorySong()) {
        adlibMusic_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
        adlibMusic_->PlayVictorySong();
        return;
    }
    if (musicSource_ == MusicSource::C64 && c64Music_ && c64Music_->HasVictorySong()) {
        c64Music_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
        c64Music_->PlayVictorySong();
        return;
    }
    if (!amigaMusic_) return;
    musicSource_ = MusicSource::AMIGA;
    if (pcSpeaker_) pcSpeaker_->Stop();
    if (adlibMusic_) adlibMusic_->Stop();
    if (c64Music_) c64Music_->Stop();
    amigaMusic_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
    // Instrument and note-volume presets for the victory piece.
    static const int kVictoryInstruments[4] = {0, 5, 1, 2};
    static const int kVictoryVolumes[4] = {64, 22, 64, 64};
    amigaMusic_->PlayArrangement(3, 42000, kVictoryInstruments, kVictoryVolumes);
}

void AudioManager::InitAmigaMusic(const uint8_t* disk1AdfData, size_t disk1AdfSize, bool sr2) {
    LOG_INFO("AmigaMusic: init from %zu byte ADF buffer", disk1AdfSize);

    static const char* NOTE_NAMES[]  = { "msr.note", nullptr };
    static const char* WAVE1_NAMES[] = { "srl1.wave", nullptr };
    static const char* WAVE0_NAMES[] = { "srl0.wave", nullptr };
    static const char* INS_NAMES[]   = { "srl.ins", nullptr };
    static const char* ENV_NAMES[]   = { "srl.env", nullptr };

    auto tryLoad = [&](const char** names) -> AdfFileData* {
        for (int i = 0; names[i]; i++) {
            AdfFileData* d = extract_adf_file_from_buffer(disk1AdfData, disk1AdfSize, names[i]);
            if (d) {
                LOG_INFO("AmigaMusic: loaded %s (%zu bytes)", names[i], d->size);
                return d;
            }
            LOG_INFO("AmigaMusic: %s not found in ADF", names[i]);
        }
        return nullptr;
    };

    AdfFileData* noteFile  = tryLoad(NOTE_NAMES);
    AdfFileData* wave1File = tryLoad(WAVE1_NAMES);
    AdfFileData* wave0File = tryLoad(WAVE0_NAMES);
    AdfFileData* insFile   = tryLoad(INS_NAMES);
    AdfFileData* envFile   = tryLoad(ENV_NAMES);

    if (!noteFile || !wave1File || !wave0File || !insFile || !envFile) {
        LOG_INFO("AmigaMusic: required files missing (note=%d wave1=%d wave0=%d ins=%d env=%d)",
                 noteFile != nullptr, wave1File != nullptr, wave0File != nullptr,
                 insFile != nullptr, envFile != nullptr);
        if (noteFile) free_adf_file_data(noteFile);
        if (wave1File) free_adf_file_data(wave1File);
        if (wave0File) free_adf_file_data(wave0File);
        if (insFile) free_adf_file_data(insFile);
        if (envFile) free_adf_file_data(envFile);
        return;
    }

    amigaMusic_ = std::make_unique<AmigaMusic>();
    bool ok = amigaMusic_->Init() && amigaMusic_->Load(
        noteFile->data, noteFile->size,
        wave1File->data, wave1File->size,
        wave0File->data, wave0File->size,
        insFile->data, insFile->size,
        envFile->data, envFile->size, sr2);

    free_adf_file_data(noteFile);
    free_adf_file_data(wave1File);
    free_adf_file_data(wave0File);
    free_adf_file_data(insFile);
    free_adf_file_data(envFile);

    if (!ok) {
        LOG_INFO("AmigaMusic: Load() failed, no Amiga music");
        amigaMusic_.reset();
        return;
    }

    LOG_INFO("AmigaMusic: initialized successfully");
}

AudioManager::MusicSource AudioManager::GetMusicSource() const {
    return musicSource_;
}

void AudioManager::SetMusicSource(MusicSource src, bool start) {
    victoryPlaying_ = false;
    if (pcSpeaker_) pcSpeaker_->Stop();
    if (amigaMusic_) amigaMusic_->Stop();
    if (adlibMusic_) adlibMusic_->Stop();
    if (c64Music_) c64Music_->Stop();

    musicSource_ = src;
    if (!start) return;

    // Muting is a gain control; returning early here once left music never started.
    switch (src) {
        case MusicSource::DOS:
            if (pcSpeaker_) {
                pcSpeaker_->SetAmplitude(musicMuted_ ? 0 : (int16_t)(musicVolume_ * 800));
                pcSpeaker_->Play();
            }
            break;
        case MusicSource::AMIGA:
            if (amigaMusic_) {
                amigaMusic_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
                amigaMusic_->Play();
            }
            break;
        case MusicSource::ADLIB:
            if (HasAdlibMusic()) {
                adlibMusic_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
                adlibMusic_->Play();
            }
            break;
        case MusicSource::C64:
            if (HasC64Music()) {
                c64Music_->SetVolume(musicMuted_ ? 0.0f : musicVolume_ / 10.0f);
                c64Music_->Play();
            }
            break;
        case MusicSource::NONE:
            break;
    }
}

bool AudioManager::LoadAudioFromADF(const uint8_t* adfData, size_t adfSize, const char* filename) {
    LOG_INFO("AudioManager: Loading %s from ADF buffer", filename);

    AdfFileData* fileData = extract_adf_file_from_buffer(adfData, adfSize, filename);
    if (!fileData) {
        LOG_INFO("AudioManager: Failed to extract %s from ADF", filename);
        return false;
    }

    bool success = false;

    if (strcmp(filename, "srl0.wave") == 0) {
        ExtractSRL0Samples(fileData->data, fileData->size);
        success = true;
    }
    else if (strcmp(filename, "srl1.wave") == 0) {
        ExtractSRL1Samples(fileData->data, fileData->size);
        success = true;
    }
    else if (fileData->size > 8 && memcmp(fileData->data, "FORM", 4) == 0) {
        AudioSample sample;
        if (Load8SVX(fileData->data, fileData->size, sample)) {
            samples[AudioSampleType::MUSIC_TONE] = sample;
            success = true;
        }
    }

    free_adf_file_data(fileData);
    return success;
}

void AudioManager::ExtractSRL0Samples(const uint8_t* data, size_t size) {
    LOG_INFO("AudioManager: Extracting samples from srl0.wave (%zu bytes)", size);

    // Looped and pitch-modulated per tick. Signed 8-bit.
    if (size >= size_t{SRL0_ENGINE_LOOP_OFFSET} + SRL0_ENGINE_LOOP_LENGTH) {
        AudioSample engineSample;
        LoadRawAudio(data + SRL0_ENGINE_LOOP_OFFSET, SRL0_ENGINE_LOOP_LENGTH,
                     engineSample, SRL0_ENGINE_BASE_RATE);
        engineSample.isLooped = true;
        engineSample.loopStart = 0;
        engineSample.loopLength = SRL0_ENGINE_LOOP_LENGTH;
        samples[AudioSampleType::ENGINE_LOOP] = engineSample;
        LOG_INFO("  - Racing engine loop: %u bytes (looped) from offset 0x%X",
                SRL0_ENGINE_LOOP_LENGTH, SRL0_ENGINE_LOOP_OFFSET);
    }

    // Period 320 = 11186 Hz. Signed 8-bit.
    if (size >= SRL0_ENGINE_START_LENGTH) {
        AudioSample engineStartSample;
        LoadRawAudio(data + SRL0_ENGINE_START_OFFSET, SRL0_ENGINE_START_LENGTH,
                     engineStartSample, SRL0_ENGINE_START_RATE);
        engineStartSample.isLooped = false;
        engineStartSample.loopStart = 0;
        engineStartSample.loopLength = 0;
        samples[AudioSampleType::ENGINE_START] = engineStartSample;
        LOG_INFO("  - Engine starting sound: %u bytes (one-shot) at %u Hz",
                SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    }

    if (size >= SRL0_CAR_BASH_OFFSET + SRL0_CAR_BASH_LENGTH) {
        AudioSample carBashSample;
        LoadRawAudio(data + SRL0_CAR_BASH_OFFSET, SRL0_CAR_BASH_LENGTH,
                     carBashSample, SRL0_CAR_BASH_RATE);
        carBashSample.isLooped = false;
        carBashSample.loopStart = 0;
        carBashSample.loopLength = 0;
        samples[AudioSampleType::CAR_BASH] = carBashSample;
        LOG_INFO("  - Car bash sound: %u bytes (one-shot) at %u Hz",
                SRL0_CAR_BASH_LENGTH, SRL0_CAR_BASH_RATE);
    }

    // The original lays the full sample across four channels at four periods; one voice at the lowest reads the same.
    if (size >= size_t{SRL0_CRASH_OFFSET} + SRL0_CRASH_LENGTH) {
        AudioSample crashSample;
        LoadRawAudio(data + SRL0_CRASH_OFFSET, SRL0_CRASH_LENGTH, crashSample,
                     3579545u / SRL0_CRASH_PERIODS[3]);
        crashSample.isLooped = false;
        crashSample.loopStart = 0;
        crashSample.loopLength = 0;
        samples[AudioSampleType::CRASH] = crashSample;
        LOG_INFO("  - Crash sound: %u bytes (one-shot) at period %u",
                SRL0_CRASH_LENGTH, SRL0_CRASH_PERIODS[3]);
    }

    if (size >= size_t{SRL0_BAY_IGNITION_OFFSET} + SRL0_BAY_IGNITION_LENGTH) {
        AudioSample baySample;
        LoadRawAudio(data + SRL0_BAY_IGNITION_OFFSET, SRL0_BAY_IGNITION_LENGTH, baySample,
                     3579545u / SRL0_BAY_IGNITION_PERIOD_BASE);
        baySample.isLooped = false;   // Plays once.
        baySample.loopStart = 0;
        baySample.loopLength = 0;
        samples[AudioSampleType::BAY_IGNITION] = baySample;
        LOG_INFO("  - Engine bay drone: %u bytes (looped) from offset 0x%X",
                SRL0_BAY_IGNITION_LENGTH, SRL0_BAY_IGNITION_OFFSET);
    }

    // Re-pitched on every press; callers override the rate.
    if (size >= size_t{SRL0_PART_MOUNT_OFFSET} + SRL0_PART_MOUNT_LENGTH) {
        AudioSample toolSample;
        LoadRawAudio(data + SRL0_PART_MOUNT_OFFSET, SRL0_PART_MOUNT_LENGTH, toolSample,
                     3579545u / SRL0_PART_MOUNT_PERIOD_BASE);
        toolSample.isLooped = false;
        toolSample.loopStart = 0;
        toolSample.loopLength = 0;
        samples[AudioSampleType::PART_MOUNT] = toolSample;
        LOG_INFO("  - Part mount sound: %u bytes (one-shot) at period %u",
                SRL0_PART_MOUNT_LENGTH, SRL0_PART_MOUNT_PERIOD_BASE);
    }

    if (size >= size_t{SRL0_CUTSCENE_HIT_OFFSET} + SRL0_CUTSCENE_HIT_LENGTH) {
        AudioSample hitSample;
        LoadRawAudio(data + SRL0_CUTSCENE_HIT_OFFSET, SRL0_CUTSCENE_HIT_LENGTH, hitSample,
                     3579545u / SRL0_CUTSCENE_HIT_PERIOD_BIAS);
        samples[AudioSampleType::CUTSCENE_HIT] = hitSample;
    }

    if (size >= size_t{SRL0_HISS_OFFSET} + SRL0_HISS_LENGTH) {
        AudioSample hiss;
        LoadRawAudio(data + SRL0_HISS_OFFSET, SRL0_HISS_LENGTH, hiss, SRL0_HISS_RATE);
        hiss.isLooped = false;
        samples[AudioSampleType::HISS] = hiss;
    }

    if (size >= SRL0_CAR_JACK_OFFSET + SRL0_CAR_JACK_LENGTH) {
        AudioSample carJackSample;
        LoadRawAudio(data + SRL0_CAR_JACK_OFFSET, SRL0_CAR_JACK_LENGTH,
                     carJackSample, SRL0_CAR_JACK_RATE);
        carJackSample.isLooped = false;
        carJackSample.loopStart = 0;
        carJackSample.loopLength = 0;
        samples[AudioSampleType::CAR_JACK] = carJackSample;
        LOG_INFO("  - Car jack sound: %u bytes (one-shot) at %u Hz",
                SRL0_CAR_JACK_LENGTH, SRL0_CAR_JACK_RATE);
    }

    if (size >= SRL0_TIRE_SCREECH_OFFSET + SRL0_TIRE_SCREECH_LENGTH) {
        AudioSample tireScreechSample;
        LoadRawAudio(data + SRL0_TIRE_SCREECH_OFFSET, SRL0_TIRE_SCREECH_LENGTH,
                     tireScreechSample, SRL0_TIRE_SCREECH_RATE);
        tireScreechSample.isLooped = false;  // One-shot sound, retriggered by the caller
        tireScreechSample.loopStart = 0;
        tireScreechSample.loopLength = 0;
        samples[AudioSampleType::TIRE_SCREECH] = tireScreechSample;
        LOG_INFO("  - Tire screech sound: %u bytes (one-shot) at %u Hz",
                SRL0_TIRE_SCREECH_LENGTH, SRL0_TIRE_SCREECH_RATE);
    }

    // Two Paula channels at volume 0x36, retriggered while the cop closes.
    if (size >= SRL0_COP_SIREN_OFFSET + SRL0_COP_SIREN_LENGTH) {
        AudioSample copSirenSample;
        LoadRawAudio(data + SRL0_COP_SIREN_OFFSET, SRL0_COP_SIREN_LENGTH,
                     copSirenSample, SRL0_COP_SIREN_RATE);
        copSirenSample.isLooped = false;  // one-shot, retriggered by the caller
        copSirenSample.loopStart = 0;
        copSirenSample.loopLength = 0;
        samples[AudioSampleType::COP_SIREN] = copSirenSample;
        LOG_INFO("  - Cop siren sound: %u bytes (one-shot) at %u Hz",
                SRL0_COP_SIREN_LENGTH, SRL0_COP_SIREN_RATE);
    }

    if (size > 12000) {
        LOG_INFO("  - Additional sound effects: %zu bytes (offsets TBD)", size - 12000);
    }
}

void AudioManager::ExtractSRL1Samples(const uint8_t* data, size_t size) {
    LOG_INFO("AudioManager: Extracting samples from srl1.wave (%zu bytes)", size);

    if (size >= SRL1_TONE_OFFSET + SRL1_TONE_LENGTH) {
        AudioSample toneSample;
        LoadRawAudio(data + SRL1_TONE_OFFSET, SRL1_TONE_LENGTH,
                     toneSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_TONE] = toneSample;
        LOG_INFO("  - Tone: %u bytes at offset 0x%04X", SRL1_TONE_LENGTH, SRL1_TONE_OFFSET);
    }

    if (size >= SRL1_BASSDRUM_OFFSET + SRL1_BASSDRUM_LENGTH) {
        AudioSample bassSample;
        LoadRawAudio(data + SRL1_BASSDRUM_OFFSET, SRL1_BASSDRUM_LENGTH,
                     bassSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_BASSDRUM] = bassSample;
        LOG_INFO("  - Bassdrum: %u bytes at offset 0x%04X", SRL1_BASSDRUM_LENGTH, SRL1_BASSDRUM_OFFSET);
    }

    if (size >= SRL1_HIHAT_OFFSET + SRL1_HIHAT_LENGTH) {
        AudioSample hihatSample;
        LoadRawAudio(data + SRL1_HIHAT_OFFSET, SRL1_HIHAT_LENGTH,
                     hihatSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_HIHAT] = hihatSample;
        LOG_INFO("  - Hihat: %u bytes at offset 0x%04X", SRL1_HIHAT_LENGTH, SRL1_HIHAT_OFFSET);
    }

    if (size >= SRL1_BUZZ_OFFSET) {
        uint32_t buzzLength = size - SRL1_BUZZ_OFFSET;
        AudioSample buzzSample;
        LoadRawAudio(data + SRL1_BUZZ_OFFSET, buzzLength,
                     buzzSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_BUZZ] = buzzSample;
        LOG_INFO("  - Buzz: %u bytes at offset 0x%04X", buzzLength, SRL1_BUZZ_OFFSET);
    }
}

bool AudioManager::LoadRawAudio(const uint8_t* data, size_t size, AudioSample& sample, uint32_t sampleRate) {
    if (!data || size == 0) return false;

    // Amiga audio samples are SIGNED 8-bit (-128 to 127), use directly
    sample.data.resize(size);
    for (size_t i = 0; i < size; i++) {
        sample.data[i] = static_cast<int8_t>(data[i]);
    }

    sample.sampleRate = sampleRate;
    return true;
}

bool AudioManager::Load8SVX(const uint8_t* data, size_t size, AudioSample& sample) {
    if (size < 12) return false;

    if (memcmp(data, "FORM", 4) != 0) return false;

    uint32_t formSize = ReadBE32(data + 4);

    if (size < 12 || memcmp(data + 8, "8SVX", 4) != 0) return false;

    return Parse8SVXChunks(data + 12, std::min(formSize, static_cast<uint32_t>(size - 8)), sample);
}

bool AudioManager::Parse8SVXChunks(const uint8_t* data, size_t size, AudioSample& sample) {
    size_t offset = 0;
    bool foundVHDR = false;
    bool foundBODY = false;

    IFF8SVXHeader header;

    while (offset + 8 <= size) {
        char chunkID[5] = {0};
        memcpy(chunkID, data + offset, 4);
        uint32_t chunkSize = ReadBE32(data + offset + 4);
        offset += 8;

        if (offset + chunkSize > size) break;

        if (strcmp(chunkID, "VHDR") == 0 && chunkSize >= 20) {
            header.oneShotHiSamples = ReadBE32(data + offset);
            header.repeatHiSamples = ReadBE32(data + offset + 4);
            header.samplesPerHiCycle = ReadBE32(data + offset + 8);
            header.samplesPerSec = ReadBE16(data + offset + 12);
            header.ctOctave = data[offset + 14];
            header.sCompression = data[offset + 15];
            header.volume = ReadBE32(data + offset + 16);

            sample.sampleRate = header.samplesPerSec;
            sample.loopStart = header.oneShotHiSamples;
            sample.loopLength = header.repeatHiSamples;
            sample.isLooped = (header.repeatHiSamples > 0);

            foundVHDR = true;
            LOG_INFO("  8SVX VHDR: rate=%d, oneshot=%u, repeat=%u, loop=%d",
                    header.samplesPerSec, header.oneShotHiSamples,
                    header.repeatHiSamples, sample.isLooped);
        }
        else if (strcmp(chunkID, "BODY") == 0) {
            sample.data.assign(
                reinterpret_cast<const int8_t*>(data + offset),
                reinterpret_cast<const int8_t*>(data + offset + chunkSize)
            );
            foundBODY = true;
            LOG_INFO("  8SVX BODY: %u bytes", chunkSize);
        }

        offset += chunkSize;
        // IFF chunks are word-aligned
        if (chunkSize & 1) offset++;
    }

    return foundVHDR && foundBODY;
}

AudioSample* AudioManager::GetSample(AudioSampleType type) {
    auto it = samples.find(type);
    if (it != samples.end()) {
        return &it->second;
    }
    return nullptr;
}

namespace {
// The DOS tyre change loops each run through one animation step; RodLegacy plays them for this long.
constexpr double kTireLoopSeconds = 0.8;
}

PcSpeakerSfx* AudioManager::Speaker() {
    if (!pcSpeakerSfx_) {
        auto speaker = std::make_unique<PcSpeakerSfx>();
        if (!speaker->Initialize()) return nullptr;
        speaker->SetVolume(sfxVolume_ / 10.0f);
        speaker->SetLoops(pcLoops_);
        speaker->SetSr2(sr2Game_);
        speaker->SetMufflerRemoved(mufflerRemoved_);
        pcSpeakerSfx_ = std::move(speaker);
    }
    return pcSpeakerSfx_.get();
}

// The DOS game's counterpart of each Amiga one-shot (docs/pc_speaker.md). False leaves it to the samples.
bool AudioManager::PlayOnSpeaker(AudioSampleType type, double seconds) {
    PcSpeakerSfx* speaker = Speaker();
    if (!speaker) return false;
    switch (type) {
        case AudioSampleType::CRASH:        speaker->Play(pcspk::Effect::Crash); return true;
        // The race's bump is the DOS collision hit; the garage's bash is the body shop clank.
        case AudioSampleType::CAR_BASH:
            speaker->Play(engineLoopWanted_ && lastEnginePeriod_ == 0 ? pcspk::Effect::Hit : pcspk::Effect::Clank);
            return true;
        case AudioSampleType::CUTSCENE_HIT: speaker->Play(pcspk::Effect::Hit); return true;
        // The looping sounds play for a fixed while rather than being hooked to start and stop.
        case AudioSampleType::PART_MOUNT:   speaker->PlayLoop(pcspk::Loop::Wrench, 1.0); return true;
        case AudioSampleType::ENGINE_START: speaker->PlayLoop(pcspk::Loop::Crank, 1.0); return true;
        // SR2's paint_job only keeps the buzz on while it redraws the screen: a few ticks.
        // ponytail: 3 ticks by ear against DOSBox, the redraw time is not measured.
        case AudioSampleType::PAINT_SPRAY:
            speaker->PlayLoop(pcspk::Loop::Spray, sr2Game_ ? 3 / pcspk::kTickHz : 2.0);
            return true;
        // SR2's tire_change_anim: the 8 kHz / 100 Hz pulse for the wheels off, the low clicks for the lug nuts.
        case AudioSampleType::TIRE_AIR_WRENCH:
            speaker->PlayLoop(sr2Game_ ? pcspk::Loop::Pulse : pcspk::Loop::Wrench, kTireLoopSeconds);
            return true;
        case AudioSampleType::TIRE_PUTT:    speaker->PlayLoop(pcspk::Loop::IdlePutt, kTireLoopSeconds); return true;
        case AudioSampleType::TIRE_LUG_NUT:
            speaker->PlayLoop(sr2Game_ ? pcspk::Loop::Crank : pcspk::Loop::Tick, kTireLoopSeconds);
            return true;
        case AudioSampleType::REJECT_BEEP:  speaker->Play(pcspk::Effect::Click); return true;
        case AudioSampleType::DRIVETRAIN_BLOWN: speaker->Play(pcspk::Effect::Clank); return true;
        case AudioSampleType::STICKER:      return true;
        case AudioSampleType::HISS:         speaker->Play(pcspk::Effect::ChirpLow); return true;
        case AudioSampleType::CAR_JACK:     speaker->Play(pcspk::Effect::Thud); return true;
        case AudioSampleType::COP_SIREN:    speaker->SirenFor(seconds); return true;
        case AudioSampleType::TIRE_SCREECH: speaker->SquealFor(seconds); return true;
        // No speaker sound in the DOS game.
        case AudioSampleType::IGNITION:
        case AudioSampleType::BAY_IGNITION:
        case AudioSampleType::HONK:
            return true;
        default:
            return false;
    }
}

AdlibSfx* AudioManager::AdlibEffects() {
    if (!adlibSfx_ && !adlibBank_.empty()) {
        auto fx = std::make_unique<AdlibSfx>();
        if (!fx->Init(adlibBank_)) return nullptr;
        fx->SetVolume(sfxVolume_ / 10.0f);
        adlibSfx_ = std::move(fx);
    }
    return adlibSfx_.get();
}

// Street Rod 2's DOS AdLib counterpart of each one-shot (adlib_sfx.h). The held ones play for a fixed while.
bool AudioManager::PlayOnAdlib(AudioSampleType type, double seconds) {
    AdlibSfx* fx = AdlibEffects();
    if (!fx) return false;
    switch (type) {
        case AudioSampleType::CRASH:        fx->Play(adlibfx::Sound::Crash); return true;
        // The race's bump is the collision hit; the garage's bash is the customize clank.
        case AudioSampleType::CAR_BASH:
            fx->Play(engineLoopWanted_ && lastEnginePeriod_ == 0 ? adlibfx::Sound::Hit : adlibfx::Sound::Clank);
            return true;
        case AudioSampleType::CUTSCENE_HIT: fx->Play(adlibfx::Sound::Hit); return true;
        case AudioSampleType::PART_MOUNT:   fx->Play(adlibfx::Sound::Slide, 1.0); return true;
        case AudioSampleType::HISS:         fx->Play(adlibfx::Sound::ChirpLow); return true;
        case AudioSampleType::CAR_JACK:     fx->Play(adlibfx::Sound::Thud); return true;
        case AudioSampleType::PAINT_SPRAY:  fx->Play(adlibfx::Sound::Spray, 2.0); return true;
        case AudioSampleType::TIRE_AIR_WRENCH: fx->Play(adlibfx::Sound::AirWrench, kTireLoopSeconds); return true;
        case AudioSampleType::TIRE_PUTT:    fx->Play(adlibfx::Sound::Putt, kTireLoopSeconds); return true;
        case AudioSampleType::TIRE_LUG_NUT: fx->Play(adlibfx::Sound::LugNut, kTireLoopSeconds); return true;
        case AudioSampleType::REJECT_BEEP:  fx->Play(adlibfx::Sound::Click); return true;
        case AudioSampleType::DRIVETRAIN_BLOWN: fx->Play(adlibfx::Sound::Blown); return true;
        case AudioSampleType::STICKER:      return true;
        case AudioSampleType::COP_SIREN:    fx->Play(adlibfx::Sound::Siren, seconds); return true;
        case AudioSampleType::TIRE_SCREECH: fx->Play(adlibfx::Sound::Squeal, seconds); return true;
        case AudioSampleType::ENGINE_START:
        case AudioSampleType::IGNITION:
        case AudioSampleType::BAY_IGNITION:
        case AudioSampleType::HONK:
            return true;
        default:
            return false;
    }
}

C64Sfx* AudioManager::C64Effects() {
    if (!c64Sfx_ && !c64SfxPrg_.empty()) {
        auto fx = std::make_unique<C64Sfx>();
        if (!fx->Init(c64SfxPrg_)) return nullptr;
        fx->SetVolume(sfxVolume_ / 10.0f);
        c64Sfx_ = std::move(fx);
    }
    return c64Sfx_.get();
}

// The C64 effects identified so far (RodLegacyResearch/docs/c64/sfx.md). The rest are silent.
void AudioManager::PlayOnC64(AudioSampleType type, double seconds) {
    C64Sfx* fx = C64Effects();
    if (!fx) return;
    switch (type) {
        case AudioSampleType::CRASH:       fx->Play(9); break;
        case AudioSampleType::COP_SIREN:
            if (!c64OffRoad_) fx->Play(8, seconds);
            break;
        case AudioSampleType::PAINT_SPRAY: fx->Play(4, 0xE0 / 50.0); break;   // garq $6AC6 holds it $E0 frames
        case AudioSampleType::PART_MOUNT:  fx->Play(6); break;
        // The garage's bash is the roof/bumper customize sound; in the race it is the bump into a car (17).
        case AudioSampleType::CAR_BASH:
            fx->Play(engineLoopWanted_ && lastEnginePeriod_ == 0 ? 17 : 3);
            break;
        default: break;
    }
}

void AudioManager::PlaySoundVia(SfxSource src, AudioSampleType type) {
    if (src == SfxSource::ADLIB && !AdlibEffects()) return;
    const SfxSource saved = sfxSource_;
    sfxSource_ = src;
    PlaySound(type);
    sfxSource_ = saved;
}

void AudioManager::SetSfxSource(SfxSource src) {
    if (src == SfxSource::ADLIB && !AdlibEffects()) {
        LOG_WARN("AdLib effects unavailable (no MSR.SND), keeping the current ones");
        return;
    }
    if (src == SfxSource::C64 && !C64Effects()) {
        LOG_WARN("C64 effects unavailable (no STREETR1.D64), keeping the current ones");
        return;
    }
    sfxSource_ = src;
    LOG_INFO("AudioManager: effects source is %s",
             src == SfxSource::PCSPEAKER ? "pc speaker" : src == SfxSource::ADLIB ? "adlib"
             : src == SfxSource::C64 ? "c64" : "amiga");
}

void AudioManager::PlaySound(AudioSampleType type, float volume, int sampleRateOverride) {
    // Standing conditions with frequent retriggers, so report each once.
    static std::set<int> reportedMissing;
    static bool reportedNoDevice = false;

    if (sfxSource_ == SfxSource::AMIGA) {
        switch (type) {
            // The paint job: the bash sample at period 530, volume 0x1a (SR1 0x2286cc, SR2 0x22c04c).
            case AudioSampleType::PAINT_SPRAY:
                PlaySound(AudioSampleType::CAR_BASH, 0x1a / 64.0f, 3579545 / 530);
                return;
            // Drivetrain torn off: the same sample at period 130, volume 0x40 (SR1 0x2285c8).
            case AudioSampleType::DRIVETRAIN_BLOWN:
                PlaySound(AudioSampleType::CAR_BASH, 0x40 / 64.0f, 3579545 / 130);
                return;
            // The sticker wipe: the same sample at period 330, volume 0x14 (SR1 0x22873a, called from 0x240734).
            case AudioSampleType::STICKER:
                PlaySound(AudioSampleType::CAR_BASH, 0x14 / 64.0f, 3579545 / 330);
                return;
            // The Amiga games have no sound for these.
            case AudioSampleType::TIRE_AIR_WRENCH:
            case AudioSampleType::TIRE_PUTT:
            case AudioSampleType::TIRE_LUG_NUT:
            case AudioSampleType::REJECT_BEEP:
                return;
            default:
                break;
        }
    }
    auto it = samples.find(type);
    if (sfxSource_ == SfxSource::PCSPEAKER) {
        double seconds = 0.5;
        if (it != samples.end() && !it->second.data.empty()) {
            const int rate = sampleRateOverride > 0 ? sampleRateOverride : static_cast<int>(it->second.sampleRate);
            seconds = static_cast<double>(it->second.data.size()) / std::max(rate, 1);
        }
        if (PlayOnSpeaker(type, seconds)) return;
    }
    if (sfxSource_ == SfxSource::ADLIB) {
        double seconds = 0.5;
        if (it != samples.end() && !it->second.data.empty()) {
            const int rate = sampleRateOverride > 0 ? sampleRateOverride : static_cast<int>(it->second.sampleRate);
            seconds = static_cast<double>(it->second.data.size()) / std::max(rate, 1);
        }
        if (PlayOnAdlib(type, seconds)) return;
    }
    if (sfxSource_ == SfxSource::C64) {
        double seconds = 0.5;
        if (it != samples.end() && !it->second.data.empty()) {
            const int rate = sampleRateOverride > 0 ? sampleRateOverride : static_cast<int>(it->second.sampleRate);
            seconds = static_cast<double>(it->second.data.size()) / std::max(rate, 1);
        }
        PlayOnC64(type, seconds);   // sounds the C64 has no effect for stay silent
        return;
    }
    if (it == samples.end() || it->second.data.empty()) {
        if (reportedMissing.insert(static_cast<int>(type)).second) {
            LOG_INFO("AudioManager::PlaySound - sample %d not loaded", static_cast<int>(type));
        }
        return;
    }

    const AudioSample& sample = it->second;

    if (audioDevice == 0) {
        if (!reportedNoDevice) {
            reportedNoDevice = true;
            LOG_INFO("AudioManager::PlaySound - no audio device, one-shots are silent");
        }
        return;
    }

    SDL_AudioStream* stream = SDL_NewAudioStream(
        AUDIO_S8, 1, sampleRateOverride > 0 ? sampleRateOverride : static_cast<int>(sample.sampleRate),
        AUDIO_S16SYS, 2, 22050                              // Dest: 16-bit stereo at 22050 Hz
    );

    if (!stream) {
        LOG_INFO("AudioManager::PlaySound - Failed to create audio stream: %s", SDL_GetError());
        return;
    }

    if (SDL_AudioStreamPut(stream, sample.data.data(), static_cast<int>(sample.data.size())) < 0) {
        LOG_INFO("AudioManager::PlaySound - Failed to put data in stream: %s", SDL_GetError());
        SDL_FreeAudioStream(stream);
        return;
    }

    SDL_AudioStreamFlush(stream);
    int available = SDL_AudioStreamAvailable(stream);
    if (available > 0) {
        std::vector<int16_t> convertedData(available / 2);
        int got = SDL_AudioStreamGet(stream, convertedData.data(), available);

        if (got > 0) {
            float effectiveVolume = volume * (sfxVolume_ / 10.0f);
            for (size_t i = 0; i < convertedData.size(); i++) {
                convertedData[i] = static_cast<int16_t>(convertedData[i] * effectiveVolume);
            }

            SDL_QueueAudio(audioDevice, convertedData.data(), got);
            record_tap::Write("samples", 22050, 2, convertedData.data(), got);
        }
    }

    SDL_FreeAudioStream(stream);
}

int AudioManager::StartLoop(AudioSampleType type, float volume) {
    if (type == AudioSampleType::ENGINE_LOOP && engineSource_ == EngineSource::PCSPEAKER) {
        engineLoopWanted_ = true;
        if (PcSpeakerSfx* speaker = Speaker()) {
            if (lastEnginePeriod_ > 0) UpdateEngineDistance(sceneDistance_);
            else speaker->SetRpm(currentRPM);
            speaker->StartEngine();
        }
        return 0;
    }
    if (type == AudioSampleType::ENGINE_LOOP && engineSource_ == EngineSource::ADLIB) {
        engineLoopWanted_ = true;
        if (adlibEngine) {
            adlibEngine->Start();
            // Start() re-sends from lastRpm; a scene that already set its tone
            // must not be dragged back onto the rpm ladder.
            if (lastEnginePeriod_ > 0) {
                UpdateEngineDistance(sceneDistance_);
            } else {
                adlibEngine->SetRpm(currentRPM);
            }
        }
        return 0;
    }
    if (type == AudioSampleType::ENGINE_LOOP && engineSource_ == EngineSource::C64) {
        engineLoopWanted_ = true;
        if (c64Engine_) {
            c64ScenePending_ = lastEnginePeriod_ > 0;
            if (!c64ScenePending_) {
                c64Scene_ = false;
                c64Engine_->StartRace();
                c64Engine_->SetRpm(currentRPM);
            }
        }
        return 0;
    }
    if (type == AudioSampleType::ENGINE_LOOP && engineSound) {
        engineLoopWanted_ = true;
        if (AudioSample* loop = GetSample(AudioSampleType::ENGINE_LOOP)) {
            engineSound->SetSample(loop->data);
        }
        if (lastEnginePeriod_ > 0) engineSound->UpdatePeriod(lastEnginePeriod_, lastEngineVolume_);
        engineSound->SetVolume(volume * (sfxVolume_ / 10.0f));
        engineSound->Start();
        LOG_INFO("AudioManager::StartLoop - Engine sound started at volume %.2f", volume);
        return 0;
    }

    LOG_INFO("AudioManager::StartLoop(%d) - not yet implemented for this type", static_cast<int>(type));
    return -1;
}

void AudioManager::StopLoop(int channelId) {
    if (channelId != 0) {   // Channel 0 is engine sound
        return;
    }
    engineLoopWanted_ = false;
    SetOffRoad(false);
    if (adlibEngine) {
        adlibEngine->Stop();
    }
    c64ScenePending_ = false;
    if (c64Engine_ && !c64Scene_) {
        c64Engine_->Stop();
    }
    if (engineSound) {
        engineSound->Stop();
    }
    if (pcSpeakerSfx_) {
        pcSpeakerSfx_->StopEngine();
    }
    LOG_INFO("AudioManager::StopLoop - Engine sound stopped");
}

void AudioManager::UpdateEnginePitch(int16_t rpm) {
    currentRPM = rpm;
    lastEnginePeriod_ = 0;   // the race owns the voice again
    // Only the selected one is driven, so the other cannot be left humming.
    if (engineSource_ == EngineSource::ADLIB) {
        if (adlibEngine) {
            adlibEngine->SetRpm(rpm);
        }
    } else if (engineSource_ == EngineSource::PCSPEAKER) {
        if (pcSpeakerSfx_) {
            pcSpeakerSfx_->SetRpm(rpm);
        }
    } else if (engineSource_ == EngineSource::C64) {
        if (c64Engine_) {
            // StartLoop saw a stale scene period and waited for a scene; the race takes the voice here.
            if (engineLoopWanted_ && (c64ScenePending_ || c64Scene_)) {
                c64ScenePending_ = false;
                c64Scene_ = false;
                c64Engine_->StartRace();
            }
            c64Engine_->SetRpm(rpm);
        }
    } else if (engineSound) {
        engineSound->UpdateRPM(rpm);
    }
}

void AudioManager::SetOffRoad(bool offRoad) {
    if (offRoad == c64OffRoad_) return;
    c64OffRoad_ = offRoad;
    if (sfxSource_ != SfxSource::C64) return;
    if (C64Sfx* fx = C64Effects()) {
        if (offRoad) fx->Play(18);
        else fx->Stop();
    }
}

void AudioManager::SetMufflerRemoved(bool removed) {
    mufflerRemoved_ = removed;
    if (pcSpeakerSfx_) pcSpeakerSfx_->SetMufflerRemoved(removed);
    if (adlibEngine) {
        adlibEngine->SetMufflerRemoved(removed);
    }
}

void AudioManager::SetEngineSource(EngineSource src) {
    if (src == engineSource_) {
        return;
    }
    if (src == EngineSource::ADLIB && !adlibEngine) {
        adlibEngine = std::make_unique<AdlibEngine>();
        adlibEngine->SetMufflerRemoved(mufflerRemoved_);
        adlibEngine->SetVolume(sfxVolume_ / 10.0f);
        if (!adlibEngine->Initialize()) {
            LOG_WARN("AdLib engine sound unavailable, keeping the current one");
            adlibEngine.reset();
            return;
        }
    }
    if (src == EngineSource::PCSPEAKER && !Speaker()) {
        LOG_WARN("PC speaker engine sound unavailable, keeping the current one");
        return;
    }
    if (src == EngineSource::C64 && !c64Engine_) {
        auto engine = std::make_unique<C64Engine>();
        if (c64SfxPrg_.empty() || !engine->Init(c64SfxPrg_)) {
            LOG_WARN("C64 engine sound unavailable (no STREETR1.D64), keeping the current one");
            return;
        }
        engine->SetVolume(sfxVolume_ / 10.0f);
        c64Engine_ = std::move(engine);
    }
    // Only one may run, and a running loop is handed over.
    const bool running = engineLoopWanted_;
    if (adlibEngine) adlibEngine->Stop();
    if (engineSound) engineSound->Stop();
    if (pcSpeakerSfx_) pcSpeakerSfx_->StopEngine();
    if (c64Engine_) c64Engine_->Stop();
    engineSource_ = src;
    if (running) {
        if (src == EngineSource::ADLIB) adlibEngine->Start();
        else if (src == EngineSource::PCSPEAKER) pcSpeakerSfx_->StartEngine();
        else if (src == EngineSource::C64 && lastEnginePeriod_ == 0) c64Engine_->StartRace();
        else if (engineSound) engineSound->Start();
        if (lastEnginePeriod_ > 0) UpdateEnginePeriod(lastEnginePeriod_, lastEngineVolume_);
        else UpdateEnginePitch(currentRPM);
    }
    LOG_INFO("AudioManager: engine sound source is %s",
             src == EngineSource::ADLIB ? "adlib" : src == EngineSource::PCSPEAKER ? "pc speaker"
             : src == EngineSource::C64 ? "c64" : "amiga");
}

void AudioManager::UpdateEnginePeriod(int16_t period, int paulaVolume) {
    // The scene car animations drive the engine by Amiga period; the AdLib engine only speaks rpm.
    currentRPM = static_cast<int16_t>(EngineSound::PeriodToRPM(period));
    lastEnginePeriod_ = period;
    lastEngineVolume_ = paulaVolume;
    // The C64 plays a whole drive in / out effect instead of following the car.
    if (engineSource_ == EngineSource::C64) return;
    if (engineSource_ != EngineSource::AMIGA) {
        UpdateEngineDistance(sceneDistance_);
        return;
    }
    if (engineSound) {
        engineSound->UpdatePeriod(period, paulaVolume);
    }
}

// The DOS games pitch the animation engine on the car's distance from its parking spot, not on a period.
void AudioManager::UpdateEngineDistance(int px) {
    sceneDistance_ = px;
    // Leaving starts at the parked spot, arriving far from it.
    if (engineSource_ == EngineSource::C64 && c64Engine_ && c64ScenePending_) {
        c64ScenePending_ = false;
        c64Scene_ = true;
        c64Engine_->PlayScene(std::abs(px) >= 20);
        return;
    }
    const int tone = pcspk::AnimationTone(px);
    if (engineSource_ == EngineSource::PCSPEAKER && pcSpeakerSfx_) {
        pcSpeakerSfx_->SetSceneDivisor(static_cast<uint16_t>(60000 - tone));
    } else if (engineSource_ == EngineSource::ADLIB && adlibEngine) {
        adlibEngine->SetSceneTone(tone);
    }
}

void AudioManager::PlayBayIgnition() {
    // A one-shot slice not heard in the bay on hardware; left unhooked.
}

void AudioManager::StartEngineBayDrone(int tuneOffset) {
    if (engineSource_ == EngineSource::C64 && c64Engine_) {
        engineLoopWanted_ = true;
        c64Scene_ = false;
        c64Engine_->StartBay(tuneOffset);
        return;
    }
    if (engineSource_ == EngineSource::PCSPEAKER) {
        SetEngineBayTune(tuneOffset);
        return;
    }
    if (!engineSound) {
        return;
    }
    SetEngineBayTune(tuneOffset);
    StartLoop(AudioSampleType::ENGINE_LOOP, BayDroneVolume(tuneOffset) / 64.0f);
}

void AudioManager::SetEngineBayTune(int tuneOffset) {
    if (engineSource_ == EngineSource::C64 && c64Engine_) {
        c64Engine_->SetBayTune(tuneOffset);
        return;
    }
    // The DOS tune-up screen's tone, its setting runs -8..4.
    if (engineSource_ == EngineSource::PCSPEAKER) {
        if (PcSpeakerSfx* speaker = Speaker()) {
            speaker->SetTuneTone(pcspk::TuneDivisor(std::max(-8, std::min(8, tuneOffset))));
        }
        return;
    }
    if (!engineSound) {
        return;
    }
    // All channels meet at 670 at perfect tune; the gap widens with the mistune, faster when advancing.
    // Two voices rather than two pairs, one each.
    const int left  = BayDronePeriodA(tuneOffset);
    const int right = BayDronePeriodB(tuneOffset);
    engineSound->UpdatePeriodPair(static_cast<int16_t>(left), static_cast<int16_t>(right));
    engineSound->SetVolume((BayDroneVolume(tuneOffset) / 64.0f) * (sfxVolume_ / 10.0f));
}

void AudioManager::StopEngineBayDrone() {
    if (pcSpeakerSfx_) {
        pcSpeakerSfx_->SetTuneTone(0);
    }
    StopLoop(0);
}

// SR2's rolling wheel: the racing loop at 3550 and 3650.
void AudioManager::StartSampleLoop(int16_t left, int16_t right, float volume) {
    if (!engineSound) {
        return;
    }
    engineLoopWanted_ = true;
    if (AudioSample* loop = GetSample(AudioSampleType::ENGINE_LOOP)) {
        engineSound->SetSample(loop->data);
    }
    engineSound->UpdatePeriodPair(left, right);
    engineSound->SetVolume(volume * (sfxVolume_ / 10.0f));
    engineSound->Start();
    engineSound->UpdatePeriodPair(left, right);
}

void AudioManager::StartTireRollLoop() {
    StartSampleLoop(SRL0_TIRE_ROLL_PERIOD_LEFT, SRL0_TIRE_ROLL_PERIOD_RIGHT, SRL0_TIRE_ROLL_VOLUME / 64.0f);
}

void AudioManager::StopTireRollLoop() {
    StopLoop(0);
}

// The racing loop at 2850 and 1850, a slow mechanical chug. Shares the bay drone's EngineSound.
void AudioManager::StartGasPumpLoop() {
    if (sfxSource_ == SfxSource::ADLIB) {
        if (AdlibSfx* fx = AdlibEffects()) fx->Play(adlibfx::Sound::Tick);
        return;
    }
    if (sfxSource_ == SfxSource::PCSPEAKER) {
        if (PcSpeakerSfx* speaker = Speaker()) speaker->PlayLoop(pcspk::Loop::Tick, 0);
        return;
    }
    StartSampleLoop(SRL0_GAS_PUMP_PERIOD_LEFT, SRL0_GAS_PUMP_PERIOD_RIGHT, SRL0_GAS_PUMP_VOLUME / 64.0f);
}

void AudioManager::StopGasPumpLoop() {
    if (pcSpeakerSfx_) pcSpeakerSfx_->StopLoop();
    if (adlibSfx_) adlibSfx_->Stop(adlibfx::Sound::Tick);
    StopLoop(0);
}


void AudioManager::PlayInstrumentNote(int channel, int instrument, int period, int volume) {
    if (!amigaMusic_) return;
    amigaMusic_->SetVolume(sfxVolume_ / 10.0f);
    amigaMusic_->PlayEffectNote(channel, instrument, period, volume);
}

void AudioManager::PlayChime(int chA, int period, int chB) {
    PlayInstrumentNote(chA, 6, period, 0x32);
    PlayInstrumentNote(chB, 6, period + 1, 0x32);
}

void AudioManager::StopMusicPlayback() {
    victoryPlaying_ = false;
    if (pcSpeaker_) pcSpeaker_->Stop();
    if (amigaMusic_) amigaMusic_->Stop();
    if (adlibMusic_) adlibMusic_->Stop();
    if (c64Music_) c64Music_->Stop();
}

void AudioManager::SetChannelVolume(int /*channelId*/, float /*volume*/) {
}

static bool IsIntroSegment(const std::vector<SrNote>& seg) {
    return !seg.empty() && seg[0].duration_ticks == 5 && seg[0].pit_divisor == 0x0474;
}

static bool IsRestSegment(const std::vector<SrNote>& seg) {
    for (const auto& n : seg)
        if (n.pit_divisor != 0) return false;
    return !seg.empty();
}

// Fallback when the segment count doesn't match the expected 18-segment layout.
static std::vector<SrNote> BuildShuffledPlaylist(const std::vector<std::vector<SrNote>>& segments) {
    const std::vector<SrNote>* intro = nullptr;
    std::vector<size_t> songIdxs;

    for (size_t i = 0; i < segments.size(); ++i) {
        if (IsIntroSegment(segments[i])) {
            intro = &segments[i];
        } else if (!IsRestSegment(segments[i])) {
            songIdxs.push_back(i);
        }
    }

    if (songIdxs.empty()) {
        std::vector<SrNote> all;
        for (const auto& seg : segments)
            for (const auto& n : seg)
                all.push_back(n);
        return all;
    }

    std::shuffle(songIdxs.begin(), songIdxs.end(),
                 std::default_random_engine(std::random_device{}()));

    std::vector<SrNote> playlist;
    for (size_t idx : songIdxs) {
        if (intro)
            for (const auto& n : *intro) playlist.push_back(n);
        for (const auto& n : segments[idx]) playlist.push_back(n);
    }
    return playlist;
}

void AudioManager::InitPCSpeaker(const std::string& dosExePath) {
    std::ifstream f(dosExePath, std::ios::binary | std::ios::ate);
    if (!f) {
        LOG_INFO("AudioManager: cannot open %s, no PC speaker music", dosExePath.c_str());
        return;
    }
    auto fsz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(fsz);
    f.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)fsz);
    AudioManager::InitPCSpeakerFromExeBytes(buf.data(), buf.size());
}

void AudioManager::SetPcSpeakerLoops(std::vector<pcspk::LoopSong> loops) {
    pcLoops_ = std::move(loops);
    if (pcSpeakerSfx_) pcSpeakerSfx_->SetLoops(pcLoops_);
}

void AudioManager::InitPCSpeakerFromExeBytes(const uint8_t* data, size_t len) {
    std::vector<std::vector<SrNote>> segs;
    if (!sr_exe_parse_music_bytes(data, len, segs)) {
        LOG_INFO("AudioManager: DOS EXE buffer not recognized as SR.EXE, no PC speaker music");
        return;
    }
    // Only now: ReadLoops reads fixed SR1 offsets, which would be noise in another exe.
    SetPcSpeakerLoops(pcspk::ReadLoops(data, len));

    if (sr_exe_build_known_songs(segs, pcSongs_)) {
        pcSongNames_ = {"Song 1", "Song 2", "Song 3", "Song 4", "Song 5"};
        LOG_INFO("AudioManager: PC speaker: %zu songs loaded", pcSongs_.size());
    } else {
        LOG_INFO("AudioManager: unexpected segment count (%zu), using fallback", segs.size());
        pcSongs_.clear();
        pcSongs_.push_back(BuildShuffledPlaylist(segs));
        pcSongNames_ = {"Music"};
    }
    StartPcSpeakerMusic();
}

void AudioManager::SetPcSpeakerSongs(std::vector<std::vector<SrNote>> songs, std::vector<std::string> names) {
    pcSongs_ = std::move(songs);
    pcSongNames_ = std::move(names);
    StartPcSpeakerMusic();
}

void AudioManager::StartPcSpeakerMusic() {
    currentPCSong_ = 0;
    if (pcSongs_.empty()) return;
    if (!pcSpeaker_) {
        pcSpeaker_ = std::make_unique<PCSpeaker>();
        if (!pcSpeaker_->Init()) {
            LOG_INFO("AudioManager: PCSpeaker audio init failed");
            pcSpeaker_.reset();
            pcSongs_.clear();
            pcSongNames_.clear();
            return;
        }
        pcSpeaker_->SetAmplitude(0);  // volume applied when SetMusicSource selects DOS
    }
    pcSpeaker_->SetNotes(pcSongs_[0]);
    LOG_INFO("AudioManager: PC speaker music loaded (%zu songs)", pcSongs_.size());
}

// A different song than the one playing, picked at random.
static int PickOtherSong(int count, int current) {
    static std::mt19937 rng(std::random_device{}());
    int next;
    do {
        next = std::uniform_int_distribution<int>(0, count - 1)(rng);
    } while (next == current);
    return next;
}

void AudioManager::UpdateMusic() {
    if (musicSource_ == MusicSource::DOS && pcSpeaker_) {
        if (pcSongs_.size() < 2) return;
        if (!pcSpeaker_->ConsumeLoopFlag()) return;
        currentPCSong_ = PickOtherSong((int)pcSongs_.size(), currentPCSong_);
        pcSpeaker_->SetNotes(pcSongs_[(size_t)currentPCSong_]);
        return;
    }
    if (musicSource_ == MusicSource::ADLIB && HasAdlibMusic()) {
        if (adlibMusic_->SongCount() < 2) return;
        if (!adlibMusic_->ConsumeLoopFlag()) return;
        adlibMusic_->SetSong(PickOtherSong(adlibMusic_->SongCount(), adlibMusic_->CurrentSong()));
    }
    if (musicSource_ == MusicSource::C64 && HasC64Music()) {
        if (c64Music_->SongCount() < 2) return;
        if (!c64Music_->ConsumeLoopFlag()) return;
        c64Music_->SetSong(PickOtherSong(c64Music_->SongCount(), c64Music_->CurrentSong()));
    }
}

void AudioManager::NextPCSong() {
    if (!pcSpeaker_ || pcSongs_.empty()) return;
    currentPCSong_ = (currentPCSong_ + 1) % (int)pcSongs_.size();
    pcSpeaker_->SetNotes(pcSongs_[(size_t)currentPCSong_]);
    pcSpeaker_->Play();
}

void AudioManager::PrevPCSong() {
    if (!pcSpeaker_ || pcSongs_.empty()) return;
    currentPCSong_ = (currentPCSong_ - 1 + (int)pcSongs_.size()) % (int)pcSongs_.size();
    pcSpeaker_->SetNotes(pcSongs_[(size_t)currentPCSong_]);
    pcSpeaker_->Play();
}

void AudioManager::PausePCSpeaker() {
    if (pcSpeaker_) pcSpeaker_->Pause();
}

void AudioManager::ResumePCSpeaker() {
    if (pcSpeaker_) pcSpeaker_->Resume();
}

bool AudioManager::IsPCSpeakerPaused() const {
    return pcSpeaker_ && pcSpeaker_->IsPaused();
}

int AudioManager::GetCurrentPCSongIndex() const {
    return currentPCSong_;
}

int AudioManager::GetPCSongCount() const {
    return (int)pcSongs_.size();
}

std::string AudioManager::GetCurrentPCSongName() const {
    if (pcSongNames_.empty()) return "";
    return pcSongNames_[(size_t)currentPCSong_];
}

bool AudioManager::HasActiveMusic() const {
    if (musicSource_ == MusicSource::DOS)   return HasPCSpeaker();
    if (musicSource_ == MusicSource::AMIGA) return HasAmigaMusic();
    if (musicSource_ == MusicSource::ADLIB) return HasAdlibMusic();
    if (musicSource_ == MusicSource::C64)   return HasC64Music();
    return false;
}

void AudioManager::NextSong() {
    if (musicSource_ == MusicSource::DOS) NextPCSong();
    if (musicSource_ == MusicSource::ADLIB && HasAdlibMusic()) {
        adlibMusic_->SetSong(adlibMusic_->CurrentSong() + 1);
        adlibMusic_->Play();
    }
    if (musicSource_ == MusicSource::C64 && HasC64Music()) {
        c64Music_->SetSong(c64Music_->CurrentSong() + 1);
        c64Music_->Play();
    }
}

void AudioManager::PrevSong() {
    if (musicSource_ == MusicSource::DOS) PrevPCSong();
    if (musicSource_ == MusicSource::ADLIB && HasAdlibMusic()) {
        adlibMusic_->SetSong(adlibMusic_->CurrentSong() - 1);
        adlibMusic_->Play();
    }
    if (musicSource_ == MusicSource::C64 && HasC64Music()) {
        c64Music_->SetSong(c64Music_->CurrentSong() - 1);
        c64Music_->Play();
    }
}

void AudioManager::PauseMusic() {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  pcSpeaker_->Pause();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) amigaMusic_->Pause();
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) adlibMusic_->Pause();
    if (musicSource_ == MusicSource::C64   && c64Music_)   c64Music_->Pause();
}

void AudioManager::ResumeMusic() {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  pcSpeaker_->Resume();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) amigaMusic_->Resume();
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) adlibMusic_->Resume();
    if (musicSource_ == MusicSource::C64   && c64Music_)   c64Music_->Resume();
}

void AudioManager::PauseAll() {
    if (allPaused_) return;
    allPaused_ = true;
    musicWasPlaying_ = HasActiveMusic() && !IsMusicPaused();
    if (musicWasPlaying_) PauseMusic();
    // Every sample, engine and effect output is frozen where it is; only those that were running resume.
    const SDL_AudioDeviceID devices[] = {
        audioDevice,
        engineSound ? engineSound->Device() : 0,
        adlibEngine ? adlibEngine->Device() : 0,
        c64Engine_ ? c64Engine_->Device() : 0,
        pcSpeakerSfx_ ? pcSpeakerSfx_->Device() : 0,
        adlibSfx_ ? adlibSfx_->Device() : 0,
        c64Sfx_ ? c64Sfx_->Device() : 0,
    };
    pausedDevices_.clear();
    for (SDL_AudioDeviceID id : devices) {
        if (id != 0 && SDL_GetAudioDeviceStatus(id) == SDL_AUDIO_PLAYING) {
            SDL_PauseAudioDevice(id, 1);
            pausedDevices_.push_back(id);
        }
    }
}

void AudioManager::ResumeAll() {
    if (!allPaused_) return;
    allPaused_ = false;
    if (musicWasPlaying_) ResumeMusic();
    for (SDL_AudioDeviceID id : pausedDevices_) SDL_PauseAudioDevice(id, 0);
    pausedDevices_.clear();
}

bool AudioManager::IsMusicPaused() const {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  return pcSpeaker_->IsPaused();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) return amigaMusic_->IsPaused();
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) return adlibMusic_->IsPaused();
    if (musicSource_ == MusicSource::C64   && c64Music_)   return c64Music_->IsPaused();
    return false;
}

std::string AudioManager::GetCurrentSongName() const {
    if (musicSource_ == MusicSource::DOS)   return GetCurrentPCSongName();
    if (musicSource_ == MusicSource::AMIGA) return "Amiga Music";
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) return adlibMusic_->CurrentSongName();
    if (musicSource_ == MusicSource::C64   && c64Music_)   return c64Music_->CurrentSongName();
    return "";
}

int AudioManager::GetCurrentSongIndex() const {
    if (musicSource_ == MusicSource::DOS) return GetCurrentPCSongIndex();
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) return adlibMusic_->CurrentSong();
    if (musicSource_ == MusicSource::C64   && c64Music_)   return c64Music_->CurrentSong();
    return 0;
}

int AudioManager::GetSongCount() const {
    if (musicSource_ == MusicSource::DOS) return GetPCSongCount();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) return 1;
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_) return adlibMusic_->SongCount();
    if (musicSource_ == MusicSource::C64   && c64Music_)   return c64Music_->SongCount();
    return 0;
}

void AudioManager::SetSfxVolume(int v) {
    sfxVolume_ = std::max(0, std::min(10, v));
    if (engineSound)
        engineSound->SetVolume(sfxVolume_ / 10.0f);
    if (adlibEngine)
        adlibEngine->SetVolume(sfxVolume_ / 10.0f);
    if (c64Engine_)
        c64Engine_->SetVolume(sfxVolume_ / 10.0f);
    if (pcSpeakerSfx_)
        pcSpeakerSfx_->SetVolume(sfxVolume_ / 10.0f);
    if (adlibSfx_)
        adlibSfx_->SetVolume(sfxVolume_ / 10.0f);
    if (c64Sfx_)
        c64Sfx_->SetVolume(sfxVolume_ / 10.0f);
}

void AudioManager::SetMusicVolume(int v) {
    musicVolume_ = std::max(0, std::min(10, v));
    if (musicMuted_) return;
    if (musicSource_ == MusicSource::DOS && pcSpeaker_)
        pcSpeaker_->SetAmplitude((int16_t)(musicVolume_ * 800));
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_)
        amigaMusic_->SetVolume(musicVolume_ / 10.0f);
    if (musicSource_ == MusicSource::ADLIB && adlibMusic_)
        adlibMusic_->SetVolume(musicVolume_ / 10.0f);
    if (musicSource_ == MusicSource::C64 && c64Music_)
        c64Music_->SetVolume(musicVolume_ / 10.0f);
}

void AudioManager::SetMusicMuted(bool muted) {
    musicMuted_ = muted;
    if (muted) {
        if (pcSpeaker_)  pcSpeaker_->SetAmplitude(0);
        if (amigaMusic_) amigaMusic_->SetVolume(0.0f);
        if (adlibMusic_) adlibMusic_->SetVolume(0.0f);
        if (c64Music_)   c64Music_->SetVolume(0.0f);
    } else {
        if (musicSource_ == MusicSource::DOS && pcSpeaker_)
            pcSpeaker_->SetAmplitude((int16_t)(musicVolume_ * 800));
        if (musicSource_ == MusicSource::AMIGA && amigaMusic_)
            amigaMusic_->SetVolume(musicVolume_ / 10.0f);
        if (musicSource_ == MusicSource::ADLIB && adlibMusic_)
            adlibMusic_->SetVolume(musicVolume_ / 10.0f);
        if (musicSource_ == MusicSource::C64 && c64Music_)
            c64Music_->SetVolume(musicVolume_ / 10.0f);
    }
}

float AudioManager::RPMToPitchMultiplier(int16_t rpm) {
    const float MIN_RPM = 1500.0f;
    const float MAX_RPM = 7000.0f;
    const float MIN_FREQ = 0.7f;
    const float MAX_FREQ = 2.2f;

    float clampedRPM = std::clamp(static_cast<float>(rpm), MIN_RPM, MAX_RPM);
    float t = (clampedRPM - MIN_RPM) / (MAX_RPM - MIN_RPM);
    return MIN_FREQ + t * (MAX_FREQ - MIN_FREQ);
}
