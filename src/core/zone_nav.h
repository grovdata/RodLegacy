#ifndef ZONE_NAV_H
#define ZONE_NAV_H

// SDL-free for unit tests; callers convert their rects to nav::Rect.

#include <cstdlib>
#include <vector>

namespace nav {

struct Rect {
    int x, y, w, h;
    int CenterX() const { return x + w / 2; }
    int CenterY() const { return y + h / 2; }
    bool Contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

enum class Dir { Up, Down, Left, Right };

inline int PickNearest(const std::vector<Rect>& zones, int px, int py) {
    int best = -1;
    long bestScore = 0;
    for (size_t i = 0; i < zones.size(); ++i) {
        const long dx = zones[i].CenterX() - px;
        const long dy = zones[i].CenterY() - py;
        const long score = dx * dx + dy * dy;
        if (best < 0 || score < bestScore) {
            bestScore = score;
            best = (int)i;
        }
    }
    return best;
}

// Off-axis distance is penalised so a straight neighbour beats a closer diagonal one.
inline int PickDirectional(const std::vector<Rect>& zones, int px, int py, Dir dir) {
    int best = -1;
    long bestScore = 0;
    for (size_t i = 0; i < zones.size(); ++i) {
        const Rect& z = zones[i];
        if (z.Contains(px, py)) continue;

        const long dx = z.CenterX() - px;
        const long dy = z.CenterY() - py;

        long primary = 0, perp = 0;
        switch (dir) {
            case Dir::Up:    primary = -dy; perp = labs(dx); break;
            case Dir::Down:  primary =  dy; perp = labs(dx); break;
            case Dir::Left:  primary = -dx; perp = labs(dy); break;
            case Dir::Right: primary =  dx; perp = labs(dy); break;
        }
        if (primary <= 0) continue;

        const long score = primary + 2 * perp;
        if (best < 0 || score < bestScore) {
            bestScore = score;
            best = (int)i;
        }
    }
    return best;
}

}

#endif
