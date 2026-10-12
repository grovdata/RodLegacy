#pragma once
// Street Rod 2's speedometer and tachometer needles, walked along a chain code.

#include <array>
#include <cstdint>
#include <vector>

namespace srrace {

struct Sr2Needle { int x0, y0, x1, y1; };

constexpr int SR2_DIAL_COUNT = 6;     // tacho and speedo for each of three dashboards
constexpr int SR2_GAUGE_MAX = 130;    // dial input scale, as in Street Rod

class Sr2Gauges {
public:
    // False, with no gauges, when the executable does not hold the tables.
    bool Load(const std::vector<uint8_t>& exe);
    bool Ready() const { return ready; }

    // dial = type + 2 * variant, type 0 tacho, 1 speedo. Pivot to tip, relative to the dashboard.
    Sr2Needle Tip(int dial, int value) const;

private:
    struct Dial {
        int x0 = 0, y0 = 0, dx = 0, dy = 0, len = 0;
        std::vector<uint8_t> stream;
    };
    std::array<Dial, SR2_DIAL_COUNT> dials;
    bool ready = false;
};

}  // namespace srrace
