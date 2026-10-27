#pragma once

#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <vector>

#include "audio/c64_sid.h"

// The C64 version's engine sounds on a SID of their own: the race engine as me.o drives it (register table at
// $89E8, voice 1 pitched by rpm every frame), and the seqgar1 effects the game uses for its other engines:
// 11 the engine-bay tuning drone, 12 / 16 a car driving in / out. See RodLegacyResearch/docs/c64/engine.md.
class C64Engine {
public:
    ~C64Engine();

    bool Init(const std::vector<uint8_t>& seqgar1);
    void Shutdown();
    SDL_AudioDeviceID Device() const { return device_; }

    // 0.0 - 1.0, the SFX volume.
    void SetVolume(float v);

    void StartRace();
    void SetRpm(int rpm);
    void PlayScene(bool arriving);
    void StartBay(int tuneOffset);
    void SetBayTune(int tuneOffset);
    void Stop();

    // Stops the driver and loads me.o's race voice into the SID: the $89E8 table into $D400-$D418 ($8A01),
    // voice 1 pulse with the gate on through the lowpass, then $D418 = $17 (lowpass, volume 7).
    static void LoadRace(c64::Tune& tune) {
        static constexpr uint8_t kRaceSid[25] = {0xF0, 0x00, 0x00, 0x0A, 0x41, 0x00, 0xC7, 0x00, 0x1C,
                                                 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0x00, 0x00,
                                                 0x81, 0x00, 0x00, 0x00, 0xE0, 0xF1, 0x00};
        tune.Stop();
        for (int r = 0; r < 25; r++) tune.WriteSid(r, kRaceSid[r]);
        tune.WriteSid(0x18, 0x17);
    }

    // me.o $895A: voice 1 frequency = rpm / 4 + $100.
    static uint16_t FreqForRpm(int rpm) {
        const int r = rpm < 0 ? 0 : (rpm > 0xFFFF ? 0xFFFF : rpm);
        return static_cast<uint16_t>((r >> 2) + 0x100);
    }
    // b1q $92F4: $BF12 = $2C - |tune| * 4, the knob running $7F..$93 around $89.
    static uint8_t BayPitch(int tuneOffset) {
        int t = tuneOffset < 0 ? -tuneOffset : tuneOffset;
        if (t > 10) t = 10;
        return static_cast<uint8_t>(0x2C - t * 4);
    }

private:
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len);

    SDL_AudioDeviceID device_ = 0;
    c64::Tune tune_;
    std::vector<int16_t> mono_;
    std::atomic<int> volume_ {0};
};
