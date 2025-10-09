#pragma once

#include "paula_emulator.h"
#include <vector>
#include <cstdint>
#include <string>
#include <memory>

/**
 * Street Rod music sequencer
 * 
 * Plays music using California Dreams' custom format:
 * - .note files: Pattern/sequence data (tracker-style)
 * - .wave files: Sample data (8-bit signed PCM)
 * - .ins files: Instrument definitions (maps notes to samples)
 * - .env files: Envelope data (volume/pitch modulation)
 */
class MusicSequencer {
public:
    MusicSequencer();
    ~MusicSequencer();
    
    // Initialize with Paula emulator
    bool Initialize(PaulaEmulator* paula);
    
    // Load music files
    bool LoadMusicFiles(const std::string& noteFile,
                       const std::string& waveFile, 
                       const std::string& insFile,
                       const std::string& envFile);
    
    // Select which song to play (0-2 for the three songs in msr.note)
    void SelectSong(int songIndex);
    
    // Manually set instrument for a specific channel (for testing)
    void SetChannelInstrument(int channel, int instrumentIndex);
    
    // Show current instrument assignments
    void ShowInstrumentAssignments();
    
    // Playback control
    void StartMusic();
    void StopMusic();
    void PauseMusic(bool pause);
    
    // Update music playback (call from game loop or timer)
    void Update(float deltaTime);
    
    // Status
    bool IsPlaying() const { return playing; }
    
private:
    PaulaEmulator* paula;
    
    // Music data
    std::vector<uint8_t> noteData;
    std::vector<uint8_t> waveData;
    std::vector<uint8_t> insData;
    std::vector<uint8_t> envData;
    
    // Playback state
    bool playing;
    bool paused;
    float tickTimer;
    float tickDuration;  // Time per tick in seconds
    
    uint32_t currentTick;
    uint32_t currentPattern;
    uint32_t patternLength;
    
    // Song selection (three songs in msr.note)
    int currentSong;
    uint32_t songOffset;  // Offset in noteData where current song starts
    
    // Instrument mapping (from .ins file)
    struct Instrument {
        uint32_t offset;     // Offset in waveData
        uint32_t length;     // Sample length
    };
    std::vector<Instrument> instruments;
    
    // Per-channel state for tracker-style playback
    struct ChannelState {
        uint32_t notePos;         // Current position in note data (shared, only channel 0 uses this)
        uint32_t ticksRemaining;  // Ticks before next note (shared, only channel 0 uses this)
        int currentInstrument;    // Current instrument for this channel
        
        ChannelState() : notePos(0), ticksRemaining(0), currentInstrument(0) {}
    };
    ChannelState channels[4];  // 4 Paula channels
    int activeChannel;  // Currently selected channel in the sequential stream
    
    // Process one tick of music
    void ProcessTick();
    
    // Process commands for a specific channel
    void ProcessChannelTick(int channelIndex);
    
    // Helper to read music file
    static std::vector<uint8_t> ReadFile(const std::string& filename);
};

