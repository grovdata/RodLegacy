#include "audio/pc_speaker_sfx.h"
#include "audio/record_tap.h"

#include "core/log.h"
#include "core/utils.h"

#include <climits>
#include <cmath>
#include <random>

namespace {
constexpr int kOversample = 8;
constexpr double kAmplitude = 14000.0;
// The speaker is AC coupled; kept low so the 20-65 Hz engine note survives.
constexpr double kDcCutoffHz = 10.0;
constexpr double kLowPassHz = 10000.0;
constexpr double kPi = 3.14159265358979323846;
}

PcSpeakerSfx::~PcSpeakerSfx() { Shutdown(); }

bool PcSpeakerSfx::Initialize() {
    if (device != 0) return true;
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = &PcSpeakerSfx::Mix;
    want.userdata = this;
    SDL_AudioSpec got{};
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (device == 0) {
        LOG_ERROR("PcSpeakerSfx: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }
    rate = got.freq;
    return true;
}

void PcSpeakerSfx::Shutdown() {
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

void PcSpeakerSfx::Resume() {
    if (device != 0) SDL_PauseAudioDevice(device, 0);
}

void PcSpeakerSfx::StartEngine() {
    engineOn.store(true);
    Resume();
}

void PcSpeakerSfx::StopEngine() {
    engineOn.store(false);
    sceneDivisor.store(0);
}

void PcSpeakerSfx::PlayLoop(pcspk::Loop loop, double seconds) {
    const size_t i = static_cast<size_t>(loop);
    if (i >= loops.size() || loops[i].events.empty()) return;
    loopUntil.store(seconds > 0 ? sampleClock.load() + static_cast<int64_t>(seconds * rate) : INT64_MAX);
    loopWanted.store(static_cast<int>(i));
    Resume();
}

void PcSpeakerSfx::StopLoop() {
    loopWanted.store(-1);
}

// The music sequencer, every tick: a note runs its ticks, then the next; the vibrato steps every tick on top.
bool PcSpeakerSfx::TickLoop() {
    const int want = loopUntil.load() > sampleClock.load(std::memory_order_relaxed) ? loopWanted.load() : -1;
    if (want < 0) {
        loopPlaying = -1;
        return false;
    }
    const pcspk::LoopSong& song = loops[static_cast<size_t>(want)];
    if (want != loopPlaying) {
        loopPlaying = want;
        loopEvent = song.events.size() - 1;
        loopTicks = 1;
    }
    if (--loopTicks > 0) {
        if (song.vibrato && loopNote != 0) {
            const int r = vibPhase % song.period;
            vibPhase = r + 1;
            divisor = static_cast<uint16_t>(song.wave[r] * vibAmount + loopNote);
        }
        return true;
    }
    loopEvent = (loopEvent + 1) % song.events.size();
    loopTicks = song.events[loopEvent].first;
    loopNote = song.events[loopEvent].second;
    if (loopNote == 0) {
        port61 = 0;   // a rest gates the speaker off
        return true;
    }
    vibAmount = pcspk::VibratoAmount(song, loopNote);
    divisor = loopNote;
    port61 = 3;
    return true;
}

void PcSpeakerSfx::SetTuneTone(uint16_t divisor) {
    tuneDivisor.store(divisor);
    if (divisor != 0) Resume();
}

void PcSpeakerSfx::SirenFor(double seconds) {
    sirenUntil.store(sampleClock.load() + static_cast<int64_t>(seconds * rate));
    Resume();
}

void PcSpeakerSfx::SquealFor(double seconds) {
    squealUntil.store(sampleClock.load() + static_cast<int64_t>(seconds * rate));
    Resume();
}

void PcSpeakerSfx::Play(pcspk::Effect fx) {
    if (device == 0) return;
    std::vector<pcspk::Segment> segs = pcspk::BuildEffect(fx, [](int n) {
        return n > 0 ? std::uniform_int_distribution<int>(0, n - 1)(RandomEngine()) : 0;
    });
    SDL_LockAudioDevice(device);
    effect = std::move(segs);
    effectAt = 0;
    effectLeftUs = effect.empty() ? 0.0 : effect[0].us;
    SDL_UnlockAudioDevice(device);
    Resume();
}

// The driving handler's schedule: the BIOS tick is silent, the engine note goes on one tick of the other three and
// the siren or squeal on the remaining two.
void PcSpeakerSfx::Tick() {
    if (TickLoop()) return;
    const int old = tickCounter;
    tickCounter = (tickCounter + 3) & 3;
    // SR1 sounds on three ticks of four, engine on one; SR2 reads the counter before the decrement and uses all
    // four, the odd ones for the engine.
    const bool sr2Schedule = sr2.load();
    if (!sr2Schedule && old == 0) return;
    const bool engineTick = sr2Schedule ? (old & 1) != 0 : tickCounter == 1;

    const int64_t now = sampleClock.load(std::memory_order_relaxed);
    const bool siren = sirenUntil.load() > now;
    const bool squeal = squealUntil.load() > now;
    const bool engine = engineOn.load();
    const uint16_t tune = tuneDivisor.load();

    if (!engine && tune == 0 && !siren && !squeal) {
        port61 = 0;
        return;
    }
    port61 = 3;
    if (!engine && tune != 0) {
        divisor = tune;
        return;
    }
    if (engineTick) {
        if (engine) {
            const uint16_t scene = sceneDivisor.load();
            divisor = scene != 0 ? scene : pcspk::EngineDivisor(rpm_.load(), sr2Schedule);
            if (sr2Schedule && noMuffler.load()) divisor = pcspk::MufflerPulse(divisor, pulseCount, scene != 0 ? 2 : 1);
        }
        return;
    }
    squealPhase = !squealPhase;
    if (siren) {
        divisor = sirenCount++ < 10 ? 0x054B : 0x0712;   // 880 / 659 Hz
        if (sirenCount > 20) sirenCount = 0;
    } else if (squeal && squealPhase) {
        divisor = 0x0095;   // an 8 kHz blip
    }
}

void SDLCALL PcSpeakerSfx::Mix(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<PcSpeakerSfx*>(userdata);
    auto* out = reinterpret_cast<int16_t*>(stream);
    const int frames = len / (2 * static_cast<int>(sizeof(int16_t)));
    const double usPerSample = 1e6 / self->rate;
    const double tickStep = pcspk::kTickHz / self->rate;
    const double dcR = std::exp(-2.0 * kPi * kDcCutoffHz / self->rate);
    const double lpA = 1.0 - std::exp(-2.0 * kPi * kLowPassHz / self->rate);
    const double gain = kAmplitude * self->volume.load();

    for (int i = 0; i < frames; ++i) {
        self->tickAcc += tickStep;
        if (self->tickAcc >= 1.0) {
            self->tickAcc -= 1.0;
            self->Tick();
        }

        uint16_t div = self->divisor;
        uint8_t p61 = self->port61;
        if (self->effectAt < self->effect.size()) {
            const pcspk::Segment& s = self->effect[self->effectAt];
            div = s.divisor;
            p61 = s.port61;
            self->effectLeftUs -= usPerSample;
            while (self->effectLeftUs <= 0.0 && ++self->effectAt < self->effect.size()) {
                self->effectLeftUs += self->effect[self->effectAt].us;
            }
        }

        // Cone position: bit 1 high, and either the gate is off or the PIT's square wave is high.
        const double step = pcspk::kPitHz / (div == 0 ? 65536 : div) / (self->rate * kOversample);
        int high = 0;
        for (int k = 0; k < kOversample; ++k) {
            self->phase += step;
            self->phase -= std::floor(self->phase);
            if ((p61 & 2) && (!(p61 & 1) || self->phase < 0.5)) ++high;
        }
        const double x = static_cast<double>(high) / kOversample;
        self->dcOut = x - self->dcIn + dcR * self->dcOut;
        self->dcIn = x;
        self->lowPass += lpA * (self->dcOut - self->lowPass);

        const double v = std::max(-32768.0, std::min(32767.0, self->lowPass * gain));
        out[i * 2] = out[i * 2 + 1] = static_cast<int16_t>(v);
        self->sampleClock.fetch_add(1, std::memory_order_relaxed);
    }
    record_tap::Write("pc_speaker_sfx", self->rate, 2, stream, len);
}
