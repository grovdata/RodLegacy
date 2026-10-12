#include "audio/adlib_engine.h"
#include "audio/record_tap.h"
#include "audio/adlib_music.h"

#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <new>

extern "C" {
#include "opl3/opl3.h"
}

namespace {

constexpr int kSampleRate = AdlibEngine::kOplRate;   // no resampling needed

// Voice 3's patch, read straight off captures/sr_001.dro at t=0. Modulator on
// slot 0x08, carrier on slot 0x0b.
struct RegValue { uint8_t reg, value; };
constexpr RegValue kPatch[] = {
    {0x01, 0x20},   // waveform select enable - the patch uses waveforms 1 and 3
    {0x28, 0x03},   // mod: multiple 3
    {0x48, 0x0b},   // mod: KSL/total level
    {0x68, 0xf0},   // mod: attack 15, decay 0
    {0x88, 0xf0},   // mod: sustain 15, release 0
    {0xe8, 0x01},   // mod: half sine
    {0x2b, 0x31},   // car: multiple 1, sustaining, vibrato
    {0x4b, 0x1f},   // car: KSL/total level - the level the engine modulates
    {0x6b, 0x60},   // car: attack 6, decay 0
    {0x8b, 0x06},   // car: sustain 0, release 6
    {0xeb, 0x03},   // car: pulse sine
    {0xc3, 0x06},   // voice: feedback 3, FM
};

}  // namespace

AdlibEngine::~AdlibEngine() { Shutdown(); }

bool AdlibEngine::Initialize() {
    if (device != 0) {
        return true;
    }
    auto* c = new (std::nothrow) opl3_chip{};
    if (c == nullptr) {
        return false;
    }
    chip = c;
    OPL3_Reset(c, kSampleRate);

    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &AdlibEngine::Mix;
    want.userdata = this;
    SDL_AudioSpec got{};
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device == 0) {
        LOG_ERROR("AdlibEngine: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        delete c;
        chip = nullptr;
        return false;
    }
    WritePatch();
    return true;
}

void AdlibEngine::Shutdown() {
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
    delete static_cast<opl3_chip*>(chip);
    chip = nullptr;
    running = false;
}

void AdlibEngine::WritePatch() {
    for (const RegValue& rv : kPatch) {
        Queue(rv.reg, rv.value);
    }
}

// False when the ring is full and the write was dropped.
bool AdlibEngine::Queue(uint8_t reg, uint8_t value) {
    const int h = head.load(std::memory_order_relaxed);
    const int next = (h + 1) % kQueueSize;
    if (next == tail.load(std::memory_order_acquire)) {
        return false;
    }
    queue[h] = {reg, value};
    head.store(next, std::memory_order_release);
    return true;
}

void AdlibEngine::Start() {
    if (device == 0) {
        return;
    }
    running = true;
    // Forget what was last written so the first SetRpm sends all three again -
    // Stop() keys the voice off, so nothing may be assumed still set.
    lastA = lastB = lastLevel = 0xff;
    SetRpm(lastRpm);
    SDL_PauseAudioDevice(device, 0);
}

void AdlibEngine::Stop() {
    if (device == 0) {
        return;
    }
    Queue(0xb0 + kVoice, 0);   // key off, the original's OplNoteOff
    running = false;
    SDL_PauseAudioDevice(device, 1);
}

void AdlibEngine::Apply(double hz, int volume) {
    const Fnum f = FnumForHz(hz);
    // A3 the low byte, B3 the key bit with the block and the top two bits -
    // exactly what SetVoicePitch writes, and the key stays down throughout.
    const uint8_t a = static_cast<uint8_t>(f.fnum & 0xff);
    const uint8_t b =
        static_cast<uint8_t>(0x20 | ((f.block & 7) << 2) | ((f.fnum >> 8) & 3));
    const uint8_t level = LevelForVolume(volume);
    // Only what actually changed.
    if (a != lastA && Queue(0xa0 + kVoice, a)) {
        lastA = a;
    }
    if (b != lastB && Queue(0xb0 + kVoice, b)) {
        lastB = b;
    }
    if (level != lastLevel && Queue(0x40 + kCarSlot, level)) {
        lastLevel = level;
    }
}

void AdlibEngine::SetRpm(int rpm) {
    lastRpm = rpm;      // before the guard, so a pre-Initialize rpm is not lost
    sceneHz = 0.0;
    if (device == 0) {
        return;
    }
    Apply(HzForRpm(rpm), VolumeForRpm(rpm, mufflerRemoved));
}

void AdlibEngine::SetSceneTone(int tone) {
    sceneHz = HzForSceneTone(tone);
    if (device == 0) {
        return;
    }
    // SR2's driving handler holds the car animations at one of two levels, no ramp.
    Apply(sceneHz, mufflerRemoved ? kVolumeMax : kVolumeMuffledIdle);
}

void AdlibEngine::SetMufflerRemoved(bool removed) {
    if (removed == mufflerRemoved) {
        return;
    }
    mufflerRemoved = removed;
    if (device != 0 && running && sceneHz == 0.0) {
        SetRpm(lastRpm);   // take effect now rather than at the next rev change
    } else if (device != 0 && running) {
        Apply(sceneHz, removed ? kVolumeMax : kVolumeMuffledIdle);
    }
}

void AdlibEngine::SetVolume(float v) {
    volume.store(VolumeScale(v), std::memory_order_relaxed);
}

void SDLCALL AdlibEngine::Mix(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<AdlibEngine*>(userdata);
    auto* c = static_cast<opl3_chip*>(self->chip);
    auto* out = reinterpret_cast<int16_t*>(stream);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    if (c == nullptr) {
        SDL_memset(stream, 0, static_cast<size_t>(len));
        return;
    }
    // Drain whatever the game thread queued, then fill the buffer.
    int t = self->tail.load(std::memory_order_relaxed);
    while (t != self->head.load(std::memory_order_acquire)) {
        const Pending p = self->queue[t];
        OPL3_WriteRegBuffered(c, p.reg, p.value);
        t = (t + 1) % kQueueSize;
    }
    self->tail.store(t, std::memory_order_release);

    const int vol = self->volume.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i) {
        int16_t s[2] = {0, 0};
        OPL3_Generate(c, s);
        out[i * 2] = AdlibMusic::Scale(s[0], vol);
        out[i * 2 + 1] = AdlibMusic::Scale(s[1], vol);
    }
    record_tap::Write("adlib_engine", kSampleRate, 2, stream, len);
}
