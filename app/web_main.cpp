// Belfry Bats in a browser: the night (src/sim) on Strata's WebGL 2 backend
// (Emscripten), with the page's words and buttons in web/shell.html.
//
// The page tells the game where the pointer is (belfry_pointer: 0..1 across
// and down the canvas); while it is down (a finger) or over the canvas (a
// mouse), the flock heads for the point under it, every frame, so holding it
// ahead of the flock keeps them flying. The page reads the game's state as
// JSON (belfry_status) for its words. Nothing is saved but the page's best
// score, which the page keeps itself.
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <string>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "audio/cues.h"
#include "audio/device.h"
#include "play/view.h"
#include "sim/autopilot.h"
#include "sim/world.h"
#include "strata/strata.hpp"

using namespace belfry;

namespace {

struct Game {
    std::unique_ptr<strata::Engine> engine;
    std::unique_ptr<play::Scene> scene;
    audio::Sound sound;
    bool soundTried = false;
    World world;
    double last = 0, acc = 0, real = 0;
    float px = 0.5f, py = 0.5f;
    bool pointing = false, paused = false, pilot = false, mouse = false;
    std::string status;
};

Game* g = nullptr;

void tick(double dt) {
    World& w = g->world;
    if (!g->paused && w.phase == Phase::Flying) {
        g->acc += dt;
        int steps = 0;
        while (g->acc >= kDt && steps < 4) {
            Input in;
            if (g->pilot) {
                in = autopilot(w);
            } else if (g->pointing) {
                in.guide = g->scene->unproject(g->px, g->py);
                in.steering = true;
            }
            step(w, in);
            for (audio::Cue c : audio::cues(w)) g->sound.play(c);
            g->acc -= kDt;
            ++steps;
        }
        if (steps == 4) g->acc = 0;  // a slow device: the night slows with it rather than jumping
    } else {
        g->acc = 0;
    }
    g->sound.setAmbience(audio::ambience(w));
}

void frame(void*) {
    StrataEvent e;
    while (g->engine->pollEvent(e)) {
    }
    const double now = emscripten_get_now() / 1000.0;
    double dt = now - g->last;
    g->last = now;
    if (dt > 0.25) dt = 0.25;
    if (dt < 0) dt = 0;
    g->real += dt;
    tick(dt);
    if (g->scene->loadedNight() != g->world.night) g->scene->load(g->world);
    g->scene->follow(g->world, static_cast<float>(dt));
    g->scene->draw(g->world, static_cast<float>(g->real), g->mouse && g->pointing);
    g->engine->present();
}

void startSound() {
    if (g->soundTried) return;
    g->soundTried = true;
    g->sound.start();
}

const char* endingName(Ending e) {
    switch (e) {
        case Ending::AllHome: return "home";
        case Ending::Dawn: return "dawn";
        case Ending::NoneLeft: return "none";
        default: return "";
    }
}

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Title: return "title";
        case Phase::Flying: return "flying";
        case Phase::NightOver: return "over";
        default: return "finished";
    }
}

}  // namespace

extern "C" {

// The pointer, 0..1 across and down the canvas; active: a finger down, or a
// mouse over the canvas.
EMSCRIPTEN_KEEPALIVE void belfry_pointer(double nx, double ny, int active, int mouse) {
    if (!g) return;
    g->px = static_cast<float>(nx);
    g->py = static_cast<float>(ny);
    g->pointing = active != 0;
    g->mouse = mouse != 0;
}
EMSCRIPTEN_KEEPALIVE void belfry_start() {
    if (!g) return;
    startSound();
    g->pointing = false;
    startNight(g->world, 0);
    g->world.score = 0;
    g->world.nightHome = {};
    for (audio::Cue c : audio::cues(g->world)) g->sound.play(c);
}
EMSCRIPTEN_KEEPALIVE void belfry_next() {
    if (!g || g->world.phase != Phase::NightOver) return;
    startSound();
    g->pointing = false;
    startNight(g->world, g->world.night + 1);
    for (audio::Cue c : audio::cues(g->world)) g->sound.play(c);
}
EMSCRIPTEN_KEEPALIVE void belfry_restart() {
    if (!g) return;
    g->world = newGame();
    g->pointing = false;
}
EMSCRIPTEN_KEEPALIVE void belfry_pause(int on) {
    if (g) g->paused = on != 0;
}
EMSCRIPTEN_KEEPALIVE void belfry_sound(int on) {
    if (!g) return;
    if (on) startSound();
    g->sound.setMuted(on == 0);
}
// For the smoke test and the curious: let the built-in pilot fly.
EMSCRIPTEN_KEEPALIVE void belfry_autopilot(int on) {
    if (g) g->pilot = on != 0;
}
// For the smoke test: the flock (and its guide) moved to x, at the belfry's height.
EMSCRIPTEN_KEEPALIVE void belfry_warp(double x) {
    if (!g || g->world.phase != Phase::Flying) return;
    World& w = g->world;
    const float y = (w.level.belfryY0 + w.level.belfryY1) * 0.5f;
    for (Bat& b : w.bats)
        if (b.state == BatState::Flock) {
            b.p = {static_cast<float>(x) + b.offset.x, y + b.offset.y * 0.5f};
            b.v = {1, 0};
        }
    w.guide = {static_cast<float>(x), y};
    w.centre = w.guide;
    g->scene->follow(w, 0);
}
// For the smoke test: where on the canvas (0..1) a world point is, so a test
// can point at it like a player.
EMSCRIPTEN_KEEPALIVE const char* belfry_guide() {
    if (!g) return "{}";
    const World& w = g->world;
    char buf[256];
    std::snprintf(buf, sizeof buf, "{\"gx\":%.3f,\"gy\":%.3f,\"cx\":%.3f,\"cy\":%.3f,\"camX\":%.3f}", static_cast<double>(w.guide.x),
                  static_cast<double>(w.guide.y), static_cast<double>(w.centre.x), static_cast<double>(w.centre.y),
                  static_cast<double>(g->scene->camera().x));
    g->status = buf;
    return g->status.c_str();
}
EMSCRIPTEN_KEEPALIVE void belfry_resize(double w, double h) {
    if (!g || w < 1 || h < 1) return;
    g->engine->resize(static_cast<int32_t>(w), static_cast<int32_t>(h));
    g->scene->follow(g->world, 0);
}
EMSCRIPTEN_KEEPALIVE const char* belfry_status() {
    if (!g) return "{}";
    const World& w = g->world;
    int strays = 0;
    for (const Bat& b : w.bats) strays += (b.state == BatState::Perched) ? 1 : 0;
    char buf[640];
    std::snprintf(buf, sizeof buf,
                  "{\"phase\":\"%s\",\"night\":%d,\"nights\":%d,\"flock\":%d,\"adrift\":%d,\"home\":%d,\"lost\":%d,"
                  "\"gathered\":%d,\"strays\":%d,\"of\":%d,\"score\":%d,\"dawn\":%.4f,\"ending\":\"%s\",\"tick\":%" PRId64
                  ",\"muted\":%s,\"pilot\":%s,\"paused\":%s,\"cx\":%.2f,\"towerX\":%.1f,\"owls\":%zu,\"nightHome\":[%d,%d,%d,%d,%d],"
                  "\"nightOf\":[%d,%d,%d,%d,%d]}",
                  phaseName(w.phase), w.night + 1, kNights, count(w, BatState::Flock) + count(w, BatState::Dazed),
                  count(w, BatState::Adrift), w.home, w.lost, w.gathered, strays, w.nightOf[static_cast<size_t>(w.night)],
                  w.score, static_cast<double>(dawnProgress(w)), endingName(w.ending), w.tick, g->sound.muted() ? "true" : "false",
                  g->pilot ? "true" : "false", g->paused ? "true" : "false", static_cast<double>(w.centre.x),
                  static_cast<double>(w.level.towerX), w.level.owls.size(), w.nightHome[0], w.nightHome[1], w.nightHome[2],
                  w.nightHome[3], w.nightHome[4], w.nightOf[0], w.nightOf[1], w.nightOf[2], w.nightOf[3], w.nightOf[4]);
    g->status = buf;
    return g->status.c_str();
}

}  // extern "C"

int main() {
    static Game game;
    g = &game;
    g->world = newGame();
    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw < 1 || ch < 1) {
        cw = 1280;
        ch = 720;
    }
    strata::EngineDesc desc;
    desc.surface = STRATA_SURFACE_WINDOW;
    desc.width = static_cast<int32_t>(cw);
    desc.height = static_cast<int32_t>(ch);
    desc.title = "Belfry Bats";
    try {
        g->engine = strata::Engine::create(desc);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "belfry-bats: cannot start the renderer: %s\n", ex.what());
        std::string js = "if (Module.onRendererFailed) Module.onRendererFailed(";
        js += '"';
        for (const char* p = ex.what(); *p; ++p)
            if (*p != '"' && *p != '\\' && static_cast<unsigned char>(*p) >= 0x20) js += *p;
        js += "\")";
        emscripten_run_script(js.c_str());
        return 1;
    }
    g->scene = std::make_unique<play::Scene>(*g->engine);
    g->scene->load(g->world);
    g->last = emscripten_get_now() / 1000.0;
    emscripten_run_script("if (Module.onGameReady) Module.onGameReady()");
    emscripten_set_main_loop_arg(frame, nullptr, 0, false);
    return 0;
}
