/*
 * RodLegacy - The Amiga music engine's volume envelope (srl.env): AmigaEnvelope steps one note's volume per
 * interrupt tick.
 *
 * Created: 2026-09-19
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>

// An srl.env entry is 8 levels then 8 rates; segment s ramps toward level[s] at rate[s].
// Rate high byte: step per tick. Low byte only: one unit every n ticks. Zero: jump and hold.
struct AmigaEnvelope {
    const int16_t* points = nullptr;  // the entry's 16 words
    int prevLast = 0;                 // word before the entry: level[-1]
    int noteVolume = 64;
    int master = 15;
    int step = 0;                     // 1-based segment, 0 = finished
    int hold = 0;
    int volume = 0;

    void Start(const int16_t* entry, int previousLast, int noteVol, int masterVol) {
        points = entry;
        prevLast = previousLast;
        noteVolume = noteVol > 0 ? noteVol : 1;
        master = masterVol > 0 ? masterVol : 1;
        volume = Scale(points[8] / 256);
        step = 1;
        hold = 0;
    }

    bool Active() const { return step != 0; }

    void Tick() {
        if (step == 0 || step > 8) {
            step = 0;
            return;
        }
        if (hold > 0) {
            hold--;
            return;
        }
        const int s = step - 1;
        const int rate = points[8 + s];
        if (rate / 256 != 0) {
            Move(s, Scale(rate / 256) | 1);
            return;
        }
        const int wait = (((rate & 0xff) << 6) / noteVolume) * 15 / master;
        if (wait == 0) {
            Move(s, 0);
            return;
        }
        hold = wait - 1;
        const int target = Scale(points[s]);
        if (points[s] - Level(s - 1) > 0) {
            if (volume < target) volume++;
            else step++;
        } else if (volume > target) {
            volume--;
        } else if (points[s] != Level(s - 1)) {
            step++;
        }
        if (volume == 0) step = 0;
    }

private:
    int Scale(int level) const { return ((level * noteVolume) / 64) * master / 15; }
    int Level(int s) const { return s < 0 ? prevLast : points[s]; }

    void Move(int s, int delta) {
        const int target = Scale(points[s]);
        if (delta == 0) {
            volume = target;
        } else if (points[s] - Level(s - 1) > 0) {
            if (target - volume > delta) {
                volume += delta;
            } else {
                volume = target;
                step++;
            }
        } else if (volume - target > delta) {
            volume -= delta;
        } else {
            volume = target;
            if (points[s] != Level(s - 1)) step++;
        }
        if (volume == 0) step = 0;
    }
};
