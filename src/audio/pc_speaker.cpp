#include "pc_speaker.h"
#include <fstream>
#include <cstring>
#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// SR.EXE parser
// ---------------------------------------------------------------------------
//
// Memory layout (Ghidra view, load base = 0):
//   CODE_0  at 1000:0000  (linear 0x10000) -- audio/game logic
//   CODE_24 at 3e6b:0000  (linear 0x3E6B0) -- 4736 bytes of music data
//   DS      at 4e96:0000  (linear 0x4E960)
//
// CODE_24 stores (duration_u16, pit_u16) pairs.
// The first 4 bytes are a null header; music starts at offset 4.
// A pair with duration == 0 terminates the current segment.

static constexpr uint32_t GHIDRA_BASE   = 0x10000;
static constexpr uint32_t CODE24_LINEAR = 0x3E6B0;
static constexpr uint32_t CODE24_SIZE   = 4736;
static constexpr uint32_t CODE24_SKIP   = 4;   // null header bytes

// Signature at CODE_24 + 4 (first three notes: C6, A5, G5)
static const uint8_t MUSIC_SIG[] = {
    0x05, 0x00, 0x74, 0x04,  // (5, 0x0474) C6
    0x05, 0x00, 0x4B, 0x05,  // (5, 0x054B) A5
    0x05, 0x00, 0xF1, 0x05,  // (5, 0x05F1) G5
};

// Teardown signature at 1000:0fb7 used to validate the EXE
static const uint8_t TEARDOWN_SIG[] = {
    0xFA, 0xB8, 0x36, 0x00, 0xE6, 0x43, 0x2B, 0xC0, 0xE6, 0x40, 0xE6, 0x40
};

bool sr_exe_parse_music(const std::string& path,
                        std::vector<std::vector<SrNote>>& out_segments)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto size = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), (std::streamsize)size);

    if (size < 0x20 || data[0] != 'M' || data[1] != 'Z') return false;

    uint16_t header_para = (uint16_t)(data[8] | (data[9] << 8));
    uint32_t load_base   = (uint32_t)header_para * 16;
    if (load_base >= size) return false;

    // Validate via teardown signature
    uint32_t td_fo = (0x10fb7 - GHIDRA_BASE) + load_base;
    if (td_fo + sizeof(TEARDOWN_SIG) > size) return false;
    if (memcmp(&data[td_fo], TEARDOWN_SIG, sizeof(TEARDOWN_SIG)) != 0) return false;

    // Locate CODE_24
    uint32_t c24_fo = (CODE24_LINEAR - GHIDRA_BASE) + load_base;
    if (c24_fo + CODE24_SIZE > size) return false;

    // Verify music signature at offset 4
    if (c24_fo + CODE24_SKIP + sizeof(MUSIC_SIG) > size) return false;
    if (memcmp(&data[c24_fo + CODE24_SKIP], MUSIC_SIG, sizeof(MUSIC_SIG)) != 0)
        return false;

    // Parse all segments: (duration, pit) 4-byte pairs; duration==0 ends segment
    out_segments.clear();
    const uint8_t* seg = &data[c24_fo];
    uint32_t pos = CODE24_SKIP;

    while (pos + 4 <= CODE24_SIZE) {
        std::vector<SrNote> segment;

        while (pos + 4 <= CODE24_SIZE) {
            uint16_t dur = (uint16_t)(seg[pos] | (seg[pos + 1] << 8));
            uint16_t pit = (uint16_t)(seg[pos + 2] | (seg[pos + 3] << 8));
            pos += 4;

            if (dur == 0) break;  // end of this segment

            segment.push_back({ dur, pit });
        }

        if (!segment.empty())
            out_segments.push_back(std::move(segment));
    }

    return !out_segments.empty();
}

// ---------------------------------------------------------------------------
// PCSpeaker implementation
// ---------------------------------------------------------------------------

// ISR rate: PIT_CLOCK / 0x4000 = 1193182 / 16384
static constexpr float ISR_RATE_HZ = 1193182.0f / 16384.0f;  // ~72.82 Hz

PCSpeaker::PCSpeaker() = default;

PCSpeaker::~PCSpeaker()
{
    Shutdown();
}

bool PCSpeaker::Init(int sample_rate)
{
    sample_rate_ = sample_rate;

    SDL_AudioSpec want{}, have{};
    want.freq     = sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = AudioCallback;
    want.userdata = this;

    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!device_) return false;

    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void PCSpeaker::Shutdown()
{
    if (device_) {
        SDL_PauseAudioDevice(device_, 1);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

void PCSpeaker::SetNotes(const std::vector<SrNote>& notes)
{
    SDL_LockAudioDevice(device_);
    notes_    = notes;
    note_idx_ = 0;
    note_ticks_ = 0;
    SDL_UnlockAudioDevice(device_);
}

void PCSpeaker::Play()
{
    do_play_.store(true);
}

void PCSpeaker::Stop()
{
    playing_.store(false);
    gate_      = false;
    frequency_ = 0.0f;
}

void PCSpeaker::TickISR()
{
    if (do_play_.exchange(false)) {
        note_idx_   = 0;
        note_ticks_ = 0;
        playing_.store(true);
    }

    if (!playing_.load() || notes_.empty()) {
        gate_      = false;
        frequency_ = 0.0f;
        return;
    }

    if (note_ticks_ > 0) {
        --note_ticks_;
        return;
    }

    // Advance to next note, looping at end
    if (note_idx_ >= (int)notes_.size())
        note_idx_ = 0;

    const SrNote& n = notes_[note_idx_++];
    // duration_ticks is how many ISR ticks to hold the note; subtract 1 because
    // the current tick already counts as the first tick of the new note.
    note_ticks_ = (n.duration_ticks > 0) ? (n.duration_ticks - 1) : 0;

    if (n.pit_divisor == 0) {
        gate_      = false;
        frequency_ = 0.0f;
    } else {
        frequency_ = PitToHz(n.pit_divisor);
        gate_      = true;
    }
}

void PCSpeaker::FillBuffer(int16_t* buf, int frames)
{
    const float samples_per_tick = (float)sample_rate_ / ISR_RATE_HZ;

    for (int i = 0; i < frames; i++) {
        tick_accum_ += 1.0f;
        if (tick_accum_ >= samples_per_tick) {
            tick_accum_ -= samples_per_tick;
            TickISR();
        }

        int16_t sample = 0;
        if (gate_ && frequency_ > 0.0f) {
            phase_ += frequency_ / (float)sample_rate_;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            sample = (phase_ < 0.5f) ? 8000 : -8000;
        }

        buf[i] = sample;
    }
}

void PCSpeaker::AudioCallback(void* userdata, uint8_t* stream, int len)
{
    auto* self = static_cast<PCSpeaker*>(userdata);
    int frames = len / (int)sizeof(int16_t);
    self->FillBuffer(reinterpret_cast<int16_t*>(stream), frames);
}
