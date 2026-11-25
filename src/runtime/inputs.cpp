#include "inputs.h"
#include "env_switches.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

Inputs g_inputs;
std::atomic<bool> g_fixed_aim{false};
std::atomic<bool> g_swt_duel{false};
std::atomic<int> g_duel_invert{0};
std::atomic<int> g_swt_top_state{-1};
std::atomic<int> g_swt_phase_results{0};
std::atomic<float> g_sight_scale{0.8f};
std::atomic<float> g_ffb_shake{0.0f};

static std::atomic<float> g_aim_x[2] = {-1.0f, -1.0f}, g_aim_y[2] = {-1.0f, -1.0f};
static std::atomic<float> g_lever_exact[2] = {128.0f, 128.0f};   // player 1's lever before rounding

float lever_fraction(int axis) {
    float e = g_lever_exact[axis];
    int v = axis ? g_inputs.p[0].gun_y : g_inputs.p[0].gun_x;
    float d = e - float(v);
    if (v <= 0 || v >= 255 || std::fabs(d) > 0.5f) return 0.0f;
    return d;
}

void set_player_aim(int player, float x, float y) {
    g_aim_x[player] = x;
    g_aim_y[player] = y;
    if (g_fixed_aim) return;
    // The screen (0..1) spans the lever's whole travel (SWT_LEVER_RANGE
    // scales it: below 1 the edges of the screen are out of reach)
    static float range = [] {
        const char* e = swt_env("SWT_LEVER_RANGE");
        float r = e ? float(atof(e)) : 1.0f;
        return r > 0 ? r : 1.0f;
    }();
    // In the lightsaber duels the game turns the lever upside down (up
    // and down, as a flight lever): put back, so the sight goes where the
    // mouse, the stick or the light gun goes (left and right were right)
    if (g_swt_duel) y = 1.0f - y;
    // INVERT IN DUELS: some players find the duels' aim the wrong way
    // round all the same (the regular levels are left as they are)
    if (g_swt_duel) {
        int inv = g_duel_invert;
        if (inv & 1) x = 1.0f - x;
        if (inv & 2) y = 1.0f - y;
    }
    float lx = 128 + (x - 0.5f) * 255 * range, ly = 128 + (y - 0.5f) * 255 * range;
    if (player == 0) g_lever_exact[0] = lx, g_lever_exact[1] = ly;
    g_inputs.p[player].gun_x = std::clamp(int(std::floor(lx + 0.5f)), 0, 255);
    g_inputs.p[player].gun_y = std::clamp(int(std::floor(ly + 0.5f)), 0, 255);
}

PlayerAim player_aim(int player) {
    float x = g_aim_x[player], y = g_aim_y[player];
    return {x, y, x >= 0 && y >= 0 && x <= 1 && y <= 1};
}

int pad_player(int pad, uint32_t connected) {
    if (pad < 0 || pad >= 32) return 1;
    uint32_t below = connected & ((1u << pad) - 1);   // a pad before this one is player 1's
    return below ? 1 : 0;
}

static double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// A button held for 0.1 s (a pad's BACK = coin, START = start)
static double g_coin_at = -1, g_start_at = -1;
void press_coin() { g_coin_at = now_seconds(); }
void press_start() { g_start_at = now_seconds(); }

void join_update() {
    double now = now_seconds();
    if (g_coin_at >= 0 && now - g_coin_at < 0.1) g_inputs.p[0].coin = true;
    if (g_start_at >= 0 && now - g_start_at < 0.1) g_inputs.p[0].start = true;
}

void AutoInput::parse(const std::string& s) {
    size_t p = 0;
    while (p < s.size()) {
        size_t comma = s.find(',', p);
        std::string item = s.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
        size_t at = item.find('@');
        if (at != std::string::npos) events.push_back({item.substr(0, at), strtoull(item.c_str() + at + 1, nullptr, 10)});
        if (comma == std::string::npos) break;
        p = comma + 1;
    }
}

void AutoInput::apply(uint64_t frame) {
    for (auto& e : events) {
        bool on = frame >= e.frame && frame < e.frame + 6;
        if (frame < e.frame || frame > e.frame + 6) continue;
        // "aim2:X/Y" and a trailing 2 on a button are player 2's
        bool aim = e.name.rfind("aim", 0) == 0;
        int player = aim ? e.name.rfind("aim2:", 0) == 0 : e.name.size() > 1 && e.name.back() == '2';
        std::string name = aim || !player ? e.name : e.name.substr(0, e.name.size() - 1);
        PlayerInputs& pl = g_inputs.p[player];
        if (name == "coin") pl.coin = on;
        else if (name == "start") pl.start = on;
        else if (name == "trigger") pl.trigger = on;
        else if (name == "test") g_inputs.test = on;
        else if (name == "service") g_inputs.service = on;
        else if (name == "reload") pl.offscreen = pl.trigger = on;   // shot outside the screen
        else if (aim && frame == e.frame) {   // aimX/Y: raw gun position from then on
            int x = 128, y = 128;
            if (sscanf(name.c_str() + (player ? 5 : 3), "%d/%d", &x, &y) == 2) pl.gun_x = x, pl.gun_y = y;
        }
    }
}
