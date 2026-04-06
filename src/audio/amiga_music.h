#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class AmigaMusic {
public:
    AmigaMusic();
    ~AmigaMusic();

    bool Init(int sample_rate = 44100);
    void Shutdown();

    bool Load(const uint8_t* note_data, size_t note_size,
              const uint8_t* wave1_data, size_t wave1_size,
              const uint8_t* wave0_data, size_t wave0_size,
              const uint8_t* ins_data, size_t ins_size,
              const uint8_t* env_data, size_t env_size);

    void Play();
    void Stop();
    void SetVolume(float volume);

private:
    struct InstrumentDef {
        uint16_t env_index = 0;
        uint16_t unknown = 0;
        uint16_t bank_index = 0;
        uint16_t sample_length = 0;
        uint16_t sample_offset = 0;
    };

    struct EnvelopeDef {
        int16_t points[16] = {};
    };

    struct ChannelState {
        size_t stream_index = 0;
        int wait_ticks = 0;
        int instrument = 0;
        int note_volume = 64;
        int env_step = 0;
        int env_volume = 0;

        bool sample_active = false;
        const int8_t* sample_ptr = nullptr;
        int sample_len = 0;
        double sample_pos = 0.0;
        double sample_step = 0.0;

        std::array<size_t, 5> slot_ptr = {};
        std::array<bool, 5> slot_valid = {};
        std::array<int16_t, 5> loop_counter = {};
    };

    static void AudioCallback(void* userdata, uint8_t* stream, int len);
    void FillBuffer(int16_t* buffer, int frames);
    void EnvelopeTick();
    void MusicTick();
    void HandleControlByte(int ch_index, uint8_t cmd);
    void StartNote(int ch_index, int note_byte);
    void AdvanceArrangement();
    bool ResetState();

    int NoteToPeriod(int note) const;
    double PeriodToStep(int period) const;
    static uint16_t ReadBE16(const uint8_t* p);
    static bool ParsePatterns(const std::vector<uint8_t>& note_bytes, std::vector<std::array<size_t, 4>>& out_patterns);
    static bool ParseInstruments(const std::vector<uint8_t>& ins_bytes, std::vector<InstrumentDef>& out_instruments);
    static bool ParseEnvelopes(const std::vector<uint8_t>& env_bytes, std::vector<EnvelopeDef>& out_envelopes);

    std::vector<uint8_t> note_bytes_;
    std::vector<InstrumentDef> instruments_;
    std::vector<EnvelopeDef> envelopes_;
    std::vector<std::array<size_t, 4>> patterns_;
    std::vector<int8_t> wave_bank_[2];

    std::array<ChannelState, 4> channel_ = {};
    int arrangement_index_ = 0;
    int arrangement_len_ = 0;
    int master_volume_ = 15;
    int global_transpose_ = 0x54;
    int speed_ticks_ = 2;
    int cia_timer_period_ = 35000;
    bool conditional_flag_ = false;

    uint64_t speed_counter_ = 0;
    double sample_tick_accum_ = 0.0;
    int sample_rate_ = 44100;

    std::atomic<bool> loaded_ {false};
    std::atomic<bool> playing_ {false};
    std::atomic<float> volume_scale_ {0.8f};

    SDL_AudioDeviceID device_ = 0;
};
