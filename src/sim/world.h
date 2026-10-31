// The flock and the night: bats that follow the player's finger or mouse with
// boids-style flocking, the owls, the strays, the dawn. Fixed 60 Hz steps,
// deterministic: the same inputs give the same night on every machine.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "sim/level.h"

namespace belfry {

constexpr int kTickRate = 60;
constexpr float kDt = 1.0f / static_cast<float>(kTickRate);
constexpr int kMaxBats = 32;       // the most bats a night can hold (phones stay smooth)
constexpr float kMaxSpeed = 7.0f;  // world units a second
constexpr float kGatherRadius = 2.6f;
constexpr float kOwlRadius = 1.35f;

enum class BatState : int {
    Flock,    // following
    Perched,  // a stray, waiting to be gathered
    Dazed,    // bumped into something, tumbling
    Adrift,   // fell behind or bumped: hovering, can be gathered again
    Scared,   // an owl came too close: it flies off and hides till tomorrow
    Home,     // in the belfry
    Gone      // left far behind, or hidden from the owl: lost for the night
};

struct Bat {
    BatState state = BatState::Flock;
    Vec2 p, v;
    Vec2 anchor;      // where a perched or adrift bat hovers
    Vec2 offset;      // its own place in the flock, around the guide
    float timer = 0;  // seconds left dazed or scared
    float phase = 0;  // wingbeat and wander
    float depth = 0;  // in front of or behind the flight plane (looks only)
    bool stray = false;  // was a stray tonight (for the tally)
};

enum class Phase : int { Title, Flying, NightOver, Finished };
enum class Ending : int { None, AllHome, Dawn, NoneLeft };

// What happened in one step, for the sounds and the words.
enum class Event : int {
    NightStart,
    Gathered,   // a stray joined
    Rejoined,   // an adrift bat came back
    Bumped,
    Strayed,    // fell too far behind and is adrift
    Scared,     // an owl
    OwlNear,    // an owl is close to the flock
    Home,
    FirstHome,
    DawnSoon,   // three quarters of the night gone
    NightOver,
    Count
};

struct Input {
    Vec2 guide;          // where the finger or mouse is, in the flight plane
    bool steering = false;  // false: the bats keep to the last guide
};

struct World {
    Phase phase = Phase::Title;
    Ending ending = Ending::None;
    int night = 0;        // 0-based
    int64_t tick = 0;     // within the night
    Level level;
    std::vector<Bat> bats;
    Vec2 guide{kStartX, kStartY};
    Vec2 centre{kStartX, kStartY};  // of the flock
    int home = 0;         // tonight
    int lost = 0;         // tonight
    int gathered = 0;     // strays gathered tonight
    std::array<int, kNights> nightHome{};
    std::array<int, kNights> nightOf{};  // tonight's possible: the flock plus every stray
    int score = 0;        // every bat brought home, all nights
    std::vector<Event> events;  // this step's
    std::vector<float> owlCooldown;
    bool dawnWarned = false;
    uint32_t seed = 0x5EEDu;
};

// A new game at the title.
World newGame();
// Begins night `night` (0-based).
void startNight(World& w, int night);
// One step (kDt). Clears and refills w.events.
void step(World& w, const Input& in);

float seconds(const World& w);
// 0 at the start of the night, 1 at dawn.
float dawnProgress(const World& w);
Vec2 owlPosition(const World& w, size_t i);
int count(const World& w, BatState s);
// The bats in the air with the flock or still able to rejoin it.
int flying(const World& w);
// A stable hash of the state (for the tests).
uint64_t fingerprint(const World& w);

}  // namespace belfry
