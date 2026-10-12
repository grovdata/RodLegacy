#include "audio/amiga_music.h"
#include "audio/record_tap.h"
#include "core/utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace {
constexpr double kPaulaClock = 3579545.0;
constexpr double kCiaClockHz = 709379.0;
constexpr int kPeriodTable[12] = {270, 254, 240, 226, 214, 202, 190, 180, 169, 160, 151, 144};

// The original fixes the modulus at 3: step 3 of msr.note has no 0x95, so nothing could advance out of it.
constexpr int kArrangementSteps = 3;

// The game drops master volume to 9 on every music start; the envelope is scaled by it.
constexpr int kMusicMasterVolume = 9;

// Before any music has started. The intro chimes play under these.
constexpr int kIdleMasterVolume = 15;
constexpr int kIdleCiaPeriod = 43000;
}

AmigaMusic::AmigaMusic() = default;

AmigaMusic::~AmigaMusic() {
    Shutdown();
}

bool AmigaMusic::Init(int sample_rate) {
    sample_rate_ = sample_rate;

    SDL_AudioSpec want {};
    SDL_AudioSpec have {};
    want.freq = sample_rate_;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = AudioCallback;
    want.userdata = this;

    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!device_) {
        return false;
    }
    sample_rate_ = have.freq;
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void AmigaMusic::Shutdown() {
    playing_.store(false);
    loaded_.store(false);
    if (device_) {
        SDL_PauseAudioDevice(device_, 1);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

bool AmigaMusic::Load(const uint8_t* note_data, size_t note_size,
                      const uint8_t* wave1_data, size_t wave1_size,
                      const uint8_t* wave0_data, size_t wave0_size,
                      const uint8_t* ins_data, size_t ins_size,
                      const uint8_t* env_data, size_t env_size,
                      bool sr2) {
    if (!note_data || !wave1_data || !wave0_data || !ins_data || !env_data) {
        return false;
    }
    if (note_size == 0 || wave1_size == 0 || wave0_size == 0 || ins_size == 0 || env_size == 0) {
        return false;
    }

    std::vector<uint8_t> new_note(note_data, note_data + note_size);
    std::vector<uint8_t> new_ins(ins_data, ins_data + ins_size);
    std::vector<uint8_t> new_env(env_data, env_data + env_size);
    std::vector<int8_t> new_wave1(wave1_size);
    std::vector<int8_t> new_wave0(wave0_size);
    for (size_t i = 0; i < wave1_size; i++) new_wave1[i] = static_cast<int8_t>(wave1_data[i]);
    for (size_t i = 0; i < wave0_size; i++) new_wave0[i] = static_cast<int8_t>(wave0_data[i]);

    std::vector<std::array<size_t, 4>> new_patterns;
    std::vector<InstrumentDef> new_instruments;
    std::vector<EnvelopeDef> new_envelopes;

    if (!ParsePatterns(new_note, new_patterns)) return false;
    if (!ParseInstruments(new_ins, sr2, new_instruments)) return false;
    if (!ParseEnvelopes(new_env, new_envelopes)) return false;

    SDL_LockAudioDevice(device_);
    note_bytes_ = std::move(new_note);
    instruments_ = std::move(new_instruments);
    envelopes_ = std::move(new_envelopes);
    patterns_ = std::move(new_patterns);

    // bank 1 -> srl1.wave (music tones), bank 0 -> srl0.wave.
    wave_bank_[1] = std::move(new_wave1);
    wave_bank_[0] = std::move(new_wave0);

    loaded_.store(ResetState());
    SDL_UnlockAudioDevice(device_);
    return loaded_.load();
}

void AmigaMusic::Play() {
    if (!loaded_.load()) return;
    SDL_LockAudioDevice(device_);
    ResetState();
    playing_.store(true);
    SDL_UnlockAudioDevice(device_);
}

void AmigaMusic::PlayArrangement(int index, int ciaPeriod,
                                 const int* channelInstruments, const int* channelVolumes) {
    if (!loaded_.load()) return;
    SDL_LockAudioDevice(device_);
    if (ResetState() && index >= 0 && index < static_cast<int>(patterns_.size())) {
        arrangement_index_ = index;
        cia_timer_period_ = std::clamp(ciaPeriod, 1000, 65535);
        const auto& start = patterns_[static_cast<size_t>(index)];
        for (int ch = 0; ch < 4; ch++) {
            channel_[ch] = ChannelState{};
            channel_[ch].stream_index = start[ch];
            channel_[ch].loop_counter.fill(-1);
            channel_[ch].note_volume =
                channelVolumes != nullptr ? channelVolumes[ch] : 64;
            if (channelInstruments != nullptr) {
                channel_[ch].instrument = channelInstruments[ch];
            }
        }
        playing_.store(true);
    }
    SDL_UnlockAudioDevice(device_);
}

void AmigaMusic::PlayEffectNote(int channel, int instrument, int period, int volume) {
    if (!loaded_.load() || channel < 0 || channel >= 4) return;
    SDL_LockAudioDevice(device_);
    ChannelState& c = channel_[channel];
    c.instrument = instrument;
    c.note_volume = std::clamp(volume, 0, 64);
    StartSample(channel, period, kIdleMasterVolume);
    SDL_UnlockAudioDevice(device_);
}

void AmigaMusic::Stop() {
    SDL_LockAudioDevice(device_);
    playing_.store(false);
    paused_.store(false);
    for (ChannelState& c : channel_) {
        c.sample_active = false;
    }
    SDL_UnlockAudioDevice(device_);
}

void AmigaMusic::Pause() {
    paused_.store(true);
}

void AmigaMusic::Resume() {
    paused_.store(false);
}

bool AmigaMusic::IsPaused() const {
    return paused_.load();
}

void AmigaMusic::SetVolume(float volume) {
    volume = std::clamp(volume, 0.0f, 1.0f);
    volume_scale_.store(volume);
}

void AmigaMusic::AudioCallback(void* userdata, uint8_t* stream, int len) {
    auto* self = static_cast<AmigaMusic*>(userdata);
    int frames = len / static_cast<int>(sizeof(int16_t));
    self->FillBuffer(reinterpret_cast<int16_t*>(stream), frames);
    record_tap::Write("amiga_music", self->sample_rate_, 1, stream, len);
}

void AmigaMusic::FillBuffer(int16_t* buffer, int frames) {
    const bool anyActive = std::any_of(channel_.begin(), channel_.end(),
                                       [](const ChannelState& c) { return c.sample_active; });
    if (!loaded_.load() || paused_.load() || (!playing_.load() && !anyActive)) {
        std::memset(buffer, 0, static_cast<size_t>(frames) * sizeof(int16_t));
        return;
    }

    for (int i = 0; i < frames; i++) {
        sample_tick_accum_ += 1.0;

        const int period = playing_.load() ? cia_timer_period_ : kIdleCiaPeriod;
        double irq_hz = kCiaClockHz / static_cast<double>(std::max(1, period));
        if (irq_hz < 1.0) irq_hz = 1.0;
        double samples_per_irq = static_cast<double>(sample_rate_) / irq_hz;

        while (sample_tick_accum_ >= samples_per_irq) {
            sample_tick_accum_ -= samples_per_irq;
            // The interrupt runs the sequencer first, then the envelopes.
            if (playing_.load() &&
                (speed_counter_ % static_cast<uint64_t>(std::max(1, speed_ticks_))) == 0) {
                MusicTick();
            }
            EnvelopeTick();
            speed_counter_++;

            irq_hz = kCiaClockHz / static_cast<double>(
                std::max(1, playing_.load() ? cia_timer_period_ : kIdleCiaPeriod));
            if (irq_hz < 1.0) irq_hz = 1.0;
            samples_per_irq = static_cast<double>(sample_rate_) / irq_hz;
        }

        float mix = 0.0f;
        for (int ch = 0; ch < 4; ch++) {
            ChannelState& c = channel_[ch];
            if (!c.sample_active || !c.sample_ptr || c.sample_len <= 0) continue;
            int idx = static_cast<int>(c.sample_pos);
            if (idx < 0 || idx >= c.sample_len) {
                c.sample_active = false;
                continue;
            }
            float s = static_cast<float>(c.sample_ptr[idx]) / 128.0f;
            float v = static_cast<float>(c.env.volume) / 64.0f;
            mix += s * v;
            c.sample_pos += c.sample_step;
            if (static_cast<int>(c.sample_pos) >= c.sample_len) {
                if (c.loop) {
                    c.sample_pos = std::fmod(c.sample_pos, static_cast<double>(c.sample_len));
                } else {
                    c.sample_active = false;
                }
            }
        }

        mix *= volume_scale_.load();
        mix = std::clamp(mix, -1.0f, 1.0f);
        buffer[i] = static_cast<int16_t>(mix * 32767.0f);
    }
}

void AmigaMusic::EnvelopeTick() {
    for (ChannelState& c : channel_) {
        if (!c.sample_active || !c.has_env) continue;
        c.env.Tick();
        if (!c.env.Active()) c.sample_active = false;
    }
}

void AmigaMusic::MusicTick() {
    for (int ch = 0; ch < 4; ch++) {
        ChannelState& c = channel_[ch];
        if (c.wait_ticks > 0) {
            c.wait_ticks--;
            continue;
        }

        bool consumed = false;
        int guard = 0;
        while (playing_.load() && !consumed && guard < 1024) {
            guard++;
            if (c.stream_index >= note_bytes_.size()) {
                playing_.store(false);
                break;
            }
            uint8_t b = note_bytes_[c.stream_index++];
            if (b < 100) {
                StartNote(ch, static_cast<int>(b));
                if (c.stream_index >= note_bytes_.size()) {
                    playing_.store(false);
                    break;
                }
                int duration = static_cast<int>(note_bytes_[c.stream_index++]);
                c.wait_ticks = std::max(0, duration - 1);
                consumed = true;
            } else {
                HandleControlByte(ch, b);
                if (b == 0x90) consumed = true;
            }
        }
    }
}

void AmigaMusic::HandleControlByte(int ch_index, uint8_t cmd) {
    ChannelState& c = channel_[ch_index];
    auto can_read = [&](size_t i) { return i < note_bytes_.size(); };

    switch (cmd) {
        case 0x80: {
            if (can_read(c.stream_index)) c.instrument = static_cast<int>(note_bytes_[c.stream_index++]);
            break;
        }
        case 0x81: {
            if (!can_read(c.stream_index)) break;
            uint8_t slot = note_bytes_[c.stream_index++];
            if (slot < c.slot_ptr.size()) {
                c.slot_ptr[slot] = c.stream_index;
                c.slot_valid[slot] = true;
            }
            break;
        }
        case 0x82: {
            if (!can_read(c.stream_index)) break;
            uint8_t slot = note_bytes_[c.stream_index];
            if (slot < c.slot_ptr.size() && c.slot_valid[slot]) {
                c.stream_index = c.slot_ptr[slot];
            } else {
                c.stream_index++;
            }
            break;
        }
        case 0x83: {
            if (!can_read(c.stream_index)) break;
            uint8_t packed = note_bytes_[c.stream_index];
            uint8_t slot = static_cast<uint8_t>(packed & 0x0F);
            if (slot >= c.loop_counter.size()) {
                c.stream_index++;
                break;
            }
            int16_t& ctr = c.loop_counter[slot];
            if (ctr < 1) {
                if (ctr == 0) {
                    ctr = -1;
                    c.stream_index++;
                } else {
                    ctr = static_cast<int16_t>((packed >> 4) - 1);
                    if (c.slot_valid[slot]) c.stream_index = c.slot_ptr[slot];
                    else c.stream_index++;
                }
            } else {
                ctr = static_cast<int16_t>(ctr - 1);
                if (c.slot_valid[slot]) c.stream_index = c.slot_ptr[slot];
                else c.stream_index++;
            }
            break;
        }
        case 0x84: {
            if (can_read(c.stream_index)) speed_ticks_ = std::max(1, speed_ticks_ + static_cast<int>(note_bytes_[c.stream_index++]));
            break;
        }
        case 0x85: {
            if (can_read(c.stream_index)) speed_ticks_ = std::max(1, speed_ticks_ - static_cast<int>(note_bytes_[c.stream_index++]));
            break;
        }
        case 0x86: {
            if (can_read(c.stream_index)) global_transpose_ += static_cast<int>(note_bytes_[c.stream_index++]);
            break;
        }
        case 0x87: {
            if (can_read(c.stream_index)) global_transpose_ -= static_cast<int>(note_bytes_[c.stream_index++]);
            break;
        }
        case 0x88: {
            if (can_read(c.stream_index)) c.note_volume = std::clamp(c.note_volume + static_cast<int>(note_bytes_[c.stream_index++]), 0, 64);
            break;
        }
        case 0x89: {
            if (can_read(c.stream_index)) c.note_volume = std::clamp(c.note_volume - static_cast<int>(note_bytes_[c.stream_index++]), 0, 64);
            break;
        }
        case 0x90: {
            if (can_read(c.stream_index)) c.wait_ticks = std::max(0, static_cast<int>(note_bytes_[c.stream_index++]) - 1);
            break;
        }
        case 0x91: {
            if (!can_read(c.stream_index)) break;
            uint8_t slot = note_bytes_[c.stream_index];
            if (!conditional_flag_ && slot < c.slot_ptr.size() && c.slot_valid[slot]) c.stream_index = c.slot_ptr[slot];
            else c.stream_index++;
            break;
        }
        case 0x92: {
            if (can_read(c.stream_index)) {
                cia_timer_period_ += static_cast<int>(note_bytes_[c.stream_index++]) * 1000;
                cia_timer_period_ = std::clamp(cia_timer_period_, 1000, 65535);
            }
            break;
        }
        case 0x93: {
            if (can_read(c.stream_index)) {
                cia_timer_period_ -= static_cast<int>(note_bytes_[c.stream_index++]) * 1000;
                cia_timer_period_ = std::clamp(cia_timer_period_, 1000, 65535);
            }
            break;
        }
        case 0x94: {
            if (can_read(c.stream_index)) {
                cia_timer_period_ = static_cast<int>(note_bytes_[c.stream_index++]) * 1000;
                cia_timer_period_ = std::clamp(cia_timer_period_, 1000, 65535);
            }
            break;
        }
        case 0x95: {
            if (ch_index == 0) {
                AdvanceArrangement();
            } else if (arrangement_index_ < arrangement_len_) {
                c.stream_index = patterns_[static_cast<size_t>(arrangement_index_)][ch_index];
            }
            break;
        }
        case 0x96: {
            if (can_read(c.stream_index)) c.note_volume = std::clamp(static_cast<int>(note_bytes_[c.stream_index++]), 0, 64);
            break;
        }
        case 0x97: {
            if (can_read(c.stream_index)) speed_ticks_ = std::max(1, static_cast<int>(note_bytes_[c.stream_index++]));
            break;
        }
        case 0xA0: {
            playing_.store(false);
            break;
        }
        default: {
            if (can_read(c.stream_index)) c.stream_index++;
            break;
        }
    }
}

void AmigaMusic::StartNote(int ch_index, int note_byte) {
    StartSample(ch_index, NoteToPeriod(note_byte + global_transpose_), master_volume_);
    // MusicTick zeroes the volume after MusicPlayNote; the envelope's first
    // segment, in the same interrupt, brings it up.
    channel_[ch_index].env.volume = 0;
}

void AmigaMusic::StartSample(int ch_index, int period, int master) {
    ChannelState& c = channel_[ch_index];
    if (c.instrument < 0 || static_cast<size_t>(c.instrument) >= instruments_.size()) return;

    const InstrumentDef& inst = instruments_[static_cast<size_t>(c.instrument)];
    if (inst.bank_index > 1) return;

    const std::vector<int8_t>& bank = wave_bank_[inst.bank_index];
    size_t off = inst.sample_offset;
    size_t len = inst.sample_length;
    if (off >= bank.size()) return;
    if (off + len > bank.size()) len = bank.size() - off;

    c.loop = inst.unknown == 0;
    c.sample_ptr = bank.data() + off;
    c.sample_len = static_cast<int>(len);
    c.sample_pos = 0.0;
    c.sample_step = PeriodToStep(period);
    c.sample_active = (c.sample_len > 0);
    c.has_env = inst.env_index < envelopes_.size();
    if (c.has_env) {
        const int prevLast = inst.env_index > 0 ? envelopes_[inst.env_index - 1].points[15] : 0;
        c.env.Start(envelopes_[inst.env_index].points, prevLast, c.note_volume, master);
    } else {
        c.env.volume = (c.note_volume * master) / 15;
    }
}

void AmigaMusic::AdvanceArrangement() {
    if (arrangement_len_ <= 0) return;
    arrangement_index_ = (arrangement_index_ + 1) % arrangement_len_;
    const auto& start = patterns_[static_cast<size_t>(arrangement_index_)];
    for (int ch = 0; ch < 4; ch++) {
        channel_[ch].stream_index = start[ch];
    }
}

bool AmigaMusic::ResetState() {
    if (patterns_.empty()) return false;

    // The original's modulus is fixed; clamped for note files with fewer steps.
    arrangement_len_ = std::min(kArrangementSteps, static_cast<int>(patterns_.size()));

    // The original opens on a random step rather than always the first.
    std::uniform_int_distribution<int> pick(0, arrangement_len_ - 1);
    arrangement_index_ = pick(RandomEngine());

    global_transpose_ = 0x54;
    speed_ticks_ = 2;
    cia_timer_period_ = 35000;
    master_volume_ = kMusicMasterVolume;
    speed_counter_ = 0;
    sample_tick_accum_ = 0.0;
    conditional_flag_ = false;

    const auto& start = patterns_[static_cast<size_t>(arrangement_index_)];
    for (int ch = 0; ch < 4; ch++) {
        channel_[ch] = ChannelState {};
        channel_[ch].stream_index = start[ch];
        channel_[ch].loop_counter.fill(-1);
        channel_[ch].note_volume = 64;
    }
    return true;
}

int AmigaMusic::NoteToPeriod(int note) const {
    if (note < 0) note = 0;
    int semitone = note % 12;
    int octave = note / 12;
    int period = kPeriodTable[semitone];
    int shift = 10 - octave;
    for (int i = 0; i < shift; i++) {
        period <<= 1;
    }
    return period;
}

double AmigaMusic::PeriodToStep(int period) const {
    if (period <= 0) return 0.0;
    double playback_hz = kPaulaClock / static_cast<double>(period);
    return playback_hz / static_cast<double>(sample_rate_);
}

uint16_t AmigaMusic::ReadBE16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

bool AmigaMusic::ParsePatterns(const std::vector<uint8_t>& note_bytes, std::vector<std::array<size_t, 4>>& out_patterns) {
    out_patterns.clear();
    size_t p = 0;
    while (p < note_bytes.size()) {
        std::array<size_t, 4> starts {};
        starts[0] = p;
        bool complete = true;
        for (int ch = 1; ch < 5; ch++) {
            size_t ff_pos = p;
            while (ff_pos < note_bytes.size() && note_bytes[ff_pos] != 0xFF) ff_pos++;
            if (ff_pos >= note_bytes.size()) return false;
            p = ff_pos + 1;
            if (p < note_bytes.size() && note_bytes[p] == 0xEF) {
                p = ff_pos + 2;
                if (ch != 4) complete = false;
                break;
            }
            if (ch < 4) starts[ch] = p;
        }
        if (!complete) return false;
        out_patterns.push_back(starts);
    }
    return !out_patterns.empty();
}

bool AmigaMusic::ParseInstruments(const std::vector<uint8_t>& ins_bytes, bool sr2, std::vector<InstrumentDef>& out_instruments) {
    const size_t stride = sr2 ? 18 : 10;
    if (ins_bytes.size() % stride != 0) return false;
    out_instruments.clear();
    size_t n = ins_bytes.size() / stride;
    out_instruments.reserve(n);
    for (size_t i = 0; i < n; i++) {
        const uint8_t* p = ins_bytes.data() + i * stride;
        InstrumentDef d;
        d.env_index = ReadBE16(p + 0);
        d.unknown = ReadBE16(p + 2);
        d.bank_index = ReadBE16(p + 4);
        d.sample_length = ReadBE16(p + 6);
        d.sample_offset = ReadBE16(p + 8);
        out_instruments.push_back(d);
    }
    // SR2's shipped srl.ins gives instrument 4 bank 0 with a srl1.wave offset; instrument 5 is the same record on bank 1.
    if (sr2 && out_instruments.size() > 4) out_instruments[4].bank_index = 1;
    return true;
}

bool AmigaMusic::ParseEnvelopes(const std::vector<uint8_t>& env_bytes, std::vector<EnvelopeDef>& out_envelopes) {
    if (env_bytes.empty() || (env_bytes.size() % 32) != 0) return false;
    out_envelopes.clear();
    size_t n = env_bytes.size() / 32;
    out_envelopes.reserve(n);
    for (size_t i = 0; i < n; i++) {
        const uint8_t* p = env_bytes.data() + i * 32;
        EnvelopeDef env {};
        for (int j = 0; j < 16; j++) {
            env.points[j] = static_cast<int16_t>(ReadBE16(p + (j * 2)));
        }
        out_envelopes.push_back(env);
    }
    return true;
}
