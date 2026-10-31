#include "sim/level.h"

#include <algorithm>

namespace belfry {

namespace {

float sdBox(Vec2 p, Vec2 lo, Vec2 hi) {
    const Vec2 c{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f};
    const Vec2 h{(hi.x - lo.x) * 0.5f, (hi.y - lo.y) * 0.5f};
    const float dx = std::fabs(p.x - c.x) - h.x, dy = std::fabs(p.y - c.y) - h.y;
    const float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f);
}

// Inigo Quilez's exact signed distance to a triangle.
float sdTriangle(Vec2 p, Vec2 p0, Vec2 p1, Vec2 p2) {
    const Vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    const Vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    const Vec2 pq0 = v0 - e0 * clampf(dot(v0, e0) / dot(e0, e0), 0, 1);
    const Vec2 pq1 = v1 - e1 * clampf(dot(v1, e1) / dot(e1, e1), 0, 1);
    const Vec2 pq2 = v2 - e2 * clampf(dot(v2, e2) / dot(e2, e2), 0, 1);
    const float s = e0.x * e2.y - e0.y * e2.x > 0 ? 1.0f : -1.0f;
    const float d0 = dot(pq0, pq0), d1 = dot(pq1, pq1), d2 = dot(pq2, pq2);
    const float c0 = s * (v0.x * e0.y - v0.y * e0.x), c1 = s * (v1.x * e1.y - v1.y * e1.x),
                c2 = s * (v2.x * e2.y - v2.y * e2.x);
    const float d = std::min(d0, std::min(d1, d2));
    const float c = std::min(c0, std::min(c1, c2));
    return -std::sqrt(d) * (c > 0 ? 1.0f : -1.0f);
}

Shape box(float x0, float y0, float x1, float y1) {
    Shape s;
    s.kind = Shape::Box;
    s.a = {x0, y0};
    s.b = {x1, y1};
    return s;
}

Shape disc(float x, float y, float r) {
    Shape s;
    s.kind = Shape::Disc;
    s.a = {x, y};
    s.r = r;
    return s;
}

Shape tri(Vec2 a, Vec2 b, Vec2 c) {
    Shape s;
    s.kind = Shape::Triangle;
    s.a = a;
    s.b = b;
    s.c = c;
    return s;
}

float topOf(const Shape& s) {
    switch (s.kind) {
        case Shape::Box: return s.b.y;
        case Shape::Disc: return s.a.y + s.r;
        default: return std::max(s.a.y, std::max(s.b.y, s.c.y));
    }
}

struct NightPlan {
    float length, dawn;
    int owls, groups;
    float pineMax, chimneyExtra, gapMin, gapMax;
};

const NightPlan kPlans[kNights] = {
    // length dawn owls groups pineMax chimneyExtra gapMin gapMax
    {150, 62, 1, 3, 11.5f, 1.6f, 5.5f, 8.5f},
    {205, 76, 2, 4, 13.0f, 2.2f, 5.0f, 8.0f},
    {260, 90, 3, 5, 14.0f, 2.8f, 4.6f, 7.5f},
    {315, 104, 5, 6, 15.0f, 3.4f, 4.2f, 7.0f},
    {370, 118, 6, 7, 16.0f, 4.0f, 3.8f, 6.5f},
};

}  // namespace

float distanceTo(const Shape& s, Vec2 p) {
    switch (s.kind) {
        case Shape::Box: return sdBox(p, s.a, s.b);
        case Shape::Disc: return length(p - s.a) - s.r;
        default: return sdTriangle(p, s.a, s.b, s.c);
    }
}

void spanOf(const Shape& s, float& x0, float& x1) {
    switch (s.kind) {
        case Shape::Box:
            x0 = s.a.x;
            x1 = s.b.x;
            break;
        case Shape::Disc:
            x0 = s.a.x - s.r;
            x1 = s.a.x + s.r;
            break;
        default:
            x0 = std::min(s.a.x, std::min(s.b.x, s.c.x));
            x1 = std::max(s.a.x, std::max(s.b.x, s.c.x));
    }
}

Vec2 owlAt(const OwlPath& o, float seconds) {
    const float t = seconds * o.speed + o.phase;
    return {o.centre.x + o.ax * std::sin(t), o.centre.y + o.ay * std::cos(t)};
}

void shapesOf(const Prop& p, const Level& l, std::vector<Shape>& out) {
    switch (p.kind) {
        case PropKind::House: {
            const float hw = p.width * 0.5f;
            out.push_back(box(p.x - hw, 0, p.x + hw, p.height));
            out.push_back(tri({p.x - hw - 0.4f, p.height}, {p.x + hw + 0.4f, p.height}, {p.x, p.height + p.roof}));
            if (p.chimney > 0) out.push_back(box(p.chimneyX - 0.45f, p.height, p.chimneyX + 0.45f, p.chimney));
            break;
        }
        case PropKind::Oak:
            out.push_back(box(p.x - 0.4f, 0, p.x + 0.4f, p.height + 0.5f));
            out.push_back(disc(p.x, p.height + p.width * 0.7f, p.width));
            break;
        case PropKind::Pine: {
            out.push_back(box(p.x - 0.3f, 0, p.x + 0.3f, 1.6f));
            const float span = p.height - 1.2f;
            for (int i = 0; i < 3; ++i) {
                const float y0 = 1.2f + static_cast<float>(i) * span * 0.25f;
                const float y1 = i == 2 ? p.height : y0 + span * 0.5f;
                const float hw = p.width * 0.5f * (1.0f - 0.22f * static_cast<float>(i));
                out.push_back(tri({p.x - hw, y0}, {p.x + hw, y0}, {p.x, y1}));
            }
            break;
        }
        case PropKind::Church: {
            const float tx = p.x, hw = l.belfryHalf;
            out.push_back(box(tx - hw, 0, tx + hw, l.belfryY0));
            out.push_back(box(tx - hw - 0.3f, l.belfryY1, tx + hw + 0.3f, l.belfryY1 + 0.8f));
            out.push_back(tri({tx - hw, l.belfryY1 + 0.8f}, {tx + hw, l.belfryY1 + 0.8f}, {tx, kCeiling + 6.0f}));
            // The nave, beyond the tower.
            out.push_back(box(tx + hw, 0, tx + hw + 12.0f, 7.0f));
            out.push_back(tri({tx + hw, 7.0f}, {tx + hw + 12.0f, 7.0f}, {tx + hw + 6.0f, 10.5f}));
            break;
        }
        case PropKind::Grave:
            out.push_back(box(p.x - p.width * 0.5f, 0, p.x + p.width * 0.5f, p.height));
            break;
        case PropKind::Fence:
            break;  // low and to one side: only for looks
    }
}

float clearance(const Level& l, Vec2 p, float reach) {
    float best = p.y;  // the ground
    const float lo = p.x - reach - 16.0f, hi = p.x + reach;
    auto it = std::lower_bound(l.shapes.begin(), l.shapes.end(), lo, [](const Shape& s, float x) {
        float x0, x1;
        spanOf(s, x0, x1);
        return x0 < x;
    });
    for (size_t i = static_cast<size_t>(it - l.shapes.begin()); i < l.shapes.size(); ++i) {
        float x0, x1;
        spanOf(l.shapes[i], x0, x1);
        if (x0 > hi) break;
        if (l.shapeX1[i] < p.x - reach) continue;
        best = std::min(best, distanceTo(l.shapes[i], p));
    }
    return best;
}

bool inBelfry(const Level& l, Vec2 p) {
    return p.x > l.towerX - l.belfryHalf - 0.2f && p.x < l.towerX + l.belfryHalf && p.y > l.belfryY0 && p.y < l.belfryY1;
}

float skyline(const Level& l, float x0, float x1) {
    float top = 0;
    for (size_t i = 0; i < l.shapes.size(); ++i) {
        float a, b;
        spanOf(l.shapes[i], a, b);
        if (b < x0 || a > x1) continue;
        top = std::max(top, topOf(l.shapes[i]));
    }
    return top;
}

Level makeLevel(int night) {
    night = std::max(0, std::min(kNights - 1, night));
    const NightPlan& plan = kPlans[night];
    Rng r(0xB47u * 2654435761u + static_cast<uint32_t>(night) * 0x632BE5ABu);
    Level l;
    l.night = night;
    l.towerX = plan.length;
    l.belfryY0 = 12.5f;
    l.belfryY1 = 15.5f;
    l.belfryHalf = 2.0f;
    l.dawnSeconds = plan.dawn;
    l.startBats = 8;

    // The edge of the woods: a fence and two oaks the flock starts beside.
    {
        Prop f;
        f.kind = PropKind::Fence;
        f.x = 4;
        f.width = 8;
        l.props.push_back(f);
    }
    // The village, from the woods to the churchyard.
    float x = 16.0f;
    const float villageEnd = l.towerX - 24.0f;
    int lastKind = -1;
    while (x < villageEnd) {
        Prop p;
        const float roll = r.unit();
        int kind = roll < 0.45f ? 0 : (roll < 0.7f ? 1 : 2);
        if (kind == lastKind && kind != 0 && r.unit() < 0.5f) kind = 0;
        lastKind = kind;
        if (kind == 0) {
            p.kind = PropKind::House;
            p.width = r.between(5.0f, 8.0f);
            p.height = r.between(4.2f, 6.5f);
            p.roof = r.between(2.4f, 3.6f);
            p.x = x + p.width * 0.5f;
            if (r.unit() < 0.85f) {
                p.chimneyX = p.x + (r.unit() < 0.5f ? -1.0f : 1.0f) * p.width * 0.25f;
                p.chimney = p.height + p.roof + r.between(0.6f, plan.chimneyExtra);
            }
            p.variant = r.below(4);
            x += p.width;
        } else if (kind == 1) {
            p.kind = PropKind::Oak;
            p.width = r.between(2.3f, 3.3f);
            p.height = r.between(3.0f, 4.0f + 0.4f * static_cast<float>(night));
            p.x = x + p.width;
            p.variant = r.below(3);
            x += 2 * p.width;
        } else {
            p.kind = PropKind::Pine;
            p.width = r.between(3.4f, 4.6f);
            p.height = r.between(8.0f, plan.pineMax);
            p.x = x + p.width * 0.5f;
            p.variant = r.below(3);
            x += p.width;
        }
        l.props.push_back(p);
        x += r.between(plan.gapMin, plan.gapMax);
    }
    // The churchyard: low stones, and the church.
    for (float g = l.towerX - 20.0f; g < l.towerX - 4.0f; g += r.between(2.2f, 3.6f)) {
        Prop p;
        p.kind = PropKind::Grave;
        p.x = g;
        p.width = r.between(0.6f, 1.0f);
        p.height = r.between(0.8f, 1.5f);
        p.variant = r.below(3);
        l.props.push_back(p);
    }
    {
        Prop c;
        c.kind = PropKind::Church;
        c.x = l.towerX;
        l.props.push_back(c);
    }

    for (const Prop& p : l.props) shapesOf(p, l, l.shapes);
    std::sort(l.shapes.begin(), l.shapes.end(), [](const Shape& a, const Shape& b) {
        float a0, a1, b0, b1;
        spanOf(a, a0, a1);
        spanOf(b, b0, b1);
        return a0 < b0;
    });
    for (const Shape& s : l.shapes) {
        float x0, x1;
        spanOf(s, x0, x1);
        l.shapeX1.push_back(x1);
    }

    // Owls, in the open air between rooftops and the cloud, never two close together.
    float lastOwl = 20.0f;
    int tries = 0;
    while (static_cast<int>(l.owls.size()) < plan.owls && tries++ < 400) {
        const float span = (l.towerX - 36.0f) - 34.0f;
        const float ox = 34.0f + span * (static_cast<float>(l.owls.size()) + r.between(0.2f, 0.8f)) / static_cast<float>(plan.owls);
        if (ox - lastOwl < 18.0f) continue;
        const float floor = skyline(l, ox - 3.0f, ox + 3.0f);
        const float room = kCeiling - floor;
        if (room < 7.0f) continue;
        OwlPath o;
        const bool across = night >= 2 && room >= 11.0f && r.unit() < 0.5f;
        o.centre = {ox, floor + room * 0.5f};
        if (across) {
            o.centre.y = floor + room * 0.72f;
            o.ax = r.between(4.0f, 6.0f);
            o.ay = 0.6f;
        } else {
            o.ax = 0.8f;
            o.ay = std::max(1.0f, room * 0.5f - 2.4f);
        }
        o.speed = 0.75f + 0.12f * static_cast<float>(night) + r.between(0.0f, 0.15f);
        o.phase = r.between(0.0f, 2 * kPi);
        // The whole path must be in the open.
        bool clear = true;
        for (int k = 0; k < 24 && clear; ++k) {
            OwlPath probe = o;
            const Vec2 at = owlAt(probe, static_cast<float>(k) * 2 * kPi / 24.0f / o.speed);
            clear = clearance(l, at, 3.0f) > 1.0f && at.y < kCeiling - 0.6f;
        }
        if (!clear) continue;
        l.owls.push_back(o);
        lastOwl = ox;
    }

    // Stray bats, hanging under eaves and in the trees, in little groups.
    const int room = 32 - l.startBats;
    int strays = 0;
    std::vector<float> used;
    for (int gi = 0; gi < plan.groups * 6 && static_cast<int>(used.size()) < plan.groups; ++gi) {
        const Prop& p = l.props[static_cast<size_t>(1 + r.below(static_cast<int>(l.props.size()) - 1))];
        if (p.x < 24.0f || p.x > l.towerX - 26.0f) continue;
        bool near = false;
        for (float u : used) near = near || std::fabs(u - p.x) < 14.0f;
        if (near) continue;
        Vec2 at;
        if (p.kind == PropKind::House) {
            const float side = r.unit() < 0.5f ? -1.0f : 1.0f;
            at = {p.x + side * (p.width * 0.5f + 1.1f), p.height - 0.2f};
        } else if (p.kind == PropKind::Oak) {
            at = {p.x + (r.unit() < 0.5f ? -1.0f : 1.0f) * p.width * 0.9f, p.height + 0.1f};
        } else if (p.kind == PropKind::Pine) {
            at = {p.x, p.height + 1.4f};
        } else {
            continue;
        }
        // Out of the solids, with room to fly in.
        for (int k = 0; k < 40 && clearance(l, at) < 1.0f; ++k) at.y += 0.25f;
        if (at.y > kCeiling - 1.5f || clearance(l, at) < 1.0f) continue;
        used.push_back(p.x);
        const int size = 1 + r.below(3);
        for (int k = 0; k < size && strays < room; ++k) {
            Vec2 b{at.x + 0.55f * static_cast<float>(k) - 0.5f, at.y + 0.15f * static_cast<float>(k % 2)};
            for (int j = 0; j < 40 && clearance(l, b) < 1.0f; ++j) b.y += 0.25f;
            if (clearance(l, b) < 1.0f || b.y > kCeiling - 1.5f) continue;
            l.perches.push_back({b});
            ++strays;
        }
    }
    return l;
}

}  // namespace belfry
