#include "music_sequencer.h"
#include <fstream>
#include <SDL2/SDL_log.h>
#include <cstring>

MusicSequencer::MusicSequencer() 
    : paula(nullptr), playing(false), paused(false), 
      tickTimer(0.0f), tickDuration(0.08f),  // Slower tempo based on user feedback
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
    
    // Use hardcoded instrument offsets from audio.md documentation
    // These are known-good values from analyzing the actual game
    instruments.clear();
    
    // Parse instruments from .ins file (10-byte entries: offset(4), length(4), flags(2))
    if (insData.size() >= 10) {
        for (size_t i = 0; i < insData.size(); i += 10) {
            if (i + 9 < insData.size()) {
                // Read offset and length (little-endian)
                uint32_t offset = insData[i] | (insData[i+1] << 8) | (insData[i+2] << 16) | (insData[i+3] << 24);
                uint32_t length = insData[i+4] | (insData[i+5] << 8) | (insData[i+6] << 16) | (insData[i+7] << 24);
                
                if (length > 0 && offset + length <= waveData.size()) {
                    instruments.push_back({offset, length});
                }
            }
        }
    }
    
    // Fallback to hardcoded values if .ins parsing failed
    if (instruments.empty()) {
        SDL_Log("MusicSequencer: .ins parsing failed, using hardcoded values");
        instruments.push_back({0x0000, 8000});   // Instrument 0: tone (melody)
        instruments.push_back({0x14B4, 8000});   // Instrument 1: tone variant
        instruments.push_back({0x1450, 13300});  // Instrument 2: bass
        instruments.push_back({0x07D0, 18500});  // Instrument 3: harmony
        instruments.push_back({0x1A2C, 20500});  // Instrument 4: buzz
    }
    
    for (size_t i = 0; i < instruments.size(); i++) {
        SDL_Log("MusicSequencer: Instrument %zu: offset=0x%04X, length=%u", 
                i, instruments[i].offset, instruments[i].length);
    }
    
    SDL_Log("MusicSequencer: Loaded %zu instruments from .ins file", instruments.size());
    
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

void MusicSequencer::SetChannelInstrument(int channel, int instrumentIndex) {
    if (channel < 0 || channel >= 4) {
        SDL_Log("MusicSequencer: Invalid channel %d", channel);
        return;
    }
    
    if (instrumentIndex < 0 || instrumentIndex >= static_cast<int>(instruments.size())) {
        SDL_Log("MusicSequencer: Invalid instrument index %d", instrumentIndex);
        return;
    }
    
    channels[channel].currentInstrument = instrumentIndex;
    SDL_Log("MusicSequencer: Channel %d instrument set to %d", channel, instrumentIndex);
    ShowInstrumentAssignments();
}

void MusicSequencer::ShowInstrumentAssignments() {
    SDL_Log("Current instrument assignments:");
    for (int i = 0; i < 4; i++) {
        const char* instName = "unknown";
        switch (channels[i].currentInstrument) {
            case 0: instName = "tone"; break;
            case 1: instName = "bassdrum"; break;
            case 2: instName = "hihat"; break;
            case 3: instName = "buzz"; break;
        }
        SDL_Log("  Channel %d: Instrument %d (%s)", i, channels[i].currentInstrument, instName);
    }
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
    ShowInstrumentAssignments();
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
    
    // Handle 0x00 as rest/silence
    if (cmd == 0x00) {
        // Silence - stop active channel or just skip
        return;
    }
    
    // Handle 0x01-0x1F as possible instrument/volume selectors
    if (cmd >= 0x01 && cmd <= 0x1F) {
        // These might be instrument or volume changes
        // For now, just consume and ignore
        return;
    }
    
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
            if (activeChannel >= 0 && activeChannel < 4) {
                // Map the parameter to an instrument
                // Common values: 0x37, 0x40, 0x2C, etc.
                // Based on user feedback: channel 1 uses "tone", channel 3 uses "buzz"
                // Let's try a simple mapping based on the parameter value
                
                // For now, use a basic heuristic:
                // - Lower values (< 0x40) = instrument 0 (tone)
                // - Higher values (>= 0x40) = instrument 3 (buzz)
                if (param < 0x40) {
                    channels[activeChannel].currentInstrument = 0; // tone
                } else {
                    channels[activeChannel].currentInstrument = 3; // buzz
                }
                
                SDL_Log("Channel %d: Instrument select cmd=0x%02X param=0x%02X -> inst %d", 
                        activeChannel, cmd, param, channels[activeChannel].currentInstrument);
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
               // Use manually set instrument if available, otherwise use the one from note data
               int instIdx = channels[activeChannel].currentInstrument;
               if (activeChannel >= 0 && activeChannel < 4 && instIdx < static_cast<int>(instruments.size())) {
            const Instrument& inst = instruments[instIdx];
            
            // Extract sample from waveData
            std::vector<int8_t> sample;
            uint32_t sampleLen = std::min(inst.length, static_cast<uint32_t>(waveData.size() - inst.offset));
            sample.resize(sampleLen);
            
                   for (uint32_t i = 0; i < sampleLen; i++) {
                       if (inst.offset + i < waveData.size()) {
                           // Try treating as signed 8-bit directly (no -128 conversion)
                           sample[i] = static_cast<int8_t>(waveData[inst.offset + i]);
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


