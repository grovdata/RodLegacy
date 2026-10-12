/*
 * RodLegacy - The DOS PC speaker as a sound effect source: engine note, siren, squeal, tune-up tone and effects.
 *
 * Created: 2026-10-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "audio/pc_speaker_fx.h"

#include <SDL2/SDL.h>

#include <atomic>
#include <vector>

// One speaker, as in the original: a 72.82 Hz tick runs the driving handler's schedule (engine note on one tick of
// four, siren or squeal on two), and the busy-loop effects take the speaker over while they play.
// ponytail: an effect replaces the tick output instead of being chopped by it as on hardware.
class PcSpeakerSfx {
public:
    ~PcSpeakerSfx();

    bool Initialize();
    void Shutdown();

    // 0.0 - 1.0, the SFX volume.
    void SetVolume(float v) { volume.store(v); }
    SDL_AudioDeviceID Device() const { return device; }

    void StartEngine();
    void StopEngine();
    bool EngineRunning() const { return engineOn.load(); }
    void SetRpm(int rpm) { rpm_.store(rpm); sceneDivisor.store(0); }
    void SetSceneDivisor(uint16_t divisor) { sceneDivisor.store(divisor); }

    // 0 = off.
    void SetTuneTone(uint16_t divisor);

    // Street Rod 2's engine formula and tick schedule.
    void SetSr2(bool on) { sr2.store(on); }
    void SetMufflerRemoved(bool removed) { noMuffler.store(removed); }

    void SirenFor(double seconds);
    void SquealFor(double seconds);

    void Play(pcspk::Effect fx);

    // A looping sound for the given time, or until StopLoop with seconds <= 0. Silent without SR.EXE.
    void SetLoops(std::vector<pcspk::LoopSong> songs) { loops = std::move(songs); }
    void PlayLoop(pcspk::Loop loop, double seconds);
    void StopLoop();

private:
    static void SDLCALL Mix(void* userdata, Uint8* stream, int len);
    void Tick();
    bool TickLoop();
    void Resume();

    SDL_AudioDeviceID device = 0;
    int rate = 44100;

    std::atomic<float> volume {0.8F};
    std::atomic<bool> engineOn {false};
    std::atomic<bool> sr2 {false};
    std::atomic<bool> noMuffler {false};
    std::atomic<int> rpm_ {1500};
    std::atomic<uint16_t> sceneDivisor {0};
    std::atomic<uint16_t> tuneDivisor {0};
    std::atomic<int64_t> sirenUntil {0};
    std::atomic<int64_t> squealUntil {0};

    std::atomic<int64_t> sampleClock {0};
    std::vector<pcspk::LoopSong> loops;
    std::atomic<int> loopWanted {-1};
    std::atomic<int64_t> loopUntil {0};
    int loopPlaying = -1;
    size_t loopEvent = 0;
    int loopTicks = 0;
    uint16_t loopNote = 0;
    int vibAmount = 0;
    int vibPhase = 0;

    // Audio thread only, except the effect, which is swapped in under SDL_LockAudioDevice.
    double tickAcc = 0.0;
    int tickCounter = 3;
    bool squealPhase = false;
    int sirenCount = 0;
    int pulseCount = 0;
    uint16_t divisor = 30;
    uint8_t port61 = 0;
    double phase = 0.0;
    std::vector<pcspk::Segment> effect;
    size_t effectAt = 0;
    double effectLeftUs = 0.0;
    double dcIn = 0.0, dcOut = 0.0, lowPass = 0.0;
};
