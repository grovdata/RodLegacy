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
    
    // Parse instruments from .ins file (8-byte entries: offset(2), instrument#(2), length(2), params(2))
    if (insData.size() >= 8) {
        for (size_t i = 0; i < insData.size(); i += 8) {
            if (i + 7 < insData.size()) {
                // Read 4 words (little-endian)
                uint16_t offset = insData[i] | (insData[i+1] << 8);
                uint16_t inst_num = insData[i+2] | (insData[i+3] << 8);
                uint16_t length = insData[i+4] | (insData[i+5] << 8);
                uint16_t params = insData[i+6] | (insData[i+7] << 8);
                
                // Only use entries with reasonable values (minimum 1000 bytes for musical samples)
                if (length >= 1000 && length < 10000 && offset + length <= waveData.size()) {
                    instruments.push_back({offset, length});
                    SDL_Log("MusicSequencer: Parsed instrument %zu: offset=0x%04X, length=%u, inst_num=%u, params=0x%04X", 
                            instruments.size()-1, offset, length, inst_num, params);
                }
            }
        }
    }
    
        // Fallback to hardcoded values if .ins parsing failed
        // Based on disassembly analysis: srl1.wave has 5 samples with realistic lengths
        if (instruments.empty()) {
            SDL_Log("MusicSequencer: .ins parsing failed, using disassembly-based values from srl1.wave");
            instruments.push_back({0x0000, 6700});   // Instrument 0: lead tone (0x0000-0x1A2C)
            instruments.push_back({0x1A2C, 8000});   // Instrument 1: electric bass (0x1A2C-0x396C)
            instruments.push_back({0x396C, 5300});   // Instrument 2: bassdrum (0x396C-0x4E20)
            instruments.push_back({0x4E20, 5200});   // Instrument 3: hihat (0x4E20-0x6270)
            instruments.push_back({0x6270, 2000});   // Instrument 4: electric guitar (0x6270-end of file)
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
    // Use single stream with proper channel selection
    for (int i = 0; i < 4; i++) {
        channels[i].notePos = songOffset;  // All channels start at same position
        channels[i].ticksRemaining = 0;
        // Set appropriate initial instruments based on Paula log analysis
        // Channel 0: lead melody, Channel 1: lead melody, Channel 2: hihat, Channel 3: bassdrum
        switch (i) {
            case 0: channels[i].currentInstrument = 0; break; // lead melody (sample 0: 6700 bytes)
            case 1: channels[i].currentInstrument = 0; break; // lead melody (sample 0: 6700 bytes) - primary channel
            case 2: channels[i].currentInstrument = 3; break; // hihat (sample 3: 5200 bytes)
            case 3: channels[i].currentInstrument = 2; break; // bassdrum (sample 2: 5300 bytes)
        }
        channels[i].manuallyMuted = false;
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

void MusicSequencer::ToggleChannelMute(int channel) {
    if (channel < 0 || channel >= 4) {
        SDL_Log("MusicSequencer: Invalid channel %d", channel);
        return;
    }
    
    channels[channel].manuallyMuted = !channels[channel].manuallyMuted;
    
    // Immediately update the Paula emulator channel state
    if (paula) {
        paula->EnableChannel(channel, !channels[channel].manuallyMuted);
    }
    
    SDL_Log("Channel %d: %s", channel, channels[channel].manuallyMuted ? "muted" : "unmuted");
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
    
    // Decrement ticks for all channels that are playing
    for (int ch = 0; ch < 4; ch++) {
        if (channels[ch].ticksRemaining > 0) {
            channels[ch].ticksRemaining--;
        }
    }
    
    // Process single stream with proper channel selection
    ChannelState& mainCh = channels[0];
    
    // Process multiple events per tick to allow polyphony
    int eventsProcessed = 0;
    const int maxEventsPerTick = 20;
    
    while (eventsProcessed < maxEventsPerTick) {
        // If main channel has ticks remaining, don't advance the stream
        if (mainCh.ticksRemaining > 0) {
            break;
        }
        
        eventsProcessed++;
    
        // Read next command/note from data
        if (mainCh.notePos >= noteData.size() - 1) {
            // Reached end, loop back to song start
            mainCh.notePos = songOffset;
            return;
        }
        
        uint8_t cmd = noteData[mainCh.notePos++];
        
        // Handle 0x00 as rest/silence
        if (cmd == 0x00) {
            // Rest - set a short duration to pause
            mainCh.ticksRemaining = 1;
            return;
        }
        
        // Handle 0x01-0x1F as possible instrument/volume selectors
        if (cmd >= 0x01 && cmd <= 0x1F) {
            // These might be instrument or volume changes
            SDL_Log("MusicSequencer: Unknown command 0x%02X (0x01-0x1F range) - treating as instrument/volume", cmd);
            continue; // Continue processing instead of stopping
        }
        
        // Check if it's a control code (0x80-0x97)
        if (cmd >= 0x80 && cmd <= 0x97) {
            uint8_t param = (mainCh.notePos < noteData.size()) ? noteData[mainCh.notePos++] : 0;
            
            // Commands 0x80-0x83 are NOT channel selectors - they're some other control mechanism
            if (cmd >= 0x80 && cmd <= 0x83) {
                SDL_Log("MusicSequencer: Control command 0x%02X param=0x%02X (not channel select)", cmd, param);
                // Continue processing to allow immediate note on new channel
                continue; // Process the next command/note immediately
            }
            // Instrument select (0x88-0x8B)
            else if (cmd >= 0x88 && cmd <= 0x8B) {
                int channel = cmd - 0x88;
                if (channel >= 0 && channel < 4) {
                    // Map the parameter to appropriate instrument for each channel
                    // Based on Paula log analysis: Channel 0=lead melody, 1=lead melody, 2=hihat, 3=bassdrum
                    switch (channel) {
                        case 0: // Channel 0 - Lead melody
                            channels[channel].currentInstrument = 0; // lead melody (sample 0)
                            break;
                        case 1: // Channel 1 - Lead melody
                            channels[channel].currentInstrument = 0; // lead melody (sample 0)
                            break;
                        case 2: // Channel 2 - Hihat
                            channels[channel].currentInstrument = 3; // hihat (sample 3)
                            break;
                        case 3: // Channel 3 - Bassdrum
                            channels[channel].currentInstrument = 2; // bassdrum (sample 2)
                            break;
                    }
                    
                    // Load the appropriate sample for this channel
                    if (paula && channels[channel].currentInstrument < static_cast<int>(instruments.size())) {
                        const Instrument& inst = instruments[channels[channel].currentInstrument];
                        
                        // Extract sample from waveData
                        std::vector<int8_t> sample;
                        uint32_t sampleLen = std::min(inst.length, static_cast<uint32_t>(waveData.size() - inst.offset));
                        sample.resize(sampleLen);
                        
                        for (uint32_t i = 0; i < sampleLen; i++) {
                            if (inst.offset + i < waveData.size()) {
                                sample[i] = static_cast<int8_t>(waveData[inst.offset + i]);
                            }
                        }
                        
                        if (!sample.empty()) {
                            paula->SetChannelSample(channel, sample);
                            // Set up looping for tone instrument
                            if (channels[channel].currentInstrument == 0 && sampleLen > 100) {
                                paula->SetChannelLoop(channel, 0, sampleLen);
                            }
                            SDL_Log("Channel %d: Loaded instrument %d (offset=0x%X, length=%d)", 
                                    channel, channels[channel].currentInstrument, inst.offset, sampleLen);
                        }
                    }
                    
                    SDL_Log("Channel %d: Instrument select cmd=0x%02X param=0x%02X -> inst %d", 
                            channel, cmd, param, channels[channel].currentInstrument);
                }
                continue; // Continue processing to allow immediate note with new instrument
            }
            // Tempo/timing controls (0x90-0x97)
            else if (cmd >= 0x90 && cmd <= 0x97) {
                // These might control global timing - for now, just consume the parameter
                SDL_Log("MusicSequencer: Tempo/timing command 0x%02X param=0x%02X", cmd, param);
                continue; // Continue processing instead of stopping
            }
            // Unknown control codes
            else {
                SDL_Log("MusicSequencer: Unknown control command 0x%02X param=0x%02X", cmd, param);
                continue; // Continue processing instead of stopping
            }
        }
        
        // It's a note value (0x20-0x5F)
        if (cmd >= 0x20 && cmd <= 0x5F) {
            uint8_t duration = (mainCh.notePos < noteData.size()) ? noteData[mainCh.notePos++] : 1;
            
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
            
            // Play single note on ALL channels simultaneously with their assigned instruments
            for (int ch = 0; ch < 4; ch++) {
                PlayNoteOnChannel(ch, cmd, duration, period);
            }
            
            mainCh.ticksRemaining = duration;
            
            // Continue processing to allow polyphony - don't break here!
            // The note will play for 'duration' ticks, but we can process more events
        }
        // Check for other note ranges that might exist
        else if (cmd >= 0x60 && cmd <= 0x7F) {
            // Possible extended note range
            SDL_Log("MusicSequencer: Extended note range command 0x%02X (0x60-0x7F) - not implemented", cmd);
            continue; // Continue processing instead of stopping
        }
        // Check for other unknown ranges
        else if (cmd >= 0x98 && cmd <= 0xFF) {
            // Possible extended control range
            SDL_Log("MusicSequencer: Extended control command 0x%02X (0x98-0xFF) - not implemented", cmd);
            continue; // Continue processing instead of stopping
        }
        // Completely unknown command
        else {
            SDL_Log("MusicSequencer: Completely unknown command 0x%02X at position 0x%04X", cmd, mainCh.notePos - 1);
            continue; // Continue processing instead of stopping
        }
    }
}

void MusicSequencer::PlayNoteOnChannel(int channel, uint8_t noteCmd, uint8_t duration, uint16_t period) {
    if (channel < 0 || channel >= 4) {
        return;
    }
    
    // Get sample for current instrument on the specified channel
    int instIdx = channels[channel].currentInstrument;
    if (instIdx < static_cast<int>(instruments.size())) {
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
        
        // Trigger note on specified channel
        if (!sample.empty()) {
            paula->SetChannelSample(channel, sample);
            paula->SetChannelPeriod(channel, period);
            paula->SetChannelVolume(channel, 64);
            
            // Loop the tone instrument, but not drums/percussion
            if (instIdx == 0 && sampleLen > 100) {
                paula->SetChannelLoop(channel, 0, sampleLen);
            } else {
                paula->SetChannelLoop(channel, 0, 0);  // No loop
            }
            
            // Only enable channel if it's not manually muted
            paula->EnableChannel(channel, !channels[channel].manuallyMuted);
            channels[channel].ticksRemaining = duration;
            
            SDL_Log("MusicSequencer: Note 0x%02X played on channel %d, instrument %d, period %d", 
                    noteCmd, channel, instIdx, period);
        }
    }
}


