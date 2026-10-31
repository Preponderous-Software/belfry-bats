#include "audio/synth.h"

#include <cmath>
#include <initializer_list>

namespace belfry::audio {

namespace {

constexpr float kTwoPi = 6.2831853f;
constexpr float kDtA = 1.0f / static_cast<float>(kRate);
constexpr float kGlide = 1.0f / (0.5f * static_cast<float>(kRate));
constexpr float kBellApart = 2.2f;

struct Noise {
    uint32_t s;
    float next() {  // -1..1
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s >> 8) / 8388608.0f - 1.0f;
    }
    float between(float a, float b) { return a + (b - a) * (next() * 0.5f + 0.5f); }
};

float pole(float hz) { return 1.0f - std::exp(-kTwoPi * hz * kDtA); }
int samples(float seconds) { return static_cast<int>(seconds * static_cast<float>(kRate)); }
float at(int i) { return static_cast<float>(i) * kDtA; }
std::vector<float> silence(float seconds) { return std::vector<float>(static_cast<size_t>(samples(seconds)), 0.0f); }

float window(float t, float start, float len) {
    const float u = (t - start) / len;
    if (u <= 0 || u >= 1) return 0;
    const float s = std::sin(u * 3.14159265f);
    return s * s;
}

void mix(std::vector<float>& dst, const std::vector<float>& src, float start, float gain) {
    const size_t from = static_cast<size_t>(samples(start));
    for (size_t i = 0; i < src.size() && from + i < dst.size(); ++i) dst[from + i] += gain * src[i];
}

// The church bell: hum, prime, minor third, fifth and nominal, ringing on.
// `distance` 0 is close by, 1 across the village (the edge taken off).
std::vector<float> churchBell(float distance) {
    static const float ratio[] = {0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f};
    static const float amp[] = {0.7f, 1.0f, 0.5f, 0.3f, 0.45f, 0.15f};
    static const float tau[] = {3.5f, 2.2f, 1.6f, 1.2f, 0.9f, 0.4f};
    const float hz = 262.0f;  // a C
    std::vector<float> out = silence(5.0f);
    float lo = 0;
    const float cut = 4000.0f - 2800.0f * distance;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = at(static_cast<int>(i));
        float s = 0;
        for (int k = 0; k < 6; ++k) s += amp[k] * std::exp(-t / tau[k]) * std::sin(kTwoPi * hz * ratio[k] * t + static_cast<float>(k));
        lo += pole(cut) * (s - lo);
        out[i] = (0.26f - 0.14f * distance) * std::fmin(t / 0.006f, 1.0f) * lo;
    }
    return out;
}

// A bat's chirp: a quick, bright sweep, softened well below a real bat's
// ultrasound so people can hear it. `up`: rising (glad) or falling (lost).
std::vector<float> chirp(float from, float to, float len, int repeats, float gain) {
    std::vector<float> out = silence(len * static_cast<float>(repeats) * 1.6f + 0.05f);
    for (int k = 0; k < repeats; ++k) {
        const float start = static_cast<float>(k) * len * 1.6f;
        double phase = 0;
        for (int i = samples(start); i < samples(start + len); ++i) {
            const float u = (at(i) - start) / len;
            const float f = from + (to - from) * u;
            phase += static_cast<double>(f * kDtA);
            out[static_cast<size_t>(i)] += gain * window(u, 0, 1) *
                                           (static_cast<float>(std::sin(6.283185307179586 * phase)) +
                                            0.2f * static_cast<float>(std::sin(2 * 6.283185307179586 * phase)));
        }
    }
    return out;
}

// One wingbeat: a soft papery whump of low noise.
std::vector<float> flap(Noise& r) {
    std::vector<float> out = silence(0.09f);
    float lo = 0, lo2 = 0;
    const float cut = r.between(500, 900);
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = at(static_cast<int>(i));
        lo += pole(cut) * (r.next() - lo);
        lo2 += pole(cut * 0.4f) * (lo - lo2);
        out[i] = 2.2f * (lo - lo2 * 0.6f) * window(t, 0, 0.09f);
    }
    return out;
}

// An owl: hoo, hoo-hoo.
std::vector<float> owl(Noise& r, float gain) {
    std::vector<float> out = silence(2.0f);
    const float hz = r.between(330, 400);
    const float starts[] = {0.0f, 0.75f, 1.05f};
    const float lengths[] = {0.45f, 0.22f, 0.5f};
    float lo = 0;
    for (int k = 0; k < 3; ++k)
        for (int i = samples(starts[k]); i < samples(starts[k] + lengths[k]) && i < static_cast<int>(out.size()); ++i) {
            const float u = at(i) - starts[k];
            const float f = hz * (1.0f - 0.06f * u / lengths[k]);
            out[static_cast<size_t>(i)] += window(u, 0, lengths[k]) * (std::sin(kTwoPi * f * u) + 0.15f * std::sin(kTwoPi * 2 * f * u));
        }
    for (float& s : out) {
        lo += pole(1200) * (s - lo);
        s = gain * lo;
    }
    return out;
}

// A bump: a soft thud on a roof, and a small cross squeak.
std::vector<float> bump(Noise& r) {
    std::vector<float> out = silence(0.4f);
    float lo = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = at(static_cast<int>(i));
        lo += pole(220) * (r.next() - lo);
        out[i] = (3.0f * lo + 0.4f * std::sin(kTwoPi * 90.0f * t)) * std::exp(-t / 0.05f);
    }
    mix(out, chirp(2600, 1700, 0.07f, 1, 0.12f), 0.08f, 1.0f);
    return out;
}

// An owl swooping: a rush of feathers, and the bat's squeaks fading off.
std::vector<float> flurry(Noise& r) {
    std::vector<float> out = silence(1.2f);
    float lo = 0, hi = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = at(static_cast<int>(i)), n = r.next();
        lo += pole(400) * (n - lo);
        hi += pole(2500) * (n - hi);
        out[i] = 1.2f * (hi - lo) * window(t, 0, 0.6f);
    }
    mix(out, chirp(3200, 2200, 0.06f, 3, 0.09f), 0.25f, 1.0f);
    return out;
}

// Soft sine tones `apart` seconds apart.
std::vector<float> chime(std::initializer_list<float> notes, float apart, float tau, float seconds, float gain) {
    std::vector<float> out = silence(seconds);
    for (size_t i = 0; i < out.size(); ++i) {
        const float t = at(static_cast<int>(i));
        float s = 0, start = 0;
        for (float hz : notes) {
            if (t >= start) {
                const float u = t - start;
                s += std::fmin(u / 0.006f, 1.0f) * std::exp(-u / tau) * (std::sin(kTwoPi * hz * u) + 0.3f * std::sin(kTwoPi * 2 * hz * u));
            }
            start += apart;
        }
        out[i] = gain * s;
    }
    return out;
}

// A small bird waking: two or three quick whistles.
std::vector<float> bird(Noise& r) {
    const float hz = r.between(2600, 3600);
    std::vector<float> out = chirp(hz, hz * 1.25f, 0.08f, 2 + static_cast<int>(r.between(0, 1.99f)), 0.05f);
    out.resize(out.size() + static_cast<size_t>(samples(0.2f)), 0.0f);
    return out;
}

float soften(float x) {
    if (x > 3) return 1;
    if (x < -3) return -1;
    return x * (27 + x * x) / (27 + 9 * x * x);
}

}  // namespace

Synth::Synth() {
    Noise r{0x9E3779B9u};
    auto set = [&](Cue c, std::vector<float> s) { cues_[static_cast<size_t>(c)] = std::move(s); };
    set(Cue::Chirp, chirp(2400, 3400, 0.06f, 2, 0.11f));
    set(Cue::Rejoin, chirp(2200, 3000, 0.05f, 1, 0.09f));
    set(Cue::Bump, bump(r));
    set(Cue::Squeak, chirp(3000, 1900, 0.12f, 1, 0.08f));
    set(Cue::Hoot, owl(r, 0.3f));
    set(Cue::Flurry, flurry(r));
    set(Cue::Home, chime({1568, 2093}, 0.06f, 0.2f, 0.6f, 0.07f));
    set(Cue::Bell, churchBell(0.0f));
    set(Cue::FarBell, churchBell(1.0f));
    set(Cue::Dawn, chime({784, 988, 1175}, 0.35f, 1.2f, 3.0f, 0.06f));
    for (int i = 0; i < 3; ++i) owls_.push_back(owl(r, 0.12f));
    for (int i = 0; i < 6; ++i) flaps_.push_back(flap(r));
    for (int i = 0; i < 4; ++i) birds_.push_back(bird(r));
}

void Synth::play(Cue c) {
    const auto& s = cues_[static_cast<size_t>(c)];
    int64_t start = 0;
    if (c == Cue::Bell || c == Cue::FarBell) {
        const int64_t when = nextBell_ > clock_ ? nextBell_ : clock_;
        start = clock_ - when;
        nextBell_ = when + samples(kBellApart);
    }
    playing_.push_back({&s, start, 0.9f});
}

float Synth::noise() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return static_cast<float>(seed_ >> 8) / 8388608.0f - 1.0f;
}

void Synth::every(float& clock, float perSecond, const std::vector<std::vector<float>>& sounds, float gain) {
    if (perSecond <= 0) return;
    clock += perSecond * kDtA;
    if (clock < 1) return;
    clock = -0.6f * (noise() * 0.5f + 0.5f);
    const size_t pick = static_cast<size_t>((noise() * 0.5f + 0.5f) * static_cast<float>(sounds.size())) % sounds.size();
    playing_.push_back({&sounds[pick], 0, gain});
}

float Synth::next() {
    auto glide = [](float& v, float to) { v += (to - v) * kGlide; };
    glide(now_.wind, target_.wind);
    glide(now_.wings, target_.wings);
    glide(now_.owls, target_.owls);
    glide(now_.birds, target_.birds);
    time_ += static_cast<double>(kDtA);
    const float t = static_cast<float>(std::fmod(time_, 1000.0));
    const float n = noise();
    float s = 0;
    // The wind: low noise whose colour drifts, in slow gusts.
    windLow_ += pole(140 + 100 * std::sin(kTwoPi * 0.05f * t)) * (n - windLow_);
    const float gusting = 0.65f + 0.35f * std::sin(kTwoPi * 0.06f * t + 1) * std::sin(kTwoPi * 0.023f * t);
    s += now_.wind * 0.7f * windLow_ * gusting;
    // The flock: many soft wingbeats, more with more bats.
    every(flapClock_, now_.wings * 26.0f, flaps_, 0.10f + 0.06f * now_.wings);
    every(owlClock_, now_.owls * 0.06f, owls_, now_.owls);
    every(birdClock_, now_.birds * 1.2f, birds_, now_.birds);
    for (size_t i = 0; i < playing_.size();) {
        Playing& p = playing_[i];
        if (p.at >= 0) s += p.gain * (*p.samples)[static_cast<size_t>(p.at)];
        ++p.at;
        if (p.at >= static_cast<int64_t>(p.samples->size())) {
            playing_[i] = playing_.back();
            playing_.pop_back();
        } else {
            ++i;
        }
    }
    ++clock_;
    return soften(0.8f * s);
}

void Synth::render(float* out, int frames) {
    for (int i = 0; i < frames; ++i) out[i] = next();
}

}  // namespace belfry::audio
