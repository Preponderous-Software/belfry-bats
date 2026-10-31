// What the player sees: the village from the side, the sky going from night
// to dawn, the flock and the owls, all on Strata.
#pragma once

#include <cstdint>
#include <vector>

#include "play/meshes.h"
#include "sim/world.h"
#include "strata/strata.hpp"

namespace belfry::play {

// Where the camera looks: it follows the flock along the village.
struct Camera {
    float x = kStartX, y = 12;
    float distance = 36;
    StrataCamera cam{};
    int32_t width = 1, height = 1;
};

class Scene {
public:
    explicit Scene(strata::Engine& engine);
    // Builds the village of the world's night (call when the night changes).
    void load(const World& w);
    // Follows the flock (dt in seconds of real time; 0 jumps straight there).
    void follow(const World& w, float dt);
    // The canvas's size as the page laid it out (its aspect is the camera's).
    void setViewport(int32_t width, int32_t height) {
        viewW_ = width;
        viewH_ = height;
    }
    // The point in the flight plane under the pointer (0..1 across and down the canvas).
    Vec2 unproject(float nx, float ny) const;
    void draw(const World& w, float realSeconds, bool showGuide);
    const Camera& camera() const { return camera_; }
    int loadedNight() const { return night_; }

private:
    void place(uint32_t id, const float model[16]);
    strata::Engine& engine_;
    Camera camera_;
    int night_ = -1;
    int32_t viewW_ = 0, viewH_ = 0;
    std::vector<uint32_t> village_;
    struct Body {
        uint32_t body = 0, left = 0, right = 0;
        float yaw = 0;   // smoothed facing
        float roll = 0;  // a dazed bat tumbles
    };
    std::vector<Body> bats_, owls_;
    std::vector<std::array<float, 3>> windowLights_;
    std::array<float, 3> lantern_{};
    std::vector<StrataPoint> stars_;
};

}  // namespace belfry::play
