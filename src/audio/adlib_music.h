#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/sr2_mus.h"

// Street Rod 2's DOS soundtrack: MSR.SND instruments driving a .MUS event stream on Nuked-OPL3, on its own SDL
// device.
class AdlibMusic {
public:
    AdlibMusic() = default;
    ~AdlibMusic();

    bool Init();
    void Shutdown();

    // MSR.SND. Must succeed before any AddSong.
    bool LoadBank(const uint8_t* data, size_t size);
    // One .MUS file into the playlist, which is what the rotation and the song
    // buttons walk. Songs keep the order they are added in.
    bool AddSong(const std::string& name, const uint8_t* data, size_t size);
    // MSR21, the win piece. Held outside the playlist so the rotation never
    // reaches it.
    bool SetVictorySong(const std::string& name, const uint8_t* data, size_t size);
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

    // The OPL's own output is quiet next to the Amiga samples and the PC speaker's square.
    static constexpr float kNormalizeGain = 2.9F;

    // Volume as the mixer uses it: 256 is unity, so full volume is the gain.
    static int VolumeScale(float v) {
        const float clamped = v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
        return static_cast<int>(clamped * kNormalizeGain * 256.0F);
    }

    // One sample at that volume. Inline so the tests can reach it without an
    // audio device.
    static int16_t Scale(int16_t sample, int volume) {
        const int v = sample * volume / 256;
        return static_cast<int16_t>(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
    }

    // True if the song has looped back to its start since the last call
    // (consumes the flag), so a caller can move to another song instead.
    bool ConsumeLoopFlag() { return looped_.exchange(false); }

private:
    bool BuildInto(const std::string& name, const uint8_t* data, size_t size);
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len);
    void Fill(int16_t* out, int frames);
    void Rewind();

    struct Song {
        std::string name;
        std::vector<sr2mus::RegWrite> writes;
        uint32_t length = 0;      // samples, including the decay tail
    };

    SDL_AudioDeviceID device_ = 0;
    void* chip_ = nullptr;        // opl3_chip, hidden so the header stays C++-only
    std::vector<sr2mus::Patch> patches_;
    std::vector<Song> songs_;      // the playlist first, then the win piece
    int playlist_ = 0;
    int victory_ = -1;
    int current_ = 0;
    size_t next_ = 0;             // index of the next register write
    uint32_t sample_ = 0;
    bool playing_ = false;
    bool paused_ = false;
    std::atomic<int> volume_ {256};   // 256 is unity
    std::atomic<bool> looped_ {false};
};
