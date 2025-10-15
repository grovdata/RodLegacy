#include "audio_manager.h"
#include "engine_sound.h"
#include "../parsers/adf.h"
#include "../parsers/libp.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <algorithm>

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

bool AudioManager::Initialize(const char* disk1AdfPath) {
    SDL_Log("AudioManager: Initializing...");
    
    // Initialize SDL audio subsystem if not already done
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            SDL_Log("Failed to initialize SDL audio: %s", SDL_GetError());
            return false;
        }
    }
    
    // Load audio files from ADF first
    if (!LoadAudioFromADF(disk1AdfPath, "srs0.wave")) {
        SDL_Log("Warning: Failed to load srs0.wave (racing engine sound)");
    }
    
    if (!LoadAudioFromADF(disk1AdfPath, "srl0.wave")) {
        SDL_Log("Warning: Failed to load srl0.wave (other racing sounds)");
    }
    
    if (!LoadAudioFromADF(disk1AdfPath, "srl1.wave")) {
        SDL_Log("Warning: Failed to load srl1.wave (music samples)");
    }
    
    // Initialize engine sound system with the extracted engine sample
    engineSound = std::make_unique<EngineSound>();
    AudioSample* engineSample = GetSample(AudioSampleType::ENGINE_LOOP);
    if (engineSample && !engineSample->data.empty()) {
        if (!engineSound->Initialize(engineSample->data)) {
            SDL_Log("Warning: Failed to initialize engine sound");
            engineSound.reset();
        } else {
            SDL_Log("Engine sound initialized successfully from ADF data");
        }
    } else {
        SDL_Log("Warning: No engine sample data found in ADF");
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
        SDL_Log("Warning: Failed to open audio device for one-shots: %s", SDL_GetError());
    } else {
        SDL_Log("AudioManager: One-shot audio device opened successfully");
        SDL_PauseAudioDevice(audioDevice, 0);  // Unpause to allow queueing
    }
    
    SDL_Log("AudioManager: Loaded %zu audio samples", samples.size());
    return true;  // Return true even if samples not loaded, engine sound might work
}

void AudioManager::Shutdown() {
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

bool AudioManager::LoadAudioFromADF(const char* adfPath, const char* filename) {
    SDL_Log("AudioManager: Loading %s from %s", filename, adfPath);
    
    AdfFileData* fileData = extract_adf_file_to_memory(adfPath, filename);
    if (!fileData) {
        SDL_Log("AudioManager: Failed to extract %s from ADF", filename);
        return false;
    }
    
    bool success = false;
    
    // Check if this is srs0.wave (racing engine sound)
    if (strcmp(filename, "srs0.wave") == 0) {
        ExtractSRS0Samples(fileData->data, fileData->size);
        success = true;
    }
    // Check if this is srl0.wave (other racing sounds)
    else if (strcmp(filename, "srl0.wave") == 0) {
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

void AudioManager::ExtractSRS0Samples(const uint8_t* data, size_t size) {
    SDL_Log("AudioManager: Extracting samples from srs0.wave (%zu bytes)", size);
    
    // Racing engine sound - LOOPED and pitch-modulated based on RPM
    // Periods: idle=252 (~14204 Hz), max=193 (~18548 Hz)
    // Audio is SIGNED 8-bit
    if (size >= SRS0_ENGINE_LOOP_LENGTH) {
        AudioSample engineSample;
        LoadRawAudio(data + SRS0_ENGINE_LOOP_OFFSET, SRS0_ENGINE_LOOP_LENGTH, 
                     engineSample, SRS0_ENGINE_BASE_RATE);
        engineSample.isLooped = true;
        engineSample.loopStart = 0;
        engineSample.loopLength = SRS0_ENGINE_LOOP_LENGTH;
        samples[AudioSampleType::ENGINE_LOOP] = engineSample;
        SDL_Log("  - Racing engine sound: %u bytes (looped) at %u Hz base", 
                SRS0_ENGINE_LOOP_LENGTH, SRS0_ENGINE_BASE_RATE);
    }
    
    // TODO: Extract other samples from srs0.wave if needed
}

void AudioManager::ExtractSRL0Samples(const uint8_t* data, size_t size) {
    SDL_Log("AudioManager: Extracting samples from srl0.wave (%zu bytes)", size);
    
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
        SDL_Log("  - Engine starting sound: %u bytes (one-shot) at %u Hz", 
                SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    }
    
    // TODO: Extract other samples from srl0.wave
    // The exact offsets for ignition, tire screech, honk, crash, and cop siren
    // need to be determined from the Amiga disassembly or by analyzing the file
    
    // For now, we'll just note that the rest exists
    if (size > 12000) {
        SDL_Log("  - Additional sound effects: %zu bytes (offsets TBD)", size - 12000);
    }
}

void AudioManager::ExtractSRL1Samples(const uint8_t* data, size_t size) {
    SDL_Log("AudioManager: Extracting samples from srl1.wave (%zu bytes)", size);
    
    // Music samples - used by California Dreams custom music format
    // See libp.h for detailed offset documentation
    
    // Tone sample (lead melody)
    if (size >= SRL1_TONE_OFFSET + SRL1_TONE_LENGTH) {
        AudioSample toneSample;
        LoadRawAudio(data + SRL1_TONE_OFFSET, SRL1_TONE_LENGTH, 
                     toneSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_TONE] = toneSample;
        SDL_Log("  - Tone: %u bytes at offset 0x%04X", SRL1_TONE_LENGTH, SRL1_TONE_OFFSET);
    }
    
    // Bassdrum percussion
    if (size >= SRL1_BASSDRUM_OFFSET + SRL1_BASSDRUM_LENGTH) {
        AudioSample bassSample;
        LoadRawAudio(data + SRL1_BASSDRUM_OFFSET, SRL1_BASSDRUM_LENGTH, 
                     bassSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_BASSDRUM] = bassSample;
        SDL_Log("  - Bassdrum: %u bytes at offset 0x%04X", SRL1_BASSDRUM_LENGTH, SRL1_BASSDRUM_OFFSET);
    }
    
    // Hihat percussion
    if (size >= SRL1_HIHAT_OFFSET + SRL1_HIHAT_LENGTH) {
        AudioSample hihatSample;
        LoadRawAudio(data + SRL1_HIHAT_OFFSET, SRL1_HIHAT_LENGTH, 
                     hihatSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_HIHAT] = hihatSample;
        SDL_Log("  - Hihat: %u bytes at offset 0x%04X", SRL1_HIHAT_LENGTH, SRL1_HIHAT_OFFSET);
    }
    
    // Buzz effect (rest of file)
    if (size >= SRL1_BUZZ_OFFSET) {
        uint32_t buzzLength = size - SRL1_BUZZ_OFFSET;
        AudioSample buzzSample;
        LoadRawAudio(data + SRL1_BUZZ_OFFSET, buzzLength, 
                     buzzSample, SRL1_MUSIC_SAMPLE_RATE);
        samples[AudioSampleType::MUSIC_BUZZ] = buzzSample;
        SDL_Log("  - Buzz: %u bytes at offset 0x%04X", buzzLength, SRL1_BUZZ_OFFSET);
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
            SDL_Log("  8SVX VHDR: rate=%d, oneshot=%u, repeat=%u, loop=%d",
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
            SDL_Log("  8SVX BODY: %u bytes", chunkSize);
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
    auto it = samples.find(type);
    if (it == samples.end() || it->second.data.empty()) {
        SDL_Log("AudioManager::PlaySound - Sample %d not found", static_cast<int>(type));
        return;
    }
    
    const AudioSample& sample = it->second;
    SDL_Log("AudioManager::PlaySound - Playing sample %d at %.2f Hz, volume %.2f", 
            static_cast<int>(type), sample.sampleRate, volume);
    
    if (audioDevice == 0) {
        SDL_Log("AudioManager::PlaySound - No audio device available");
        return;
    }
    
    // Create an audio stream to convert the sample to device format
    // One-shot device is 22050 Hz, stereo, 16-bit (see Initialize)
    SDL_AudioStream* stream = SDL_NewAudioStream(
        AUDIO_S8, 1, static_cast<int>(sample.sampleRate),  // Source: signed 8-bit mono
        AUDIO_S16SYS, 2, 22050                              // Dest: 16-bit stereo at 22050 Hz
    );
    
    if (!stream) {
        SDL_Log("AudioManager::PlaySound - Failed to create audio stream: %s", SDL_GetError());
        return;
    }
    
    // Put the sample data into the stream
    if (SDL_AudioStreamPut(stream, sample.data.data(), static_cast<int>(sample.data.size())) < 0) {
        SDL_Log("AudioManager::PlaySound - Failed to put data in stream: %s", SDL_GetError());
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
            // Apply volume
            for (size_t i = 0; i < convertedData.size(); i++) {
                convertedData[i] = static_cast<int16_t>(convertedData[i] * volume);
            }
            
            // Queue the audio for playback on the engine sound device
            SDL_QueueAudio(audioDevice, convertedData.data(), got);
        }
    }
    
    SDL_FreeAudioStream(stream);
}

int AudioManager::StartLoop(AudioSampleType type, float volume) {
    if (type == AudioSampleType::ENGINE_LOOP && engineSound) {
        engineSound->SetVolume(volume);
        engineSound->Start();
        SDL_Log("AudioManager::StartLoop - Engine sound started at volume %.2f", volume);
        return 0;  // Return channel 0 for engine sound
    }
    
    SDL_Log("AudioManager::StartLoop(%d) - not yet implemented for this type", static_cast<int>(type));
    return -1;
}

void AudioManager::StopLoop(int channelId) {
    if (channelId == 0 && engineSound) {  // Channel 0 is engine sound
        engineSound->Stop();
        SDL_Log("AudioManager::StopLoop - Engine sound stopped");
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

void AudioManager::SetChannelVolume(int channelId, float volume) {
    // TODO: Implement volume control
}

float AudioManager::RPMToPitchMultiplier(int16_t rpm) {
    // Map RPM 1500-7000 to pitch multiplier 0.7-2.2
    const float MIN_RPM = 1500.0f;
    const float MAX_RPM = 7000.0f;
    const float MIN_FREQ = 0.7f;
    const float MAX_FREQ = 2.2f;
    
    float clampedRPM = std::max(MIN_RPM, std::min(MAX_RPM, static_cast<float>(rpm)));
    float t = (clampedRPM - MIN_RPM) / (MAX_RPM - MIN_RPM);
    return MIN_FREQ + t * (MAX_FREQ - MIN_FREQ);
}
