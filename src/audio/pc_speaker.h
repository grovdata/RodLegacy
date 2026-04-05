#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>

// One note from CODE_24 (segment 3e6b:0000 in SR.EXE).
// Format is (duration_word, PIT_word) pairs as stored in the DOS EXE.
struct SrNote {
    uint16_t duration_ticks;  // ISR ticks at ~72.8 Hz (5=69ms, 11=151ms, 27=371ms)
    uint16_t pit_divisor;     // PIT ch2 divisor: freq = 1193182 / pit_divisor
                              // 0 = rest (silence for duration_ticks)
};

// Parse CODE_24 from an unpacked DOS SR.EXE image in memory.
// Returns one vector<SrNote> per musical segment (null-duration terminators in CODE_24).
// Returns false if the buffer is not recognized as SR.EXE.
bool sr_exe_parse_music_bytes(const uint8_t* data, size_t size,
                                std::vector<std::vector<SrNote>>& out_segments);

// Same as sr_exe_parse_music_bytes after reading the file from disk.
bool sr_exe_parse_music(const std::string& path,
                        std::vector<std::vector<SrNote>>& out_segments);

// ---- PC Speaker synthesizer -------------------------------------------------
//
// Square-wave synthesis via SDL audio callback. A ~72.8 Hz scheduler fires
// inside the callback matching the original INT 8 ISR rate. Notes are played
// sequentially with their stored durations and looped.

class PCSpeaker {
public:
    PCSpeaker();
    ~PCSpeaker();

    bool Init(int sample_rate = 44100);
    void Shutdown();

    // Load notes for one segment. Thread-safe (locks audio device).
    void SetNotes(const std::vector<SrNote>& notes);

    // Start playing (loops the current note sequence).
    void Play();

    void Stop();

    // Amplitude of square wave output (default 8000). Range 0-8000.
    void SetAmplitude(int16_t amp) { amplitude_ = amp; }

    bool IsPlaying() const { return playing_.load(); }

    // Utility: PIT divisor -> Hz
    static float PitToHz(uint16_t pit_divisor)
    {
        if (pit_divisor == 0) return 0.0f;
        return 1193182.0f / pit_divisor;
    }

private:
    static void AudioCallback(void* userdata, uint8_t* stream, int len);
    void FillBuffer(int16_t* buf, int frames);
    void TickISR();

    int16_t amplitude_  = 8000;
    int   sample_rate_ = 44100;
    float phase_       = 0.0f;
    float frequency_   = 0.0f;
    bool  gate_        = false;

    float tick_accum_  = 0.0f;
    int   note_idx_    = 0;
    int   note_ticks_  = 0;   // remaining ISR ticks for current note

    std::vector<SrNote> notes_;

    std::atomic<bool> playing_ {false};
    std::atomic<bool> do_play_ {false};

    SDL_AudioDeviceID device_ = 0;
};
