// The flock: it follows the guide and keeps together without piling up;
// bumping into a roof dazes a bat, which can be gathered again; owls scare
// bats away for the night; strays join when the flock flies by; the belfry
// takes bats home; dawn ends the night; and every night can be won.
#include <algorithm>
#include <cstdio>

#include "check.h"
#include "sim/autopilot.h"
#include "sim/world.h"

using namespace belfry;

namespace {

void run(World& w, const Input& in, float secs) {
    for (int i = 0; i < static_cast<int>(secs * kTickRate) && w.phase == Phase::Flying; ++i) step(w, in);
}

Input to(float x, float y) {
    Input in;
    in.guide = {x, y};
    in.steering = true;
    return in;
}

// A night with nothing in the way (no props, owls or strays), for the flocking itself.
World openSky() {
    World w = newGame();
    startNight(w, 0);
    w.level.shapes.clear();
    w.level.shapeX1.clear();
    w.level.owls.clear();
    w.owlCooldown.clear();
    w.bats.erase(std::remove_if(w.bats.begin(), w.bats.end(), [](const Bat& b) { return b.state != BatState::Flock; }),
                 w.bats.end());
    w.level.dawnSeconds = 1000;
    return w;
}

}  // namespace

int main() {
    // A new game waits at the title; a night starts with the flock in the air.
    {
        World w = newGame();
        CHECK(w.phase == Phase::Title);
        step(w, to(20, 10));
        CHECK(w.tick == 0);
        startNight(w, 0);
        CHECK(w.phase == Phase::Flying);
        CHECK(count(w, BatState::Flock) == 8);
        CHECK(count(w, BatState::Perched) == static_cast<int>(w.level.perches.size()));
        CHECK(w.nightOf[0] == static_cast<int>(w.bats.size()));
        CHECK(std::find(w.events.begin(), w.events.end(), Event::NightStart) != w.events.end());
    }
    // Following: the flock goes where it is led and gathers round the guide.
    {
        World w = openSky();
        run(w, to(30, 12), 6);
        CHECK(std::fabs(w.centre.x - 30) < 2.5f);
        CHECK(std::fabs(w.centre.y - 12) < 2.5f);
        CHECK(count(w, BatState::Flock) == 8);
        // Kept apart: no two bats in the same place, and none far from the rest.
        float closest = 1e9f, furthest = 0;
        for (size_t i = 0; i < w.bats.size(); ++i) {
            furthest = std::max(furthest, length(w.bats[i].p - w.centre));
            for (size_t j = i + 1; j < w.bats.size(); ++j) closest = std::min(closest, length(w.bats[i].p - w.bats[j].p));
        }
        CHECK(closest > 0.15f);
        CHECK(furthest < 5.0f);
        std::printf("following: centre (%.2f, %.2f), closest pair %.2f, furthest %.2f\n", static_cast<double>(w.centre.x),
                    static_cast<double>(w.centre.y), static_cast<double>(closest), static_cast<double>(furthest));
        // Never faster than a bat can fly.
        for (const Bat& b : w.bats) CHECK(length(b.v) <= kMaxSpeed * 1.2f + 1e-3f);
        // The guide is kept below the cloud and above the ground.
        run(w, to(30, 99), 3);
        CHECK(w.guide.y <= kCeiling);
        for (const Bat& b : w.bats) CHECK(b.p.y <= kCeiling);
    }
    // Too far behind: a bat that cannot keep up is adrift, and the flock can fetch it back.
    {
        World w = openSky();
        run(w, to(20, 10), 3);
        w.bats[0].p = {w.centre.x - 20, 10};
        w.bats[0].v = {0, 0};
        step(w, to(20, 10));
        CHECK(w.bats[0].state == BatState::Adrift);
        run(w, to(w.bats[0].p.x, 10), 6);
        CHECK(w.bats[0].state == BatState::Flock);
    }
    // Left far behind, an adrift bat is lost for the night.
    {
        World w = openSky();
        w.bats[0].state = BatState::Adrift;
        w.bats[0].p = w.bats[0].anchor = {-10, 9};
        run(w, to(70, 10), 12);
        CHECK(w.bats[0].state == BatState::Gone);
        CHECK(w.lost == 1);
    }
    // Bumping: led straight into a wall, bats are dazed, then adrift, and can be gathered again.
    {
        World w = openSky();
        Shape wall;
        wall.kind = Shape::Box;
        wall.a = {16, 0};
        wall.b = {18, 30};
        w.level.shapes.push_back(wall);
        w.level.shapeX1.push_back(18);
        int bumped = 0;
        for (int i = 0; i < 6 * kTickRate; ++i) {
            step(w, to(26, 9));
            for (Event e : w.events) bumped += e == Event::Bumped ? 1 : 0;
        }
        CHECK(bumped > 0);
        for (const Bat& b : w.bats) {
            CHECK(b.p.x < 16.2f);  // nobody flies through
            CHECK(b.state != BatState::Gone);
        }
        std::printf("into a wall: %d bumps, %d still with the flock\n", bumped, count(w, BatState::Flock));
        w.level.shapes.clear();
        w.level.shapeX1.clear();
        run(w, to(12, 9), 4);
        run(w, to(12, 4), 4);
        run(w, to(12, 14), 4);
        CHECK(count(w, BatState::Flock) + count(w, BatState::Adrift) == 8);
    }
    // Owls: a bat that meets one is scared off for the night.
    {
        World w = openSky();
        OwlPath o;
        o.centre = {24, 10};
        o.speed = 0.0001f;
        w.level.owls.push_back(o);
        w.owlCooldown.push_back(0);
        bool near = false, scared = false;
        for (int i = 0; i < 5 * kTickRate; ++i) {
            step(w, to(24, 10));
            for (Event e : w.events) {
                near = near || e == Event::OwlNear;
                scared = scared || e == Event::Scared;
            }
        }
        CHECK(near);
        CHECK(scared);
        CHECK(w.lost > 0);
        CHECK(count(w, BatState::Gone) + count(w, BatState::Scared) == w.lost);
    }
    // Strays: flying past one gathers it into the flock.
    {
        World w = openSky();
        Bat s;
        s.state = BatState::Perched;
        s.stray = true;
        s.p = s.anchor = {25, 10};
        s.offset = {0.5f, 0.5f};
        w.bats.push_back(s);
        bool gathered = false;
        for (int i = 0; i < 6 * kTickRate; ++i) {
            step(w, to(32, 10));
            for (Event e : w.events) gathered = gathered || e == Event::Gathered;
        }
        CHECK(gathered);
        CHECK(w.gathered == 1);
        CHECK(count(w, BatState::Flock) == 9);
    }
    // Home: led into the belfry, every bat goes home and the night ends.
    {
        World w = newGame();
        startNight(w, 0);
        w.level.owls.clear();
        w.owlCooldown.clear();
        for (Bat& b : w.bats)
            if (b.state == BatState::Flock) {
                b.p = {w.level.towerX - 12 + b.offset.x, 14 + b.offset.y * 0.5f};
                b.v = {1, 0};
            }
        bool first = false;
        for (int i = 0; i < 20 * kTickRate && w.phase == Phase::Flying; ++i) {
            step(w, to(w.level.towerX - 0.5f, 14));
            for (Event e : w.events) first = first || e == Event::FirstHome;
        }
        CHECK(first);
        CHECK(w.phase == Phase::NightOver);
        CHECK(w.ending == Ending::AllHome);
        CHECK(w.home + w.lost == 8);
        CHECK(w.home >= 6);
        CHECK(w.score == w.home);
        CHECK(w.nightHome[0] == w.home);
        std::printf("into the belfry: %d home, %d lost\n", w.home, w.lost);
    }
    // Dawn: a night not finished in time ends when the sky is light.
    {
        World w = newGame();
        startNight(w, 0);
        bool warned = false;
        int steps = 0;
        while (w.phase == Phase::Flying && steps++ < 200 * kTickRate) {
            step(w, to(kStartX, kStartY));
            for (Event e : w.events) warned = warned || e == Event::DawnSoon;
        }
        CHECK(warned);
        CHECK(w.ending == Ending::Dawn);
        CHECK(w.home == 0);
        CHECK(std::fabs(seconds(w) - w.level.dawnSeconds) < 0.05f);
        CHECK(dawnProgress(w) == 1.0f);
    }
    // The same inputs give the same night.
    {
        World a = newGame(), b = newGame();
        startNight(a, 2);
        startNight(b, 2);
        for (int i = 0; i < 20 * kTickRate; ++i) {
            const Input ia = autopilot(a), ib = autopilot(b);
            step(a, ia);
            step(b, ib);
        }
        CHECK(fingerprint(a) == fingerprint(b));
        World c = newGame();
        startNight(c, 2);
        for (int i = 0; i < 20 * kTickRate; ++i) step(c, to(30, 15));
        CHECK(fingerprint(a) != fingerprint(c));
    }
    // Every night can be won: the pilot brings most of the flock home before dawn.
    {
        World w = newGame();
        int total = 0;
        for (int n = 0; n < kNights; ++n) {
            startNight(w, n);
            while (w.phase == Phase::Flying) step(w, autopilot(w));
            std::printf("night %d by the pilot: %d of %d home (%d gathered, %d lost) in %.1f of %.0f s\n", n + 1, w.home,
                        w.nightOf[static_cast<size_t>(n)], w.gathered, w.lost, static_cast<double>(seconds(w)),
                        static_cast<double>(w.level.dawnSeconds));
            CHECK(w.ending == Ending::AllHome);
            CHECK(w.home >= w.level.startBats * 6 / 10);
            total += w.home;
        }
        CHECK(w.phase == Phase::Finished);
        CHECK(w.score == total);
    }
    TEST_MAIN_END();
}
