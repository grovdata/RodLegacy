#include "music_sequencer.h"
#include <fstream>
#include <SDL2/SDL_log.h>
#include <cstring>

MusicSequencer::MusicSequencer() 
    : paula(nullptr), playing(false), paused(false), 
      tickTimer(0.0f), tickDuration(0.05f),  // Base tempo (will be modified by song data)
      currentTick(0), currentPattern(0), patternLength(64),
      currentSong(0), songOffset(0), activeChannel(0) {
}

MusicSequencer::~MusicSequencer() {
    StopMusic();
}

bool MusicSequencer::Initialize(PaulaEmulator* paulaEmulator) {
    if (!paulaEmulator) {
        SDL_Log("MusicSequencer: Invalid Paula emulator");
        return false;
    }
    
    paula = paulaEmulator;
    SDL_Log("MusicSequencer: Initialized");
    return true;
}

std::vector<uint8_t> MusicSequencer::ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SDL_Log("MusicSequencer: Failed to open file: %s", filename.c_str());
        return {};
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    SDL_Log("MusicSequencer: Loaded %zu bytes from %s", size, filename.c_str());
    return data;
}

bool MusicSequencer::LoadMusicFiles(const std::string& noteFile,
                                    const std::string& waveFile,
                                    const std::string& insFile,
                                    const std::string& envFile) {
    noteData = ReadFile(noteFile);
    waveData = ReadFile(waveFile);
    insData = ReadFile(insFile);
    envData = ReadFile(envFile);
    
    if (noteData.empty() || waveData.empty()) {
        SDL_Log("MusicSequencer: Failed to load required music files");
        return false;
    }
    
    SDL_Log("MusicSequencer: Music files loaded successfully");
    
    // Parse .ins file (70 bytes)
    // Format appears to be: [various params] offset(word) length(word) [more params]
    // From hexdump: 00 00 00 01 00 01 1f 40 00 00 = instrument 0
    if (insData.size() >= 10) {
        instruments.clear();
        
        // Parse the .ins file structure
        // Each entry seems to be 10 bytes
        size_t pos = 0;
        while (pos + 10 <= insData.size()) {
            // Skip first 6 bytes (unknown params)
            // Bytes 6-7: offset (big-endian word)
            uint16_t offset = (insData[pos + 6] << 8) | insData[pos + 7];
            // Bytes 8-9: length (big-endian word)  
            uint16_t length = (insData[pos + 8] << 8) | insData[pos + 9];
            
            if (length > 0) {
                instruments.push_back({offset, length});
                SDL_Log("MusicSequencer: Instrument %zu: offset=0x%04X (%u), length=%u", 
                        instruments.size() - 1, offset, offset, length);
            }
            
            pos += 10;
        }
        
        SDL_Log("MusicSequencer: Loaded %zu instruments", instruments.size());
    }
    
    SDL_Log("MusicSequencer: Three songs available:");
    SDL_Log("  Song 0: Offset 0x0000 (Main theme)");
    SDL_Log("  Song 1: Offset 0x05A3 (Variation 1)");
    SDL_Log("  Song 2: Offset 0x0CC3 (Variation 2)");
    
    // Default to song 0
    SelectSong(0);
    
    return true;
}

void MusicSequencer::SelectSong(int songIndex) {
    // Three songs in msr.note at specific offsets
    static const uint32_t songOffsets[] = {
        0x0000,  // Song 0: Main theme (0-1442)
        0x05A3,  // Song 1: Variation 1 (1443-3266)
        0x0CC3   // Song 2: Variation 2 (3267-5223)
    };
    
    if (songIndex < 0 || songIndex > 2) {
        SDL_Log("MusicSequencer: Invalid song index %d, using song 0", songIndex);
        songIndex = 0;
    }
    
    currentSong = songIndex;
    songOffset = songOffsets[songIndex];
    currentTick = 0;
    activeChannel = 0;
    
    // Reset all channel states to start of new song
    for (int i = 0; i < 4; i++) {
        channels[i].notePos = songOffset;
        channels[i].ticksRemaining = 0;
        channels[i].currentInstrument = 0;
    }
    
    SDL_Log("MusicSequencer: Selected song %d at offset 0x%04X", currentSong, songOffset);
}

void MusicSequencer::StartMusic() {
    if (!paula) {
        SDL_Log("MusicSequencer: Paula emulator not initialized");
        return;
    }
    
    currentTick = 0;
    currentPattern = 0;
    tickTimer = 0.0f;
    playing = true;
    paused = false;
    
    SDL_Log("MusicSequencer: Music started");
}

void MusicSequencer::StopMusic() {
    playing = false;
    paused = false;
    
    // Stop all channels
    if (paula) {
        for (int i = 0; i < PaulaEmulator::NUM_CHANNELS; i++) {
            paula->EnableChannel(i, false);
        }
    }
    
    SDL_Log("MusicSequencer: Music stopped");
}

void MusicSequencer::PauseMusic(bool pause) {
    paused = pause;
}

void MusicSequencer::Update(float deltaTime) {
    if (!playing || paused || !paula) {
        return;
    }
    
    tickTimer += deltaTime;
    
    // Process ticks at the specified rate
    while (tickTimer >= tickDuration) {
        tickTimer -= tickDuration;
        ProcessTick();
        
        currentTick++;
        
        // Check if pattern is done (wrap around for now)
        if (currentTick >= patternLength) {
            currentTick = 0;
            currentPattern++;
            SDL_Log("MusicSequencer: Pattern %d complete, looping", currentPattern);
        }
    }
}

void MusicSequencer::ProcessTick() {
    if (noteData.empty() || waveData.empty()) {
        return;
    }
    
    // SEQUENTIAL processing - all channels read from same stream
    // The stream has channel-select commands embedded in it
    ChannelState& ch = channels[0]; // Use channel 0 state for the sequential position
    
    // If we have ticks remaining on current note, just wait
    if (ch.ticksRemaining > 0) {
        ch.ticksRemaining--;
        return;
    }
    
    // Read next command/note from data
    if (ch.notePos >= noteData.size() - 1) {
        // Reached end, loop back to song start
        ch.notePos = songOffset;
        return;
    }
    
    uint8_t cmd = noteData[ch.notePos++];
    
    // Check if it's a control code (0x80-0x97)
    if (cmd >= 0x80 && cmd <= 0x97) {
        uint8_t param = (ch.notePos < noteData.size()) ? noteData[ch.notePos++] : 0;
        
        // Channel select (0x80-0x83)
        if (cmd >= 0x80 && cmd <= 0x83) {
            activeChannel = cmd - 0x80;
            return;
        }
        // Instrument select (0x88-0x89)
        else if (cmd == 0x88 || cmd == 0x89) {
            if (activeChannel >= 0 && activeChannel < 4 && param < instruments.size()) {
                channels[activeChannel].currentInstrument = param % instruments.size();
            }
            return;
        }
        // Tempo/timing controls (0x90-0x97)
        else if (cmd >= 0x90 && cmd <= 0x97) {
            // These might control global timing - for now, just consume the parameter
            return;
        }
        
        return;
    }
    
    // It's a note value (0x20-0x5F)
    if (cmd >= 0x20 && cmd <= 0x5F) {
        uint8_t duration = (ch.notePos < noteData.size()) ? noteData[ch.notePos++] : 1;
        
        // Proper Amiga period table (C-1 to B-3)
        static const uint16_t periodTable[] = {
            856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,  // Octave 1
            428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,  // Octave 2
            214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,  // Octave 3
            107, 101,  95,  90,  85,  80,  76,  71,  67,  64,  60,  57,  // Octave 4
             53,  50,  47,  45,  42,  40,  38,  36,  34,  32,  30,  28   // Octave 5
        };
        
        // Map note value (0x20-0x5F) to period table index
        int noteIndex = cmd - 0x20;
        uint16_t period = (noteIndex < 64) ? periodTable[noteIndex] : 320;
        
        // Get sample for current instrument on the active channel
        int instIdx = channels[activeChannel].currentInstrument;
        if (activeChannel >= 0 && activeChannel < 4 && instIdx < static_cast<int>(instruments.size())) {
            const Instrument& inst = instruments[instIdx];
            
            // Extract sample from waveData
            std::vector<int8_t> sample;
            uint32_t sampleLen = std::min(inst.length, static_cast<uint32_t>(waveData.size() - inst.offset));
            sample.resize(sampleLen);
            
            for (uint32_t i = 0; i < sampleLen; i++) {
                if (inst.offset + i < waveData.size()) {
                    sample[i] = static_cast<int8_t>(waveData[inst.offset + i] - 128);
                }
            }
            
            // Trigger note on current channel
            if (!sample.empty()) {
                paula->SetChannelSample(activeChannel, sample);
                paula->SetChannelPeriod(activeChannel, period);
                paula->SetChannelVolume(activeChannel, 64);
                
                // Loop the tone instrument, but not drums/percussion
                if (instIdx == 0 && sampleLen > 100) {
                    paula->SetChannelLoop(activeChannel, 0, sampleLen);
                } else {
                    paula->SetChannelLoop(activeChannel, 0, 0);  // No loop
                }
                
                paula->EnableChannel(activeChannel, true);
            }
        }
        
        ch.ticksRemaining = duration;
    }
}

void MusicSequencer::ProcessChannelTick(int channelIndex) {
    // Unused - kept for compatibility
}


