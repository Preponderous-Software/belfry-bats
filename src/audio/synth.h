// The night's sounds, made from nothing: every sound is synthesised (no
// recordings ship with the game). The steady sounds are generated sample by
// sample from the Ambience; the one-off sounds are rendered once, when the
// synth is made, and mixed in when played. Mono, at kRate. (After Familiar's
// synth, for a flock of bats before dawn.)
//
// Not thread-safe on its own: the device (device.h) locks around it.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "audio/cues.h"

namespace belfry::audio {

constexpr int kRate = 48000;

class Synth {
public:
    Synth();

    void setAmbience(const Ambience& a) { target_ = a; }
    // The bell's strokes follow one another, a couple of seconds apart.
    void play(Cue c);
    void render(float* out, int frames);

    const std::vector<float>& sound(Cue c) const { return cues_[static_cast<size_t>(c)]; }
    size_t playing() const { return playing_.size(); }

private:
    struct Playing {
        const std::vector<float>* samples;
        int64_t at;  // negative: still waiting to start
        float gain;
    };
    float noise();
    float next();
    void every(float& clock, float perSecond, const std::vector<std::vector<float>>& sounds, float gain);

    uint32_t seed_ = 0x2545F491u;
    Ambience target_, now_;
    std::array<std::vector<float>, static_cast<size_t>(Cue::Count)> cues_;
    std::vector<std::vector<float>> owls_, flaps_, birds_;
    std::vector<Playing> playing_;
    int64_t clock_ = 0;
    int64_t nextBell_ = 0;
    float windLow_ = 0;
    double time_ = 0;
    float owlClock_ = 0, flapClock_ = 0, birdClock_ = 0;
};

}  // namespace belfry::audio
