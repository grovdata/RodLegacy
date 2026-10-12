#include "audio/sr2_mus.h"

#include <algorithm>
#include <cstring>

namespace sr2mus {
namespace {

constexpr int kMusHeader = 70;
constexpr int kPatchLen = 56;
constexpr int kNameLen = 9;

int16_t Field(const uint8_t* d, size_t at, int i) {
    const size_t o = at + static_cast<size_t>(i) * 2;
    return static_cast<int16_t>(d[o] | (d[o + 1] << 8));
}

int EventArgs(uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0: return 1;
        case 0xA0: return 1;     // one byte here, not MIDI's two
        case 0xD0: return 1;
        case 0x80: case 0x90: case 0xB0: case 0xE0: return 2;
        default: return 0;
    }
}

// Operator slot offsets for melodic channels 0..8.
const int kSlot[9][2] = {{0, 3}, {1, 4}, {2, 5}, {8, 11}, {9, 12},
                         {10, 13}, {16, 19}, {17, 20}, {18, 21}};

// Rhythm mode takes OPL channels 6, 7 and 8 and turns five of their operators
// into one percussion voice each. The song addresses them by MIDI channel: the
// 0xBD bit is 10 minus the channel, confirmed by matching every trigger in a
// capture against the song's own note times.
//
//   ch6  bass drum   bit 4   slots 16 and 19, the only two-operator voice
//   ch7  snare       bit 3   slot 20
//   ch8  tom         bit 2   slot 18
//   ch9  top cymbal  bit 1   slot 21
//   ch10 hi-hat      bit 0   slot 17
const int kFirstPercussionChannel = 6;
const int kLastPercussionChannel = 10;
const int kPercussionSlot[5][2] = {{16, 19}, {20, -1}, {18, -1}, {21, -1}, {17, -1}};

// Which OPL channel each percussion voice takes its pitch from.
const int kPercussionOplChannel[5] = {6, 7, 8, 8, 7};

int RhythmBit(int ch) { return kLastPercussionChannel - ch; }

bool IsPercussion(int ch) {
    return ch >= kFirstPercussionChannel && ch <= kLastPercussionChannel;
}

// F-numbers for one octave.
const uint16_t kFNum[12] = {343, 364, 385, 408, 433, 459, 486, 515, 546, 579, 613, 650};

// Writes land at least one sample apart.
void Emit(std::vector<RegWrite>& w, uint32_t sample, uint16_t reg, uint8_t value) {
    if (!w.empty() && sample <= w.back().sample) {
        sample = w.back().sample + 1;
    }
    w.push_back({sample, reg, value});
}

void ProgramOperator(std::vector<RegWrite>& w, uint32_t at, int slot, const Operator& op, int tl) {
    Emit(w, at, 0x20 + slot,
         static_cast<uint8_t>(((op.eg & 1) << 5) | ((op.ksr & 1) << 4) | (op.mult & 0x0F)));
    Emit(w, at, 0x40 + slot, static_cast<uint8_t>(((op.ksl & 3) << 6) | (tl & 0x3F)));
    Emit(w, at, 0x60 + slot, static_cast<uint8_t>(((op.ar & 0xF) << 4) | (op.dr & 0xF)));
    Emit(w, at, 0x80 + slot, static_cast<uint8_t>(((op.sl & 0xF) << 4) | (op.rr & 0xF)));
    Emit(w, at, 0xE0 + slot, static_cast<uint8_t>(op.ws & 0x03));
}

}  // namespace

// The note's fnum and block, shared by the song and the effect paths.
namespace {
void KeyOn(std::vector<RegWrite>& w, uint32_t at, int ch, int note, bool on) {
    const int oct = std::clamp(note / 12 - 1, 0, 7);
    const uint16_t f = kFNum[note % 12];
    Emit(w, at, 0xA0 + ch, static_cast<uint8_t>(f & 0xFF));
    Emit(w, at, 0xB0 + ch,
         static_cast<uint8_t>((on ? 0x20 : 0x00) | ((oct & 7) << 2) | ((f >> 8) & 3)));
}
}  // namespace

void EffectVoice(std::vector<RegWrite>& w, const Patch& p, int ch,
                 int volume, int note, uint32_t at, uint32_t hold) {
    if (ch < 0 || ch >= kFirstPercussionChannel) {
        return;
    }
    ProgramOperator(w, at, kSlot[ch][0], p.op[0], p.op[0].tl);
    ProgramOperator(w, at, kSlot[ch][1], p.op[1], p.op[1].tl);
    Emit(w, at, 0xC0 + ch,
         static_cast<uint8_t>(((p.feedback & 7) << 1) | (p.connection & 1)));
    // The game hands SetVoiceLevel a 0..127 volume and it becomes the carrier's
    // total level, loudest at 127.
    const int tl = std::clamp(0x3F - (volume >> 1), 0, 0x3F);
    Emit(w, at, 0x40 + kSlot[ch][1],
         static_cast<uint8_t>(((p.op[1].ksl & 3) << 6) | tl));
    KeyOn(w, at, ch, note, true);
    Emit(w, at + hold, 0xB0 + ch, 0);
}

void EffectPitch(std::vector<RegWrite>& w, int ch, int note, uint32_t at) {
    if (ch < 0 || ch >= kFirstPercussionChannel) {
        return;
    }
    KeyOn(w, at, ch, note, true);
}

bool LoadPatches(const uint8_t* d, size_t size, std::vector<Patch>& out) {
    if (d == nullptr || size < 6) return false;
    const int count = d[2] | (d[3] << 8);
    const int patch_at = d[4] | (d[5] << 8);
    if (count <= 0 || patch_at != 6 + count * kNameLen) return false;
    if (size < static_cast<size_t>(patch_at) + static_cast<size_t>(count) * kPatchLen) return false;

    out.clear();
    for (int i = 0; i < count; i++) {
        Patch p;
        const char* nm = reinterpret_cast<const char*>(d) + 6 + i * kNameLen;
        p.name.assign(nm, strnlen(nm, kNameLen));
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

bool ParseMus(const uint8_t* d, size_t size, std::vector<Event>& out, uint32_t& lengthTicks) {
    if (d == nullptr || size <= static_cast<size_t>(kMusHeader)) return false;
    lengthTicks = static_cast<uint32_t>(d[0x26] | (d[0x27] << 8) | (d[0x28] << 16) | (d[0x29] << 24));
    const uint8_t* b = d + kMusHeader;
    const size_t n = size - kMusHeader;

    size_t i = 0;
    uint32_t t = 0;
    uint8_t status = 0;
    out.clear();
    while (i < n) {
        // Delta time is ONE byte, 0..255, not a MIDI variable-length quantity.
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
        Event e{t, status, 0, 0};
        if (args >= 1 && i < n) e.a = b[i++];
        if (args >= 2 && i < n) e.b = b[i++];
        out.push_back(e);
    }
    return !out.empty();
}

std::vector<RegWrite> Build(const std::vector<Event>& events, const std::vector<Patch>& patches,
                            int tickRate) {
    std::vector<RegWrite> w;
    if (tickRate <= 0) return w;
    const double samples_per_tick = static_cast<double>(kOplRate) / tickRate;
    int program[16] = {0};

    Emit(w, 0, 0x01, 0x20);        // enable waveform select
    Emit(w, 0, 0xBD, 0x20);        // rhythm mode on, as the game does

    uint8_t rhythm = 0x20;
    // Last frequency written to each percussion host channel, so it is only
    // rewritten when it changes, as the game does.
    int perc_fnum[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int perc_block[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    for (const Event& e : events) {
        const uint32_t at = static_cast<uint32_t>(e.tick * samples_per_tick);
        const int ch = e.status & 0x0F;
        switch (e.status & 0xF0) {
            case 0xC0: {
                program[ch] = e.a;
                if (e.a >= static_cast<int>(patches.size())) break;
                const Patch& p = patches[e.a];
                if (IsPercussion(ch)) {
                    // A percussion voice is one operator, except the bass drum
                    // which keeps both.
                    const int* slots = kPercussionSlot[ch - kFirstPercussionChannel];
                    ProgramOperator(w, at, slots[0], p.op[0], p.op[0].tl);
                    if (slots[1] >= 0) {
                        ProgramOperator(w, at, slots[1], p.op[1], p.op[1].tl);
                        Emit(w, at, 0xC0 + 6,
                             static_cast<uint8_t>(((p.feedback & 7) << 1) | (p.connection & 1)));
                    }
                } else if (ch < kFirstPercussionChannel) {
                    ProgramOperator(w, at, kSlot[ch][0], p.op[0], p.op[0].tl);
                    ProgramOperator(w, at, kSlot[ch][1], p.op[1], p.op[1].tl);
                    Emit(w, at, 0xC0 + ch,
                         static_cast<uint8_t>(((p.feedback & 7) << 1) | (p.connection & 1)));
                }
                break;
            }
            case 0x90: {
                const int note = e.a, vel = e.b;
                if (IsPercussion(ch)) {              // percussion via 0xBD
                    const uint8_t bit = static_cast<uint8_t>(1 << RhythmBit(ch));
                    if (vel > 0) {
                        // Velocity sets the voice's total level, on its carrier.
                        const int* slots = kPercussionSlot[ch - kFirstPercussionChannel];
                        const int slot = slots[1] >= 0 ? slots[1] : slots[0];
                        const int prog = program[ch];
                        const int ksl = prog < static_cast<int>(patches.size())
                                            ? patches[prog].op[slots[1] >= 0 ? 1 : 0].ksl
                                            : 0;
                        Emit(w, at, 0x40 + slot,
                             static_cast<uint8_t>(((ksl & 3) << 6) |
                                                  (std::clamp(0x3F - (vel >> 1), 0, 0x3F))));
                        // A percussion voice still needs a pitch, taken from its host channel's F-number with
                        // the key-on bit left clear.
                        const int opl = kPercussionOplChannel[ch - kFirstPercussionChannel];
                        const int oct = std::clamp(note / 12 - 1, 0, 7);
                        const uint16_t f = kFNum[note % 12];
                        const uint8_t hi = static_cast<uint8_t>(((oct & 7) << 2) | ((f >> 8) & 3));
                        if (perc_fnum[opl] != f || perc_block[opl] != hi) {
                            perc_fnum[opl] = f;
                            perc_block[opl] = hi;
                            Emit(w, at, 0xA0 + opl, static_cast<uint8_t>(f & 0xFF));
                            Emit(w, at, 0xB0 + opl, hi);
                        }
                        // Retrigger: the bit has to drop before it rises again.
                        rhythm &= ~bit;
                        Emit(w, at, 0xBD, rhythm);
                        rhythm |= bit;
                    } else {
                        rhythm &= ~bit;
                    }
                    Emit(w, at, 0xBD, rhythm);
                    break;
                }
                if (ch >= kFirstPercussionChannel) break;
                if (vel == 0) {                      // note on with velocity 0 is a note off
                    Emit(w, at, 0xB0 + ch, 0);
                    break;
                }
                const int prog = program[ch];
                if (prog < static_cast<int>(patches.size())) {
                    // The carrier's total level is the note volume.
                    const Patch& p = patches[prog];
                    const int tl = std::clamp(0x3F - (vel >> 1), 0, 0x3F);
                    Emit(w, at, 0x40 + kSlot[ch][1],
                         static_cast<uint8_t>(((p.op[1].ksl & 3) << 6) | tl));
                }
                const int oct = std::clamp(note / 12 - 1, 0, 7);
                const uint16_t f = kFNum[note % 12];
                Emit(w, at, 0xA0 + ch, static_cast<uint8_t>(f & 0xFF));
                Emit(w, at, 0xB0 + ch,
                     static_cast<uint8_t>(0x20 | ((oct & 7) << 2) | ((f >> 8) & 3)));
                break;
            }
            case 0x80:
                if (IsPercussion(ch)) {
                    rhythm &= ~static_cast<uint8_t>(1 << RhythmBit(ch));
                    Emit(w, at, 0xBD, rhythm);
                } else if (ch < kFirstPercussionChannel) {
                    Emit(w, at, 0xB0 + ch, 0);
                }
                break;
            default:
                break;
        }
    }
    return w;
}

}  // namespace sr2mus

namespace adlibfx {

namespace {

const sr2mus::Patch* Find(const std::vector<sr2mus::Patch>& bank, const char* name) {
    for (const sr2mus::Patch& p : bank) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

uint32_t Samples(double seconds) { return static_cast<uint32_t>(seconds * sr2mus::kOplRate); }

uint32_t Ticks(int ticks) { return static_cast<uint32_t>(ticks * sr2mus::kOplRate / kTickHz); }

// The voice keyed and keyed off by its own routine; the timbre's release is what is heard after.
void Note(std::vector<sr2mus::RegWrite>& w, const std::vector<sr2mus::Patch>& bank, const char* timbre, int ch,
          int volume, int note, uint32_t at, uint32_t hold) {
    if (const sr2mus::Patch* p = Find(bank, timbre)) sr2mus::EffectVoice(w, *p, ch, volume, note, at, hold);
}

}

int Voice(Sound sound) {
    switch (sound) {
        case Sound::Siren:  return 2;
        case Sound::Clank:
        case Sound::ChirpLow:
        case Sound::Thud:
        case Sound::LugNut:
        case Sound::Click:  return 3;
        case Sound::Blown:
        case Sound::Slide:
        case Sound::Tick:
        case Sound::Crash:
        case Sound::Hit:    return 4;
        default:            return 5;
    }
}

std::vector<sr2mus::RegWrite> Build(Sound sound, const std::vector<sr2mus::Patch>& bank, double seconds, int rand8) {
    std::vector<sr2mus::RegWrite> w;
    // "Key left on": the timbre decays by itself, so the voice is only freed later.
    const uint32_t decay = Samples(2.0);
    const uint32_t held = Samples(seconds);
    switch (sound) {
        case Sound::Crash:
            Note(w, bank, "hitcar", 4, 0x7f, 0x14, 0, 1);
            Note(w, bank, "crash", 5, 0x7f, 0x19, 0, 1);
            break;
        case Sound::Hit:
            Note(w, bank, "hitcar", 4, 0x7f, 0x14, 0, 1);
            Note(w, bank, "turn", 3, 0x7f, 0x3d + rand8, 0, 1);
            break;
        case Sound::Clank:    Note(w, bank, "hitcar", 3, 0x70, 0x2e, 0, decay); break;
        case Sound::ChirpLow: Note(w, bank, "turn", 3, 0x75, 0x3d, 0, decay); break;
        case Sound::Thud:     Note(w, bank, "turn1", 3, 0x75, 0x10, 0, decay); break;
        case Sound::Spray:    Note(w, bank, "part", 5, 0x6a, 0x5a, 0, held); break;
        case Sound::Slide:    Note(w, bank, "paint", 4, 0x5a, 0x52, 0, held); break;
        case Sound::Tick:     Note(w, bank, "paint", 4, 0x6a, 0x54, 0, held); break;
        case Sound::Siren: {
            Note(w, bank, "police", 2, 0x7f, 0x50, 0, held);
            // Bend range 5: full bend is five semitones up; 20 ticks down, 21 up.
            for (int t = 20; Ticks(t) < held; t += 41) {
                sr2mus::EffectPitch(w, 2, 0x55, Ticks(t));
                if (Ticks(t + 21) < held) sr2mus::EffectPitch(w, 2, 0x50, Ticks(t + 21));
            }
            break;
        }
        case Sound::Squeal:
            for (int t = 0; Ticks(t) < held; t += 16) Note(w, bank, "part", 5, 0x60, 0x78, Ticks(t), Ticks(8));
            break;
        case Sound::Putt:      Note(w, bank, "part", 5, 0x38, 0x0e, 0, held); break;
        case Sound::AirWrench: Note(w, bank, "part", 5, 0x4a, 0x1c, 0, held); break;
        case Sound::LugNut:    Note(w, bank, "turn1", 3, 0x7f, 0x0e, 0, held); break;
        case Sound::Click:     Note(w, bank, "piano1", 3, 0x7f, 0x5c, 0, 1); break;
        case Sound::Blown:     Note(w, bank, "hitcar", 4, 0x7f, 0x28, 0, decay); break;
    }
    std::stable_sort(w.begin(), w.end(), [](const sr2mus::RegWrite& a, const sr2mus::RegWrite& b) {
        return a.sample < b.sample;
    });
    return w;
}

}
