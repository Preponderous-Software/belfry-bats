// One night's village: the houses, trees and chimneys between the woods and
// the church, the owls on watch, the stray bats waiting to be gathered, and
// the bell tower at the far end. Every night is made from a fixed seed, so a
// night is the same for everyone who plays it.
#pragma once

#include <vector>

#include "sim/math.h"

namespace belfry {

constexpr int kNights = 5;
constexpr float kCeiling = 21.0f;   // the low cloud: nothing flies higher
constexpr float kStartX = 6.0f;     // where the flock leaves the woods
constexpr float kStartY = 9.0f;

// What a prop is, for drawing; its collision shapes come from shapesOf().
enum class PropKind : int { House, Oak, Pine, Church, Grave, Fence };

struct Prop {
    PropKind kind = PropKind::House;
    float x = 0;        // centre (the tower's centre for the church)
    float width = 0;    // walls (house), crown radius (oak), base width (pine)
    float height = 0;   // walls (house), trunk (oak), whole tree (pine)
    float roof = 0;     // house roof rise
    float chimney = 0;  // house chimney top above the ground; 0 for none
    float chimneyX = 0; // chimney centre
    int variant = 0;    // colours and windows
};

// Solid shapes in the flight plane.
struct Shape {
    enum Kind : int { Box, Disc, Triangle } kind = Box;
    // Box: (a.x, a.y)-(b.x, b.y). Disc: centre a, radius r. Triangle: a b c,
    // counter-clockwise.
    Vec2 a, b, c;
    float r = 0;
};

// Signed distance from p to the shape (negative inside).
float distanceTo(const Shape& s, Vec2 p);
// The shape's horizontal extent.
void spanOf(const Shape& s, float& x0, float& x1);

// An owl, gliding back and forth along a path: centre + (ax sin, ay sin).
struct OwlPath {
    Vec2 centre;
    float ax = 0, ay = 0;
    float speed = 1;  // radians a second
    float phase = 0;
};

Vec2 owlAt(const OwlPath& o, float seconds);

// A stray bat hanging from something, waiting for the flock.
struct Perch {
    Vec2 at;
};

struct Level {
    int night = 0;                 // 0-based
    float towerX = 0;              // the bell tower's centre
    float belfryY0 = 0, belfryY1 = 0;  // the open belfry, the way home
    float belfryHalf = 0;          // half its width
    float dawnSeconds = 0;         // the night's length: the sky is light at the end
    int startBats = 0;
    std::vector<Prop> props;
    std::vector<Shape> shapes;     // every prop's solids, sorted by left edge
    std::vector<float> shapeX1;    // each shape's right edge
    std::vector<OwlPath> owls;
    std::vector<Perch> perches;
};

// Night `night` (0-based, < kNights).
Level makeLevel(int night);

// The solids of one prop.
void shapesOf(const Prop& p, const Level& l, std::vector<Shape>& out);

// The distance from p to the nearest solid (the ground included), looking
// only at shapes within `reach` of p horizontally. Large when far from all.
float clearance(const Level& l, Vec2 p, float reach = 4.0f);

// True if p is inside the belfry's opening.
bool inBelfry(const Level& l, Vec2 p);

// The highest solid point over [x0, x1] (the ground is 0).
float skyline(const Level& l, float x0, float x1);

}  // namespace belfry
