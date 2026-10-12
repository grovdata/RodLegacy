/*
 * RodLegacy - Street Rod's DOS PC speaker effects as timed speaker states (RodLegacyResearch/docs/pc_speaker.md).
 *
 * Created: 2026-10-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace pcspk {

constexpr double kPitHz = 1193182.0;
constexpr double kTickHz = kPitHz / 0x4000;   // the game's INT 8 rate, 72.82 Hz

// One stretch of speaker state: the PIT channel 2 divisor and port 0x61 bits 0 (timer gate) and 1 (speaker data).
struct Segment {
    double us;
    uint16_t divisor;
    uint8_t port61;
};

// The busy-wait timing. SR.EXE calibrates Delay() at start-up so a unit is CPU-independent; these are the DOSBox
// figures. The call overheads are not calibrated and would be longer on period hardware.
constexpr double kDelayUnitUs = 4.47;
constexpr double kToneOverheadUs = 20.0;
constexpr double kNoiseOverheadUs = 25.0;

enum class Effect { Click, ChirpHigh, ChirpLow, Clank, Crash, Hit, Thud };

// rng(n) returns 0..n-1, as the game's random routine.
std::vector<Segment> BuildEffect(Effect fx, const std::function<int(int)>& rng);

double DurationUs(const std::vector<Segment>& segs);

// The looping sounds the music sequencer plays in place of the tune (0f38:7548-76da): one section of (ticks,
// divisor) events, divisor 0 a rest, repeated with an optional vibrato.
// Spray is the paint job's loop: SR1's 4.8 kHz tone, SR2's 150 Hz buzz. Pulse is SR2 only.
enum class Loop { Tick, Crank, Wrench, Spray, IdlePutt, Pulse, Count };

struct LoopSong {
    std::vector<std::pair<uint16_t, uint16_t>> events;
    bool vibrato = false;
    int16_t wave[8] = {};   // the vibrato row
    int depth = 0;
    int period = 8;         // wave steps before it repeats
    bool sr2 = false;       // SR2's vibrato amount formula
};

// The vibrato's divisor swing for a note: SR1 ((depth * note) & 0xFFFF) / 1000, SR2 (note / 500) * depth.
int VibratoAmount(const LoopSong& song, uint16_t note);

// From the unpacked DOS SR.EXE; empty songs if it is not there.
std::vector<LoopSong> ReadLoops(const uint8_t* exe, size_t size);
// Street Rod 2's own (street-rod-2-sdl3 sound.md 4.2), from its unpacked SR.EXE. Empty songs if it is not that exe.
std::vector<LoopSong> ReadLoopsSr2(const uint8_t* exe, size_t size);

// The driving handler's engine note (image 0x2404), revs to PIT divisor. The limiter rev jumps up. Street Rod 2
// (race_isr 0001:2622) drops SR1's jitter term for 60000 - 1.375 * (rpm^2 >> 10) and a smaller limiter jump.
uint16_t EngineDivisor(int rpm, bool sr2 = false);

// Street Rod 2 without a muffler: the first `pulses` engine notes of every pulses + 1 get high byte 0xF0, a 19 Hz
// rumble. Races use 1, the car animations 2. count is DS:881E.
uint16_t MufflerPulse(uint16_t divisor, int& count, int pulses);

// The engine-bay tune-up tone (1000:7fb1): 30000 + 1024 * |setting|, 39.8 Hz in tune falling to 31.2 Hz.
uint16_t TuneDivisor(int setting);

// The car animations' engine note in both DOS games: d is the car's distance in pixels from where it parks
// (SR2 |AD5C - B866|, SR1 |825E - 8B7C|). The speaker plays 60000 - tone (19.9 Hz parked to 29.8 Hz from 200 px),
// SR2's AdLib engine bends by tone / 2.
int AnimationTone(int d);

}
