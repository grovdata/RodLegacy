#include "audio/pc_speaker_fx.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace pcspk {

namespace {

// Builds a speaker timeline the way the effect library drives the hardware: each primitive writes the PIT or
// port 0x61 and then busy-waits.
class Timeline {
public:
    explicit Timeline(const std::function<int(int)>& r) : rng(r) {}

    void SpeakerOn() { SetHz(25000); port61 |= 3; }
    void SpeakerOff() { port61 &= static_cast<uint8_t>(~3); }

    void Tone(int hz, int units) {
        SetHz(hz);
        Wait(kToneOverheadUs + units * kDelayUnitUs);
    }
    void Burst(int lo, int hi, int units, int count) {
        for (; count > 0; --count) Tone(lo + rng(hi - lo), units);
    }
    void SweepDown(int f, int to, int step, int units) {
        for (; f >= to; f -= step) Tone(f, units);
    }
    void Warble(int center, int dev, int step, int units, int reps) {
        for (; reps > 0; --reps) {
            for (int i = 0, f = center - dev; i < 2 * dev; i += step, f += step) Tone(f, units);
            for (int i = 0, f = center + dev; i < 2 * dev; i += step, f -= step) Tone(f, units);
        }
    }
    // Gate off, so the cone follows bit 1 alone: clicks at random spacing.
    void Noise(int range, int orMask, int count) {
        SpeakerOff();
        for (; count > 0; --count) {
            port61 ^= 2;
            Wait(kNoiseOverheadUs + (rng(range) | orMask) * kDelayUnitUs);
        }
    }
    void NoiseDown(int a, int b, int sub, int step, int count) {
        if (b >= a) return;
        int x = a;
        do { Noise(a, a - sub, count); a -= step; x -= step; } while (b < x);
    }
    void NoiseUp(int a, int b, int sub, int step, int count) {
        if (a >= b) return;
        int x = a;
        do { Noise(a, a - sub, count); a += step; x += step; } while (x < b);
    }

    std::vector<Segment> Finish() {
        SpeakerOff();
        segs.push_back({0.0, divisor, port61});
        return std::move(segs);
    }

private:
    void SetHz(int hz) { divisor = static_cast<uint16_t>(0x1234DC / std::max(hz, 19)); }
    void Wait(double us) { segs.push_back({us, divisor, port61}); }

    const std::function<int(int)>& rng;
    std::vector<Segment> segs;
    uint16_t divisor = 0;
    uint8_t port61 = 0;
};

}

std::vector<Segment> BuildEffect(Effect fx, const std::function<int(int)>& rng) {
    Timeline t(rng);
    switch (fx) {
        case Effect::Click:     t.SpeakerOn(); t.Warble(3000, 10, 10, 60, 9); break;
        case Effect::ChirpHigh: t.SpeakerOn(); t.Warble(rng(100) + 6200, 40, 10, 1, 10); break;
        case Effect::ChirpLow:  t.SpeakerOn(); t.Warble(5200, 40, 10, 1, 10); break;
        case Effect::Clank:     t.SpeakerOn(); t.Noise(200, 10, 60); break;
        case Effect::Thud:      t.SpeakerOn(); t.SweepDown(130, 90, 4, 1000); break;
        case Effect::Hit:
            t.SpeakerOn(); t.Burst(2000, 8000, 100, 10); t.Noise(120, 2, 30);
            t.SpeakerOn(); t.Burst(4000, 9000, 100, 10); t.Noise(220, 2, 20);
            break;
        case Effect::Crash:
            for (int i = 0; i < 3; ++i) {
                t.SpeakerOn();
                t.Burst(3000, 8000, 60, 10);
                t.NoiseDown(200, rng(30) + 18, 15, 5, 2);
                t.Noise(rng(300) + 40, 2, 30);
                t.NoiseUp(16, rng(100) + 50, 15, 5, 4);
            }
            break;
    }
    return t.Finish();
}

std::vector<LoopSong> ReadLoops(const uint8_t* exe, size_t size) {
    std::vector<LoopSong> loops(static_cast<size_t>(Loop::Count));
    if (size < 0x40) return loops;
    auto word = [&](size_t at) { return static_cast<uint16_t>(exe[at] | exe[at + 1] << 8); };
    const size_t image = static_cast<size_t>(word(8)) * 16;
    constexpr size_t kSongs = 0x2f930;      // 2f93:0000
    constexpr size_t kWaves = 0x3e960 + 0x57f6;   // DS:57F6, three rows of eight
    // Offset in the song block, and the vib_set row and depth each loop routine passes (-1: none).
    static constexpr struct { uint16_t off; int row; int depth; } kLoops[] = {
        {0x0000, 1, 0x1e}, {0x002c, 1, 0x14}, {0x007c, 1, 0x0e}, {0x00a0, 0, 0x0a}, {0x00c0, -1, 0}};
    if (image + kWaves + 48 > size) return loops;
    for (size_t i = 0; i < std::size(kLoops); ++i) {
        LoopSong& song = loops[i];
        // Past the four-byte header, up to the section marker.
        for (size_t at = image + kSongs + kLoops[i].off + 4; at + 4 <= size; at += 4) {
            const uint16_t dur = word(at);
            if (dur == 0) break;
            song.events.emplace_back(dur, word(at + 2));
        }
        if (kLoops[i].row >= 0) {
            song.vibrato = true;
            song.depth = kLoops[i].depth;
            for (int k = 0; k < 8; ++k) {
                song.wave[k] = static_cast<int16_t>(word(image + kWaves + static_cast<size_t>(kLoops[i].row) * 16 + k * 2));
            }
        }
    }
    return loops;
}

std::vector<LoopSong> ReadLoopsSr2(const uint8_t* exe, size_t size) {
    std::vector<LoopSong> loops(static_cast<size_t>(Loop::Count));
    if (size < 0x40) return loops;
    auto word = [&](size_t at) { return static_cast<uint16_t>(exe[at] | exe[at + 1] << 8); };
    const size_t ds = static_cast<size_t>(word(8)) * 16 + 0x3e080;   // DGROUP 3E08
    constexpr size_t kWaves = 0x536e, kRecords = 0x539e;
    // Song and per-section vibrato table each speaker loop routine (2c57:0608-073e) passes to song_select.
    static constexpr struct { Loop loop; uint16_t song; uint16_t vib; } kLoops[] = {
        {Loop::Tick, 0x5490, 0x5480},     {Loop::Crank, 0x54bc, 0x5484}, {Loop::Wrench, 0x54dc, 0x5488},
        {Loop::Spray, 0x5534, 0x548c},    {Loop::IdlePutt, 0x5544, 0x5474}, {Loop::Pulse, 0x5500, 0x548c}};
    if (ds + 0x5600 > size) return loops;
    // The putt's first note, to tell this exe from SR1's.
    if (word(ds + 0x5544 + 4) != 1 || word(ds + 0x5544 + 6) != 36488) return loops;
    for (const auto& l : kLoops) {
        LoopSong& song = loops[static_cast<size_t>(l.loop)];
        for (size_t at = ds + l.song + 4; at + 4 <= size; at += 4) {
            const uint16_t dur = word(at);
            if (dur == 0) break;
            song.events.emplace_back(dur, word(at + 2));
        }
        // Record {phase, row, depth, period, step}; every loop's step is 1, one wave step a tick.
        const size_t rec = ds + kRecords + 10 * word(ds + l.vib);
        song.sr2 = true;
        song.depth = word(rec + 4);
        song.period = std::max<int>(1, word(rec + 6));
        song.vibrato = song.depth != 0;
        for (int k = 0; k < 8; ++k) {
            song.wave[k] = static_cast<int16_t>(word(ds + kWaves + word(rec + 2) * 16 + k * 2));
        }
    }
    return loops;
}

int VibratoAmount(const LoopSong& song, uint16_t note) {
    if (song.sr2) return static_cast<uint16_t>((note / 500) * song.depth);
    return static_cast<uint16_t>(song.depth * note) / 1000;
}

double DurationUs(const std::vector<Segment>& segs) {
    double us = 0.0;
    for (const Segment& s : segs) us += s.us;
    return us;
}

uint16_t MufflerPulse(uint16_t divisor, int& count, int pulses) {
    if (count++ < pulses) return static_cast<uint16_t>(0xF000 | (divisor & 0xFF));
    count = 0;
    return divisor;
}

uint16_t EngineDivisor(int rpm, bool sr2) {
    const int32_t square = static_cast<int32_t>(rpm) * rpm;
    uint16_t t = static_cast<uint16_t>(square >> 10);
    if (sr2) {
        const uint16_t limiter = rpm == 5700 ? 6000 : 0;
        return static_cast<uint16_t>(60000 - static_cast<uint16_t>(t + (t >> 3) + (t >> 2)) - limiter);
    }
    // SHR by a count the 286+ masks to five bits: a little pitch jitter.
    const unsigned shift = (static_cast<uint8_t>(t) + 2u) & 31u;
    const uint16_t x = static_cast<uint16_t>(shift >= 16 ? 0 : (t >> shift)) >> 3;
    t = static_cast<uint16_t>(t + x);
    const uint16_t limiter = rpm == 5700 ? 10000 : 0;
    return static_cast<uint16_t>(50000 - static_cast<uint16_t>(t + limiter));
}

uint16_t TuneDivisor(int setting) {
    return static_cast<uint16_t>(30000 + (std::abs(setting) << 10));
}

int AnimationTone(int d) {
    d = std::abs(d);
    return std::min((d >> 1) * d, 20000);
}

}
