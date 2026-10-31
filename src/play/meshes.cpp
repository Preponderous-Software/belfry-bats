#include "play/meshes.h"

#include <cmath>

namespace belfry::play {

namespace {

constexpr Rgb kGround{0.035f, 0.045f, 0.035f};
constexpr Rgb kHill{0.025f, 0.035f, 0.045f};
constexpr Rgb kWalls[4] = {{0.13f, 0.10f, 0.08f}, {0.09f, 0.09f, 0.12f}, {0.14f, 0.11f, 0.07f}, {0.10f, 0.10f, 0.09f}};
constexpr Rgb kRoofs[4] = {{0.07f, 0.04f, 0.05f}, {0.05f, 0.05f, 0.07f}, {0.09f, 0.05f, 0.03f}, {0.06f, 0.06f, 0.06f}};
constexpr Rgb kBrick{0.16f, 0.07f, 0.05f};
constexpr Rgb kBark{0.07f, 0.05f, 0.035f};
constexpr Rgb kLeaves[3] = {{0.05f, 0.08f, 0.04f}, {0.09f, 0.07f, 0.03f}, {0.10f, 0.05f, 0.03f}};  // autumn
constexpr Rgb kPine[3] = {{0.03f, 0.07f, 0.05f}, {0.03f, 0.06f, 0.04f}, {0.04f, 0.075f, 0.05f}};
constexpr Rgb kStone{0.17f, 0.16f, 0.17f};
constexpr Rgb kDarkStone{0.11f, 0.10f, 0.11f};
constexpr Rgb kSlate{0.06f, 0.06f, 0.08f};
constexpr Rgb kWindow{1.0f, 0.62f, 0.24f};
constexpr Rgb kDimWindow{0.30f, 0.16f, 0.07f};
constexpr Rgb kBronze{0.55f, 0.38f, 0.12f};
constexpr Rgb kFence{0.08f, 0.06f, 0.05f};

void push(MeshData& m, const float p[3], const float n[3], Rgb k) {
    for (int i = 0; i < 3; ++i) {
        m.pos.push_back(p[i]);
        m.nrm.push_back(n[i]);
    }
    m.col.push_back(k.r);
    m.col.push_back(k.g);
    m.col.push_back(k.b);
    m.idx.push_back(static_cast<uint32_t>(m.idx.size()));
}

// A small hash for which windows are lit.
uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Windows on the front of a wall (z = zf), a row of them.
void windows(Village& v, float x0, float x1, float y, float h, float zf, int n, uint32_t seed, bool lights) {
    const float span = x1 - x0;
    for (int i = 0; i < n; ++i) {
        const float cx = x0 + span * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const bool lit = hash(seed + static_cast<uint32_t>(i) * 97u) % 10u < 6u;
        const float w = 0.45f;
        const float a[3] = {cx - w, y, zf}, b[3] = {cx + w, y, zf}, c[3] = {cx + w, y + h, zf}, d[3] = {cx - w, y + h, zf};
        v.glow.quad(a, b, c, d, lit ? kWindow : kDimWindow);
        if (lit && lights) v.windowLights.push_back({cx, y + h * 0.5f, zf + 1.2f});
    }
}

void house(Village& v, float x, float w, float h, float roof, float chimney, float chimneyX, int variant, float zc, float depth,
           bool lights, uint32_t seed) {
    const float hw = w * 0.5f, z0 = zc - depth, z1 = zc + depth;
    v.lit.box(x - hw, 0, z0, x + hw, h, z1, kWalls[variant & 3]);
    v.lit.prism(x - hw - 0.4f, x + hw + 0.4f, h, h + roof, z0 - 0.3f, z1 + 0.3f, kRoofs[variant & 3], x);
    if (chimney > 0) v.lit.box(chimneyX - 0.45f, h, zc - 0.45f, chimneyX + 0.45f, chimney, zc + 0.45f, kBrick);
    const int n = std::max(1, static_cast<int>(w / 2.2f));
    windows(v, x - hw + 0.3f, x + hw - 0.3f, h * 0.55f, std::min(1.3f, h * 0.25f), z1 + 0.02f, n, seed, lights);
    if (h > 4.5f) windows(v, x - hw + 0.3f, x + hw - 0.3f, h * 0.15f, std::min(1.2f, h * 0.22f), z1 + 0.02f, std::max(1, n - 1),
                          seed + 13u, false);
}

}  // namespace

void MeshData::tri(const float a[3], const float b[3], const float c[3], Rgb k) {
    const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    float n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
    const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l < 1e-9f) return;
    for (float& x : n) x /= l;
    push(*this, a, n, k);
    push(*this, b, n, k);
    push(*this, c, n, k);
}

void MeshData::tri2(const float a[3], const float b[3], const float c[3], Rgb k) {
    tri(a, b, c, k);
    tri(a, c, b, k);
}

void MeshData::quad(const float a[3], const float b[3], const float c[3], const float d[3], Rgb k) {
    tri(a, b, c, k);
    tri(a, c, d, k);
}

void MeshData::box(float x0, float y0, float z0, float x1, float y1, float z1, Rgb k) {
    const float p[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                           {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    quad(p[4], p[5], p[6], p[7], k);  // front (+z)
    quad(p[1], p[0], p[3], p[2], k);  // back
    quad(p[0], p[4], p[7], p[3], k);  // left
    quad(p[5], p[1], p[2], p[6], k);  // right
    quad(p[7], p[6], p[2], p[3], k);  // top
    quad(p[0], p[1], p[5], p[4], k);  // bottom
}

void MeshData::prism(float x0, float x1, float y0, float y1, float z0, float z1, Rgb k, float ridgeX) {
    const float a0[3] = {x0, y0, z0}, b0[3] = {x1, y0, z0}, r0[3] = {ridgeX, y1, z0};
    const float a1[3] = {x0, y0, z1}, b1[3] = {x1, y0, z1}, r1[3] = {ridgeX, y1, z1};
    tri(a1, b1, r1, k);          // front gable
    tri(b0, a0, r0, k);          // back gable
    quad(a0, a1, r1, r0, k);     // left slope
    quad(b1, b0, r0, r1, k);     // right slope
    quad(a0, b0, b1, a1, k);     // underside
}

void MeshData::cone(float x, float z, float radius, float y0, float y1, int sides, Rgb k) {
    const float apex[3] = {x, y1, z}, centre[3] = {x, y0, z};
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2 * kPi * static_cast<float>(i) / static_cast<float>(sides) + 0.3f;
        const float a1 = 2 * kPi * static_cast<float>(i + 1) / static_cast<float>(sides) + 0.3f;
        const float p0[3] = {x + radius * std::cos(a0), y0, z + radius * std::sin(a0)};
        const float p1[3] = {x + radius * std::cos(a1), y0, z + radius * std::sin(a1)};
        tri(p1, p0, apex, k);
        tri(p0, p1, centre, k);
    }
}

void MeshData::ball(float x, float y, float z, float rx, float ry, float rz, Rgb k, int slices, int stacks) {
    auto at = [&](int i, int j, float out[3]) {
        const float th = kPi * static_cast<float>(j) / static_cast<float>(stacks);
        const float ph = 2 * kPi * static_cast<float>(i) / static_cast<float>(slices);
        out[0] = x + rx * std::sin(th) * std::cos(ph);
        out[1] = y + ry * std::cos(th);
        out[2] = z + rz * std::sin(th) * std::sin(ph);
    };
    for (int j = 0; j < stacks; ++j)
        for (int i = 0; i < slices; ++i) {
            float a[3], b[3], c[3], d[3];
            at(i, j, a);
            at(i + 1, j, b);
            at(i + 1, j + 1, c);
            at(i, j + 1, d);
            if (j > 0) tri(a, b, c, k);
            if (j < stacks - 1) tri(a, c, d, k);
        }
}

uint32_t MeshData::add(strata::Engine& e, bool unlit) const {
    // Lit colours are kept dark in this file (it is night) and lifted here,
    // so the moonlit village reads on a phone held at arm's length.
    std::vector<float> lifted;
    if (!unlit) {
        lifted = col;
        for (float& c : lifted) c = std::fmin(1.0f, c * 3.0f);
    }
    StrataTexturedMesh t{};
    t.mesh.positions = pos.data();
    t.mesh.normals = nrm.data();
    t.mesh.colors = unlit ? col.data() : lifted.data();
    t.mesh.indices = idx.data();
    t.mesh.vertex_count = static_cast<int32_t>(pos.size() / 3);
    t.mesh.index_count = static_cast<int32_t>(idx.size());
    t.mesh.color[0] = t.mesh.color[1] = t.mesh.color[2] = t.mesh.color[3] = 1.0f;
    t.flags = unlit ? STRATA_MESH_UNLIT : 0u;
    return e.addTexturedMesh(t);
}

Village buildVillage(const Level& l) {
    Village v;
    const float end = l.towerX + 60.0f;
    // The ground, and the hills far behind.
    v.lit.box(-60, -40, -90, end, 0, 40, kGround);  // deep and wide: no gap below it on a tall screen
    for (float x = -60; x < end; x += 34.0f) {
        const float h = 9.0f + 6.0f * std::sin(x * 0.07f) + 4.0f * std::sin(x * 0.023f + 1.0f);
        v.lit.ball(x, -2.0f, -78.0f, 26.0f, h + 4.0f, 10.0f, kHill, 10, 4);
    }
    // A row of houses behind, across a lane: only for looks.
    uint32_t seed = 17u + static_cast<uint32_t>(l.night) * 1000u;
    for (float x = -20; x < l.towerX + 20; x += 9.0f + static_cast<float>(hash(seed++) % 50u) / 10.0f) {
        const uint32_t hsh = hash(seed++);
        const float w = 5.0f + static_cast<float>(hsh % 30u) / 10.0f, h = 4.0f + static_cast<float>((hsh >> 8) % 25u) / 10.0f;
        house(v, x, w, h, 2.6f, h + 3.2f, x + w * 0.2f, static_cast<int>(hsh >> 16), -22.0f, 2.5f, false, hsh);
    }
    for (float x = -40; x < l.towerX + 40; x += 7.0f) {
        const uint32_t hsh = hash(seed++);
        v.lit.cone(x + static_cast<float>(hsh % 40u) / 10.0f, -40.0f - static_cast<float>(hsh % 7u), 2.6f, 0, 8.0f + static_cast<float>((hsh >> 4) % 60u) / 10.0f, 6,
                   kPine[hsh % 3u]);
    }
    // The woods the flock comes out of.
    for (int i = 0; i < 6; ++i) {
        const float x = -18.0f + static_cast<float>(i) * 3.2f;
        v.lit.box(x - 0.3f, 0, -2.3f, x + 0.3f, 2.0f, -1.7f, kBark);
        v.lit.cone(x, -2.0f, 2.0f, 1.2f, 12.0f + static_cast<float>(i % 3) * 2.0f, 6, kPine[i % 3]);
    }
    // The flight plane's village.
    for (const Prop& p : l.props) {
        switch (p.kind) {
            case PropKind::House:
                house(v, p.x, p.width, p.height, p.roof, p.chimney, p.chimneyX, p.variant, 0.0f, 3.0f, true,
                      static_cast<uint32_t>(p.x * 10.0f));
                break;
            case PropKind::Oak: {
                v.lit.box(p.x - 0.4f, 0, -0.4f, p.x + 0.4f, p.height + 0.6f, 0.4f, kBark);
                const Rgb k = kLeaves[p.variant % 3];
                const float cy = p.height + p.width * 0.7f;
                v.lit.ball(p.x, cy, 0, p.width, p.width * 0.95f, p.width * 0.9f, k, 9, 6);
                v.lit.ball(p.x - p.width * 0.5f, cy - p.width * 0.35f, 0.6f, p.width * 0.6f, p.width * 0.55f, p.width * 0.6f, k, 7, 4);
                break;
            }
            case PropKind::Pine: {
                v.lit.box(p.x - 0.3f, 0, -0.3f, p.x + 0.3f, 1.6f, 0.3f, kBark);
                const float span = p.height - 1.2f;
                for (int i = 0; i < 3; ++i) {
                    const float y0 = 1.2f + static_cast<float>(i) * span * 0.25f;
                    const float y1 = i == 2 ? p.height : y0 + span * 0.5f;
                    v.lit.cone(p.x, 0, p.width * 0.5f * (1.0f - 0.22f * static_cast<float>(i)), y0, y1, 7, kPine[p.variant % 3]);
                }
                break;
            }
            case PropKind::Grave:
                v.lit.box(p.x - p.width * 0.5f, 0, -0.2f, p.x + p.width * 0.5f, p.height, 0.2f, kDarkStone);
                v.lit.ball(p.x, p.height, 0, p.width * 0.5f, p.width * 0.35f, 0.2f, kDarkStone, 6, 3);
                break;
            case PropKind::Fence:
                for (float x = p.x - p.width * 0.5f; x <= p.x + p.width * 0.5f; x += 0.9f)
                    v.lit.box(x - 0.07f, 0, 2.4f, x + 0.07f, 1.1f, 2.55f, kFence);
                v.lit.box(p.x - p.width * 0.5f, 0.75f, 2.42f, p.x + p.width * 0.5f, 0.85f, 2.6f, kFence);
                break;
            case PropKind::Church: {
                const float tx = p.x, hw = l.belfryHalf, y0 = l.belfryY0, y1 = l.belfryY1;
                v.lit.box(tx - hw, 0, -hw, tx + hw, y0, hw, kStone);
                // The belfry: four corner posts, a floor, the bell and the lantern behind it.
                for (int cx = -1; cx <= 1; cx += 2)
                    for (int cz = -1; cz <= 1; cz += 2) {
                        const float px = tx + static_cast<float>(cx) * (hw - 0.25f), pz = static_cast<float>(cz) * (hw - 0.25f);
                        v.lit.box(px - 0.25f, y0, pz - 0.25f, px + 0.25f, y1, pz + 0.25f, kStone);
                    }
                v.lit.box(tx - hw + 0.3f, y0 - 0.1f, -hw + 0.3f, tx + hw - 0.3f, y0 + 0.1f, -hw + 0.4f, kDarkStone);
                v.lit.cone(tx, 0, 0.9f, y1 - 1.9f, y1 - 0.5f, 10, kBronze);
                v.lit.box(tx - 0.1f, y1 - 0.5f, -0.1f, tx + 0.1f, y1, 0.1f, kBark);
                {
                    const float a[3] = {tx - hw + 0.5f, y0 + 0.1f, -hw + 0.45f}, b[3] = {tx + hw - 0.5f, y0 + 0.1f, -hw + 0.45f},
                                c[3] = {tx + hw - 0.5f, y1, -hw + 0.45f}, d[3] = {tx - hw + 0.5f, y1, -hw + 0.45f};
                    v.glow.quad(a, b, c, d, {0.55f, 0.32f, 0.10f});
                }
                v.lantern = {tx, (y0 + y1) * 0.5f, 0.0f};
                v.lit.box(tx - hw - 0.3f, y1, -hw - 0.3f, tx + hw + 0.3f, y1 + 0.8f, hw + 0.3f, kDarkStone);
                v.lit.cone(tx, 0, hw * 1.25f, y1 + 0.8f, kCeiling + 6.0f, 4, kSlate);
                // The nave, beyond.
                v.lit.box(tx + hw, 0, -3.5f, tx + hw + 12.0f, 7.0f, 3.5f, kStone);
                v.lit.prism(tx + hw, tx + hw + 12.4f, 7.0f, 10.5f, -3.9f, 3.9f, kSlate, tx + hw + 6.0f);
                for (int i = 0; i < 3; ++i) {
                    const float wx = tx + hw + 2.5f + static_cast<float>(i) * 3.5f;
                    const float a[3] = {wx - 0.5f, 2.2f, 3.52f}, b[3] = {wx + 0.5f, 2.2f, 3.52f}, c[3] = {wx + 0.5f, 5.0f, 3.52f},
                                d[3] = {wx - 0.5f, 5.0f, 3.52f};
                    v.glow.quad(a, b, c, d, {0.35f, 0.22f, 0.12f});
                }
                // A slit window low in the tower.
                const float a[3] = {tx - 0.3f, 6.0f, hw + 0.02f}, b[3] = {tx + 0.3f, 6.0f, hw + 0.02f}, c[3] = {tx + 0.3f, 8.0f, hw + 0.02f},
                            d[3] = {tx - 0.3f, 8.0f, hw + 0.02f};
                v.glow.quad(a, b, c, d, kDimWindow);
                break;
            }
        }
    }
    return v;
}

MeshData batBody() {
    MeshData m;
    const Rgb fur{0.16f, 0.11f, 0.10f}, dark{0.08f, 0.06f, 0.06f};
    m.ball(0, 0, 0, 0.36f, 0.2f, 0.18f, fur, 7, 4);
    m.ball(0.3f, 0.06f, 0, 0.15f, 0.14f, 0.14f, fur, 6, 4);
    for (int s = -1; s <= 1; s += 2) {
        const float z = 0.07f * static_cast<float>(s);
        const float a[3] = {0.25f, 0.15f, z}, b[3] = {0.35f, 0.15f, z}, c[3] = {0.3f, 0.34f, z * 1.4f};
        m.tri2(a, b, c, dark);
    }
    return m;
}

MeshData batWing(bool left) {
    MeshData m;
    const Rgb k{0.11f, 0.08f, 0.09f};
    const float s = left ? -1.0f : 1.0f;
    // Root along the body, a long finger to the tip, and a scalloped trailing edge.
    const float pts[][2] = {{0.16f, 0.0f}, {0.22f, 0.42f}, {0.10f, 0.92f}, {-0.06f, 0.70f},
                            {-0.10f, 0.52f}, {-0.20f, 0.36f}, {-0.22f, 0.12f}, {-0.18f, 0.0f}};
    const int n = static_cast<int>(sizeof pts / sizeof pts[0]);
    const float root[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < n - 1; ++i) {
        const float a[3] = {pts[i][0], 0, s * pts[i][1]}, b[3] = {pts[i + 1][0], 0, s * pts[i + 1][1]};
        m.tri2(root, a, b, k);
    }
    return m;
}

MeshData owlBody() {
    MeshData m;
    const Rgb brown{0.32f, 0.22f, 0.13f}, pale{0.45f, 0.36f, 0.24f}, dark{0.15f, 0.10f, 0.06f};
    m.ball(0, 0, 0, 0.55f, 0.72f, 0.5f, brown, 9, 6);
    m.ball(0, 0.85f, 0.05f, 0.48f, 0.42f, 0.45f, brown, 9, 6);
    m.ball(0, 0.82f, 0.32f, 0.36f, 0.3f, 0.18f, pale, 8, 4);  // the face
    m.ball(0, -0.1f, 0.3f, 0.36f, 0.48f, 0.22f, pale, 8, 4);  // the breast
    for (int s = -1; s <= 1; s += 2) {
        const float x = 0.28f * static_cast<float>(s);
        const float a[3] = {x - 0.1f, 1.12f, 0.1f}, b[3] = {x + 0.1f, 1.12f, 0.1f}, c[3] = {x * 1.35f, 1.42f, 0.05f};
        m.tri2(a, b, c, dark);
    }
    const float b0[3] = {-0.07f, 0.74f, 0.5f}, b1[3] = {0.07f, 0.74f, 0.5f}, b2[3] = {0.0f, 0.6f, 0.52f};
    m.tri2(b0, b1, b2, {0.6f, 0.5f, 0.2f});
    return m;
}

MeshData owlWing(bool left) {
    MeshData m;
    const Rgb k{0.26f, 0.18f, 0.11f};
    const float s = left ? -1.0f : 1.0f;
    const float pts[][2] = {{0.0f, 0.35f}, {0.7f, 0.35f}, {1.35f, 0.1f}, {1.4f, -0.15f}, {1.0f, -0.25f}, {0.6f, -0.35f}, {0.0f, -0.3f}};
    const int n = static_cast<int>(sizeof pts / sizeof pts[0]);
    const float root[3] = {0, 0, 0};
    for (int i = 0; i < n - 1; ++i) {
        const float a[3] = {s * pts[i][0], pts[i][1], 0}, b[3] = {s * pts[i + 1][0], pts[i + 1][1], 0};
        m.tri2(root, a, b, k);
    }
    return m;
}

}  // namespace belfry::play
