/*
 * RodLegacy - DOS PC speaker music: SR.EXE CODE_24 note parsing and the PCSpeaker square-wave player.
 *
 * Created: 2026-04-04
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>

// One note from CODE_24 in SR.EXE: (duration_word, PIT_word) pairs.
struct SrNote {
    uint16_t duration_ticks;  // ISR ticks at ~72.8 Hz (5=69ms, 11=151ms, 27=371ms)
    uint16_t pit_divisor;     // PIT ch2 divisor: freq = 1193182 / pit_divisor
                              // 0 = rest (silence for duration_ticks)
};

// One vector per segment. False if the buffer is not SR.EXE.
bool sr_exe_parse_music_bytes(const uint8_t* data, size_t size,
                                std::vector<std::vector<SrNote>>& out_segments);

bool sr_exe_parse_music(const std::string& path,
                        std::vector<std::vector<SrNote>>& out_segments);

// Songs assembled from CODE_24's patterns via the in-EXE playlist. out_bridge is the intro scale between songs.
// False if the playlist cannot be parsed; fall back to sr_exe_parse_music_bytes.
bool sr_exe_parse_songs(const uint8_t* data, size_t size,
                        std::vector<std::vector<SrNote>>& out_songs,
                        std::vector<SrNote>& out_bridge);

// The 5 known SR1 songs, identified by ear (bridge seg repeated 3x before melody):
//   song1: 0x3, 1,1          song2: 3x3, 4,4,5
//   song3: 6x3, 7,7,8,9,9,10,11  song4: 12x3, 13,13,13,14  song5: 15x3, 16,17
bool sr_exe_build_known_songs(const std::vector<std::vector<SrNote>>& segs,
                              std::vector<std::vector<SrNote>>& out_songs);

// Square wave with a ~72.8 Hz scheduler in the callback, matching the original INT 8 ISR rate.

class PCSpeaker {
public:
    PCSpeaker();
    ~PCSpeaker();

    bool Init(int sample_rate = 44100);
    void Shutdown();

    void SetNotes(const std::vector<SrNote>& notes);

    void Play();

    void Stop();

    void Pause();
    void Resume();
    bool IsPaused() const { return paused_; }

    void SetAmplitude(int16_t amp) { amplitude_ = amp; }

    bool IsPlaying() const { return playing_.load(); }

    // True if the current song has looped back to its start since the last
    // call (consumes the flag). Used to auto-advance to a different song.
    bool ConsumeLoopFlag() { return looped_.exchange(false); }

    static float PitToHz(uint16_t pit_divisor)
    {
        if (pit_divisor == 0) return 0.0f;
        return 1193182.0f / pit_divisor;
    }

private:
    static void AudioCallback(void* userdata, uint8_t* stream, int len);
    void FillBuffer(int16_t* buf, int frames);
    void TickISR();

    bool    paused_     = false;
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
    std::atomic<bool> looped_  {false};

    SDL_AudioDeviceID device_ = 0;
};
