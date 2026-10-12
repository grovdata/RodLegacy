#include "audio/adlib_sfx.h"
#include "audio/adlib_music.h"
#include "audio/record_tap.h"

#include "core/log.h"

#include <algorithm>
#include <cstdlib>
#include <new>

extern "C" {
#include "opl3/opl3.h"
}

AdlibSfx::~AdlibSfx() { Shutdown(); }

bool AdlibSfx::Init(std::vector<sr2mus::Patch> bank) {
    if (device != 0) return true;
    auto* c = new (std::nothrow) opl3_chip{};
    if (c == nullptr) return false;
    OPL3_Reset(c, sr2mus::kOplRate);
    SDL_AudioSpec want{};
    want.freq = sr2mus::kOplRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &AdlibSfx::Mix;
    want.userdata = this;
    SDL_AudioSpec got{};
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device == 0) {
        LOG_ERROR("AdlibSfx: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        delete c;
        return false;
    }
    chip = c;
    patches = std::move(bank);
    SDL_PauseAudioDevice(device, 0);
    return true;
}

void AdlibSfx::Shutdown() {
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
    delete static_cast<opl3_chip*>(chip);
    chip = nullptr;
}

void AdlibSfx::SetVolume(float v) {
    volume.store(AdlibMusic::VolumeScale(v));
}

void AdlibSfx::Queue(std::vector<sr2mus::RegWrite> writes) {
    if (device == 0) return;
    SDL_LockAudioDevice(device);
    // A little ahead, so the first write is not already late.
    const uint32_t now = sample.load() + 256;
    for (sr2mus::RegWrite& r : writes) r.sample += now;
    pending.insert(pending.end(), writes.begin(), writes.end());
    std::stable_sort(pending.begin(), pending.end(), [](const sr2mus::RegWrite& a, const sr2mus::RegWrite& b) {
        return a.sample < b.sample;
    });
    SDL_UnlockAudioDevice(device);
}

void AdlibSfx::Play(adlibfx::Sound sound, double seconds) {
    if (patches.empty()) return;
    Queue(adlibfx::Build(sound, patches, seconds > 0 ? seconds : 3600.0, std::rand() % 8));
}

void AdlibSfx::Stop(adlibfx::Sound sound) {
    if (device == 0) return;
    const int ch = adlibfx::Voice(sound);
    SDL_LockAudioDevice(device);
    // Drop what that voice still had coming, then let it go.
    pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const sr2mus::RegWrite& r) {
                      return (r.reg & 0x0F) == ch && ((r.reg & 0xF0) == 0xA0 || (r.reg & 0xF0) == 0xB0);
                  }), pending.end());
    pending.insert(pending.begin(), sr2mus::RegWrite{sample.load(), static_cast<uint16_t>(0xB0 + ch), 0});
    SDL_UnlockAudioDevice(device);
}

void SDLCALL AdlibSfx::Mix(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<AdlibSfx*>(userdata);
    auto* c = static_cast<opl3_chip*>(self->chip);
    auto* out = reinterpret_cast<int16_t*>(stream);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    const int vol = self->volume.load(std::memory_order_relaxed);
    uint32_t now = self->sample.load(std::memory_order_relaxed);
    size_t next = 0;
    for (int i = 0; i < frames; ++i, ++now) {
        while (next < self->pending.size() && self->pending[next].sample <= now) {
            OPL3_WriteReg(c, self->pending[next].reg, self->pending[next].value);
            ++next;
        }
        int16_t s[2] = {0, 0};
        OPL3_Generate(c, s);
        out[i * 2] = AdlibMusic::Scale(s[0], vol);
        out[i * 2 + 1] = AdlibMusic::Scale(s[1], vol);
    }
    self->pending.erase(self->pending.begin(), self->pending.begin() + static_cast<std::ptrdiff_t>(next));
    self->sample.store(now, std::memory_order_relaxed);
    record_tap::Write("adlib_sfx", sr2mus::kOplRate, 2, stream, len);
}
