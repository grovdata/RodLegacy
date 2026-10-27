/*
 * RodLegacy - Street Rod 2's DOS AdLib sound effects: MSR.SND timbres played the way its effect slots play them.
 *
 * Created: 2026-10-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "audio/sr2_mus.h"

#include <SDL.h>

#include <atomic>
#include <vector>

class AdlibSfx {
public:
    ~AdlibSfx();

    bool Init(std::vector<sr2mus::Patch> bank);
    void Shutdown();

    // 0.0 - 1.0, the SFX volume.
    void SetVolume(float v);
    SDL_AudioDeviceID Device() const { return device; }

    // seconds <= 0: until Stop.
    void Play(adlibfx::Sound sound, double seconds = 0);
    void Stop(adlibfx::Sound sound);

private:
    static void SDLCALL Mix(void* userdata, Uint8* stream, int len);
    void Queue(std::vector<sr2mus::RegWrite> writes);

    SDL_AudioDeviceID device = 0;
    void* chip = nullptr;
    std::vector<sr2mus::Patch> patches;
    std::atomic<uint32_t> sample {0};
    std::atomic<int> volume {0};
    std::vector<sr2mus::RegWrite> pending;   // absolute samples, sorted; under SDL_LockAudioDevice
};
