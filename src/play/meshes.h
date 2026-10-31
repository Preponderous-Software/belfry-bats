// Geometry for Strata: flat-shaded boxes, prisms, cones and balls gathered
// into a few big meshes for the village, and the small meshes of a bat and
// an owl (a body and two wings each, moved every frame by a model matrix).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "sim/level.h"
#include "strata/strata.hpp"

namespace belfry::play {

struct Rgb {
    float r, g, b;
};

struct MeshData {
    std::vector<float> pos, nrm, col;
    std::vector<uint32_t> idx;

    void tri(const float a[3], const float b[3], const float c[3], Rgb k);
    // Both faces (thin things: wings, ears, flat cut-outs).
    void tri2(const float a[3], const float b[3], const float c[3], Rgb k);
    void quad(const float a[3], const float b[3], const float c[3], const float d[3], Rgb k);
    void box(float x0, float y0, float z0, float x1, float y1, float z1, Rgb k);
    // A roof: a triangular prism along z, the ridge at (x, y1).
    void prism(float x0, float x1, float y0, float y1, float z0, float z1, Rgb k, float ridgeX);
    // An n-sided cone or pyramid standing on y0, apex at y1.
    void cone(float x, float z, float radius, float y0, float y1, int sides, Rgb k);
    // A low-poly ball, radii (rx, ry, rz).
    void ball(float x, float y, float z, float rx, float ry, float rz, Rgb k, int slices = 8, int stacks = 5);
    bool empty() const { return idx.empty(); }
    uint32_t add(strata::Engine& e, bool unlit = false) const;
};

// The village's meshes for one night: lit (houses, trees, ground, church)
// and glowing (lit windows, the belfry's lantern light).
struct Village {
    MeshData lit, glow;
    std::vector<std::array<float, 3>> windowLights;  // warm lights at some lit windows
    std::array<float, 3> lantern{};                  // in the belfry
};

Village buildVillage(const Level& l);

// A bat: body (facing +x, wings out along +-z), one wing (the +z one; the
// other is its mirror).
MeshData batBody();
MeshData batWing(bool left);
// An owl, face toward +z (the camera): body and one wing (+x; mirrored for -x).
MeshData owlBody();
MeshData owlWing(bool left);

}  // namespace belfry::play
