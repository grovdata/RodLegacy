#include "audio/c64_music.h"
#include "audio/record_tap.h"

#include "core/log.h"

C64Music::~C64Music() { Shutdown(); }

bool C64Music::Init() {
    if (device_ != 0) {
        return true;
    }
    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &C64Music::AudioCallback;
    want.userdata = this;
    SDL_AudioSpec got{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device_ == 0) {
        LOG_ERROR("C64Music: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void C64Music::Shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    playing_ = false;
    paused_ = false;
    songs_.clear();
    playlist_ = 0;
    victory_ = -1;
    current_ = 0;
}

bool C64Music::AddSong(const std::string& name, const std::vector<uint8_t>& prg) {
    // The win piece sits at the end of songs_, so a playlist song added after it would land on the wrong side.
    if (victory_ >= 0 || !BuildInto(name, prg)) {
        return false;
    }
    playlist_++;
    return true;
}

bool C64Music::SetVictorySong(const std::string& name, const std::vector<uint8_t>& prg) {
    if (victory_ >= 0 || !BuildInto(name, prg)) {
        return false;
    }
    victory_ = static_cast<int>(songs_.size()) - 1;
    return true;
}

bool C64Music::BuildInto(const std::string& name, const std::vector<uint8_t>& prg) {
    auto tune = std::make_unique<c64::Tune>();
    if (!tune->Load(prg, kSampleRate)) {
        return false;
    }
    songs_.push_back({name, std::move(tune)});
    return true;
}

// The win piece plays to its loop point. UpdateMusic's loop check then moves back into the playlist.
void C64Music::PlayVictorySong() {
    if (device_ == 0 || victory_ < 0) {
        return;
    }
    SDL_LockAudioDevice(device_);
    current_ = victory_;
    Restart();
    SDL_UnlockAudioDevice(device_);
    playing_ = true;
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

std::string C64Music::CurrentSongName() const {
    if (current_ < 0 || current_ >= static_cast<int>(songs_.size())) {
        return "C64";
    }
    return songs_[static_cast<size_t>(current_)].name;
}

void C64Music::SetSong(int index) {
    if (playlist_ <= 0) {
        return;
    }
    const int n = playlist_;
    index = ((index % n) + n) % n;
    if (device_ != 0) SDL_LockAudioDevice(device_);
    current_ = index;
    Restart();
    if (device_ != 0) SDL_UnlockAudioDevice(device_);
}

void C64Music::Restart() {
    songs_[static_cast<size_t>(current_)].tune->Start();
    fade_ = -1;
}

void C64Music::Play() {
    if (device_ == 0 || songs_.empty()) {
        return;
    }
    if (current_ >= playlist_ && playlist_ > 0) {
        current_ = 0;      // leave the win piece; Play only starts the playlist
    }
    SDL_LockAudioDevice(device_);
    Restart();
    SDL_UnlockAudioDevice(device_);
    playing_ = true;
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

void C64Music::Stop() {
    if (device_ == 0) {
        return;
    }
    SDL_PauseAudioDevice(device_, 1);
    playing_ = false;
    paused_ = false;
}

void C64Music::Pause() {
    if (device_ == 0 || !playing_ || paused_) {
        return;
    }
    paused_ = true;
    SDL_PauseAudioDevice(device_, 1);
}

void C64Music::Resume() {
    if (device_ == 0 || !playing_ || !paused_) {
        return;
    }
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

void C64Music::SetVolume(float v) {
    const float clamped = v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
    volume_.store(static_cast<int>(clamped * kNormalizeGain * 256.0F), std::memory_order_relaxed);
}

void C64Music::Fill(int16_t* out, int frames) {
    mono_.resize(static_cast<size_t>(frames));
    const bool wrapped = songs_[static_cast<size_t>(current_)].tune->Render(mono_.data(), frames);
    // Only the rotation leaves a song; one with nowhere to go loops seamlessly.
    if (wrapped && fade_ < 0 && (playlist_ >= 2 || current_ >= playlist_)) fade_ = 0;
    const int vol = volume_.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; i++) {
        int gain = 256;
        if (fade_ >= 0) {
            gain = fade_ < kFadeFrames ? 256 * (kFadeFrames - fade_) / kFadeFrames : 0;
            if (fade_ < kFadeFrames + kGapFrames && ++fade_ == kFadeFrames + kGapFrames)
                looped_.store(true, std::memory_order_relaxed);
        }
        int v = mono_[static_cast<size_t>(i)] * gain / 256 * vol / 256;
        v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        out[i * 2] = out[i * 2 + 1] = static_cast<int16_t>(v);
    }
}

void SDLCALL C64Music::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<C64Music*>(userdata);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    if (self->songs_.empty() || self->current_ >= static_cast<int>(self->songs_.size())) {
        SDL_memset(stream, 0, static_cast<size_t>(len));
    } else {
        self->Fill(reinterpret_cast<int16_t*>(stream), frames);
    }
    record_tap::Write("c64_music", kSampleRate, 2, stream, len);
}

C64Sfx::~C64Sfx() { Shutdown(); }

bool C64Sfx::Init(const std::vector<uint8_t>& prg) {
    if (device_ != 0) return true;
    if (!tune_.Load(prg, C64Music::kSampleRate)) return false;
    SDL_AudioSpec want{};
    want.freq = C64Music::kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &C64Sfx::AudioCallback;
    want.userdata = this;
    SDL_AudioSpec got{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device_ == 0) {
        LOG_ERROR("C64Sfx: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void C64Sfx::Shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

void C64Sfx::SetVolume(float v) {
    const float clamped = v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
    volume_.store(static_cast<int>(clamped * C64Music::kNormalizeGain * 256.0F), std::memory_order_relaxed);
}

void C64Sfx::Play(int song, double seconds) {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    tune_.Start(song);
    stopIn_ = seconds > 0 ? static_cast<int>(seconds * C64Music::kSampleRate) : -1;
    SDL_UnlockAudioDevice(device_);
}

void C64Sfx::Stop() {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    tune_.Stop();
    stopIn_ = -1;
    SDL_UnlockAudioDevice(device_);
}

void SDLCALL C64Sfx::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<C64Sfx*>(userdata);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    self->mono_.resize(static_cast<size_t>(frames));
    int done = 0;
    while (done < frames) {
        int n = frames - done;
        if (self->stopIn_ >= 0 && self->stopIn_ < n) n = self->stopIn_;
        self->tune_.Render(self->mono_.data() + done, n);
        done += n;
        if (self->stopIn_ >= 0 && (self->stopIn_ -= n) == 0) {
            self->tune_.Stop();
            self->stopIn_ = -1;
        }
    }
    auto* out = reinterpret_cast<int16_t*>(stream);
    const int vol = self->volume_.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; i++) {
        int v = self->mono_[static_cast<size_t>(i)] * vol / 256;
        v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        out[i * 2] = out[i * 2 + 1] = static_cast<int16_t>(v);
    }
    record_tap::Write("c64_sfx", C64Music::kSampleRate, 2, stream, len);
}
