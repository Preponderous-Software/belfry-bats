#include "play/view.h"

#include <algorithm>
#include <cmath>

namespace belfry::play {

namespace {

constexpr float kFovY = 40.0f;  // degrees
// The morning after Halloween, in a New England hill village (42.4 N): the
// sky goes from deep night toward sunrise as the night's timer runs out.
constexpr double kLatitude = 42.4;
constexpr double kLongitude = -72.5;
constexpr double kMidnightUtc = 1793419200.0 + 86400.0;  // 2026-11-01 00:00 at UTC-4
constexpr double kFirstMinute = 5 * 60 + 40;  // 5:40 am: deep blue
constexpr double kLastMinute = 7 * 60 + 2;    // 7:02 am: first light, the sun nearly up

struct Mat {
    float m[16];
};

Mat identity() {
    Mat r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

Mat mul(const Mat& a, const Mat& b) {
    Mat r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

Mat translate(float x, float y, float z) {
    Mat r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat rotX(float a) {
    Mat r = identity();
    r.m[5] = std::cos(a);
    r.m[6] = std::sin(a);
    r.m[9] = -std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

Mat rotY(float a) {
    Mat r = identity();
    r.m[0] = std::cos(a);
    r.m[2] = -std::sin(a);
    r.m[8] = std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

Mat rotZ(float a) {
    Mat r = identity();
    r.m[0] = std::cos(a);
    r.m[1] = std::sin(a);
    r.m[4] = -std::sin(a);
    r.m[5] = std::cos(a);
    return r;
}

Mat scale(float s) {
    Mat r = identity();
    r.m[0] = r.m[5] = r.m[10] = s;
    return r;
}

// At least 27 units of height, and 20 of width on a phone held upright (narrower would
// hide what is ahead; wider would make the bats specks).
float halfHeightFor(float aspect) { return std::max(13.5f, 10.0f / std::max(aspect, 0.2f)); }

StrataPoint point(float x, float y, float z, float px, float world, float r, float g, float b, float a, uint32_t flags) {
    StrataPoint p{};
    p.position[0] = x;
    p.position[1] = y;
    p.position[2] = z;
    p.size_pixels = px;
    p.world_diameter = world;
    p.color[0] = r;
    p.color[1] = g;
    p.color[2] = b;
    p.color[3] = a;
    p.flags = flags;
    return p;
}

float wrapAngle(float a) {
    while (a > kPi) a -= 2 * kPi;
    while (a < -kPi) a += 2 * kPi;
    return a;
}

}  // namespace

Scene::Scene(strata::Engine& engine) : engine_(engine) {
    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    light.cascade_count = 0;  // no shadows: smooth on a phone, and the night casts none to speak of
    light.latitude_degrees = static_cast<float>(kLatitude);
    light.fog = 0;  // the village is small and the hills are far: fog only made it murky
    engine_.setLighting(light);
    const MeshData body = batBody(), left = batWing(true), right = batWing(false);
    for (int i = 0; i < kMaxBats; ++i) {
        Body b;
        b.body = body.add(engine_);
        b.left = left.add(engine_);
        b.right = right.add(engine_);
        bats_.push_back(b);
    }
    const MeshData ob = owlBody(), ol = owlWing(true), orr = owlWing(false);
    for (int i = 0; i < 8; ++i) {
        Body b;
        b.body = ob.add(engine_);
        b.left = ol.add(engine_);
        b.right = orr.add(engine_);
        owls_.push_back(b);
    }
    Rng r(0x57A25u);
    for (int i = 0; i < 160; ++i) {
        const float x = r.between(-420, 420), y = r.between(10, 330), bright = r.between(0.4f, 1.0f);
        stars_.push_back(point(x, y, -420, r.between(1.5f, 3.0f), 0, bright, bright, bright * 0.95f + 0.05f, 1, STRATA_POINT_SOFT));
    }
}

void Scene::place(uint32_t id, const float model[16]) { engine_.setMeshTransform(id, model); }

void Scene::load(const World& w) {
    for (uint32_t id : village_) engine_.removeMesh(id);
    village_.clear();
    const Village v = buildVillage(w.level);
    village_.push_back(v.lit.add(engine_));
    if (!v.glow.empty()) village_.push_back(v.glow.add(engine_, true));
    windowLights_ = v.windowLights;
    lantern_ = v.lantern;
    night_ = w.night;
    follow(w, 0);
}

void Scene::follow(const World& w, float dt) {
    engine_.drawableSize(camera_.width, camera_.height);
    if (viewW_ > 0 && viewH_ > 0) {
        camera_.width = viewW_;
        camera_.height = viewH_;
    }
    const float aspect = camera_.height > 0 ? static_cast<float>(camera_.width) / static_cast<float>(camera_.height) : 1.6f;
    const float halfH = halfHeightFor(aspect);
    const float tanH = std::tan(kFovY * 0.5f * kPi / 180.0f);
    camera_.distance = halfH / tanH;
    const float halfW = halfH * aspect;
    float want = w.phase == Phase::Flying ? w.centre.x : kStartX + 4.0f;
    want = clampf(want, kStartX + std::min(4.0f, halfW * 0.4f), w.level.towerX + 2.0f);
    if (dt <= 0) camera_.x = want;
    else camera_.x += (want - camera_.x) * (1.0f - std::exp(-dt * 2.2f));
    // Ground near the bottom of a wide view; on a tall one, more ground and less empty sky.
    camera_.y = std::max(11.5f, std::min(halfH - 1.5f, 15.0f));
    const float eye[3] = {camera_.x, camera_.y + 3.0f, camera_.distance};
    const float target[3] = {camera_.x, camera_.y, 0};
    const float up[3] = {0, 1, 0};
    camera_.cam = strata::cameraLookAt(eye, target, up, kFovY, aspect, 0.5f, 900.0f);
}

Vec2 Scene::unproject(float nx, float ny) const {
    const float aspect = camera_.height > 0 ? static_cast<float>(camera_.width) / static_cast<float>(camera_.height) : 1.6f;
    const float tanH = std::tan(kFovY * 0.5f * kPi / 180.0f);
    // The camera's basis, from its eye and target.
    const float ex = camera_.x, ey = camera_.y + 3.0f, ez = camera_.distance;
    float fx = 0, fy = -3.0f, fz = -camera_.distance;
    const float fl = std::sqrt(fy * fy + fz * fz);
    fy /= fl;
    fz /= fl;
    // right = f x up = (-fz, 0, fx) normalised = (1, 0, 0); up' = right x f.
    const float rx = 1, ry = 0, rz = 0;
    const float ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;
    const float sx = (2 * nx - 1) * tanH * aspect, sy = (1 - 2 * ny) * tanH;
    const float dx = fx + rx * sx + ux * sy, dy = fy + ry * sx + uy * sy, dz = fz + rz * sx + uz * sy;
    if (std::fabs(dz) < 1e-5f) return {ex, ey};
    const float t = -ez / dz;
    return {ex + dx * t, ey + dy * t};
}

void Scene::draw(const World& w, float realSeconds, bool showGuide) {
    const float dawn = dawnProgress(w);
    // The sky: real sun positions before sunrise.
    const double minute = kFirstMinute + (kLastMinute - kFirstMinute) * static_cast<double>(dawn);
    const StrataSolarPosition sun = strata::solarPosition(kMidnightUtc + minute * 60.0, kLatitude, kLongitude);
    engine_.setSunDirection(sun.to_sun);
    engine_.setCamera(camera_.cam);
    const float night = 1.0f - dawn;
    const float halfW = camera_.distance * std::tan(kFovY * 0.5f * kPi / 180.0f) *
                        (camera_.height > 0 ? static_cast<float>(camera_.width) / static_cast<float>(camera_.height) : 1.6f);

    // Lights: the moon's wash over everything, the belfry's lantern, the nearest lit windows.
    std::vector<StrataPointLight> lights;
    auto light = [&](float x, float y, float z, float range, float r, float g, float b, float intensity) {
        StrataPointLight l{};
        l.position[0] = x;
        l.position[1] = y;
        l.position[2] = z;
        l.range = range;
        l.color[0] = r;
        l.color[1] = g;
        l.color[2] = b;
        l.intensity = intensity;
        lights.push_back(l);
    };
    // Moonlight: one broad wash from high in the west, and a softer fill from the front.
    // A row of soft lights above and in front of the village along the view,
    // so every roof and wall has an edge, as under a bright moon.
    for (int k = -3; k <= 3; ++k)
        light(camera_.x + static_cast<float>(k) * 14.0f, 26.0f, 22.0f, 48.0f, 0.55f, 0.62f, 1.0f, 1.3f);
    light(camera_.x - 30.0f, 60.0f, 45.0f, 160.0f, 0.55f, 0.62f, 1.0f, 1.2f);
    const float flicker = 0.9f + 0.1f * std::sin(realSeconds * 7.0f) * std::sin(realSeconds * 3.1f);
    light(lantern_[0], lantern_[1], 1.5f, 16.0f, 1.0f, 0.65f, 0.3f, 2.4f * flicker);
    std::vector<std::pair<float, size_t>> near;
    for (size_t i = 0; i < windowLights_.size(); ++i) {
        const float d = std::fabs(windowLights_[i][0] - camera_.x);
        if (d < halfW + 10.0f) near.push_back({d, i});
    }
    std::sort(near.begin(), near.end());
    for (size_t k = 0; k < near.size() && lights.size() < 24; ++k) {
        const auto& p = windowLights_[near[k].second];
        light(p[0], p[1], p[2], 6.5f, 1.0f, 0.6f, 0.25f, 1.1f);
    }
    engine_.setPointLights(lights.data(), lights.size());

    // The bats.
    for (size_t i = 0; i < bats_.size(); ++i) {
        Body& m = bats_[i];
        if (i >= w.bats.size() || w.bats[i].state == BatState::Home || w.bats[i].state == BatState::Gone ||
            w.phase == Phase::Title) {
            const Mat away = translate(0, -500, 0);
            place(m.body, away.m);
            place(m.left, away.m);
            place(m.right, away.m);
            continue;
        }
        const Bat& b = w.bats[i];
        const float t = realSeconds + b.phase;
        float flap = 0.25f + 0.75f * std::sin(t * 14.0f);
        float pitch = clampf(std::atan2(b.v.y, std::fabs(b.v.x) + 0.5f) * 0.6f, -0.6f, 0.6f);
        float upside = 0;
        float size = b.stray ? 1.3f : 1.45f;  // drawn larger than they collide, to read on a phone
        if (b.state == BatState::Flock || b.state == BatState::Scared) {
            const float want = b.v.x >= -0.3f ? 0.0f : kPi;
            m.yaw = wrapAngle(m.yaw + wrapAngle(want - m.yaw) * 0.15f);
            if (b.state == BatState::Scared) flap = 0.2f + 0.9f * std::sin(t * 22.0f);
        } else if (b.state == BatState::Perched) {
            upside = kPi;  // hanging by its feet
            flap = -1.2f + 0.08f * std::sin(t * 2.0f);
            pitch = 0;
        } else if (b.state == BatState::Adrift) {
            flap = 0.2f + 0.7f * std::sin(t * 11.0f);
            pitch = 0;
        } else if (b.state == BatState::Dazed) {
            m.roll += 0.35f;
            flap = 0.3f;
        }
        if (b.state != BatState::Dazed) m.roll *= 0.8f;
        const Mat base = mul(mul(mul(mul(translate(b.p.x, b.p.y, b.depth), rotX(0.4f)), rotY(m.yaw)),
                                 mul(rotZ(pitch + upside), rotX(m.roll))),
                             scale(size));
        place(m.body, base.m);
        const Mat r = mul(base, rotX(-flap)), l = mul(base, rotX(flap));
        place(m.right, r.m);
        place(m.left, l.m);
    }
    // The owls.
    for (size_t i = 0; i < owls_.size(); ++i) {
        Body& m = owls_[i];
        if (i >= w.level.owls.size()) {
            const Mat away = translate(0, -500, 0);
            place(m.body, away.m);
            place(m.left, away.m);
            place(m.right, away.m);
            continue;
        }
        const Vec2 o = owlPosition(w, i);
        const float t = realSeconds * 4.0f + static_cast<float>(i);
        const float flap = 0.15f + 0.45f * std::sin(t);
        const Mat base = mul(translate(o.x, o.y, 0.3f), rotY(0.15f * std::sin(realSeconds * 0.7f + static_cast<float>(i))));
        place(m.body, base.m);
        const Mat r = mul(mul(base, translate(0.42f, 0.3f, -0.1f)), rotZ(flap));
        const Mat l = mul(mul(base, translate(-0.42f, 0.3f, -0.1f)), rotZ(-flap));
        place(m.right, r.m);
        place(m.left, l.m);
    }

    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    fd.clear_color[0] = 0.01f;
    fd.clear_color[1] = 0.012f;
    fd.clear_color[2] = 0.03f;
    fd.clear_color[3] = 1;
    engine_.beginFrame(fd);

    std::vector<StrataPoint> pts;
    const float starAlpha = clampf(1.0f - dawn * 1.25f, 0, 1);
    if (starAlpha > 0.01f)
        for (StrataPoint s : stars_) {
            s.position[0] += camera_.x * 0.9f;  // far away: they barely move
            s.color[3] = starAlpha;
            pts.push_back(s);
        }
    // The moon, setting in the west.
    pts.push_back(point(camera_.x * 0.9f - 140.0f, 95.0f - 40.0f * dawn, -400, 6, 16, 0.95f, 0.92f, 0.82f, 1, 0));
    pts.push_back(point(camera_.x * 0.9f - 140.0f, 95.0f - 40.0f * dawn, -401, 6, 40, 0.5f, 0.55f, 0.7f, 0.25f * night, STRATA_POINT_SOFT));
    // The belfry's warm glow: the way home.
    const float pulse = 0.75f + 0.25f * std::sin(realSeconds * 2.0f);
    pts.push_back(point(lantern_[0], lantern_[1], 2.6f, 8, 7.0f, 1.0f, 0.6f, 0.25f, 0.35f * pulse, STRATA_POINT_SOFT));
    // Owls' eyes.
    for (size_t i = 0; i < w.level.owls.size() && i < owls_.size(); ++i) {
        const Vec2 o = owlPosition(w, i);
        for (int s = -1; s <= 1; s += 2)
            pts.push_back(point(o.x + 0.17f * static_cast<float>(s), o.y + 0.86f, 0.75f, 4, 0.2f, 1.0f, 0.82f, 0.15f, 1, 0));
    }
    // Strays shimmer a little, so they can be found.
    if (w.phase == Phase::Flying)
        for (const Bat& b : w.bats)
            if (b.state == BatState::Perched || b.state == BatState::Adrift) {
                const float a = 0.12f + 0.08f * std::sin(realSeconds * 3.0f + b.phase);
                pts.push_back(point(b.p.x, b.p.y, b.depth, 10, 1.8f, 0.7f, 0.8f, 1.0f, a, STRATA_POINT_SOFT));
            }
    // Where the flock is headed.
    if (showGuide && w.phase == Phase::Flying)
        pts.push_back(point(w.guide.x, w.guide.y, 0, 10, 0.9f, 0.75f, 0.85f, 1.0f, 0.45f + 0.15f * std::sin(realSeconds * 5.0f),
                            STRATA_POINT_SOFT));
    engine_.submitPoints(pts.data(), pts.size());
    engine_.endFrame();
}

}  // namespace belfry::play
