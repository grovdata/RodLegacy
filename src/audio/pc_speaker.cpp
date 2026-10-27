#include "audio/pc_speaker.h"
#include "audio/record_tap.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <random>
#include <algorithm>
#include <SDL.h>

// CODE_24 stores (duration_u16, pit_u16) pairs after a 4-byte null header; duration 0 ends a segment.

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

// Validates the EXE.
static const uint8_t TEARDOWN_SIG[] = {
    0xFA, 0xB8, 0x36, 0x00, 0xE6, 0x43, 0x2B, 0xC0, 0xE6, 0x40, 0xE6, 0x40
};

bool sr_exe_parse_music_bytes(const uint8_t* data, size_t size,
                              std::vector<std::vector<SrNote>>& out_segments)
{
    if (size < 0x20 || data[0] != 'M' || data[1] != 'Z') return false;

    uint16_t header_para = (uint16_t)(data[8] | (data[9] << 8));
    uint32_t load_base   = (uint32_t)header_para * 16;
    if (load_base >= size) return false;

    uint32_t td_fo = (0x10fb7 - GHIDRA_BASE) + load_base;
    if (td_fo + sizeof(TEARDOWN_SIG) > size) return false;
    if (memcmp(data + td_fo, TEARDOWN_SIG, sizeof(TEARDOWN_SIG)) != 0) return false;

    uint32_t c24_fo = (CODE24_LINEAR - GHIDRA_BASE) + load_base;
    if (c24_fo + CODE24_SIZE > size) return false;

    if (c24_fo + CODE24_SKIP + sizeof(MUSIC_SIG) > size) return false;
    if (memcmp(data + c24_fo + CODE24_SKIP, MUSIC_SIG, sizeof(MUSIC_SIG)) != 0)
        return false;

    out_segments.clear();
    const uint8_t* seg = data + c24_fo;
    uint32_t pos = CODE24_SKIP;

    while (pos + 4 <= CODE24_SIZE) {
        std::vector<SrNote> segment;

        while (pos + 4 <= CODE24_SIZE) {
            uint16_t dur = (uint16_t)(seg[pos] | (seg[pos + 1] << 8));
            uint16_t pit = (uint16_t)(seg[pos + 2] | (seg[pos + 3] << 8));
            pos += 4;

            if (dur == 0) break;

            segment.push_back({ dur, pit });
        }

        if (!segment.empty())
            out_segments.push_back(std::move(segment));
    }

    return !out_segments.empty();
}

bool sr_exe_parse_songs(const uint8_t* data, size_t size,
                        std::vector<std::vector<SrNote>>& out_songs,
                        std::vector<SrNote>& out_bridge)
{
    if (size < 0x20 || data[0] != 'M' || data[1] != 'Z') return false;

    uint16_t header_para = (uint16_t)(data[8] | (data[9] << 8));
    uint32_t load_base   = (uint32_t)header_para * 16;
    if (load_base >= size) return false;

    uint32_t td_fo = (0x10fb7u - GHIDRA_BASE) + load_base;
    if (td_fo + sizeof(TEARDOWN_SIG) > size) return false;
    if (memcmp(data + td_fo, TEARDOWN_SIG, sizeof(TEARDOWN_SIG)) != 0) return false;

    uint32_t c24_fo = (CODE24_LINEAR - GHIDRA_BASE) + load_base;
    if (c24_fo + CODE24_SIZE > size) return false;
    if (c24_fo + CODE24_SKIP + sizeof(MUSIC_SIG) > size) return false;
    if (memcmp(data + c24_fo + CODE24_SKIP, MUSIC_SIG, sizeof(MUSIC_SIG)) != 0) return false;

    std::vector<std::vector<SrNote>> segs;
    {
        const uint8_t* base = data + c24_fo;
        uint32_t pos = CODE24_SKIP;
        while (pos + 4 <= CODE24_SIZE) {
            std::vector<SrNote> seg;
            while (pos + 4 <= CODE24_SIZE) {
                uint16_t dur = (uint16_t)(base[pos]   | (base[pos+1] << 8));
                uint16_t pit = (uint16_t)(base[pos+2] | (base[pos+3] << 8));
                pos += 4;
                if (dur == 0) break;
                seg.push_back({dur, pit});
            }
            if (!seg.empty()) segs.push_back(std::move(seg));
        }
    }
    if (segs.empty()) return false;

    // Pattern 0 is the intro scale; 1-4 are the non-intro, non-rest segments in order.
    auto isIntro = [](const std::vector<SrNote>& s) {
        return !s.empty() && s[0].duration_ticks == 5 && s[0].pit_divisor == 0x0474;
    };
    auto isRest = [](const std::vector<SrNote>& s) {
        for (const auto& n : s)
            if (n.pit_divisor != 0) return false;
        return !s.empty();
    };

    const std::vector<SrNote>* pat[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int nSongs = 0;
    for (const auto& seg : segs) {
        if (!pat[0] && isIntro(seg)) {
            pat[0] = &seg;
        } else if (!isIntro(seg) && !isRest(seg) && nSongs < 4) {
            pat[1 + nSongs] = &seg;
            nSongs++;
        }
    }
    if (!pat[0]) return false;

    // Playlist at DS:0x5858, a fixed distance from CODE_24.
    uint32_t pl_fo = c24_fo + 0x15B08u;
    if (pl_fo + 2 > size) return false;

    // Playlist words: 0-4 pattern index, 0x79AA random pattern, 0xFFFF block end (an empty block ends the list).
    std::vector<std::vector<uint16_t>> blocks;
    {
        std::vector<uint16_t> cur;
        for (int i = 0; i < 512 && pl_fo + 2 <= size; ++i, pl_fo += 2) {
            uint16_t v = (uint16_t)(data[pl_fo] | (data[pl_fo+1] << 8));
            if (v == 0xFFFF) {
                if (cur.empty()) break;  // empty block = playlist end
                blocks.push_back(std::move(cur));
                cur.clear();
            } else if (v <= 4 || v == 0x79AA) {
                cur.push_back(v);
            } else {
                if (!cur.empty()) blocks.push_back(std::move(cur));
                break;
            }
        }
        if (!cur.empty()) blocks.push_back(std::move(cur));
    }
    if (blocks.empty()) return false;

    out_bridge = *pat[0];

    // Blocks with only index 0 are intro-only, so skip them.
    std::mt19937 rng(std::random_device{}());
    out_songs.clear();

    for (const auto& block : blocks) {
        bool hasMelody = false;
        for (uint16_t e : block)
            if (e != 0) { hasMelody = true; break; }
        if (!hasMelody) continue;

        std::vector<SrNote> song;
        for (uint16_t e : block) {
            int idx;
            if (e == 0x79AA) {
                idx = nSongs >= 1 ? std::uniform_int_distribution<int>(1, nSongs)(rng) : 0;
            } else {
                idx = (int)e;
            }
            if (idx >= 0 && idx < 5 && pat[idx])
                for (const auto& n : *pat[idx]) song.push_back(n);
        }
        if (!song.empty()) out_songs.push_back(std::move(song));
    }

    return !out_songs.empty();
}

bool sr2_exe_build_songs(const uint8_t* data, size_t size, std::vector<std::vector<SrNote>>& out)
{
    out.clear();
    if (size < 0x40) return false;
    auto word = [&](size_t at) { return static_cast<uint16_t>(data[at] | data[at + 1] << 8); };
    const size_t ds = static_cast<size_t>(word(8)) * 16 + 0x3e080;   // DGROUP 3E08
    const size_t songEnd = ds + 0x6e48;                               // the jingle follows
    if (songEnd > size) return false;

    // Past the 0,0 header; a 0,m marker ends a section, 0,0 the song.
    std::vector<std::vector<SrNote>> sections(1);
    for (size_t at = ds + 0x5594 + 4; at + 4 <= songEnd; at += 4) {
        const uint16_t dur = word(at), value = word(at + 2);
        if (dur == 0) {
            if (value == 0) break;
            sections.emplace_back();
            continue;
        }
        sections.back().push_back({dur, value});
    }
    // Total plays per section, -1 restarts the song (DS:540C). The intro (section 0) starts each song.
    for (size_t i = 0; i < sections.size(); ++i) {
        const int plays = static_cast<int16_t>(word(ds + 0x540c + 2 * i));
        if (plays < 0) break;
        const bool intro = std::equal(sections[i].begin(), sections[i].end(), sections[0].begin(), sections[0].end(),
                                      [](const SrNote& a, const SrNote& b) {
                                          return a.duration_ticks == b.duration_ticks && a.pit_divisor == b.pit_divisor;
                                      });
        if (out.empty() || intro) out.emplace_back();
        for (int p = 0; p < plays; ++p) out.back().insert(out.back().end(), sections[i].begin(), sections[i].end());
    }
    return !out.empty();
}

bool sr_exe_build_known_songs(const std::vector<std::vector<SrNote>>& segs,
                              std::vector<std::vector<SrNote>>& out_songs)
{
    if (segs.size() < 18) return false;

    static const int kDefs[][13] = {
        {0, 0, 0, 1, 1,                   -1},
        {3, 3, 3, 4, 4, 5,                -1},
        {6, 6, 6, 7, 7, 8, 9, 9, 10, 11, -1},
        {12, 12, 12, 13, 13, 13, 14,      -1},
        {15, 15, 15, 16, 17,              -1},
    };
    static const char* kNames[] = {"song1", "song2", "song3", "song4", "song5"};
    (void)kNames;

    out_songs.clear();
    for (const auto& def : kDefs) {
        std::vector<SrNote> song;
        for (int i = 0; def[i] >= 0; ++i)
            for (const auto& n : segs[(size_t)def[i]]) song.push_back(n);
        out_songs.push_back(std::move(song));
    }
    return true;
}

bool sr_exe_parse_music(const std::string& path,
                        std::vector<std::vector<SrNote>>& out_segments)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto size = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), (std::streamsize)size);
    return sr_exe_parse_music_bytes(data.data(), data.size(), out_segments);
}

// ISR rate: PIT_CLOCK / 0x4000 = 1193182 / 16384
static constexpr float ISR_RATE_HZ = 1193182.0f / 16384.0f;

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
    Resume();  // AmigaMusic::Stop clears its pause too, so a paused speaker would stay dead across a source switch.
}

void PCSpeaker::Pause()
{
    if (!paused_ && device_) {
        SDL_PauseAudioDevice(device_, 1);
        paused_ = true;
    }
}

void PCSpeaker::Resume()
{
    if (paused_ && device_) {
        SDL_PauseAudioDevice(device_, 0);
        paused_ = false;
    }
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

    if (note_idx_ >= (int)notes_.size()) {
        note_idx_ = 0;
        looped_.store(true);
    }

    const SrNote& n = notes_[note_idx_++];
    // Minus 1: the current tick is already the note's first.
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
            sample = (phase_ < 0.5f) ? amplitude_ : (int16_t)(-amplitude_);
        }

        buf[i] = sample;
    }
}

void PCSpeaker::AudioCallback(void* userdata, uint8_t* stream, int len)
{
    auto* self = static_cast<PCSpeaker*>(userdata);
    int frames = len / (int)sizeof(int16_t);
    self->FillBuffer(reinterpret_cast<int16_t*>(stream), frames);
    record_tap::Write("pc_speaker", self->sample_rate_, 1, stream, len);
}
