#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Street Rod 2 (DOS) AdLib music: the MSR.SND instrument bank and a .MUS event stream, turned into a stream of
// OPL register writes at absolute sample positions.
namespace sr2mus {

constexpr int kOplRate = 49716;          // the OPL2's own rate, so no resampling
constexpr int kTicksPerSecond = 480;

struct RegWrite {
    uint32_t sample;
    uint16_t reg;
    uint8_t value;
};

// A patch is 28 little-endian int16 fields: 13 per operator, then two waveforms.
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

struct Event {
    uint32_t tick;
    uint8_t status;              // 0x80..0xEF
    uint8_t a = 0, b = 0;
};

bool LoadPatches(const uint8_t* data, size_t size, std::vector<Patch>& out);

bool ParseMus(const uint8_t* data, size_t size, std::vector<Event>& out, uint32_t& lengthTicks);

// One effect voice, the way the DOS game's effect table drives them.
void EffectVoice(std::vector<RegWrite>& out, const Patch& patch, int ch,
                 int volume, int note, uint32_t at, uint32_t hold);

// Re-pitch a sounding voice without re-keying it - what the siren's bend does.
void EffectPitch(std::vector<RegWrite>& out, int ch, int note, uint32_t at);

// Register writes in sample order, the first at sample 0.
std::vector<RegWrite> Build(const std::vector<Event>& events, const std::vector<Patch>& patches,
                            int tickRate = kTicksPerSecond);

}  // namespace sr2mus

// The effect slots of SR2.EXE's AdLib table (2c57:0c88-12e0), read in street-rod-2-sdl3's port/spec/sound.md.
namespace adlibfx {

enum class Sound {
    Crash,     // slot 15: hitcar + crash, keyed off at once
    Hit,       // slot 16: hitcar + turn, keyed off at once
    Clank,     // slot 9: hitcar, left to decay
    ChirpLow,  // slot 14: turn
    Thud,      // slot 17: turn1
    Spray,     // slot 2: part, held
    Slide,     // slot 10: paint, held
    Tick,      // slot 4: paint, held
    Siren,     // slot 11: police, bent up five semitones and back every 20 ticks
    Squeal,    // slot 1: part, keyed 8 ticks on, 8 off
    Putt,      // slot 0: part, held
    AirWrench, // slot 6: part, held
    LugNut,    // slot 7: turn1, held
    Click,     // slot 12: piano1, keyed off at once
    Blown,     // slot 8: hitcar, left to decay
};

constexpr double kTickHz = 1193182.0 / 0x4000;

// Writes for one sound lasting `seconds` (the held ones), from sample 0. rand8 is the 0..7 the hit adds to its note.
std::vector<sr2mus::RegWrite> Build(Sound sound, const std::vector<sr2mus::Patch>& bank, double seconds, int rand8);

// The voice a sound keys, so a held one can be stopped.
int Voice(Sound sound);

}
