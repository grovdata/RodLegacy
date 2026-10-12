#include "scenes/sr2_end_script.h"

namespace {
constexpr size_t kRectBytes = 504;
constexpr size_t kRectSize = 6;
}

bool Sr2EndParse(const std::vector<uint8_t>& data, Sr2EndScript& out) {
    out = {};
    if (data.size() < kRectBytes) return false;

    for (size_t i = 0; i < kRectBytes; i += kRectSize) {
        out.rects.push_back({data[i], data[i + 1], (data[i + 2] << 8) | data[i + 3], data[i + 4]});
    }

    Sr2EndFrame cur{};
    size_t o = kRectBytes;
    while (o < data.size() && data[o] != 0xff) {
        if (o + 4 > data.size() || data[o] >= out.rects.size()) {
            out = {};
            return false;
        }
        cur.sprites.push_back({data[o], data[o + 1], data[o + 2]});
        if (data[o + 3] != 0) {
            cur.hold = data[o + 3];
            out.frames.push_back(cur);
            cur = {};
        }
        o += 4;
    }
    if (o >= data.size() || !cur.sprites.empty() || out.frames.empty()) {
        out = {};
        return false;
    }
    return true;
}
