#include "../log.h"
#include "audio_manager.h"
#include "amiga_music.h"
#include "engine_sound.h"
#include "pc_speaker.h"
#include "../parsers/adf.h"
#include "../parsers/libp.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <random>
#include <set>

// Helper to read big-endian values
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

    // Initialize SDL audio subsystem if not already done
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            LOG_INFO("Failed to initialize SDL audio: %s", SDL_GetError());
            return false;
        }
    }

    // Load audio files from ADF image
    if (disk1AdfData && disk1AdfSize > 0) {
        if (!LoadAudioFromADF(disk1AdfData, disk1AdfSize, "srl0.wave")) {
            LOG_INFO("Warning: Failed to load srl0.wave (engine loop and effects)");
        }
        if (!LoadAudioFromADF(disk1AdfData, disk1AdfSize, "srl1.wave")) {
            LOG_INFO("Warning: Failed to load srl1.wave (music samples)");
        }
    }

    // Initialize engine sound system with the extracted engine sample
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

    // Open a second audio device for one-shot sounds (using queue mode)
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
        SDL_PauseAudioDevice(audioDevice, 0);  // Unpause to allow queueing
    }

    LOG_INFO("AudioManager: Loaded %zu audio samples", samples.size());
    return true;  // Return true even if samples not loaded, engine sound might work
}

void AudioManager::Shutdown() {
    // Shutdown PC speaker music
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

    // Shutdown engine sound
    if (engineSound) {
        engineSound->Shutdown();
        engineSound.reset();
    }

    // Close one-shot audio device
    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }

    samples.clear();
}

bool AudioManager::HasAmigaMusic() const { return amigaMusic_ != nullptr; }

void AudioManager::InitAmigaMusic(const uint8_t* disk1AdfData, size_t disk1AdfSize) {
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
        envFile->data, envFile->size);

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

void AudioManager::SetMusicSource(MusicSource src) {
    // Stop whatever is currently playing.
    if (pcSpeaker_) pcSpeaker_->Stop();
    if (amigaMusic_) amigaMusic_->Stop();

    musicSource_ = src;

    // Muting is a gain control, not a reason to skip starting the source.
    // Returning early here used to leave the player never started: LoadSettings
    // applies music_muted before music_source, so a config with music_muted=1
    // meant Play() was never reached, and unmuting later only re-applied volume
    // to something that had never begun. Music stayed dead for the whole run.
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

    // Check if this is srl0.wave (engine loop and effects)
    if (strcmp(filename, "srl0.wave") == 0) {
        ExtractSRL0Samples(fileData->data, fileData->size);
        success = true;
    }
    // Check if this is srl1.wave (music samples)
    else if (strcmp(filename, "srl1.wave") == 0) {
        ExtractSRL1Samples(fileData->data, fileData->size);
        success = true;
    }
    // Try to load as 8SVX format
    else if (fileData->size > 8 && memcmp(fileData->data, "FORM", 4) == 0) {
        AudioSample sample;
        if (Load8SVX(fileData->data, fileData->size, sample)) {
            // Store with generic type for now
            samples[AudioSampleType::MUSIC_TONE] = sample;
            success = true;
        }
    }

    free_adf_file_data(fileData);
    return success;
}

void AudioManager::ExtractSRL0Samples(const uint8_t* data, size_t size) {
    LOG_INFO("AudioManager: Extracting samples from srl0.wave (%zu bytes)", size);

    // Racing engine loop - LOOPED and pitch-modulated per tick.
    // Periods: idle=252 (~14204 Hz), redline=193 (~18548 Hz). SIGNED 8-bit.
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

    // Engine starting sound - for car startup in garage/gas station
    // Amiga period 320 = 3579545 / 320 = 11186 Hz
    // Audio is SIGNED 8-bit (not unsigned!)
    if (size >= SRL0_ENGINE_START_LENGTH) {
        AudioSample engineStartSample;
        LoadRawAudio(data + SRL0_ENGINE_START_OFFSET, SRL0_ENGINE_START_LENGTH,
                     engineStartSample, SRL0_ENGINE_START_RATE);
        engineStartSample.isLooped = false;  // One-shot sound
        engineStartSample.loopStart = 0;
        engineStartSample.loopLength = 0;
        samples[AudioSampleType::ENGINE_START] = engineStartSample;
        LOG_INFO("  - Engine starting sound: %u bytes (one-shot) at %u Hz",
                SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    }

    // Car bash/impact sound - for chopping roof and stripping bumpers
    if (size >= SRL0_CAR_BASH_OFFSET + SRL0_CAR_BASH_LENGTH) {
        AudioSample carBashSample;
        LoadRawAudio(data + SRL0_CAR_BASH_OFFSET, SRL0_CAR_BASH_LENGTH,
                     carBashSample, SRL0_CAR_BASH_RATE);
        carBashSample.isLooped = false;  // One-shot sound
        carBashSample.loopStart = 0;
        carBashSample.loopLength = 0;
        samples[AudioSampleType::CAR_BASH] = carBashSample;
        LOG_INFO("  - Car bash sound: %u bytes (one-shot) at %u Hz",
                SRL0_CAR_BASH_LENGTH, SRL0_CAR_BASH_RATE);
    }

    // Car jack sound - for raising/lowering car during tire change
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

    // Tire screech sound - RaceLogicUpdate (0x244d50), see RACING_TODO.md
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

    // Cop siren - the in-race speeding pursuit. The original plays this on two
    // Paula channels at volume 0x36 (AudioStartCopSiren 0x228994), retriggered
    // while the cop is closing. See RodLegacyResearch/docs/driving/cop.md
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

    // TODO: Extract other samples from srl0.wave
    // The exact offsets for ignition, honk and crash
    // need to be determined from the Amiga disassembly or by analyzing the file

    // For now, we'll just note that the rest exists
    if (size > 12000) {
        LOG_INFO("  - Additional sound effects: %zu bytes (offsets TBD)", size - 12000);
    }
}

void AudioManager::ExtractSRL1Samples(const uint8_t* data, size_t size) {
    LOG_INFO("AudioManager: Extracting samples from srl1.wave (%zu bytes)", size);

    // Music samples - used by California Dreams custom music format
    // See libp.h for detailed offset documentation

    // Tone sample (lead melody)
    if (size >= SRL1_TONE_OFFSET + SRL1_TONE_LENGTH) {
        AudioSample toneSample;
        LoadRawAudio(data + SRL1_TONE_OFFSET, SRL1_TONE_LENGTH,
                     toneSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_TONE] = toneSample;
        LOG_INFO("  - Tone: %u bytes at offset 0x%04X", SRL1_TONE_LENGTH, SRL1_TONE_OFFSET);
    }

    // Bassdrum percussion
    if (size >= SRL1_BASSDRUM_OFFSET + SRL1_BASSDRUM_LENGTH) {
        AudioSample bassSample;
        LoadRawAudio(data + SRL1_BASSDRUM_OFFSET, SRL1_BASSDRUM_LENGTH,
                     bassSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_BASSDRUM] = bassSample;
        LOG_INFO("  - Bassdrum: %u bytes at offset 0x%04X", SRL1_BASSDRUM_LENGTH, SRL1_BASSDRUM_OFFSET);
    }

    // Hihat percussion
    if (size >= SRL1_HIHAT_OFFSET + SRL1_HIHAT_LENGTH) {
        AudioSample hihatSample;
        LoadRawAudio(data + SRL1_HIHAT_OFFSET, SRL1_HIHAT_LENGTH,
                     hihatSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_HIHAT] = hihatSample;
        LOG_INFO("  - Hihat: %u bytes at offset 0x%04X", SRL1_HIHAT_LENGTH, SRL1_HIHAT_OFFSET);
    }

    // Buzz effect (rest of file)
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

    // Check for FORM chunk
    if (memcmp(data, "FORM", 4) != 0) return false;

    uint32_t formSize = ReadBE32(data + 4);

    // Check for 8SVX identifier
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
            // Voice header
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
            // Sample data (already signed 8-bit)
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

void AudioManager::PlaySound(AudioSampleType type, float volume) {
    // A missing sample or a dead device is a standing condition, and callers
    // retrigger sounds every few ticks, so report each of these once rather
    // than on every call.
    static std::set<int> reportedMissing;
    static bool reportedNoDevice = false;

    auto it = samples.find(type);
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

    // Create an audio stream to convert the sample to device format
    // One-shot device is 22050 Hz, stereo, 16-bit (see Initialize)
    SDL_AudioStream* stream = SDL_NewAudioStream(
        AUDIO_S8, 1, static_cast<int>(sample.sampleRate),  // Source: signed 8-bit mono
        AUDIO_S16SYS, 2, 22050                              // Dest: 16-bit stereo at 22050 Hz
    );

    if (!stream) {
        LOG_INFO("AudioManager::PlaySound - Failed to create audio stream: %s", SDL_GetError());
        return;
    }

    // Put the sample data into the stream
    if (SDL_AudioStreamPut(stream, sample.data.data(), static_cast<int>(sample.data.size())) < 0) {
        LOG_INFO("AudioManager::PlaySound - Failed to put data in stream: %s", SDL_GetError());
        SDL_FreeAudioStream(stream);
        return;
    }

    // Flush and get converted data
    SDL_AudioStreamFlush(stream);
    int available = SDL_AudioStreamAvailable(stream);
    if (available > 0) {
        std::vector<int16_t> convertedData(available / 2);  // 16-bit = 2 bytes per sample
        int got = SDL_AudioStreamGet(stream, convertedData.data(), available);

        if (got > 0) {
            // Apply volume (caller volume * sfx master volume)
            float effectiveVolume = volume * (sfxVolume_ / 10.0f);
            for (size_t i = 0; i < convertedData.size(); i++) {
                convertedData[i] = static_cast<int16_t>(convertedData[i] * effectiveVolume);
            }

            // Queue the audio for playback on the engine sound device
            SDL_QueueAudio(audioDevice, convertedData.data(), got);
        }
    }

    SDL_FreeAudioStream(stream);
}

int AudioManager::StartLoop(AudioSampleType type, float volume) {
    if (type == AudioSampleType::ENGINE_LOOP && engineSound) {
        engineSound->SetVolume(volume * (sfxVolume_ / 10.0f));
        engineSound->Start();
        LOG_INFO("AudioManager::StartLoop - Engine sound started at volume %.2f", volume);
        return 0;  // Return channel 0 for engine sound
    }

    LOG_INFO("AudioManager::StartLoop(%d) - not yet implemented for this type", static_cast<int>(type));
    return -1;
}

void AudioManager::StopLoop(int channelId) {
    if (channelId == 0 && engineSound) {  // Channel 0 is engine sound
        engineSound->Stop();
        LOG_INFO("AudioManager::StopLoop - Engine sound stopped");
    }
}

void AudioManager::UpdateEnginePitch(int16_t rpm) {
    currentRPM = rpm;
    if (engineSound) {
        engineSound->UpdateRPM(rpm);
    }
}

void AudioManager::UpdateEnginePeriod(int16_t period) {
    if (engineSound) {
        engineSound->UpdatePeriod(period);
    }
}

void AudioManager::SetChannelVolume(int /*channelId*/, float /*volume*/) {
    // not yet implemented
}

// CODE_24 segment classification helpers.
// Intro = ascending scale: first note is (dur=5, pit=0x0474).
static bool IsIntroSegment(const std::vector<SrNote>& seg) {
    return !seg.empty() && seg[0].duration_ticks == 5 && seg[0].pit_divisor == 0x0474;
}

// Rest = all notes are silent (pit_divisor == 0).
static bool IsRestSegment(const std::vector<SrNote>& seg) {
    for (const auto& n : seg)
        if (n.pit_divisor != 0) return false;
    return !seg.empty();
}

// Fallback: build a shuffled playlist treating each CODE_24 segment as a song.
// Used when the segment count doesn't match the expected 18-segment SR.EXE layout.
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
        // No songs identified -- fall back to flat concatenation.
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

void AudioManager::InitPCSpeakerFromExeBytes(const uint8_t* data, size_t len) {
    std::vector<std::vector<SrNote>> segs;
    if (!sr_exe_parse_music_bytes(data, len, segs)) {
        LOG_INFO("AudioManager: DOS EXE buffer not recognized as SR.EXE, no PC speaker music");
        return;
    }

    if (sr_exe_build_known_songs(segs, pcSongs_)) {
        pcSongNames_ = {"Song 1", "Song 2", "Song 3", "Song 4", "Song 5"};
        LOG_INFO("AudioManager: PC speaker: %zu songs loaded", pcSongs_.size());
    } else {
        LOG_INFO("AudioManager: unexpected segment count (%zu), using fallback", segs.size());
        // Fallback: one big playlist as a single "song".
        pcSongs_.clear();
        pcSongs_.push_back(BuildShuffledPlaylist(segs));
        pcSongNames_ = {"Music"};
    }
    currentPCSong_ = 0;

    pcSpeaker_ = std::make_unique<PCSpeaker>();
    if (!pcSpeaker_->Init()) {
        LOG_INFO("AudioManager: PCSpeaker audio init failed");
        pcSpeaker_.reset();
        pcSongs_.clear();
        pcSongNames_.clear();
        return;
    }

    pcSpeaker_->SetAmplitude(0);  // volume applied when SetMusicSource selects DOS
    pcSpeaker_->SetNotes(pcSongs_[0]);
    LOG_INFO("AudioManager: PC speaker music loaded (%zu songs)", pcSongs_.size());
}

void AudioManager::UpdateMusic() {
    if (musicSource_ != MusicSource::DOS || !pcSpeaker_) return;
    if (pcSongs_.size() < 2) return;
    if (!pcSpeaker_->ConsumeLoopFlag()) return;

    static std::mt19937 rng(std::random_device{}());
    int next;
    do {
        next = std::uniform_int_distribution<int>(0, (int)pcSongs_.size() - 1)(rng);
    } while (next == currentPCSong_);

    currentPCSong_ = next;
    pcSpeaker_->SetNotes(pcSongs_[(size_t)currentPCSong_]);
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

// --- Source-agnostic music control ---

bool AudioManager::HasActiveMusic() const {
    if (musicSource_ == MusicSource::DOS)   return HasPCSpeaker();
    if (musicSource_ == MusicSource::AMIGA) return HasAmigaMusic();
    return false;
}

void AudioManager::NextSong() {
    if (musicSource_ == MusicSource::DOS) NextPCSong();
    // Amiga has a single continuous track -- no-op
}

void AudioManager::PrevSong() {
    if (musicSource_ == MusicSource::DOS) PrevPCSong();
}

void AudioManager::PauseMusic() {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  pcSpeaker_->Pause();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) amigaMusic_->Pause();
}

void AudioManager::ResumeMusic() {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  pcSpeaker_->Resume();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) amigaMusic_->Resume();
}

bool AudioManager::IsMusicPaused() const {
    if (musicSource_ == MusicSource::DOS   && pcSpeaker_)  return pcSpeaker_->IsPaused();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) return amigaMusic_->IsPaused();
    return false;
}

std::string AudioManager::GetCurrentSongName() const {
    if (musicSource_ == MusicSource::DOS)   return GetCurrentPCSongName();
    if (musicSource_ == MusicSource::AMIGA) return "Amiga Music";
    return "";
}

int AudioManager::GetCurrentSongIndex() const {
    if (musicSource_ == MusicSource::DOS) return GetCurrentPCSongIndex();
    return 0;
}

int AudioManager::GetSongCount() const {
    if (musicSource_ == MusicSource::DOS) return GetPCSongCount();
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_) return 1;
    return 0;
}

void AudioManager::SetSfxVolume(int v) {
    sfxVolume_ = std::max(0, std::min(10, v));
    if (engineSound)
        engineSound->SetVolume(sfxVolume_ / 10.0f);
}

void AudioManager::SetMusicVolume(int v) {
    musicVolume_ = std::max(0, std::min(10, v));
    if (musicMuted_) return;
    if (musicSource_ == MusicSource::DOS && pcSpeaker_)
        pcSpeaker_->SetAmplitude((int16_t)(musicVolume_ * 800));
    if (musicSource_ == MusicSource::AMIGA && amigaMusic_)
        amigaMusic_->SetVolume(musicVolume_ / 10.0f);
}

void AudioManager::SetMusicMuted(bool muted) {
    musicMuted_ = muted;
    if (muted) {
        if (pcSpeaker_)  pcSpeaker_->SetAmplitude(0);
        if (amigaMusic_) amigaMusic_->SetVolume(0.0f);
    } else {
        // Re-apply volume only to the active source.
        if (musicSource_ == MusicSource::DOS && pcSpeaker_)
            pcSpeaker_->SetAmplitude((int16_t)(musicVolume_ * 800));
        if (musicSource_ == MusicSource::AMIGA && amigaMusic_)
            amigaMusic_->SetVolume(musicVolume_ / 10.0f);
    }
}

float AudioManager::RPMToPitchMultiplier(int16_t rpm) {
    // Map RPM 1500-7000 to pitch multiplier 0.7-2.2
    const float MIN_RPM = 1500.0f;
    const float MAX_RPM = 7000.0f;
    const float MIN_FREQ = 0.7f;
    const float MAX_FREQ = 2.2f;

    float clampedRPM = std::clamp(static_cast<float>(rpm), MIN_RPM, MAX_RPM);
    float t = (clampedRPM - MIN_RPM) / (MAX_RPM - MIN_RPM);
    return MIN_FREQ + t * (MAX_FREQ - MIN_FREQ);
}
