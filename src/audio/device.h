// The sound card: a miniaudio playback device that pulls from the synth. If
// there is no device (a headless machine, a browser page nobody has touched
// yet), the game runs silent and nothing else changes.
#pragma once

#include <memory>

#include "audio/synth.h"

namespace belfry::audio {

class Sound {
public:
    Sound();
    ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    // Opens the device and starts playing; false if there is none.
    bool start();
    bool started() const;

    void setAmbience(const Ambience& a);
    void play(Cue c);
    void setMuted(bool muted);
    bool muted() const { return muted_; }

private:
    struct Device;
    std::unique_ptr<Device> device_;
    bool muted_ = false;
};

}  // namespace belfry::audio
