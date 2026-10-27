#pragma once

#include <SDL.h>

#include <atomic>
#include <cstdint>

// Street Rod 2's DOS AdLib engine sound: one note held on OPL voice 3, its pitch following the revs.
// StartEngine 0x2d5e2, EngineSound 0x2d116, SetVoicePitch 0x2e63f; see RodLegacyResearch/docs/sr2_dos/adlib.md.
// The patch and pitch range are measured off captures/sr_001.dro.
class AdlibEngine {
public:
    AdlibEngine() = default;
    ~AdlibEngine();

    bool Initialize();
    void Shutdown();

    void Start();
    void Stop();
    bool IsRunning() const { return running; }
    SDL_AudioDeviceID Device() const { return device; }

    // rpm as the sim reports it. Anything outside the range is clamped.
    void SetRpm(int rpm);
    // The car animations: pcspk::AnimationTone of the car's distance from its parking spot.
    void SetSceneTone(int tone);

    static constexpr int kRpmMin = 1500;
    static constexpr int kRpmMax = 7000;

    // What the capture shows voice 3 covering, in Hz, tracking the running block across all 163 of its A3
    // writes: it idles at 23 and the highest it ever reaches is 41.
    static constexpr double kHzAtIdle = 23.0;
    static constexpr double kHzAtRedline = 41.0;

    // OPL2 voice 3 lives on operator slots 0x08 (modulator) and 0x0b
    // (carrier); its own registers are 0xa3 / 0xb3 / 0xc3.
    static constexpr uint8_t kVoice = 3;
    static constexpr uint8_t kModSlot = 0x08;
    static constexpr uint8_t kCarSlot = 0x0b;

    // frequency -> {fnum, block}, the OPL's own encoding. Exposed so it can be
    // tested without an audio device.
    struct Fnum {
        uint16_t fnum;
        uint8_t block;
    };
    // Pure, and inline so the tests can reach them without linking SDL.
    static int ClampRpm(int rpm) {
        return rpm < kRpmMin ? kRpmMin : (rpm > kRpmMax ? kRpmMax : rpm);
    }

    // SR2 bends the animation note by tone / 2, where a race bends by rpm * 3 - 0x1194: the same note as that rpm.
    static double HzForSceneTone(int tone) {
        return HzForRpm(((tone >> 1) + 0x1194) / 3);
    }

    static double HzForRpm(int rpm) {
        const double t = static_cast<double>(ClampRpm(rpm) - kRpmMin) / (kRpmMax - kRpmMin);
        return kHzAtIdle + t * (kHzAtRedline - kHzAtIdle);
    }

    // The OPL's frequency is fnum * rate / 2^20, shifted up by the block.
    static Fnum FnumForHz(double hz) {
        for (uint8_t block = 0; block < 8; ++block) {
            const double scale = static_cast<double>(1u << 20) /
                                 (static_cast<double>(kOplRate) * (1u << block));
            const double f = hz * scale;
            if (f <= 1023.0) {
                const double v = f < 0.0 ? 0.0 : f;
                return {static_cast<uint16_t>(v + 0.5), block};
            }
        }
        return {1023, 7};
    }

    static constexpr int kOplRate = 49716;   // the OPL2's own rate

    // SetVoiceLevel (2c57:2128) takes a 0..127 volume: 127 with no muffler, else 80 + (rev deviation >> 7).
    // Total level = (127 - vol) * 63 / 127, which matches the capture's 0x17 and 0x00.
    static constexpr int kVolumeMax = 127;
    static constexpr int kVolumeMuffledIdle = 80;

    static uint8_t LevelForVolume(int volume) {
        const int v = volume < 0 ? 0 : (volume > kVolumeMax ? kVolumeMax : volume);
        return static_cast<uint8_t>((kVolumeMax - v) * 63 / kVolumeMax);
    }

    static int VolumeForRpm(int rpm, bool mufflerRemoved) {
        if (mufflerRemoved) {
            return kVolumeMax;
        }
        const int span = kVolumeMax - kVolumeMuffledIdle;
        return kVolumeMuffledIdle + span * (ClampRpm(rpm) - kRpmMin) / (kRpmMax - kRpmMin);
    }

    static uint8_t LevelForRpm(int rpm, bool mufflerRemoved) {
        return LevelForVolume(VolumeForRpm(rpm, mufflerRemoved));
    }

    // Whether the car being driven has had its muffler taken off.
    void SetMufflerRemoved(bool removed);

    // 0.0 - 1.0, the SFX volume.
    void SetVolume(float v);
    // One OPL voice comes out around -24 dBFS at full level and -41 muffled at idle, far under the samples.
    static constexpr float kNormalizeGain = 6.0F;
    static int VolumeScale(float v) {
        const float clamped = v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
        return static_cast<int>(clamped * kNormalizeGain * 256.0F);
    }

private:
    void Apply(double hz, int volume);

    void WritePatch();
    bool Queue(uint8_t reg, uint8_t value);
    static void SDLCALL Mix(void* userdata, Uint8* stream, int len);

    struct Pending {
        uint8_t reg;
        uint8_t value;
    };

    SDL_AudioDeviceID device = 0;
    void* chip = nullptr;              // opl3_chip, hidden so the header stays C++-only
    // A tiny ring the game thread writes and the audio thread drains. Only ever
    // a handful of register writes per update.
    static constexpr int kQueueSize = 64;
    Pending queue[kQueueSize] {};
    std::atomic<int> head {0};
    std::atomic<int> volume {VolumeScale(0.8F)};   // 256 is unity
    std::atomic<int> tail {0};
    bool running = false;
    bool mufflerRemoved = false;
    int lastRpm = kRpmMin;
    // Non-zero while a scene tone is held, which is what tells the muffler
    // change not to yank the pitch back onto the rpm ladder.
    double sceneHz = 0.0;
    // What was last written, so an unchanged value is not resent every frame.
    // 0xff is not a value any of them takes, so it forces a full resend.
    uint8_t lastA = 0xff;
    uint8_t lastB = 0xff;
    uint8_t lastLevel = 0xff;
};
