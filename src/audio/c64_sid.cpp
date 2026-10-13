#include "audio/c64_sid.h"

#include "mos6502/mos6502.h"
#include "resid/sid.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace c64 {
namespace {

// The bus callbacks carry no userdata, so each call names the tune it runs. Per thread: the music and the
// effects run on different audio threads.
thread_local Tune* g_bus = nullptr;

constexpr uint16_t kStub = 0x0200;   // JSR target, then the return lands on kStub + 3
constexpr int kMaxInstructions = 2000000;

size_t SectorOffset(int track, int sector) {
    static const int kSectors[] = {21, 19, 18, 17};
    size_t off = 0;
    for (int t = 1; t < track; t++) {
        off += static_cast<size_t>(t <= 17 ? kSectors[0] : t <= 24 ? kSectors[1] : t <= 30 ? kSectors[2] : kSectors[3]);
    }
    return (off + static_cast<size_t>(sector)) * 256;
}

}  // namespace

std::vector<uint8_t> D64ExtractPrg(const uint8_t* d64, size_t size, const std::string& name) {
    auto sector = [&](int t, int s) -> const uint8_t* {
        if (t < 1 || t > 40 || s < 0 || s > 20) return nullptr;
        const size_t off = SectorOffset(t, s);
        return off + 256 <= size ? d64 + off : nullptr;
    };
    int t = 18, s = 1;
    for (int guard = 0; t != 0 && guard < 64; guard++) {
        const uint8_t* dir = sector(t, s);
        if (!dir) return {};
        for (int e = 0; e < 8; e++) {
            const uint8_t* ent = dir + e * 32;
            if ((ent[2] & 0x07) != 2) continue;   // PRG
            std::string n;
            for (int i = 0; i < 16 && ent[5 + i] != 0xA0; i++) n += static_cast<char>(std::tolower(ent[5 + i]));
            if (n != name) continue;
            std::vector<uint8_t> out;
            int ft = ent[3], fs = ent[4];
            for (int g = 0; ft != 0 && g < 800; g++) {
                const uint8_t* d = sector(ft, fs);
                if (!d) return {};
                const int last = d[0] == 0 ? d[1] : 255;
                out.insert(out.end(), d + 2, d + last + 1);
                ft = d[0];
                fs = d[1];
            }
            return out;
        }
        t = dir[0];
        s = dir[1];
    }
    return {};
}

Tune::Tune() : ram_(65536, 0), cpu_(new mos6502(&Tune::BusRead, &Tune::BusWrite)), sid_(new reSID::SID) {}

Tune::~Tune() = default;

uint8_t Tune::BusRead(uint16_t addr) { return g_bus->ram_[addr]; }

void Tune::BusWrite(uint16_t addr, uint8_t value) {
    g_bus->ram_[addr] = value;
    if (addr >= 0xD400 && addr <= 0xD418) {
        g_bus->sid_->write(addr - 0xD400, value);
        if (g_bus->recordWrites) g_bus->writes.push_back(static_cast<uint16_t>(((addr - 0xD400) << 8) | value));
    }
}

bool Tune::Load(const std::vector<uint8_t>& prg, int sampleRate) {
    if (prg.size() < 32) return false;
    base_ = static_cast<uint16_t>(prg[0] | (prg[1] << 8));
    if (base_ + prg.size() - 2 > 0xD000) return false;
    std::fill(ram_.begin(), ram_.end(), 0);
    std::copy(prg.begin() + 2, prg.end(), ram_.begin() + base_);
    if (ram_[base_] != 0x4C) return false;
    // PLAY starts STA/STX then LDA <active flag>; wseq.o has a NOP first.
    const uint16_t play = static_cast<uint16_t>(ram_[base_ + 1] | (ram_[base_ + 2] << 8));
    activeFlag_ = 0;
    for (uint16_t a = play; a < play + 10; a++) {
        if (ram_[a] == 0xAD) {
            activeFlag_ = static_cast<uint16_t>(ram_[a + 1] | (ram_[a + 2] << 8));
            break;
        }
    }
    if (activeFlag_ == 0) return false;
    size_ = prg.size() - 2;
    FindLoop();
    sid_->set_chip_model(reSID::MOS6581);
    sid_->set_sampling_parameters(kPalClock, reSID::SAMPLE_INTERPOLATE, sampleRate);
    sid_->reset();
    frameLeft_ = 0;
    return true;
}

void Tune::Call(uint16_t addr, uint8_t x) {
    Tune* prev = g_bus;
    g_bus = this;
    ram_[kStub] = 0x20;
    ram_[kStub + 1] = static_cast<uint8_t>(addr);
    ram_[kStub + 2] = static_cast<uint8_t>(addr >> 8);
    cpu_->SetPC(kStub);
    cpu_->SetS(0xFF);
    cpu_->SetX(x);
    uint64_t cycles = 0;
    for (int i = 0; i < kMaxInstructions && cpu_->GetPC() != kStub + 3; i++) {
        cpu_->Run(1, cycles, mos6502::INST_COUNT);
    }
    g_bus = prev;
}

// Runs the song without sound until zero page plus the driver image (all of its state) is one already seen.
void Tune::FindLoop() {
    constexpr int kMaxFrames = 50 * 60 * 15;
    std::unordered_map<size_t, int> seen;
    const std::vector<uint8_t> image(ram_.begin(), ram_.end());
    loopStart_ = loopEnd_ = 0;
    Start();
    for (int f = 0; f < kMaxFrames && Active(); f++) {
        std::string state(ram_.begin(), ram_.begin() + 256);
        state.append(ram_.begin() + base_, ram_.begin() + base_ + static_cast<std::ptrdiff_t>(size_));
        auto [it, fresh] = seen.emplace(std::hash<std::string>{}(state), f);
        if (!fresh) {
            loopStart_ = it->second;
            loopEnd_ = f;
            break;
        }
        Frame();
    }
    ram_ = image;
}

void Tune::Start(int song) {
    sid_->reset();
    Call(static_cast<uint16_t>(base_ + 3), static_cast<uint8_t>(song));
    frameLeft_ = 0;
    frame_ = 0;
}

void Tune::StartEffect(int song) {
    Call(static_cast<uint16_t>(base_ + 0x13), static_cast<uint8_t>(song));
    frameLeft_ = 0;
}

void Tune::WriteSid(int reg, uint8_t value) {
    ram_[static_cast<size_t>(0xD400 + reg)] = value;
    sid_->write(static_cast<reSID::reg8>(reg), value);
}

void Tune::Stop() { Call(static_cast<uint16_t>(base_ + 6), 0); }

void Tune::Frame() { Call(base_, 0); }

bool Tune::Active() const { return ram_[activeFlag_] != 0; }

bool Tune::Render(int16_t* out, int frames) {
    bool ended = false;
    while (frames > 0) {
        if (frameLeft_ == 0) {
            const bool was = Active();
            Frame();
            if (was && !Active()) ended = true;
            if (++frame_ == loopEnd_ && loopEnd_ > 0) {
                ended = true;
                frame_ = loopStart_;
            }
            frameLeft_ = kFrameCycles;
        }
        reSID::cycle_count dt = frameLeft_;
        const int got = sid_->clock(dt, out, frames);
        frameLeft_ = dt;
        out += got;
        frames -= got;
    }
    return ended;
}

}  // namespace c64
