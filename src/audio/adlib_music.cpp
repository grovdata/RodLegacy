#include "audio/adlib_music.h"
#include "audio/record_tap.h"

#include "core/log.h"

#include <new>

extern "C" {
#include "opl3/opl3.h"
}

namespace {

constexpr int kSampleRate = sr2mus::kOplRate;   // no resampling needed
constexpr uint32_t kTail = sr2mus::kOplRate;    // a second of decay before the loop

}  // namespace

AdlibMusic::~AdlibMusic() { Shutdown(); }

bool AdlibMusic::Init() {
    if (device_ != 0) {
        return true;
    }
    auto* c = new (std::nothrow) opl3_chip{};
    if (c == nullptr) {
        return false;
    }
    chip_ = c;
    OPL3_Reset(c, kSampleRate);

    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &AdlibMusic::AudioCallback;
    want.userdata = this;
    SDL_AudioSpec got{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device_ == 0) {
        LOG_ERROR("AdlibMusic: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        delete c;
        chip_ = nullptr;
        return false;
    }
    return true;
}

void AdlibMusic::Shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    delete static_cast<opl3_chip*>(chip_);
    chip_ = nullptr;
    playing_ = false;
    paused_ = false;
    songs_.clear();
    patches_.clear();
    playlist_ = 0;
    victory_ = -1;
    current_ = 0;
}

bool AdlibMusic::LoadBank(const uint8_t* data, size_t size) {
    return sr2mus::LoadPatches(data, size, patches_);
}

bool AdlibMusic::AddSong(const std::string& name, const uint8_t* data, size_t size) {
    // The win piece sits at the end of songs_, so a playlist song added after
    // it would land on the wrong side of the split.
    if (victory_ >= 0) {
        return false;
    }
    if (!BuildInto(name, data, size)) {
        return false;
    }
    playlist_++;
    return true;
}

bool AdlibMusic::SetVictorySong(const std::string& name, const uint8_t* data, size_t size) {
    if (victory_ >= 0 || !BuildInto(name, data, size)) {
        return false;
    }
    victory_ = static_cast<int>(songs_.size()) - 1;
    return true;
}

bool AdlibMusic::BuildInto(const std::string& name, const uint8_t* data, size_t size) {
    if (patches_.empty()) {
        return false;
    }
    std::vector<sr2mus::Event> events;
    uint32_t lengthTicks = 0;
    if (!sr2mus::ParseMus(data, size, events, lengthTicks)) {
        return false;
    }
    Song song;
    song.name = name;
    song.writes = sr2mus::Build(events, patches_);
    if (song.writes.empty()) {
        return false;
    }
    song.length = song.writes.back().sample + kTail;
    songs_.push_back(std::move(song));
    return true;
}

// The win piece plays once. UpdateMusic's loop check then moves back into the playlist.
void AdlibMusic::PlayVictorySong() {
    if (device_ == 0 || victory_ < 0) {
        return;
    }
    SDL_LockAudioDevice(device_);
    current_ = victory_;
    Rewind();
    SDL_UnlockAudioDevice(device_);
    playing_ = true;
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

std::string AdlibMusic::CurrentSongName() const {
    if (current_ < 0 || current_ >= static_cast<int>(songs_.size())) {
        return "AdLib";
    }
    return songs_[static_cast<size_t>(current_)].name;
}

void AdlibMusic::Rewind() {
    next_ = 0;
    sample_ = 0;
    if (chip_ != nullptr) {
        OPL3_Reset(static_cast<opl3_chip*>(chip_), kSampleRate);
    }
}

void AdlibMusic::SetSong(int index) {
    if (playlist_ <= 0) {
        return;
    }
    const int n = playlist_;
    index = ((index % n) + n) % n;
    if (device_ != 0) SDL_LockAudioDevice(device_);
    current_ = index;
    Rewind();
    if (device_ != 0) SDL_UnlockAudioDevice(device_);
}

void AdlibMusic::Play() {
    if (device_ == 0 || songs_.empty()) {
        return;
    }
    if (current_ >= playlist_ && playlist_ > 0) {
        current_ = 0;      // leave the win piece; Play only starts the playlist
    }
    SDL_LockAudioDevice(device_);
    Rewind();
    SDL_UnlockAudioDevice(device_);
    playing_ = true;
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

void AdlibMusic::Stop() {
    if (device_ == 0) {
        return;
    }
    SDL_PauseAudioDevice(device_, 1);
    SDL_LockAudioDevice(device_);
    Rewind();
    SDL_UnlockAudioDevice(device_);
    playing_ = false;
    paused_ = false;
}

void AdlibMusic::Pause() {
    if (device_ == 0 || !playing_ || paused_) {
        return;
    }
    paused_ = true;
    SDL_PauseAudioDevice(device_, 1);
}

void AdlibMusic::Resume() {
    if (device_ == 0 || !playing_ || !paused_) {
        return;
    }
    paused_ = false;
    SDL_PauseAudioDevice(device_, 0);
}

void AdlibMusic::SetVolume(float v) {
    volume_.store(VolumeScale(v), std::memory_order_relaxed);
}

void AdlibMusic::Fill(int16_t* out, int frames) {
    const Song& song = songs_[static_cast<size_t>(current_)];
    auto* c = static_cast<opl3_chip*>(chip_);
    const int vol = volume_.load(std::memory_order_relaxed);

    for (int i = 0; i < frames; i++) {
        while (next_ < song.writes.size() && song.writes[next_].sample <= sample_) {
            OPL3_WriteReg(c, song.writes[next_].reg, song.writes[next_].value);
            next_++;
        }
        int16_t s[2] = {0, 0};
        OPL3_Generate(c, s);
        out[i * 2] = Scale(s[0], vol);
        out[i * 2 + 1] = Scale(s[1], vol);
        sample_++;
        if (sample_ >= song.length) {
            Rewind();
            looped_.store(true, std::memory_order_relaxed);
        }
    }
}

void SDLCALL AdlibMusic::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<AdlibMusic*>(userdata);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    if (self->chip_ == nullptr || self->songs_.empty() ||
        self->current_ >= static_cast<int>(self->songs_.size())) {
        SDL_memset(stream, 0, static_cast<size_t>(len));
    } else {
        self->Fill(reinterpret_cast<int16_t*>(stream), frames);
    }
    record_tap::Write("adlib_music", kSampleRate, 2, stream, len);
}
