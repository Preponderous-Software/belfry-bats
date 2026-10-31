#include "audio/device.h"

#include <mutex>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#include "miniaudio.h"

namespace belfry::audio {

struct Sound::Device {
    ma_device device{};
    bool open = false;
    std::mutex lock;  // the synth is shared with the device's callback
    Synth synth;
    float volume = 1, target = 1;  // the mute fades rather than clicks
    std::vector<float> mono;

    static void callback(ma_device* d, void* output, const void*, ma_uint32 frames) {
        auto* self = static_cast<Device*>(d->pUserData);
        auto* out = static_cast<float*>(output);
        std::lock_guard<std::mutex> hold(self->lock);
        self->mono.resize(frames);
        self->synth.render(self->mono.data(), static_cast<int>(frames));
        for (ma_uint32 i = 0; i < frames; ++i) {
            self->volume += (self->target - self->volume) * 0.002f;
            const float s = self->mono[i] * self->volume;
            out[2 * i] = s;
            out[2 * i + 1] = s;
        }
    }
};

Sound::Sound() : device_(std::make_unique<Device>()) {}

Sound::~Sound() {
    if (device_->open) ma_device_uninit(&device_->device);
}

bool Sound::start() {
    if (device_->open) return true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = kRate;
    config.dataCallback = Device::callback;
    config.pUserData = device_.get();
    if (ma_device_init(nullptr, &config, &device_->device) != MA_SUCCESS) return false;
    if (ma_device_start(&device_->device) != MA_SUCCESS) {
        ma_device_uninit(&device_->device);
        return false;
    }
    device_->open = true;
    return true;
}

bool Sound::started() const { return device_->open; }

void Sound::setAmbience(const Ambience& a) {
    if (!device_->open) return;
    std::lock_guard<std::mutex> hold(device_->lock);
    device_->synth.setAmbience(a);
}

void Sound::play(Cue c) {
    if (!device_->open || muted_) return;
    std::lock_guard<std::mutex> hold(device_->lock);
    device_->synth.play(c);
}

void Sound::setMuted(bool muted) {
    muted_ = muted;
    std::lock_guard<std::mutex> hold(device_->lock);
    device_->target = muted ? 0.0f : 1.0f;
}

}  // namespace belfry::audio
