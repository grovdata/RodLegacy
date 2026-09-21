// Street Rod 2 (DOS) AdLib music player - Nuked-OPL3 + SDL3.
//
// Two modes:
//   --mus <file.MUS>  synthesise a song from MSR.SND + the .MUS event stream
//   --dro <file.dro>  replay a DOSBox Raw OPL capture, which is the real
//                     game's own register stream and therefore the reference
//
// Formats are documented in RodLegacyResearch/docs/sr2_dos/adlib.md.

#include <SDL3/SDL.h>

extern "C" {
#include "opl3/opl3.h"
}

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kSampleRate = 49716;   // the OPL2's own rate, so no resampling
constexpr int kMusHeader = 70;
constexpr int kPatchLen = 56;
constexpr int kNameLen = 9;
constexpr int kTicksPerSecond = 480;

// One scheduled register write.
struct RegWrite {
    uint32_t sample;   // when to apply, in output samples
    uint16_t reg;
    uint8_t value;
};

std::vector<uint8_t> ReadFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return {}; }
    std::vector<uint8_t> out(static_cast<size_t>(n));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got != out.size()) return {};
    return out;
}

// ---------------------------------------------------------------- MSR.SND

// A patch is 28 little-endian int16 fields: 13 per operator, then two
// waveforms. Every field position below was recovered by matching the game's
// own OPL register writes, captured from DOSBox-X, back to this file.
//
//   +0  KSL          +7  RR
//   +1  MULT         +8  TL
//   +2  feedback     +9  AM or vibrato, the two cannot be told apart yet
//   +3  AR           +10 the other of those two
//   +4  SL           +11 KSR
//   +5  EG sustain   +12 connection, INVERTED: cnt = 1 - field
//   +6  DR
//
// field 26 is operator 1's waveform, field 27 operator 2's.
struct Operator {
    int ksl = 0, mult = 0, ar = 0, sl = 0, eg = 0, dr = 0, rr = 0, tl = 0;
    int ksr = 0, ws = 0;
};

struct Patch {
    std::string name;
    Operator op[2];
    int feedback = 0;
    int connection = 0;
};

int16_t Field(const std::vector<uint8_t>& d, size_t at, int i) {
    size_t o = at + static_cast<size_t>(i) * 2;
    return static_cast<int16_t>(d[o] | (d[o + 1] << 8));
}

bool LoadPatches(const std::vector<uint8_t>& d, std::vector<Patch>& out) {
    if (d.size() < 6) return false;
    const int count = d[2] | (d[3] << 8);
    const int patch_at = d[4] | (d[5] << 8);
    if (patch_at != 6 + count * kNameLen) return false;
    if (static_cast<int>(d.size()) < patch_at + count * kPatchLen) return false;

    out.clear();
    for (int i = 0; i < count; i++) {
        Patch p;
        const uint8_t* nm = d.data() + 6 + i * kNameLen;
        p.name.assign(reinterpret_cast<const char*>(nm),
                      strnlen(reinterpret_cast<const char*>(nm), kNameLen));
        const size_t at = static_cast<size_t>(patch_at) + static_cast<size_t>(i) * kPatchLen;
        for (int half = 0; half < 2; half++) {
            const int b = half * 13;
            Operator& op = p.op[half];
            op.ksl  = Field(d, at, b + 0) & 0x03;
            op.mult = Field(d, at, b + 1) & 0x0F;
            op.ar   = Field(d, at, b + 3) & 0x0F;
            op.sl   = Field(d, at, b + 4) & 0x0F;
            op.eg   = Field(d, at, b + 5) & 0x01;
            op.dr   = Field(d, at, b + 6) & 0x0F;
            op.rr   = Field(d, at, b + 7) & 0x0F;
            op.tl   = Field(d, at, b + 8) & 0x3F;
            op.ksr  = Field(d, at, b + 11) & 0x01;
            op.ws   = Field(d, at, 26 + half) & 0x03;
        }
        p.feedback = Field(d, at, 2) & 0x07;
        p.connection = 1 - (Field(d, at, 12) & 0x01);
        out.push_back(p);
    }
    return true;
}

// ---------------------------------------------------------------- .MUS

struct MusEvent {
    uint32_t tick;
    uint8_t status;              // 0x80..0xEF, or 0xF8 realtime
    uint8_t a = 0, b = 0;
};

int EventArgs(uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0: return 1;
        case 0xA0: return 1;     // one byte here, not MIDI's two
        case 0xD0: return 1;
        case 0x80: case 0x90: case 0xB0: case 0xE0: return 2;
        default: return 0;
    }
}

bool ParseMus(const std::vector<uint8_t>& d, std::vector<MusEvent>& out, uint32_t& length_ticks) {
    if (d.size() <= kMusHeader) return false;
    length_ticks = static_cast<uint32_t>(d[0x26] | (d[0x27] << 8) | (d[0x28] << 16) | (d[0x29] << 24));
    const uint8_t* b = d.data() + kMusHeader;
    const size_t n = d.size() - kMusHeader;

    size_t i = 0;
    uint32_t t = 0;
    uint8_t status = 0;
    out.clear();
    while (i < n) {
        // Delta time is ONE byte, 0..255, not a MIDI variable-length
        // quantity: a delta of 0xa0 is 160 ticks, not the start of a
        // multi-byte value. 0xF8 is reserved as a bare clock tick and may
        // precede the delta byte.
        uint32_t dt = 0;
        while (i < n && b[i] == 0xF8) { dt++; i++; }
        if (i < n) { dt += b[i]; i++; }
        t += dt;
        if (i >= n) break;

        if (b[i] & 0x80) status = b[i++];
        if (status == 0xF0) {                       // sysex, skip to F7
            while (i < n && b[i] != 0xF7) i++;
            if (i < n) i++;
            continue;
        }
        if (status >= 0xF0) continue;
        const int args = EventArgs(status);
        MusEvent e{t, status};
        if (args >= 1 && i < n) e.a = b[i++];
        if (args >= 2 && i < n) e.b = b[i++];
        out.push_back(e);
    }
    return !out.empty();
}

// ---------------------------------------------------------------- OPL2

// Operator slot offsets for melodic channels 0..8.
const int kSlot[9][2] = {{0, 3}, {1, 4}, {2, 5}, {8, 11}, {9, 12},
                         {10, 13}, {16, 19}, {17, 20}, {18, 21}};

// Rhythm mode takes OPL channels 6, 7 and 8 and turns five of their operators
// into one percussion voice each. The song addresses them by MIDI channel:
// the 0xBD bit is 10 minus the channel, confirmed by matching every trigger
// in a capture against the song's own note times.
//
//   ch6  bass drum   bit 4   slots 16 and 19, the only two-operator voice
//   ch7  snare       bit 3   slot 20
//   ch8  tom         bit 2   slot 18
//   ch9  top cymbal  bit 1   slot 21
//   ch10 hi-hat      bit 0   slot 17
const int kFirstPercussionChannel = 6;
const int kLastPercussionChannel = 10;
const int kPercussionSlot[5][2] = {{16, 19}, {20, -1}, {18, -1}, {21, -1}, {17, -1}};

// Which OPL channel each percussion voice takes its pitch from. Channel 7
// carries both the snare and the hi-hat, and channel 8 both the tom and the
// top cymbal, so those pairs share one frequency.
const int kPercussionOplChannel[5] = {6, 7, 8, 8, 7};

int RhythmBit(int ch) { return kLastPercussionChannel - ch; }
bool IsPercussion(int ch) {
    return ch >= kFirstPercussionChannel && ch <= kLastPercussionChannel;
}

// F-numbers for one octave. Read off the game's own register writes in a
// capture rather than computed: it runs a little sharp of equal temperament,
// by up to three units at the top of the octave. Classes 8 and 10 never
// occurred in the captures and are interpolated, which puts them within one
// unit either way.
const uint16_t kFNum[12] = {343, 364, 385, 408, 433, 459, 486, 515, 546, 579, 613, 650};

class Scheduler {
public:
    // Writes land at least one sample apart. On the real chip a player writes
    // registers one after another and the chip keeps running between them, so
    // a key-off is always seen before the key-on that follows it. Collapsing a
    // tick's writes onto a single sample loses that: the envelope never
    // retriggers and the voice stays silent after its first note.
    void Write(uint32_t sample, uint16_t reg, uint8_t value) {
        if (!writes_.empty() && sample <= writes_.back().sample) {
            sample = writes_.back().sample + 1;
        }
        writes_.push_back({sample, reg, value});
    }
    std::vector<RegWrite>& Writes() { return writes_; }

private:
    std::vector<RegWrite> writes_;
};

void ProgramOperator(Scheduler& s, uint32_t at, int slot, const Operator& op, int tl) {
    s.Write(at, 0x20 + slot, static_cast<uint8_t>(((op.eg & 1) << 5) | ((op.ksr & 1) << 4) |
                                                  (op.mult & 0x0F)));
    s.Write(at, 0x40 + slot, static_cast<uint8_t>(((op.ksl & 3) << 6) | (tl & 0x3F)));
    s.Write(at, 0x60 + slot, static_cast<uint8_t>(((op.ar & 0xF) << 4) | (op.dr & 0xF)));
    s.Write(at, 0x80 + slot, static_cast<uint8_t>(((op.sl & 0xF) << 4) | (op.rr & 0xF)));
    s.Write(at, 0xE0 + slot, static_cast<uint8_t>(op.ws & 0x03));
}

// Build the register stream for a song.
void BuildFromMus(const std::vector<MusEvent>& events, const std::vector<Patch>& patches,
                  int tick_rate, Scheduler& s) {
    const double samples_per_tick = static_cast<double>(kSampleRate) / tick_rate;
    int program[16] = {0};

    s.Write(0, 0x01, 0x20);        // enable waveform select
    s.Write(0, 0xBD, 0x20);        // rhythm mode on, as the game does

    uint8_t rhythm = 0x20;
    // Last frequency written to each percussion host channel, so it is only
    // rewritten when it changes, as the game does.
    int perc_fnum[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int perc_block[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    for (const MusEvent& e : events) {
        const uint32_t at = static_cast<uint32_t>(e.tick * samples_per_tick);
        const int ch = e.status & 0x0F;
        switch (e.status & 0xF0) {
            case 0xC0: {
                program[ch] = e.a;
                if (e.a >= static_cast<int>(patches.size())) break;
                const Patch& p = patches[e.a];
                if (IsPercussion(ch)) {
                    // A percussion voice is one operator, except the bass
                    // drum which keeps both.
                    const int* slots = kPercussionSlot[ch - kFirstPercussionChannel];
                    ProgramOperator(s, at, slots[0], p.op[0], p.op[0].tl);
                    if (slots[1] >= 0) {
                        ProgramOperator(s, at, slots[1], p.op[1], p.op[1].tl);
                        s.Write(at, 0xC0 + 6,
                                static_cast<uint8_t>(((p.feedback & 7) << 1) | (p.connection & 1)));
                    }
                } else if (ch < kFirstPercussionChannel) {
                    ProgramOperator(s, at, kSlot[ch][0], p.op[0], p.op[0].tl);
                    ProgramOperator(s, at, kSlot[ch][1], p.op[1], p.op[1].tl);
                    s.Write(at, 0xC0 + ch,
                            static_cast<uint8_t>(((p.feedback & 7) << 1) | (p.connection & 1)));
                }
                break;
            }
            case 0x90: {
                const int note = e.a, vel = e.b;
                if (IsPercussion(ch)) {              // percussion via 0xBD
                    const uint8_t bit = static_cast<uint8_t>(1 << RhythmBit(ch));
                    if (vel > 0) {
                        // Velocity sets the voice's total level, on its
                        // carrier: the bass drum's second operator, or the
                        // single operator the other voices get.
                        const int* slots = kPercussionSlot[ch - kFirstPercussionChannel];
                        const int slot = slots[1] >= 0 ? slots[1] : slots[0];
                        const int prog = program[ch];
                        const int ksl = prog < static_cast<int>(patches.size())
                                            ? patches[prog].op[slots[1] >= 0 ? 1 : 0].ksl
                                            : 0;
                        s.Write(at, 0x40 + slot,
                                static_cast<uint8_t>(((ksl & 3) << 6) |
                                                     (std::clamp(0x3F - (vel >> 1), 0, 0x3F))));
                        // A percussion voice still needs a pitch, taken from
                        // its host channel's F-number with the key-on bit
                        // left clear. The game sets it once per song rather
                        // than per note, so only write it when it changes.
                        const int opl = kPercussionOplChannel[ch - kFirstPercussionChannel];
                        const int oct = std::clamp(note / 12 - 1, 0, 7);
                        const uint16_t f = kFNum[note % 12];
                        const uint8_t hi =
                            static_cast<uint8_t>(((oct & 7) << 2) | ((f >> 8) & 3));
                        if (perc_fnum[opl] != f || perc_block[opl] != hi) {
                            perc_fnum[opl] = f;
                            perc_block[opl] = hi;
                            s.Write(at, 0xA0 + opl, static_cast<uint8_t>(f & 0xFF));
                            s.Write(at, 0xB0 + opl, hi);
                        }
                        // Retrigger: the bit has to drop before it rises again.
                        rhythm &= ~bit;
                        s.Write(at, 0xBD, rhythm);
                        rhythm |= bit;
                    } else {
                        rhythm &= ~bit;
                    }
                    s.Write(at, 0xBD, rhythm);
                    break;
                }
                if (ch >= kFirstPercussionChannel) break;
                if (vel == 0) {                      // note on with velocity 0 is a note off
                    s.Write(at, 0xB0 + ch, 0);
                    break;
                }
                const int prog = program[ch];
                if (prog < static_cast<int>(patches.size())) {
                    // The carrier's total level is the note volume.
                    const Patch& p = patches[prog];
                    const int tl = std::clamp(0x3F - (vel >> 1), 0, 0x3F);
                    s.Write(at, 0x40 + kSlot[ch][1],
                            static_cast<uint8_t>(((p.op[1].ksl & 3) << 6) | tl));
                }
                const int oct = std::clamp(note / 12 - 1, 0, 7);
                const uint16_t f = kFNum[note % 12];
                s.Write(at, 0xA0 + ch, static_cast<uint8_t>(f & 0xFF));
                s.Write(at, 0xB0 + ch,
                        static_cast<uint8_t>(0x20 | ((oct & 7) << 2) | ((f >> 8) & 3)));
                break;
            }
            case 0x80:
                if (IsPercussion(ch)) {
                    rhythm &= ~static_cast<uint8_t>(1 << RhythmBit(ch));
                    s.Write(at, 0xBD, rhythm);
                } else if (ch < kFirstPercussionChannel) {
                    s.Write(at, 0xB0 + ch, 0);
                }
                break;
            default:
                break;
        }
    }
}

// Build the register stream from a DRO v2 capture.
bool BuildFromDro(const std::vector<uint8_t>& d, Scheduler& s, std::string& err) {
    if (d.size() < 26 || std::memcmp(d.data(), "DBRAWOPL", 8) != 0) {
        err = "not a DRO file";
        return false;
    }
    const int major = d[8] | (d[9] << 8);
    if (major != 2) { err = "only DRO v2 is supported"; return false; }
    const uint32_t pairs = d[12] | (d[13] << 8) | (d[14] << 16) | (d[15] << 24);
    const uint8_t short_code = d[23], long_code = d[24], cmlen = d[25];
    const uint8_t* codemap = d.data() + 26;
    const uint8_t* data = codemap + cmlen;
    if (data + pairs * 2 > d.data() + d.size()) { err = "DRO data is truncated"; return false; }

    uint32_t ms = 0;
    for (uint32_t i = 0; i < pairs; i++) {
        const uint8_t code = data[i * 2], val = data[i * 2 + 1];
        if (code == short_code) { ms += val + 1; continue; }
        if (code == long_code) { ms += (val + 1) << 8; continue; }
        const uint16_t reg = codemap[code & 0x7F] | ((code & 0x80) ? 0x100 : 0);
        s.Write(static_cast<uint32_t>(static_cast<uint64_t>(ms) * kSampleRate / 1000), reg, val);
    }
    return true;
}

// ---------------------------------------------------------------- playback

struct Player {
    opl3_chip chip{};
    std::vector<RegWrite> writes;
    size_t next = 0;
    uint32_t sample = 0;
    std::atomic<bool> done{false};
};

void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int) {
    Player& p = *static_cast<Player*>(userdata);
    const int frames = additional / static_cast<int>(sizeof(int16_t) * 2);
    if (frames <= 0) return;
    std::vector<int16_t> buf(static_cast<size_t>(frames) * 2);

    for (int i = 0; i < frames; i++) {
        while (p.next < p.writes.size() && p.writes[p.next].sample <= p.sample) {
            OPL3_WriteReg(&p.chip, p.writes[p.next].reg, p.writes[p.next].value);
            p.next++;
        }
        int16_t s2[2] = {0, 0};
        OPL3_Generate(&p.chip, s2);
        buf[i * 2] = s2[0];
        buf[i * 2 + 1] = s2[1];
        p.sample++;
    }
    if (p.next >= p.writes.size()) p.done = true;
    SDL_PutAudioStreamData(stream, buf.data(), static_cast<int>(buf.size() * sizeof(int16_t)));
}

// Render the whole stream offline to a 16-bit stereo WAV, so timing and
// balance can be checked without a sound device in the loop.
bool WriteWav(const std::string& path, std::vector<RegWrite>& writes) {
    opl3_chip chip{};
    OPL3_Reset(&chip, kSampleRate);
    const uint32_t tail = kSampleRate;            // one second of decay
    const uint32_t total = (writes.empty() ? 0 : writes.back().sample) + tail;

    std::vector<int16_t> pcm(static_cast<size_t>(total) * 2);
    size_t next = 0;
    for (uint32_t i = 0; i < total; i++) {
        while (next < writes.size() && writes[next].sample <= i) {
            OPL3_WriteReg(&chip, writes[next].reg, writes[next].value);
            next++;
        }
        int16_t s2[2] = {0, 0};
        OPL3_Generate(&chip, s2);
        pcm[static_cast<size_t>(i) * 2] = s2[0];
        pcm[static_cast<size_t>(i) * 2 + 1] = s2[1];
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    const uint32_t rate = kSampleRate;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16v = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16v(1); u16v(2);
    u32(rate); u32(rate * 4); u16v(4); u16v(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(pcm.data(), 1, bytes, f);
    std::fclose(f);
    std::printf("wrote %s, %.1f s\n", path.c_str(), static_cast<double>(total) / kSampleRate);
    return true;
}

void Usage() {
    std::puts("Usage:");
    std::puts("  sr2_dos_player --mus <MSRBEG.MUS> [--snd <MSR.SND>] [--tickrate N]");
    std::puts("  sr2_dos_player --dro <capture.dro>");
    std::puts("Options:");
    std::puts("  --dump         print the register stream instead of playing");
    std::puts("  --tickrate N   .MUS ticks per second (default 480)");
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string mus, dro, snd, wav;
    int tick_rate = kTicksPerSecond;
    bool dump = false;

    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--mus") && i + 1 < argc) mus = argv[++i];
        else if (!std::strcmp(argv[i], "--dro") && i + 1 < argc) dro = argv[++i];
        else if (!std::strcmp(argv[i], "--snd") && i + 1 < argc) snd = argv[++i];
        else if (!std::strcmp(argv[i], "--tickrate") && i + 1 < argc) tick_rate = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--dump")) dump = true;
        else if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else { Usage(); return 0; }
    }
    if (mus.empty() == dro.empty()) { Usage(); return 1; }

    Scheduler sched;
    if (!dro.empty()) {
        std::vector<uint8_t> d = ReadFile(dro);
        if (d.empty()) { std::fprintf(stderr, "cannot read %s\n", dro.c_str()); return 1; }
        std::string err;
        if (!BuildFromDro(d, sched, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        std::printf("%s: %zu register writes\n", dro.c_str(), sched.Writes().size());
    } else {
        if (snd.empty()) {
            const size_t slash = mus.find_last_of("/\\");
            snd = (slash == std::string::npos ? std::string() : mus.substr(0, slash + 1)) + "MSR.SND";
        }
        std::vector<uint8_t> sd = ReadFile(snd);
        std::vector<Patch> patches;
        if (sd.empty() || !LoadPatches(sd, patches)) {
            std::fprintf(stderr, "cannot read patches from %s\n", snd.c_str());
            return 1;
        }
        std::vector<uint8_t> md = ReadFile(mus);
        std::vector<MusEvent> events;
        uint32_t length_ticks = 0;
        if (md.empty() || !ParseMus(md, events, length_ticks)) {
            std::fprintf(stderr, "cannot parse %s\n", mus.c_str());
            return 1;
        }
        std::printf("%s: %zu events, %u ticks (%.1f s at %d Hz), %zu patches from %s\n",
                    mus.c_str(), events.size(), length_ticks,
                    static_cast<double>(length_ticks) / tick_rate, tick_rate,
                    patches.size(), snd.c_str());
        BuildFromMus(events, patches, tick_rate, sched);
        std::printf("  -> %zu register writes\n", sched.Writes().size());
    }

    std::vector<RegWrite>& writes = sched.Writes();
    std::stable_sort(writes.begin(), writes.end(),
                     [](const RegWrite& a, const RegWrite& b) { return a.sample < b.sample; });

    if (!wav.empty()) return WriteWav(wav, writes) ? 0 : 1;

    if (dump) {
        for (const RegWrite& w : writes) {
            std::printf("%8.3f s  reg %03x = %02x\n",
                        static_cast<double>(w.sample) / kSampleRate, w.reg, w.value);
        }
        return 0;
    }

    Player player;
    OPL3_Reset(&player.chip, kSampleRate);
    player.writes = writes;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = kSampleRate;
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, &player);
    if (!stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_ResumeAudioStreamDevice(stream);

    std::puts("Playing. Press Enter to stop.");
    (void)std::getchar();

    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 0;
}
