// Street Rod (Amiga) music player - SDL3
// Goal: mirror MusicTick/MusicControlByte behavior from Street_Rod.c.

#include "amiga_music_player.h"

#include "../../parsers/adf.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace sr {

namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels = 4;
constexpr int kLoopSlots = 5;
constexpr int kMaxEnvelopePoints = 16;
constexpr double kCiaClockHz = 709379.0;
constexpr double kPaulaClock = 3579545.0;
constexpr int kPeriodTable[12] = {270, 254, 240, 226, 214, 202, 190, 180, 169, 160, 151, 144};

struct ChannelState {
    size_t stream_index = 0;
    int wait_ticks = 0;
    int instrument = 0;
    int note_volume = 64;
    int env_step = 0;
    int env_volume = 0;
    bool sample_active = false;

    const int8_t* sample_ptr = nullptr;
    int sample_len = 0;
    double sample_pos = 0.0;
    double sample_step = 0.0;

    std::array<size_t, kLoopSlots> slot_ptr = {};
    std::array<bool, kLoopSlots> slot_valid = {};
    std::array<int16_t, kLoopSlots> loop_counter = {};
};

struct RuntimeState {
    std::array<ChannelState, kChannels> channel = {};
    std::vector<std::array<size_t, kChannels>> patterns;
    std::vector<InstrumentDef> instruments;
    std::vector<EnvelopeDef> envelopes;
    std::vector<uint8_t> notes;
    std::vector<int8_t> wave_bank[2];

    int arrangement_index = 0;
    int arrangement_len = 0;
    int master_volume = 15;
    // InitializeCarMemoryBlocks + music start path sets this to 0x54 in game.
    int global_transpose = 0x54;
    int speed_ticks = 2;
    int cia_timer_period = 42000;
    bool conditional_flag = false;
    bool playing = true;
    bool debug = false;
    bool debug_truncated = false;
    int debug_events_left = 0;

    double sample_tick_accum = 0.0;
    uint64_t irq_counter = 0;
    uint64_t total_seq_ticks = 0;
    uint64_t last_debug_summary_irq = 0;
    uint64_t speed_counter = 0;
    std::array<uint64_t, kChannels> note_count = {};
    std::array<uint64_t, kChannels> cmd_count = {};
};

void DebugLog(RuntimeState& s, const char* fmt, ...) {
    if (!s.debug) {
        return;
    }
    if (s.debug_events_left <= 0) {
        if (!s.debug_truncated) {
            s.debug_truncated = true;
            std::fprintf(stderr, "[DBG] event budget exhausted, continuing with summaries only\n");
        }
        return;
    }
    s.debug_events_left--;
    std::fprintf(stderr, "[DBG] ");
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "\n");
}

uint16_t ReadBE16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}

std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::rewind(f);
    if (n <= 0) {
        std::fclose(f);
        return {};
    }
    std::vector<uint8_t> out(static_cast<size_t>(n));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got != out.size()) {
        return {};
    }
    return out;
}

void ConvertU8ToS8(const std::vector<uint8_t>& in, std::vector<int8_t>& out) {
    out.resize(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        out[i] = static_cast<int8_t>(in[i]);
    }
}

bool ParsePatterns(const std::vector<uint8_t>& note_bytes, std::vector<std::array<size_t, kChannels>>& out_patterns) {
    out_patterns.clear();
    size_t p = 0;
    while (p < note_bytes.size()) {
        std::array<size_t, kChannels> starts = {};
        starts[0] = p;
        bool complete = true;
        for (int ch = 1; ch < 5; ch++) {
            size_t ff_pos = p;
            while (ff_pos < note_bytes.size() && note_bytes[ff_pos] != 0xFF) {
                ff_pos++;
            }
            if (ff_pos >= note_bytes.size()) {
                return false;
            }
            p = ff_pos + 1;
            if (p < note_bytes.size() && note_bytes[p] == 0xEF) {
                p = ff_pos + 2;
                if (ch != 4) {
                    complete = false;
                }
                break;
            }
            if (ch < 4) {
                starts[ch] = p;
            }
        }
        if (!complete) {
            return false;
        }
        out_patterns.push_back(starts);
    }
    return !out_patterns.empty();
}

bool ParseInstruments(const std::vector<uint8_t>& ins_bytes, std::vector<InstrumentDef>& out_instruments) {
    if (ins_bytes.size() % 10 != 0) {
        return false;
    }
    out_instruments.clear();
    size_t count = ins_bytes.size() / 10;
    out_instruments.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = ins_bytes.data() + i * 10;
        InstrumentDef d;
        d.envelope_index = ReadBE16(p + 0);
        d.unknown = ReadBE16(p + 2);
        d.bank_index = ReadBE16(p + 4);
        d.sample_length_words = ReadBE16(p + 6);
        d.sample_offset_bytes = ReadBE16(p + 8);
        out_instruments.push_back(d);
    }
    return true;
}

bool ParseEnvelopes(const std::vector<uint8_t>& env_bytes, std::vector<EnvelopeDef>& out_envelopes) {
    if (env_bytes.empty() || (env_bytes.size() % 32) != 0) {
        return false;
    }
    out_envelopes.clear();
    size_t count = env_bytes.size() / 32;
    out_envelopes.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = env_bytes.data() + i * 32;
        EnvelopeDef e;
        for (int j = 0; j < kMaxEnvelopePoints; j++) {
            e.points[j] = static_cast<int16_t>(ReadBE16(p + j * 2));
        }
        out_envelopes.push_back(e);
    }
    return true;
}

int NoteToPeriod(int note) {
    note = std::clamp(note, 0, 99);
    int semitone = note % 12;
    int octave = note / 12;
    int period = kPeriodTable[semitone];
    int shift = 10 - octave;
    for (int i = 0; i < shift; i++) {
        period <<= 1;
    }
    return period;
}

double PeriodToStep(int period) {
    if (period <= 0) {
        return 0.0;
    }
    double playback_hz = kPaulaClock / static_cast<double>(period);
    return playback_hz / static_cast<double>(kSampleRate);
}

void StartSample(RuntimeState& s, int ch_index, int note_byte) {
    ChannelState& ch = s.channel[ch_index];
    if (ch.instrument < 0 || static_cast<size_t>(ch.instrument) >= s.instruments.size()) {
        return;
    }
    const InstrumentDef& inst = s.instruments[static_cast<size_t>(ch.instrument)];
    if (inst.bank_index > 1) {
        return;
    }
    const std::vector<int8_t>& bank = s.wave_bank[inst.bank_index];
    size_t off = inst.sample_offset_bytes;
    // Decompile shows this field passed directly as sample length to audio.device.
    size_t len = static_cast<size_t>(inst.sample_length_words);
    if (off >= bank.size()) {
        return;
    }
    if (off + len > bank.size()) {
        len = bank.size() - off;
    }
    int note = note_byte + s.global_transpose;
    int period = NoteToPeriod(note);
    ch.sample_ptr = bank.data() + off;
    ch.sample_len = static_cast<int>(len);
    ch.sample_pos = 0.0;
    ch.sample_step = PeriodToStep(period);
    ch.sample_active = (ch.sample_len > 0);
    ch.env_step = 0;
    // Start audible on note-on, then let envelope tick shape it.
    ch.env_volume = std::clamp((ch.note_volume * s.master_volume) / 15, 1, 64);
    s.note_count[ch_index]++;
    DebugLog(
        s,
        "seq=%llu ch=%d NOTE raw=%d note=%d inst=%d env=%u bank=%u off=%u len=%d period=%d step=%.6f",
        static_cast<unsigned long long>(s.total_seq_ticks),
        ch_index,
        note_byte,
        note,
        ch.instrument,
        static_cast<unsigned>(inst.envelope_index),
        static_cast<unsigned>(inst.bank_index),
        static_cast<unsigned>(inst.sample_offset_bytes),
        ch.sample_len,
        period,
        ch.sample_step);
}

void NextArrangement(RuntimeState& s) {
    s.arrangement_index = (s.arrangement_index + 1) % s.arrangement_len;
    const std::array<size_t, kChannels>& start = s.patterns[static_cast<size_t>(s.arrangement_index)];
    for (int ch = 0; ch < kChannels; ch++) {
        s.channel[ch].stream_index = start[ch];
    }
}

void HandleControlByte(RuntimeState& s, int ch_index, uint8_t cmd) {
    ChannelState& ch = s.channel[ch_index];
    const std::vector<uint8_t>& n = s.notes;
    size_t before = ch.stream_index;
    auto can_read = [&](size_t i) -> bool { return i < n.size(); };
    switch (cmd) {
        case 0x80: {
            if (!can_read(ch.stream_index)) return;
            ch.instrument = static_cast<int>(n[ch.stream_index++]);
            break;
        }
        case 0x81: {
            if (!can_read(ch.stream_index)) return;
            uint8_t slot = n[ch.stream_index++];
            if (slot < kLoopSlots) {
                ch.slot_ptr[slot] = ch.stream_index;
                ch.slot_valid[slot] = true;
            }
            break;
        }
        case 0x82: {
            if (!can_read(ch.stream_index)) return;
            uint8_t slot = n[ch.stream_index];
            if (slot < kLoopSlots && ch.slot_valid[slot]) {
                ch.stream_index = ch.slot_ptr[slot];
            } else {
                ch.stream_index++;
            }
            break;
        }
        case 0x83: {
            if (!can_read(ch.stream_index)) return;
            uint8_t packed = n[ch.stream_index];
            uint8_t slot = static_cast<uint8_t>(packed & 0x0F);
            if (slot >= kLoopSlots) {
                ch.stream_index++;
                return;
            }
            int16_t& ctr = ch.loop_counter[slot];
            if (ctr < 1) {
                if (ctr == 0) {
                    ctr = -1;
                    ch.stream_index++;
                } else {
                    ctr = static_cast<int16_t>((packed >> 4) - 1);
                    if (ch.slot_valid[slot]) {
                        ch.stream_index = ch.slot_ptr[slot];
                    } else {
                        ch.stream_index++;
                    }
                }
            } else {
                ctr = static_cast<int16_t>(ctr - 1);
                if (ch.slot_valid[slot]) {
                    ch.stream_index = ch.slot_ptr[slot];
                } else {
                    ch.stream_index++;
                }
            }
            break;
        }
        case 0x84: {
            if (can_read(ch.stream_index)) {
                s.speed_ticks += static_cast<int>(n[ch.stream_index++]);
                s.speed_ticks = std::max(1, s.speed_ticks);
            }
            break;
        }
        case 0x85: {
            if (can_read(ch.stream_index)) {
                s.speed_ticks -= static_cast<int>(n[ch.stream_index++]);
                s.speed_ticks = std::max(1, s.speed_ticks);
            }
            break;
        }
        case 0x86: {
            if (can_read(ch.stream_index)) {
                s.global_transpose += static_cast<int>(n[ch.stream_index++]);
            }
            break;
        }
        case 0x87: {
            if (can_read(ch.stream_index)) {
                s.global_transpose -= static_cast<int>(n[ch.stream_index++]);
            }
            break;
        }
        case 0x88: {
            if (can_read(ch.stream_index)) {
                ch.note_volume += static_cast<int>(n[ch.stream_index++]);
                ch.note_volume = std::clamp(ch.note_volume, 0, 64);
            }
            break;
        }
        case 0x89: {
            if (can_read(ch.stream_index)) {
                ch.note_volume -= static_cast<int>(n[ch.stream_index++]);
                ch.note_volume = std::clamp(ch.note_volume, 0, 64);
            }
            break;
        }
        case 0x90: {
            if (can_read(ch.stream_index)) {
                ch.wait_ticks = static_cast<int>(n[ch.stream_index++]) - 1;
                ch.wait_ticks = std::max(0, ch.wait_ticks);
            }
            break;
        }
        case 0x91: {
            if (!can_read(ch.stream_index)) return;
            uint8_t slot = n[ch.stream_index];
            if (!s.conditional_flag && slot < kLoopSlots && ch.slot_valid[slot]) {
                ch.stream_index = ch.slot_ptr[slot];
            } else {
                ch.stream_index++;
            }
            break;
        }
        case 0x92: {
            if (can_read(ch.stream_index)) {
                s.cia_timer_period += static_cast<int>(n[ch.stream_index++]) * 1000;
                s.cia_timer_period = std::clamp(s.cia_timer_period, 1000, 65535);
            }
            break;
        }
        case 0x93: {
            if (can_read(ch.stream_index)) {
                s.cia_timer_period -= static_cast<int>(n[ch.stream_index++]) * 1000;
                s.cia_timer_period = std::clamp(s.cia_timer_period, 1000, 65535);
            }
            break;
        }
        case 0x94: {
            if (can_read(ch.stream_index)) {
                s.cia_timer_period = static_cast<int>(n[ch.stream_index++]) * 1000;
                s.cia_timer_period = std::clamp(s.cia_timer_period, 1000, 65535);
            }
            break;
        }
        case 0x95: {
            if (ch_index == 0) {
                NextArrangement(s);
            } else {
                const std::array<size_t, kChannels>& start = s.patterns[static_cast<size_t>(s.arrangement_index)];
                ch.stream_index = start[ch_index];
            }
            break;
        }
        case 0x96: {
            if (can_read(ch.stream_index)) {
                ch.note_volume = std::clamp(static_cast<int>(n[ch.stream_index++]), 0, 64);
            }
            break;
        }
        case 0x97: {
            if (can_read(ch.stream_index)) {
                s.speed_ticks = std::max(1, static_cast<int>(n[ch.stream_index++]));
            }
            break;
        }
        case 0xA0: {
            s.playing = false;
            break;
        }
        default: {
            if (can_read(ch.stream_index)) {
                ch.stream_index++;
            }
            break;
        }
    }
    s.cmd_count[ch_index]++;
    int consumed = static_cast<int>(ch.stream_index - before);
    if (consumed < 0) {
        consumed = 0;
    }
    DebugLog(
        s,
        "seq=%llu ch=%d CMD=%02X ptr=%zu->%zu consumed=%d wait=%d inst=%d vol=%d transpose=%d speed=%d cia=%d arr=%d/%d",
        static_cast<unsigned long long>(s.total_seq_ticks),
        ch_index,
        static_cast<unsigned>(cmd),
        before,
        ch.stream_index,
        consumed,
        ch.wait_ticks,
        ch.instrument,
        ch.note_volume,
        s.global_transpose,
        s.speed_ticks,
        s.cia_timer_period,
        s.arrangement_index,
        s.arrangement_len);
}

void MusicTick(RuntimeState& s) {
    s.total_seq_ticks++;
    for (int ch_index = 0; ch_index < kChannels; ch_index++) {
        ChannelState& ch = s.channel[ch_index];
        if (ch.wait_ticks > 0) {
            ch.wait_ticks--;
            continue;
        }
        bool consumed = false;
        int guard = 0;
        while (s.playing && !consumed && guard < 1024) {
            guard++;
            if (ch.stream_index >= s.notes.size()) {
                s.playing = false;
                break;
            }
            uint8_t b = s.notes[ch.stream_index++];
            if (b < 100) {
                StartSample(s, ch_index, static_cast<int>(b));
                if (ch.stream_index >= s.notes.size()) {
                    s.playing = false;
                    break;
                }
                int duration = static_cast<int>(s.notes[ch.stream_index++]);
                ch.wait_ticks = duration - 1;
                ch.wait_ticks = std::max(0, ch.wait_ticks);
                DebugLog(
                    s,
                    "seq=%llu ch=%d DUR=%d wait=%d",
                    static_cast<unsigned long long>(s.total_seq_ticks),
                    ch_index,
                    duration,
                    ch.wait_ticks);
                consumed = true;
            } else {
                HandleControlByte(s, ch_index, b);
                if (b == 0x90) {
                    consumed = true;
                }
            }
        }
    }
}

void EnvelopeTick(RuntimeState& s) {
    for (int ch_index = 0; ch_index < kChannels; ch_index++) {
        ChannelState& ch = s.channel[ch_index];
        if (!ch.sample_active) {
            continue;
        }
        if (ch.instrument < 0 || static_cast<size_t>(ch.instrument) >= s.instruments.size()) {
            continue;
        }
        uint16_t env_idx = s.instruments[static_cast<size_t>(ch.instrument)].envelope_index;
        if (env_idx >= s.envelopes.size()) {
            continue;
        }
        int step = std::clamp(ch.env_step, 0, kMaxEnvelopePoints - 1);
        const EnvelopeDef& env = s.envelopes[env_idx];
        int cur = env.points[step];
        int prev = env.points[std::max(0, step - 1)];
        int target = (((cur * ch.note_volume) / 64) * s.master_volume) / 15;
        target = std::clamp(target, 0, 64);

        // Common envelopes have zero plateaus in early points. If we sit on
        // consecutive zero points, advance to avoid getting stuck at silence.
        if (cur == 0 && prev == 0 && ch.env_step < (kMaxEnvelopePoints - 1)) {
            ch.env_step++;
            continue;
        }

        if ((cur - prev) < 1) {
            if (target < ch.env_volume) {
                ch.env_volume--;
            } else if (cur != prev && ch.env_step < (kMaxEnvelopePoints - 1)) {
                ch.env_step++;
            }
        } else if (ch.env_volume < target) {
            ch.env_volume++;
        } else if (ch.env_step < (kMaxEnvelopePoints - 1)) {
            ch.env_step++;
        }

        if (ch.env_volume <= 0 && cur == 0) {
            ch.sample_active = false;
        }
    }
}

void MixSamples(RuntimeState& s, float* out, int frame_count) {
    std::fill(out, out + frame_count, 0.0f);
    for (int i = 0; i < frame_count; i++) {
        s.sample_tick_accum += 1.0;
        double irq_hz = kCiaClockHz / static_cast<double>(std::max(1, s.cia_timer_period));
        if (irq_hz < 1.0) {
            irq_hz = 1.0;
        }
        double samples_per_irq = static_cast<double>(kSampleRate) / irq_hz;
        while (s.sample_tick_accum >= samples_per_irq) {
            s.sample_tick_accum -= samples_per_irq;
            s.irq_counter++;
            EnvelopeTick(s);
            if ((s.speed_counter % static_cast<uint64_t>(std::max(1, s.speed_ticks))) == 0) {
                MusicTick(s);
            }
            s.speed_counter++;
            irq_hz = kCiaClockHz / static_cast<double>(std::max(1, s.cia_timer_period));
            if (irq_hz < 1.0) {
                irq_hz = 1.0;
            }
            samples_per_irq = static_cast<double>(kSampleRate) / irq_hz;
            if (s.debug && (s.irq_counter - s.last_debug_summary_irq) >= static_cast<uint64_t>(std::max(1.0, irq_hz))) {
                s.last_debug_summary_irq = s.irq_counter;
                std::fprintf(
                    stderr,
                    "[DBG] summary irq=%llu seq=%llu arr=%d/%d speed=%d cia=%d irq_hz=%.2f playing=%d\n",
                    static_cast<unsigned long long>(s.irq_counter),
                    static_cast<unsigned long long>(s.total_seq_ticks),
                    s.arrangement_index,
                    s.arrangement_len,
                    s.speed_ticks,
                    s.cia_timer_period,
                    irq_hz,
                    s.playing ? 1 : 0);
                for (int ch = 0; ch < kChannels; ch++) {
                    const ChannelState& c = s.channel[ch];
                    std::fprintf(
                        stderr,
                        "      ch%d ptr=%zu wait=%d inst=%d nvol=%d evol=%d estep=%d act=%d pos=%.1f/%d notes=%llu cmds=%llu\n",
                        ch,
                        c.stream_index,
                        c.wait_ticks,
                        c.instrument,
                        c.note_volume,
                        c.env_volume,
                        c.env_step,
                        c.sample_active ? 1 : 0,
                        c.sample_pos,
                        c.sample_len,
                        static_cast<unsigned long long>(s.note_count[ch]),
                        static_cast<unsigned long long>(s.cmd_count[ch]));
                }
            }
        }

        float mix = 0.0f;
        for (int ch_index = 0; ch_index < kChannels; ch_index++) {
            ChannelState& ch = s.channel[ch_index];
            if (!ch.sample_active || ch.sample_ptr == nullptr || ch.sample_len <= 0) {
                continue;
            }
            int idx = static_cast<int>(ch.sample_pos);
            if (idx < 0 || idx >= ch.sample_len) {
                ch.sample_active = false;
                continue;
            }
            float sample = static_cast<float>(ch.sample_ptr[idx]) / 128.0f;
            float vol = static_cast<float>(ch.env_volume) / 64.0f;
            mix += sample * vol;
            ch.sample_pos += ch.sample_step;
            if (static_cast<int>(ch.sample_pos) >= ch.sample_len) {
                ch.sample_active = false;
            }
        }
        out[i] = std::clamp(mix * 0.5f, -1.0f, 1.0f);
    }
}

void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    (void)total;
    RuntimeState* state = static_cast<RuntimeState*>(userdata);
    int frame_count = additional / static_cast<int>(sizeof(float));
    if (frame_count <= 0) {
        return;
    }
    std::vector<float> buffer(static_cast<size_t>(frame_count));
    if (state && state->playing) {
        MixSamples(*state, buffer.data(), frame_count);
    } else {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    SDL_PutAudioStreamData(stream, buffer.data(), static_cast<int>(buffer.size() * sizeof(float)));
}

bool ExtractAdfFile(const std::string& adf_path, const char* name, std::vector<uint8_t>& out) {
    AdfFileData* d = extract_adf_file_to_memory(adf_path.c_str(), name);
    if (!d) {
        return false;
    }
    out.assign(d->data, d->data + d->size);
    free_adf_file_data(d);
    return true;
}

}  // namespace

bool LoadSongDataFromDirectory(const std::string& directory, SongData& out_data, std::string& out_error) {
    out_data.note_bytes = ReadFileBytes(directory + "/msr.note");
    out_data.ins_bytes = ReadFileBytes(directory + "/srl.ins");
    out_data.env_bytes = ReadFileBytes(directory + "/srl.env");
    std::vector<uint8_t> wave0 = ReadFileBytes(directory + "/srl1.wave");
    std::vector<uint8_t> wave1 = ReadFileBytes(directory + "/srl0.wave");

    if (out_data.note_bytes.empty()) {
        out_error = "missing msr.note";
        return false;
    }
    if (out_data.ins_bytes.empty()) {
        out_error = "missing srl.ins";
        return false;
    }
    if (out_data.env_bytes.empty()) {
        out_error = "missing srl.env";
        return false;
    }
    if (wave0.empty() || wave1.empty()) {
        out_error = "missing srl1.wave or srl0.wave";
        return false;
    }

    ConvertU8ToS8(wave0, out_data.wave0_bytes);
    ConvertU8ToS8(wave1, out_data.wave1_bytes);
    return true;
}

bool LoadSongDataFromAdf(const std::string& adf_path, SongData& out_data, std::string& out_error) {
    std::vector<uint8_t> wave0;
    std::vector<uint8_t> wave1;
    if (!ExtractAdfFile(adf_path, "msr.note", out_data.note_bytes)) {
        out_error = "cannot read msr.note from ADF";
        return false;
    }
    if (!ExtractAdfFile(adf_path, "srl.ins", out_data.ins_bytes)) {
        out_error = "cannot read srl.ins from ADF";
        return false;
    }
    if (!ExtractAdfFile(adf_path, "srl.env", out_data.env_bytes)) {
        out_error = "cannot read srl.env from ADF";
        return false;
    }
    if (!ExtractAdfFile(adf_path, "srl1.wave", wave0) || !ExtractAdfFile(adf_path, "srl0.wave", wave1)) {
        out_error = "cannot read srl1.wave or srl0.wave from ADF";
        return false;
    }
    ConvertU8ToS8(wave0, out_data.wave0_bytes);
    ConvertU8ToS8(wave1, out_data.wave1_bytes);
    return true;
}

int RunMusicPlayer(const SongData& song, bool loop_forever, bool debug, int debug_events, int base_transpose) {
    RuntimeState s;
    s.notes = song.note_bytes;
    // Executable behavior indicates instrument bank index 1 points at the
    // main music sample bank (srl1.wave) for Street Rod.
    s.wave_bank[0] = song.wave1_bytes;
    s.wave_bank[1] = song.wave0_bytes;

    if (!ParseInstruments(song.ins_bytes, s.instruments)) {
        std::fprintf(stderr, "Invalid srl.ins: expected 10-byte entries\n");
        return 1;
    }
    if (!ParseEnvelopes(song.env_bytes, s.envelopes)) {
        std::fprintf(stderr, "Invalid srl.env: expected 32-byte entries\n");
        return 1;
    }
    if (!ParsePatterns(song.note_bytes, s.patterns)) {
        std::fprintf(stderr, "Invalid msr.note: failed to split into 4-channel pattern blocks\n");
        return 1;
    }

    s.arrangement_len = static_cast<int>(s.patterns.size());
    s.debug = debug;
    s.debug_events_left = std::max(0, debug_events);
    s.global_transpose = base_transpose;
    for (int ch = 0; ch < kChannels; ch++) {
        s.channel[ch].stream_index = s.patterns[0][ch];
        s.channel[ch].loop_counter.fill(-1);
    }

    std::printf("Patterns: %d  Instruments: %zu  Envelopes: %zu  BaseTranspose: %d\n",
                s.arrangement_len, s.instruments.size(), s.envelopes.size(), s.global_transpose);
    if (s.debug) {
        std::fprintf(stderr, "[DBG] enabled with event budget=%d\n", s.debug_events_left);
    }

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec spec = {};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = kSampleRate;

    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, &s);
    if (!stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_ResumeAudioStreamDevice(stream);

    std::puts("Playing. Press Enter to stop.");
    (void)std::getchar();

    if (!loop_forever) {
        s.playing = false;
    }

    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 0;
}

}  // namespace sr

int main(int argc, char* argv[]) {
    std::string source_dir = ".";
    std::string adf_path;
    bool use_adf = false;
    bool loop_forever = true;
    bool debug = false;
    int debug_events = 600;
    int base_transpose = 0x54;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            source_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--adf") == 0 && i + 1 < argc) {
            adf_path = argv[++i];
            use_adf = true;
        } else if (std::strcmp(argv[i], "--no-loop") == 0) {
            loop_forever = false;
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            debug = true;
        } else if (std::strcmp(argv[i], "--debug-events") == 0 && i + 1 < argc) {
            debug_events = std::max(0, std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--transpose") == 0 && i + 1 < argc) {
            base_transpose = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::puts("Usage:");
            std::puts("  amiga_music_player --dir <folder-with-msr.note+srl.*>");
            std::puts("  amiga_music_player --adf <SR_disk1.adf>");
            std::puts("Options:");
            std::puts("  --no-loop    stop after stream stop");
            std::puts("  --debug      print sequencer/channel debug logs");
            std::puts("  --debug-events <N>  max detailed event lines (default 600)");
            std::puts("  --transpose <N>  base note transpose (default 84)");
            return 0;
        }
    }

    sr::SongData song;
    std::string err;
    bool ok = false;
    if (use_adf) {
        ok = sr::LoadSongDataFromAdf(adf_path, song, err);
    } else {
        ok = sr::LoadSongDataFromDirectory(source_dir, song, err);
    }
    if (!ok) {
        std::fprintf(stderr, "Load failed: %s\n", err.c_str());
        return 1;
    }

    return sr::RunMusicPlayer(song, loop_forever, debug, debug_events, base_transpose);
}
