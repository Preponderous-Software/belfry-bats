// The sounds: the night's events become the right cues, every cue is a real
// sound, and the synth never clips, however much is going on.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio/cues.h"
#include "audio/synth.h"
#include "check.h"
#include "sim/autopilot.h"

using namespace belfry;
using namespace belfry::audio;

namespace {
bool has(const std::vector<Cue>& v, Cue c) { return std::find(v.begin(), v.end(), c) != v.end(); }
}  // namespace

int main() {
    Synth synth;
    for (int c = 0; c < static_cast<int>(Cue::Count); ++c) {
        const auto& s = synth.sound(static_cast<Cue>(c));
        CHECK(!s.empty());
        float peak = 0;
        for (float v : s) peak = std::max(peak, std::fabs(v));
        CHECK(peak > 0.005f);  // audible
        CHECK(peak < 1.0f);
    }
    // Events become cues.
    {
        World w = newGame();
        startNight(w, 0);
        CHECK(has(cues(w), Cue::FarBell));
        w.events = {Event::Gathered, Event::Bumped, Event::Bumped, Event::Scared, Event::OwlNear, Event::FirstHome, Event::Home};
        const auto c = cues(w);
        CHECK(has(c, Cue::Chirp));
        CHECK(std::count(c.begin(), c.end(), Cue::Bump) == 1);
        CHECK(has(c, Cue::Flurry));
        CHECK(has(c, Cue::Hoot));
        CHECK(has(c, Cue::Bell));
        CHECK(std::count(c.begin(), c.end(), Cue::Home) == 1);
        w.events = {Event::NightOver};
        const auto over = cues(w);
        CHECK(std::count(over.begin(), over.end(), Cue::Bell) == 3);
    }
    // The steady sounds follow the night: wings with the flock, birds at dawn.
    {
        World w = newGame();
        CHECK(ambience(w).wings == 0);
        startNight(w, 0);
        const Ambience early = ambience(w);
        CHECK(early.wings > 0);
        CHECK(early.birds == 0);
        CHECK(early.owls > 0.5f);
        w.tick = static_cast<int64_t>(w.level.dawnSeconds * 0.95f * kTickRate);
        const Ambience late = ambience(w);
        CHECK(late.birds > 0.5f);
        CHECK(late.owls < early.owls);
    }
    // A whole busy night through the synth: finite, never past full scale.
    {
        World w = newGame();
        startNight(w, 1);
        std::vector<float> buf(800);
        float peak = 0;
        bool finite = true;
        int frames = 0;
        while (w.phase == Phase::Flying && frames < 60 * kTickRate) {
            step(w, autopilot(w));
            synth.setAmbience(ambience(w));
            for (Cue c : cues(w)) synth.play(c);
            synth.render(buf.data(), static_cast<int>(buf.size()));
            for (float v : buf) {
                finite = finite && std::isfinite(v);
                peak = std::max(peak, std::fabs(v));
            }
            ++frames;
        }
        CHECK(finite);
        CHECK(peak <= 1.0f);
        CHECK(peak > 0.01f);
        std::printf("a night through the synth: peak %.3f, %zu sounds still playing\n", static_cast<double>(peak), synth.playing());
    }
    TEST_MAIN_END();
}
