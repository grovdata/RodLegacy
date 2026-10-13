#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The C64 version's SID player: the original 6502 driver run on mos6502 against reSID.
// See RodLegacyResearch/docs/c64/music.md.

namespace reSID { class SID; }
class mos6502;

namespace c64 {

constexpr double kPalClock = 985248.0;
constexpr int kFrameCycles = 19705;   // PAL clock / 50
constexpr int kMusicSong = 10;

// A PRG (2-byte load address + body) out of a 35/40-track .d64, by its PETSCII name. Empty if not found.
std::vector<uint8_t> D64ExtractPrg(const uint8_t* d64, size_t size, const std::string& name);

class Tune {
public:
    Tune();
    ~Tune();

    // A seq* / wseq.o PRG: driver with PLAY at +0, INIT at +3, START at +$13.
    bool Load(const std::vector<uint8_t>& prg, int sampleRate);

    // INIT X: SID reset and song X from the top.
    void Start(int song = kMusicSong);
    // START X: song X without the SID reset (how the game plays its effects).
    void StartEffect(int song);
    // STOP: the song ends and voices 1 and 2 are gated off, so they release.
    void Stop();
    // One 50 Hz PLAY call.
    void Frame();
    bool Active() const;

    // The driver's load address, and a byte of its RAM (the game pokes $BF12 to bend an effect's pitch).
    uint16_t Base() const { return base_; }
    void Poke(uint16_t addr, uint8_t value) { ram_[addr] = value; }
    // A SID register write from outside the driver, the way the race code drives the engine voice.
    void WriteSid(int reg, uint8_t value);

    // Mono samples, PLAY called every kFrameCycles SID cycles. Returns true if the song ended or wrapped
    // around to its loop point in there.
    bool Render(int16_t* out, int frames);

    // Frames from Start() to the first time the driver state repeats, and the frame it repeats; 0 if not found.
    int LoopEnd() const { return loopEnd_; }
    int LoopStart() const { return loopStart_; }

    // Every SID register write since the last call, as (reg << 8) | value. Only kept when recordWrites is set.
    bool recordWrites = false;
    std::vector<uint16_t> writes;

private:
    void Call(uint16_t addr, uint8_t x);
    void FindLoop();
    static uint8_t BusRead(uint16_t addr);
    static void BusWrite(uint16_t addr, uint8_t value);

    std::vector<uint8_t> ram_;
    std::unique_ptr<mos6502> cpu_;
    std::unique_ptr<reSID::SID> sid_;
    uint16_t base_ = 0;
    uint16_t activeFlag_ = 0;
    size_t size_ = 0;
    int loopStart_ = 0;
    int loopEnd_ = 0;
    int frame_ = 0;         // PLAY calls since Start()
    int frameLeft_ = 0;     // SID cycles until the next PLAY
};

}  // namespace c64
