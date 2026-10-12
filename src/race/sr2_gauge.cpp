#include "race/sr2_gauge.h"

#include <algorithm>

namespace srrace {

namespace {

// File offsets in the Street Rod 2 Amiga executable: 6 records of 14 bytes, and the stream hunk they point into.
constexpr size_t kDialTableOffset = 0x3f5a0;
constexpr size_t kStreamBase = 0x14f24;
constexpr size_t kRecordSize = 14;

constexpr int kStepX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
constexpr int kStepY[8] = {1, 1, 1, 0, 0, -1, -1, -1};

int16_t S16(const uint8_t* p) { return static_cast<int16_t>((p[0] << 8) | p[1]); }

}  // namespace

bool Sr2Gauges::Load(const std::vector<uint8_t>& exe) {
    ready = false;
    if (exe.size() < kDialTableOffset + SR2_DIAL_COUNT * kRecordSize) return false;

    std::array<Dial, SR2_DIAL_COUNT> loaded;
    for (int i = 0; i < SR2_DIAL_COUNT; ++i) {
        const uint8_t* r = exe.data() + kDialTableOffset + i * kRecordSize;
        Dial& d = loaded[i];
        d.x0 = S16(r);
        d.y0 = S16(r + 2);
        d.dx = S16(r + 4);
        d.dy = S16(r + 6);
        d.len = (r[8] << 8) | r[9];
        const size_t ptr = (static_cast<size_t>(r[10]) << 24) | (r[11] << 16) | (r[12] << 8) | r[13];

        // A stream is exactly `len` steps long.
        int steps = 0;
        for (size_t p = kStreamBase + ptr; steps < d.len; ++p) {
            if (p >= exe.size()) return false;
            d.stream.push_back(exe[p]);
            steps += (exe[p] & 0x1f) + 1;
        }
        if (d.len <= 0 || steps != d.len) return false;
    }
    dials = std::move(loaded);
    ready = true;
    return true;
}

Sr2Needle Sr2Gauges::Tip(int dial, int value) const {
    if (!ready) return {0, 0, 0, 0};
    const Dial& d = dials[static_cast<size_t>(std::clamp(dial, 0, SR2_DIAL_COUNT - 1))];

    int n = std::clamp(value, 0, SR2_GAUGE_MAX) * d.len / 130;
    n = std::min(n, d.len - 1);

    int x = d.x0 + d.dx;
    int y = d.y0 - d.dy;
    for (uint8_t b : d.stream) {
        if (n <= 0) break;
        const int run = std::min((b & 0x1f) + 1, n);
        x += kStepX[b >> 5] * run;
        y += kStepY[b >> 5] * run;
        n -= run;
    }
    return {d.x0, d.y0, x, y};
}

}  // namespace srrace
