#include "audio/cues.h"

namespace belfry::audio {

Ambience ambience(const World& w) {
    Ambience a;
    if (w.phase == Phase::Title) {
        a.wind = 0.5f;
        a.owls = 0.6f;
        return a;
    }
    const float dawn = dawnProgress(w);
    a.wind = 0.45f + 0.25f * clampf(w.centre.y / kCeiling, 0, 1);
    a.wings = w.phase == Phase::Flying ? clampf(static_cast<float>(count(w, BatState::Flock)) / 16.0f, 0, 1) : 0.0f;
    a.owls = 0.7f * (1.0f - dawn);
    a.birds = w.phase == Phase::Flying ? clampf((dawn - 0.6f) / 0.4f, 0, 1) : (w.ending == Ending::Dawn ? 1.0f : 0.4f);
    return a;
}

std::vector<Cue> cues(const World& w) {
    std::vector<Cue> out;
    bool home = false, bump = false;
    for (Event e : w.events) {
        switch (e) {
            case Event::NightStart: out.push_back(Cue::FarBell); break;
            case Event::Gathered: out.push_back(Cue::Chirp); break;
            case Event::Rejoined: out.push_back(Cue::Rejoin); break;
            case Event::Bumped:
                if (!bump) out.push_back(Cue::Bump);  // one a step, however many
                bump = true;
                break;
            case Event::Strayed: out.push_back(Cue::Squeak); break;
            case Event::Scared: out.push_back(Cue::Flurry); break;
            case Event::OwlNear: out.push_back(Cue::Hoot); break;
            case Event::FirstHome: out.push_back(Cue::Bell); [[fallthrough]];
            case Event::Home:
                if (!home) out.push_back(Cue::Home);
                home = true;
                break;
            case Event::DawnSoon: out.push_back(Cue::Dawn); break;
            case Event::NightOver:
                out.push_back(Cue::Bell);
                out.push_back(Cue::Bell);
                out.push_back(Cue::Bell);
                break;
            case Event::Count: break;
        }
    }
    return out;
}

}  // namespace belfry::audio
