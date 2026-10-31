// What the night sounds like, worked out from the world: the steady sounds
// (the wind, the flock's wings, an owl now and then) and the one-off sounds
// of things happening (a stray's chirp, a bump, an owl's hoot, a bat home,
// the bell). No device and no engine: the synth (synth.h) turns these into
// samples, and the tests check them without either. Gentle throughout: a
// game for a family.
#pragma once

#include <vector>

#include "sim/world.h"

namespace belfry::audio {

// The steady sounds, each 0 (silent) to 1 (full).
struct Ambience {
    float wind = 0;
    float wings = 0;  // the flock's flutter, more with more bats
    float owls = 0;   // far-off owls, quieter toward dawn
    float birds = 0;  // the first birds, at dawn
};

enum class Cue : int {
    Chirp,     // a stray joins the flock
    Rejoin,    // an adrift bat comes back
    Bump,      // a bat bumps a roof or a tree
    Squeak,    // a bat falls behind
    Hoot,      // an owl close by
    Flurry,    // an owl scares a bat off
    Home,      // a bat into the belfry
    Bell,      // one stroke of the church bell
    FarBell,   // the bell across the village, as the night starts
    Dawn,      // the sky is getting light
    Count
};

Ambience ambience(const World& w);
std::vector<Cue> cues(const World& w);  // for this step's events

}  // namespace belfry::audio
