// The nights: each one is the same every time, longer and trickier than the
// last, with strays and owls in open air and a clear way into the belfry.
#include <cstdio>

#include "check.h"
#include "sim/level.h"
#include "sim/world.h"

using namespace belfry;

int main() {
    float lastLength = 0;
    int lastOwls = -1;
    for (int n = 0; n < kNights; ++n) {
        const Level a = makeLevel(n), b = makeLevel(n);
        // The same night every time.
        CHECK(a.props.size() == b.props.size());
        CHECK(a.shapes.size() == b.shapes.size());
        CHECK(a.perches.size() == b.perches.size());
        for (size_t i = 0; i < a.props.size() && i < b.props.size(); ++i) CHECK(a.props[i].x == b.props[i].x);
        // Longer, with more owls.
        CHECK(a.towerX > lastLength);
        CHECK(static_cast<int>(a.owls.size()) >= lastOwls);
        lastLength = a.towerX;
        lastOwls = static_cast<int>(a.owls.size());
        CHECK(a.owls.size() >= 1);
        CHECK(a.dawnSeconds > a.towerX / kMaxSpeed * 1.5f);  // time to spare at full speed
        // Strays to gather, but never more bats than a phone can draw.
        CHECK(!a.perches.empty());
        CHECK(a.startBats + static_cast<int>(a.perches.size()) <= kMaxBats);
        for (const Perch& p : a.perches) {
            CHECK(clearance(a, p.at) >= 0.8f);
            CHECK(p.at.y < kCeiling - 1.0f);
            CHECK(p.at.x > 20.0f && p.at.x < a.towerX - 20.0f);
        }
        // Owls glide in the open.
        for (const OwlPath& o : a.owls)
            for (int k = 0; k < 48; ++k) {
                const Vec2 at = owlAt(o, static_cast<float>(k) * 0.25f);
                CHECK(clearance(a, at, 3.0f) > 0.9f);
            }
        // The start is clear, and the belfry is open on the side the flock comes from.
        CHECK(clearance(a, {kStartX, kStartY}) > 3.0f);
        const Vec2 door{a.towerX - a.belfryHalf - 1.0f, (a.belfryY0 + a.belfryY1) * 0.5f};
        CHECK(clearance(a, door) > 0.9f);
        CHECK(inBelfry(a, {a.towerX - 0.5f, door.y}));
        CHECK(!inBelfry(a, {a.towerX - 0.5f, a.belfryY0 - 1.0f}));
        // Nothing in the churchyard stands higher than the belfry's sill.
        CHECK(skyline(a, a.towerX - 22.0f, a.towerX - a.belfryHalf - 0.5f) < a.belfryY0 - 3.0f);
        // Nothing reaches the cloud: there is always a way over.
        for (float x = 10; x < a.towerX - 10; x += 0.5f) CHECK(skyline(a, x, x + 0.5f) < kCeiling - 3.0f);
        std::printf("night %d: %.0f long, %zu props, %zu owls, %zu strays, dawn at %.0f s\n", n + 1,
                    static_cast<double>(a.towerX), a.props.size(), a.owls.size(), a.perches.size(),
                    static_cast<double>(a.dawnSeconds));
    }
    // Distances: inside is negative.
    {
        Level l = makeLevel(0);
        Shape box;
        box.kind = Shape::Box;
        box.a = {0, 0};
        box.b = {2, 2};
        CHECK(distanceTo(box, {1, 1}) < 0);
        CHECK(std::fabs(distanceTo(box, {4, 1}) - 2.0f) < 1e-5f);
        Shape tri;
        tri.kind = Shape::Triangle;
        tri.a = {0, 0};
        tri.b = {4, 0};
        tri.c = {2, 3};
        CHECK(distanceTo(tri, {2, 1}) < 0);
        CHECK(distanceTo(tri, {2, 4}) > 0.9f);
        Shape disc;
        disc.kind = Shape::Disc;
        disc.a = {0, 0};
        disc.r = 1;
        CHECK(std::fabs(distanceTo(disc, {3, 0}) - 2.0f) < 1e-5f);
        CHECK(clearance(l, {kStartX, 0.5f}) <= 0.5f);  // the ground counts
    }
    TEST_MAIN_END();
}
