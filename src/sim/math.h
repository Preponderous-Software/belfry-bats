// Small 2D maths for the flight plane, and a deterministic random source.
//
// The bats fly in the x-y plane (x toward the bell tower, y up). Depth (z) is
// only for looks and never changes what happens.
#pragma once

#include <cmath>
#include <cstdint>

namespace belfry {

constexpr float kPi = 3.14159265358979f;

struct Vec2 {
    float x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float k) { return {a.x * k, a.y * k}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
inline float length2(Vec2 a) { return a.x * a.x + a.y * a.y; }
inline Vec2 normalized(Vec2 a) {
    const float l = length(a);
    return l > 1e-6f ? Vec2{a.x / l, a.y / l} : Vec2{0, 0};
}
inline Vec2 clampLength(Vec2 a, float max) {
    const float l = length(a);
    return l > max && l > 0 ? a * (max / l) : a;
}
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// xorshift32: the same numbers on every compiler.
struct Rng {
    uint32_t s = 0x9E3779B9u;
    explicit Rng(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }  // 0..1
    float between(float a, float b) { return a + (b - a) * unit(); }
    int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

}  // namespace belfry
