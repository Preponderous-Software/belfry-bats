#include "sim/autopilot.h"

#include <algorithm>

namespace belfry {

Input autopilot(const World& w) {
    Input in;
    in.steering = true;
    const Level& l = w.level;
    const Vec2 c = w.centre;
    const Vec2 belfry{l.towerX - 0.5f, (l.belfryY0 + l.belfryY1) * 0.5f};
    if (c.x > l.towerX - 22.0f) {
        in.guide = belfry;
        return in;
    }
    const float floor = skyline(l, c.x - 1.5f, c.x + 9.0f);
    const float low = std::min(kCeiling - 2.2f, std::max(4.0f, floor + 2.6f));
    const float squeeze = (floor + kCeiling) * 0.5f;
    const float base = floor + 2.6f > kCeiling - 2.2f ? squeeze : low;
    // Of a few heights, the one that keeps furthest from the owls over the next
    // two and a half seconds.
    const float t = seconds(w);
    float bestY = base, bestGap = -1;
    for (int k = 0; k < 12; ++k) {
        const float y = base + 1.0f * static_cast<float>(k);
        if (y > kCeiling - 1.8f) break;
        float gap = 1e9f;
        for (const OwlPath& o : l.owls) {
            if (std::fabs(o.centre.x - c.x) > 18.0f) continue;
            for (int s = 0; s <= 10; ++s) {
                const float dt = 0.25f * static_cast<float>(s);
                const Vec2 owl = owlAt(o, t + dt);
                const Vec2 me{c.x + kMaxSpeed * 0.8f * dt, c.y + (y - c.y) * std::min(1.0f, dt)};
                gap = std::min(gap, length(owl - me));
            }
        }
        const float score = std::min(gap, 6.0f) - 0.02f * static_cast<float>(k);
        if (score > bestGap) {
            bestGap = score;
            bestY = y;
        }
    }
    if (bestGap < 2.6f) {
        in.guide = {c.x - 1.5f, bestY};  // wait for the owl to pass
    } else {
        in.guide = {c.x + 7.0f, bestY};
    }
    return in;
}

}  // namespace belfry
