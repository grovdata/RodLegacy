#include "audio/c64_engine.h"
#include "audio/c64_music.h"
#include "audio/record_tap.h"

#include "core/log.h"

namespace {

constexpr int kBayDrone = 11;
constexpr int kDriveIn = 12;
constexpr int kDriveOut = 16;

}  // namespace

C64Engine::~C64Engine() { Shutdown(); }

bool C64Engine::Init(const std::vector<uint8_t>& seqgar1) {
    if (device_ != 0) return true;
    if (!tune_.Load(seqgar1, C64Music::kSampleRate)) return false;
    SDL_AudioSpec want{};
    want.freq = C64Music::kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &C64Engine::AudioCallback;
    want.userdata = this;
    SDL_AudioSpec got{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device_ == 0) {
        LOG_ERROR("C64Engine: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void C64Engine::Shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

void C64Engine::SetVolume(float v) {
    const float clamped = v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
    volume_.store(static_cast<int>(clamped * C64Music::kNormalizeGain * 256.0F), std::memory_order_relaxed);
}

void C64Engine::StartRace() {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    LoadRace(tune_);
    SDL_UnlockAudioDevice(device_);
    SDL_PauseAudioDevice(device_, 0);
}

void C64Engine::SetRpm(int rpm) {
    if (device_ == 0) return;
    const uint16_t f = FreqForRpm(rpm);
    SDL_LockAudioDevice(device_);
    tune_.WriteSid(0, static_cast<uint8_t>(f));
    tune_.WriteSid(1, static_cast<uint8_t>(f >> 8));
    SDL_UnlockAudioDevice(device_);
}

void C64Engine::PlayScene(bool arriving) {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    tune_.Start(arriving ? kDriveIn : kDriveOut);
    SDL_UnlockAudioDevice(device_);
    SDL_PauseAudioDevice(device_, 0);
}

void C64Engine::StartBay(int tuneOffset) {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    tune_.Poke(static_cast<uint16_t>(tune_.Base() + 0x12), BayPitch(tuneOffset));
    tune_.Start(kBayDrone);
    SDL_UnlockAudioDevice(device_);
    SDL_PauseAudioDevice(device_, 0);
}

void C64Engine::SetBayTune(int tuneOffset) {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    tune_.Poke(static_cast<uint16_t>(tune_.Base() + 0x12), BayPitch(tuneOffset));
    SDL_UnlockAudioDevice(device_);
}

void C64Engine::Stop() {
    if (device_ == 0) return;
    SDL_PauseAudioDevice(device_, 1);
    SDL_LockAudioDevice(device_);
    tune_.Stop();
    tune_.WriteSid(4, 0x40);
    SDL_UnlockAudioDevice(device_);
}

void SDLCALL C64Engine::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<C64Engine*>(userdata);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    self->mono_.resize(static_cast<size_t>(frames));
    self->tune_.Render(self->mono_.data(), frames);
    auto* out = reinterpret_cast<int16_t*>(stream);
    const int vol = self->volume_.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; i++) {
        int v = self->mono_[static_cast<size_t>(i)] * vol / 256;
        v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        out[i * 2] = out[i * 2 + 1] = static_cast<int16_t>(v);
    }
    record_tap::Write("c64_engine", C64Music::kSampleRate, 2, stream, len);
}
