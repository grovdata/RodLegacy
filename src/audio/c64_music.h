#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/c64_sid.h"

// The C64 version's soundtrack: its seq* / wseq.o driver PRGs run on an emulated 6502 + SID, on its own SDL device.
class C64Music {
public:
    C64Music() = default;
    ~C64Music();

    bool Init();
    void Shutdown();

    // One driver PRG into the playlist, which is what the rotation and the song buttons walk.
    bool AddSong(const std::string& name, const std::vector<uint8_t>& prg);
    // wseq.o, the win piece. Held outside the playlist so the rotation never reaches it.
    bool SetVictorySong(const std::string& name, const std::vector<uint8_t>& prg);
    bool HasVictorySong() const { return victory_ >= 0; }
    void PlayVictorySong();

    int  SongCount() const { return playlist_; }
    int  CurrentSong() const { return current_; }
    std::string CurrentSongName() const;
    // Restarts playback from the top of that playlist song.
    void SetSong(int index);

    void Play();
    void Stop();
    void Pause();
    void Resume();
    bool IsPaused() const { return paused_; }
    bool IsPlaying() const { return playing_; }

    // 0.0 - 1.0.
    void SetVolume(float v);

    static constexpr int kSampleRate = 44100;
    static constexpr float kNormalizeGain = 1.5F;

    // At a loop point that leads to another song: fade, then a gap, then the loop flag.
    static constexpr int kFadeFrames = kSampleRate * 3;
    static constexpr int kGapFrames = kSampleRate;

    // True once the song has reached its loop point and faded out (consumes the flag).
    bool ConsumeLoopFlag() { return looped_.exchange(false); }

private:
    bool BuildInto(const std::string& name, const std::vector<uint8_t>& prg);
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len);
    void Fill(int16_t* out, int frames);
    void Restart();

    struct Song {
        std::string name;
        std::unique_ptr<c64::Tune> tune;
    };

    SDL_AudioDeviceID device_ = 0;
    std::vector<Song> songs_;      // the playlist first, then the win piece
    std::vector<int16_t> mono_;
    int fade_ = -1;                // frames into the fade-out; -1 when not fading
    int playlist_ = 0;
    int victory_ = -1;
    int current_ = 0;
    bool playing_ = false;
    bool paused_ = false;
    std::atomic<int> volume_ {256};   // 256 is unity
    std::atomic<bool> looped_ {false};
};

// The C64 effects: songs of the seqgar1 driver's effect bank, on a SID of their own so they mix with the music.
// See RodLegacyResearch/docs/c64/sfx.md.
class C64Sfx {
public:
    ~C64Sfx();

    bool Init(const std::vector<uint8_t>& prg);
    void Shutdown();

    // 0.0 - 1.0, the SFX volume.
    void SetVolume(float v);
    SDL_AudioDeviceID Device() const { return device_; }

    // Effect song number. seconds > 0 stops it after that long (the held and looping ones need it).
    void Play(int song, double seconds = 0);
    void Stop();

private:
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len);

    SDL_AudioDeviceID device_ = 0;
    c64::Tune tune_;
    std::vector<int16_t> mono_;
    int stopIn_ = -1;              // frames until STOP; -1 when not timed
    std::atomic<int> volume_ {0};
};
