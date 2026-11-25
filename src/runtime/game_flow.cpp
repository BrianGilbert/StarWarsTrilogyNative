// The game's flow under the menu (moved from runtime.cpp): PLAY and QUIT TO
// MAIN MENU run on to the mission select unseen, DEBUG jumps to a phase, a
// game that ends goes back behind the menu, and the lever's force feedback
// becomes pad rumble. Called by rt_event once a frame (game_flow.h)
#define WIN32_LEAN_AND_MEAN   // runtime.h brings windows.h: no min/max macros (std::max)
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "drive_board.h"
#include "env_switches.h"
#include "game_flow.h"
#include "inputs.h"
#include "ppc_rt.h"
#include "runtime.h"
#include "stats.h"

// PLAY from the menu (app.cpp): the game, held at its SEGA logo, runs on
// unseen, with a coin and START every second (the logo takes neither), until
// its mode byte (0x1BC85B: 0 attract, 1 credit taken, 4 mission select,
// 9 playing) reaches the mission select; true once the mission select is up
//
// QUIT TO MAIN MENU (g_arcade_entry 3; Star Wars Trilogy has no state point to
// put a home state back from): the game is over at once, its shield
// (0x22159A) kept at 0 until it is back in the attract, and on to the mission
// select again, all behind the menu
// DEBUG (menu): from the mission select on to a phase of a stage, unseen.
// The game's top state (0x2210E8) runs a stage as a pair (init, run):
// 2/3 Yavin, 4/5 Hoth, 6/7 Endor, 8/9 a second space stage, 10/11 and 12/13
// the stages of area 0x341. A stage's run state steps through a script whose
// step is 0x221136 (each step a function of a table, e.g. Hoth's at
// 0x14631C): a phase plays in one step (Hoth: 10 snow, 18 tunnels, 24
// hangar; the space stages 11, 19 / 21, 27 / 28), and the steps after it end
// the phase (its closing scene, the next phase's textures and models, the
// fade). The stage the mission select starts is the one asked for (its
// entry is set), and once in a phase's play the script is moved on to the
// step after it, phase by phase, so each one ends and the next is set up as
// in a game played through
namespace {
struct DebugStageSteps {
    uint32_t init;
    int play[3];   // the step each phase plays in
};
const DebugStageSteps DEBUG_STEPS[6] = {
    {2, {11, 19, 27}},   // Yavin
    {4, {10, 18, 24}},   // Hoth
    {6, {10, 18, 24}},   // Endor
    {8, {11, 21, 28}},   // space stage 2
    {10, {0, 0, 0}},
    {12, {0, 0, 0}},
};
}  // namespace

static bool debug_jump_frame(uint64_t from) {
    stats_game_cheated();   // a game jumped through: no records
    static int jumps = 0;
    static uint64_t wait_at = 0;
    int choice = g_debug_jump;
    const DebugStageSteps& d = DEBUG_STEPS[std::clamp(choice / 4, 0, 5)];
    int phase = std::clamp(choice % 4, 0, 2);
    PlayerInputs& p = g_inputs.p[0];
    uint32_t state = rd32(0x2210E8);
    int step = int16_t(rd16(0x221136));
    auto finish = [&](const char* why) {
        printf("[debug] %s: state %u step %d after %llu frames\n", why, state, step, (unsigned long long)(g_frame - from));
        jumps = 0, wait_at = 0;
        g_debug_jump = -1;
        return true;
    };
    if (g_frame - from > 30000) return finish("gave up");
    // The mission select's entry (0x23B53C: 0 Hoth, 1 Yavin, 2 Endor) picks
    // the stage state and the episode (0x221548 bits 12-15: 1 Yavin, 2 Hoth,
    // 3 Endor), and the episode the textures: the select itself is set to the
    // stage asked for, so it starts with its own textures
    static const int SELECT_ENTRY[6] = {1, 0, 2, -1, -1, -1};
    int entry = SELECT_ENTRY[std::clamp(choice / 4, 0, 5)];
    if (state < 2 || state > 13) {   // the mission select: the entry, then the trigger
        if (entry >= 0) wr32(0x23B53C, uint32_t(entry));
        p.trigger = (g_frame - from) % 60 < 6;
        return false;
    }
    if (!(state & 1)) {   // a stage about to start: the one asked for (the later stages: no entry of their own)
        if (state != d.init) {
            printf("[debug] stage state %u -> %u\n", state, d.init);
            wr32(0x2210E8, d.init);
        }
        return false;
    }
    if (state != d.init + 1) return false;
    // The step alone moves the script on but not the scenery: the phase is
    // also counted in these bytes (F9 RAM of Yavin's three phases played: one
    // more, or two more, each phase). Kept at the phase's value from the jump
    // until the phase plays
    struct PhaseByte { uint32_t addr; int per_phase; };
    static const PhaseByte PHASE_BYTES[] = {{0x221269, 1}, {0x221275, 1}, {0x221547, 1}, {0x221549, 1}, {0x22154A, 1},
                                            {0x221555, 2}, {0x23B4D9, 2}};
    constexpr int PHASE_BYTE_COUNT = int(sizeof(PHASE_BYTES) / sizeof(PHASE_BYTES[0]));
    static uint8_t base[PHASE_BYTE_COUNT];
    if (jumps > 0)
        for (int i = 0; i < PHASE_BYTE_COUNT; i++)
            wr8(PHASE_BYTES[i].addr, uint8_t(base[i] + jumps * PHASE_BYTES[i].per_phase));
    if (jumps < phase) {
        if (step == d.play[jumps]) {
            if (!wait_at) wait_at = g_frame;
            if (g_frame - wait_at >= 30) {
                printf("[debug] phase %d playing (step %d): on to step %d\n", jumps + 1, step, step + 1);
                if (jumps == 0)
                    for (int i = 0; i < PHASE_BYTE_COUNT; i++) base[i] = rd8(PHASE_BYTES[i].addr);
                wr16(0x221136, uint16_t(step + 1));
                jumps++;
                wait_at = 0;
            }
        }
        return false;
    }
    if (step < d.play[phase]) return false;
    return finish("in the phase");
}

// The game's main loop (0x29C0) runs a sequence, its step at 0x2210FC
// (table 0x118C40): 0 attract init (0x16F4), 1 the attract (0x179C: title,
// rankings, demo; its own states at 0x2210E0), 2 game init (0x24DC: top state
// 0x2210E8 to 0), 3 the game (0x256C: the top states, the mission select's
// credit state 1 included). The last game over state (50, 0x25D4) only steps
// the sequence on, to 4, an empty entry that wraps it to 0: back in the
// attract. The mode byte is not a sure sign of it: it can stay at 9 through
// the attract's title and rankings (it goes to 0 only once the attract's demo
// game starts), so a game over waited on by the mode byte alone left the
// title on screen. In the attract: the mode byte at 0, or the sequence out
// of its game steps (2, 3) with the mode byte left from a game (6-9; 1-4 is
// a credit being taken)
bool in_attract() {
    uint8_t mode = rd8(0x1BC85B);
    uint32_t seq = rd32(0x2210FC);
    return mode == 0 || (seq != 2 && seq != 3 && mode >= 6);
}

bool arcade_entry_frame() {
    static uint64_t from = 0, select_at = 0, settle = 0;
    PlayerInputs& p = g_inputs.p[0];
    p.trigger = p.offscreen = p.event = false;
    uint8_t mode = rd8(0x1BC85B);
    bool attract = in_attract();
    if (g_arcade_entry == 3) {
        from = g_frame, select_at = 0;
        g_arcade_entry = mode == 4 ? 2 : 4;   // at the mission select already: nothing to leave
    }
    if (g_arcade_entry == 4) {
        p.coin = p.start = false;
        if (!attract && g_frame - from < 12000) {
            if (swt_playing()) {   // the shield (0x22159A, player struct; 0x1BC974 a copy)
                wr16(0x22159A, 0);
                wr32(0x1BC974, 0);
            }
            return false;
        }
        printf("[play] back in the attract after %llu frames (mode %u, sequence %u)\n", (unsigned long long)(g_frame - from),
               unsigned(mode), unsigned(rd32(0x2210FC)));
        g_arcade_entry = 1;
        settle = 180;   // the attract clears the game over's screens (CONTINUE?) first
    }
    if (g_arcade_entry == 1) {
        g_arcade_entry = 2;
        from = g_frame, select_at = 0;
    }
    if (attract && g_frame - from < settle) {
        p.coin = p.start = false;
        return false;
    }
    if (attract && g_frame - from < 6000 + settle) {
        uint64_t k = (g_frame - from - settle) % 60;
        p.coin = k < 6;
        p.start = k >= 30 && k < 36;
        return false;
    }
    p.coin = p.start = false;
    // 1-3: the credit taken, the SEGA logo or the game's start screens still
    // up. Done only at the mission select (4) or past it (a DEBUG jump); the
    // menu is never left over anything else, short of giving up
    if (mode != 4 && !(g_debug_jump >= 0 && select_at) && g_frame - from < 9000) return false;
    if (!select_at) {
        select_at = g_frame;
        printf("[play] mission select after %llu frames (mode %u)%s\n", (unsigned long long)(g_frame - from), unsigned(mode),
               mode == 4 ? "" : ": not reached, given up");
    }
    if (g_frame - select_at < 20) return false;   // its first frames drawn
    if (g_debug_jump >= 0 && !debug_jump_frame(select_at)) return false;
    g_arcade_entry = 0;
    settle = 0;
    return true;
}

// The attract never shows with the menu on: a game that ends (no credit at
// the CONTINUE? count, the game over, the ending and its name entry) goes
// back to the attract (its sequence out of the game steps). On that frame,
// before its picture is taken, the run on to the mission select starts behind the main menu,
// unseen and muted, as QUIT TO MAIN MENU does (arcade_entry_frame from entry
// 3), so the attract's title, rankings and demo never get on screen
void attract_guard_frame() {
    static bool was = true;
    static uint32_t seq_was = 0;
    uint32_t seq = rd32(0x2210FC);
    if (seq != seq_was) printf("[play] sequence %u -> %u (mode %u, frame %llu)\n", unsigned(seq_was), unsigned(seq),
                               unsigned(rd8(0x1BC85B)), (unsigned long long)g_frame);
    seq_was = seq;
    // The game is over only once its sequence leaves the game steps (2, 3):
    // the last game over state steps it on to 4, which wraps to 0. The mode
    // byte is left out here: a game goes on over several missions (one
    // cleared, back to the mission select for the next, the duels after the
    // second and third, then the last stage), and a mode byte at 0 in
    // between was taken for a game over, so a cleared mission ended the game
    // at the main menu (issue #6)
    bool attract = seq != 2 && seq != 3;
    bool ended = !was && attract;
    was = attract;
    if (!ended || !g_skip_attract || g_arcade_entry || g_boot_skip) return;
    printf("[play] the game is over: back to the menu, on to the mission select unseen (frame %llu)\n",
           (unsigned long long)g_frame);
    set_unseen(true);
    g_arcade_entry = 3;
    g_game_over_menu = true;
}

// The lever's force feedback as gamepad rumble: the drive board drives two
// motors (0x7F = no force). Their slow part is the spring that holds the
// lever to the middle (it follows the lever); the quick changes on top are
// the game's jolts and shakes (hits, crashes, the sabres clashing), kept for
// the pads (app.cpp). SWT_FFB_LOG prints the motors as they change
void ffb_frame() {
    static const bool log = swt_env_on("SWT_FFB_LOG");
    static float slow[2] = {0, 0};
    static int last[2] = {-1, -1};
    int m[2] = {g_drive.motor_x(), g_drive.motor_y()};
    float shake = 0;
    for (int a = 0; a < 2; a++) {
        float f = float(m[a] - 0x7F);
        slow[a] += 0.2f * (f - slow[a]);
        shake = std::max(shake, std::fabs(f - slow[a]) / 128.0f);
    }
    if (log && (m[0] != last[0] || m[1] != last[1]))
        printf("[ffb] frame %llu  x %02X y %02X  shake %.2f  lever %d %d\n", (unsigned long long)g_frame, m[0], m[1], shake,
               int(g_inputs.p[0].gun_x), int(g_inputs.p[0].gun_y));
    last[0] = m[0], last[1] = m[1];
    float was = g_ffb_shake.load();
    if (shake > was) g_ffb_shake = shake;
}
