#include "sim/world.h"

#include <cstring>

namespace belfry {

namespace {

void emit(World& w, Event e) { w.events.push_back(e); }

Vec2 gradient(const Level& l, Vec2 p) {
    const float e = 0.05f;
    const float gx = clearance(l, {p.x + e, p.y}) - clearance(l, {p.x - e, p.y});
    const float gy = clearance(l, {p.x, p.y + e}) - clearance(l, {p.x, p.y - e});
    return normalized({gx, gy});
}

void hover(Bat& b, float t) {
    const Vec2 to = b.anchor + Vec2{0.3f * std::sin(t * 1.3f + b.phase), 0.2f * std::cos(t * 1.7f + b.phase * 2)};
    b.v = (to - b.p) * 3.0f;
    b.p += b.v * kDt;
}

void flockStep(World& w, Bat& b, size_t self, float t) {
    const Level& l = w.level;
    // Toward the guide, each bat to its own place around it.
    const Vec2 seek = w.guide + b.offset * (1.0f + 0.3f * std::sin(t * 0.7f + b.phase));
    const Vec2 d = seek - b.p;
    const float dist = length(d);
    const Vec2 desired = normalized(d) * (kMaxSpeed * std::fmin(1.0f, dist / 2.5f));
    Vec2 steer = (desired - b.v) * 2.5f;
    // Boids: keep apart, fly alike, stay together.
    Vec2 avgV{0, 0};
    int near = 0;
    for (size_t j = 0; j < w.bats.size(); ++j) {
        const Bat& o = w.bats[j];
        if (j == self || o.state != BatState::Flock) continue;
        const Vec2 dv = b.p - o.p;
        const float d2 = length2(dv);
        if (d2 < 0.81f && d2 > 1e-8f) steer += normalized(dv) * ((0.9f - std::sqrt(d2)) * 14.0f);
        if (d2 < 6.25f) {
            avgV += o.v;
            ++near;
        }
    }
    if (near) steer += (avgV * (1.0f / static_cast<float>(near)) - b.v) * 0.5f;
    steer += (w.centre - b.p) * 0.3f;
    steer += Vec2{std::sin(t * 2.1f + b.phase * 3), std::cos(t * 1.7f + b.phase * 5)} * 2.0f;
    // Look ahead and veer from roofs, trees and chimneys.
    const Vec2 probe = b.p + b.v * 0.3f;
    const float c = clearance(l, probe);
    if (c < 1.0f) {
        const Vec2 n = gradient(l, probe);
        steer += n * ((1.0f - c) * 30.0f);
        const float into = dot(b.v, n);
        if (into < 0) steer += n * (-into * 3.0f);
    }
    if (b.p.y > kCeiling - 1.0f) steer.y -= (b.p.y - (kCeiling - 1.0f)) * 20.0f;
    if (b.p.y < 1.2f) steer.y += (1.2f - b.p.y) * 15.0f;
    steer = clampLength(steer, 45.0f);
    b.v += steer * kDt;
    const float speed = length(b.v);
    if (speed > kMaxSpeed * 1.2f) b.v = b.v * (kMaxSpeed * 1.2f / speed);
    else if (speed < 1.0f) b.v = (speed > 1e-4f ? normalized(b.v) : Vec2{1, 0});
    b.p += b.v * kDt;
    if (b.p.y > kCeiling) {
        b.p.y = kCeiling;
        if (b.v.y > 0) b.v.y = 0;
    }
}

void endNight(World& w, Ending e) {
    w.ending = e;
    w.nightHome[static_cast<size_t>(w.night)] = w.home;
    w.score += w.home;
    w.phase = w.night + 1 >= kNights ? Phase::Finished : Phase::NightOver;
    emit(w, Event::NightOver);
}

}  // namespace

World newGame() {
    World w;
    w.level = makeLevel(0);
    w.phase = Phase::Title;
    return w;
}

void startNight(World& w, int night) {
    w.night = night < 0 ? 0 : (night >= kNights ? kNights - 1 : night);
    w.level = makeLevel(w.night);
    w.phase = Phase::Flying;
    w.ending = Ending::None;
    w.tick = 0;
    w.home = w.lost = w.gathered = 0;
    w.dawnWarned = false;
    w.bats.clear();
    w.owlCooldown.assign(w.level.owls.size(), 0.0f);
    Rng r(w.seed + static_cast<uint32_t>(w.night) * 7919u);
    w.guide = {kStartX, kStartY};
    for (int i = 0; i < w.level.startBats; ++i) {
        Bat b;
        const float a = r.between(0, 2 * kPi), rad = 1.6f * std::sqrt(r.unit());
        b.offset = {std::cos(a) * rad, std::sin(a) * rad * 0.8f};
        b.p = w.guide + b.offset + Vec2{-3.0f, 0.0f};
        b.v = {2.0f + r.unit(), r.between(-0.5f, 0.5f)};
        b.phase = r.between(0, 2 * kPi);
        b.depth = r.between(-1.2f, 1.2f);
        w.bats.push_back(b);
    }
    for (const Perch& p : w.level.perches) {
        if (static_cast<int>(w.bats.size()) >= kMaxBats) break;
        Bat b;
        b.state = BatState::Perched;
        b.stray = true;
        b.p = b.anchor = p.at;
        const float a = r.between(0, 2 * kPi), rad = 1.6f * std::sqrt(r.unit());
        b.offset = {std::cos(a) * rad, std::sin(a) * rad * 0.8f};
        b.phase = r.between(0, 2 * kPi);
        b.depth = r.between(-1.2f, 1.2f);
        w.bats.push_back(b);
    }
    w.centre = w.guide;
    w.nightOf[static_cast<size_t>(w.night)] = static_cast<int>(w.bats.size());
    w.events.clear();
    emit(w, Event::NightStart);
}

float seconds(const World& w) { return static_cast<float>(w.tick) * kDt; }

float dawnProgress(const World& w) {
    if (w.phase == Phase::Title) return 0;
    return clampf(seconds(w) / w.level.dawnSeconds, 0, 1);
}

Vec2 owlPosition(const World& w, size_t i) { return owlAt(w.level.owls[i], seconds(w)); }

int count(const World& w, BatState s) {
    int n = 0;
    for (const Bat& b : w.bats) n += b.state == s ? 1 : 0;
    return n;
}

int flying(const World& w) { return count(w, BatState::Flock) + count(w, BatState::Dazed); }

void step(World& w, const Input& in) {
    w.events.clear();
    if (w.phase != Phase::Flying) return;
    const Level& l = w.level;
    if (in.steering)
        w.guide = {clampf(in.guide.x, -2.0f, l.towerX + 3.0f), clampf(in.guide.y, 0.8f, kCeiling - 0.5f)};
    const float t = seconds(w);

    // The flock's centre.
    Vec2 sum{0, 0};
    int n = 0;
    for (const Bat& b : w.bats)
        if (b.state == BatState::Flock) {
            sum += b.p;
            ++n;
        }
    if (n) w.centre = sum * (1.0f / static_cast<float>(n));

    std::vector<Vec2> owls(l.owls.size());
    for (size_t i = 0; i < owls.size(); ++i) {
        owls[i] = owlAt(l.owls[i], t);
        w.owlCooldown[i] = std::fmax(0.0f, w.owlCooldown[i] - kDt);
        if (n && w.owlCooldown[i] <= 0 && length(owls[i] - w.centre) < 7.0f) {
            w.owlCooldown[i] = 6.0f;
            emit(w, Event::OwlNear);
        }
    }

    for (size_t i = 0; i < w.bats.size(); ++i) {
        Bat& b = w.bats[i];
        switch (b.state) {
            case BatState::Flock: {
                flockStep(w, b, i, t);
                if (inBelfry(l, b.p)) {
                    b.state = BatState::Home;
                    ++w.home;
                    emit(w, w.home == 1 ? Event::FirstHome : Event::Home);
                    break;
                }
                if (clearance(l, b.p, 2.0f) < 0.28f) {
                    b.state = BatState::Dazed;
                    b.timer = 0.9f;
                    b.v = b.v * -0.25f + Vec2{0, -1.0f};
                    emit(w, Event::Bumped);
                    break;
                }
                bool scared = false;
                for (size_t k = 0; k < owls.size() && !scared; ++k)
                    if (length(b.p - owls[k]) < kOwlRadius) {
                        b.state = BatState::Scared;
                        b.timer = 2.5f;
                        b.v = normalized(b.p - owls[k]) * 9.0f + Vec2{0, 3.0f};
                        ++w.lost;
                        emit(w, Event::Scared);
                        scared = true;
                    }
                if (scared) break;
                if (length(b.p - w.guide) > 15.0f && length(b.p - w.centre) > 8.0f) {
                    b.state = BatState::Adrift;
                    b.anchor = b.p;
                    emit(w, Event::Strayed);
                }
                break;
            }
            case BatState::Dazed: {
                b.v.y -= 9.0f * kDt;
                b.v = b.v * (1.0f - 1.5f * kDt);
                const Vec2 was = b.p;
                b.p += b.v * kDt;
                if (clearance(l, b.p, 2.0f) < 0.3f) {
                    b.p = was;
                    b.v = b.v * -0.3f;
                }
                if (b.p.y < 0.5f) b.p.y = 0.5f;
                b.timer -= kDt;
                if (b.timer <= 0) {
                    b.state = BatState::Adrift;
                    b.anchor = b.p;
                    for (int k = 0; k < 30 && clearance(l, b.anchor) < 0.9f; ++k) b.anchor.y += 0.2f;
                }
                break;
            }
            case BatState::Perched:
            case BatState::Adrift:
                hover(b, t);
                if (b.state == BatState::Adrift && w.centre.x - b.p.x > 45.0f && n) {
                    b.state = BatState::Gone;
                    ++w.lost;
                }
                break;
            case BatState::Scared:
                b.v.y += 1.0f * kDt;
                b.p += b.v * kDt;
                b.timer -= kDt;
                if (b.timer <= 0) b.state = BatState::Gone;
                break;
            case BatState::Home:
            case BatState::Gone: break;
        }
    }

    // Strays and adrift bats join when the flock flies close.
    for (Bat& b : w.bats) {
        if (b.state != BatState::Perched && b.state != BatState::Adrift) continue;
        for (const Bat& o : w.bats) {
            if (o.state != BatState::Flock || length2(o.p - b.p) > kGatherRadius * kGatherRadius) continue;
            if (b.state == BatState::Perched) {
                ++w.gathered;
                emit(w, Event::Gathered);
            } else {
                emit(w, Event::Rejoined);
            }
            b.state = BatState::Flock;
            b.v = (o.v * 0.5f) + Vec2{0, 1.0f};
            break;
        }
    }

    ++w.tick;
    if (!w.dawnWarned && dawnProgress(w) >= 0.8f) {
        w.dawnWarned = true;
        emit(w, Event::DawnSoon);
    }
    if (flying(w) == 0) endNight(w, w.home > 0 ? Ending::AllHome : Ending::NoneLeft);
    else if (seconds(w) >= l.dawnSeconds) endNight(w, Ending::Dawn);
}

uint64_t fingerprint(const World& w) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, size_t len) {
        const auto* c = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < len; ++i) {
            h ^= c[i];
            h *= 1099511628211ull;
        }
    };
    mix(&w.tick, sizeof w.tick);
    mix(&w.night, sizeof w.night);
    mix(&w.home, sizeof w.home);
    mix(&w.lost, sizeof w.lost);
    for (const Bat& b : w.bats) {
        mix(&b.state, sizeof b.state);
        mix(&b.p, sizeof b.p);
        mix(&b.v, sizeof b.v);
    }
    return h;
}

}  // namespace belfry
