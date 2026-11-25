#include "env_switches.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <stdlib.h>   // _environ
#endif

namespace {

struct EnvSwitch {
    const char* name;
    const char* old_name;   // The Lost World's name, still accepted (nullptr = none)
    const char* what;
};

// Every switch, by subject. Set any value to turn on a switch that takes none.
const EnvSwitch kSwitches[] = {
    // Test runs and recordings
    {"SWT_BACKGROUND", nullptr, "Mac / Linux: the window stays hidden and errors go to the log (rendered checks without focus)"},
    {"SWT_FAST", "LW_FAST", "a window at full speed instead of real time"},
    {"SWT_FILM", "LW_FILM", "first,count: records that many game frames as snapshots in out/film, to replay as a sequence"},
    {"SWT_SHOTS", "LW_SHOTS", "N[,from[,to]]: saves one game frame in N as out/shots/<frame>.bmp"},
    {"SWT_MENU_KEYS", "LW_MENU_KEYS", "\"40,13,27\": presses those keys in the menus (\"cX/Y\" clicks), then saves out/menu.bmp and quits"},
    {"SWT_MENU_SHOT", "LW_MENU_SHOT", "path: saves the menu's picture there"},
    {"SWT_MENU_PAD", nullptr, "the menus' key help shows the gamepad's buttons"},
    {"SWT_DEBUG_MENU", nullptr, "the main menu's DEBUG entry: straight to any phase of any stage"},
    {"SWT_RESTARTED", nullptr, "set by the game when it restarts itself (a new resolution...): opens the main menu, no splash"},
    {"SWT_ROM_ZIP", nullptr, "path of the ROM zip (as --rom)"},
    {"SWT_NOSOUND", "LW_NOSOUND", "no sound board"},
    {"SWT_NOMUSIC", nullptr, "no music board (DSB2)"},
    {"SWT_DISPLAY_DRYRUN", nullptr, "a display mode change is only tested, the screen stays as it is"},

    // Logs
    {"SWT_INIT_LOG", nullptr, "times each step of the renderer's start (a driver that takes long over the shaders, or hangs)"},
    {"SWT_GPUTIME", "LW_GPUTIME", "prints the GPU time of each pass every few seconds"},
    {"SWT_PACING", "LW_PACING", "prints how evenly the game frames reach the screen, every 5 s"},
    {"SWT_FRAMELOG", "LW_FRAMELOG", "path: one line per present and per game frame there (tools/frame_log.py reads it)"},
    {"SWT_QUEUE", "LW_QUEUE", "N: presents queued ahead (0 = the whole swapchain; default 2)"},
    {"SWT_SPOTLOG", "LW_SPOTLOG", "path: the world viewport's spotlight every game frame there"},
    {"SWT_FOGDBG", "LW_FOGDBG", "prints the camera, ground and fog the haze is worked out from"},
    {"SWT_SNDDBG", "LW_SNDDBG", "prints the sound board's start"},
    {"SWT_SNDLOG", "LW_SNDLOG", "the sound board's voices in out/voices.csv and the explosions' in out/explosions.csv"},
    {"SWT_DSB_LOG", nullptr, "prints every command byte, decoder byte and stream started of the music board"},
    {"SWT_FFB_LOG", nullptr, "prints the lever's force feedback motors as they change"},
    {"SWT_GUN_LOG", nullptr, "prints the mouse / light gun aim"},
    {"SWT_WATCH", nullptr, "lo,hi (hex): prints the game's writes to that address range (300 at most)"},
    {"SWT_TILEGEN_DUMP", nullptr, "first,last,step: writes the 2D chip's memory of those frames to out/"},

    // Picture: debugging aids
    {"SWT_INTERP", "LW_INTERP", "F: the 3D always drawn that part of the way between two game frames (e.g. 0.5)"},
    {"SWT_NOFOG", "LW_NOFOG", "no fog in the game's viewports"},
    {"SWT_NOLOD", "LW_NOLOD", "every model at full detail, however far"},
    {"SWT_LOD", nullptr, "F: how much further the models keep their detail (1 = the game's distances; default 1.4)"},
    {"SWT_LOD_FADE", nullptr, "F: how much longer the detail takes to fade in (1 = the game's; default 4)"},
    {"SWT_HIDE_TEX", nullptr, "a,b,... (hex): polygons with those texture words (out/scene.txt, F9) are not drawn"},
    {"SWT_FOGSUN", "LW_FOGSUN", "x,y,z: the sun from that direction in view space"},
    {"SWT_MIST", "LW_MIST", "upwards,banks: the shape of the light in the game's fog"},
    {"SWT_RTSPLIT", "LW_RTSPLIT", "traced sun shadows on the left half, the shadow map's on the right"},
    {"SWT_SHADOW_SIZE", nullptr, "Mac: the shadow map's size (default 4096)"},
    {"SWT_DYNRES_SCALE", nullptr, "F: the 3D always drawn at that share of the size, with DLAA's dynamic resolution"},
    {"SWT_NO_DYNRES", nullptr, "DLAA always at the full size"},
    {"SWT_NO_DESCRIPTOR_INDEXING", nullptr, "renders as on a GPU without descriptor indexing"},
    {"SWT_VULKAN_1_2", nullptr, "Vulkan 1.2 with dynamic rendering as an extension, as on the Raspberry Pi 5"},

    // Picture: Star Wars Trilogy's fixes and effects, off or changed
    {"SWT_NO_CINEMA", nullptr, "space without the films' grey hulls and cinematic grade"},
    {"SWT_FAR_RELIEF", nullptr, "the surface detail reaches 1200 units instead of 120"},
    {"SWT_ACTOR_AO", nullptr, "ambient occlusion on the characters too"},
    {"SWT_APPEAR_FADE", nullptr, "models fade in from black as they appear (space showed holes)"},
    {"SWT_PLAIN_SAND", nullptr, "Tatooine's sand as it was, without grains and ripples"},
    {"SWT_PLAIN_VADER", nullptr, "Vader's suit and helmet matt"},
    {"SWT_PLAIN_DEATH_STAR", nullptr, "no Death Star texture tags (issue #23)"},
    {"SWT_NO_TRENCH_FADE", nullptr, "the trench's far end without the fade"},
    {"SWT_KEEP_WIRES", nullptr, "keeps the thin wires along the tunnels"},
    {"SWT_NO_GLOW_LIGHT", nullptr, "no light from the glowing parts (the sight...) on what is around them"},
    {"SWT_NO_WIDE_CULL", nullptr, "the game's own culling, for 4:3"},
    {"SWT_DUEL_WIDE_CULL", nullptr, "the widened culling in the lightsaber duels too (they flickered)"},
    {"SWT_EXPLOSIONS_ANYWHERE", nullptr, "the enhanced explosions everywhere the sprite is drawn, replays included"},

    // Controls
    {"SWT_NARROW_AIM", nullptr, "the lever over the 4:3 area only"},
    {"SWT_LEVER_RANGE", nullptr, "F: scales the lever's reach (below 1 the edges of the screen are out of reach)"},
    {"SWT_LEVER_STEPS", nullptr, "the game's own aim steps, without the lever's precision between them"},
    {"SWT_AIM_RADIUS", nullptr, "F: how close to a target the mouse aim snaps (world units; default 5)"},
    {"SWT_SPACE_AIM_ONLY", nullptr, "the mouse aim snaps to targets only in space"},
    {"SWT_PAD_ABSOLUTE", nullptr, "the sticks place the lever directly"},
    {"SWT_NO_FFB_RUMBLE", nullptr, "no lever force feedback on the gamepad"},
    {"SWT_DUEL_ORIGINAL", nullptr, "the lightsaber duels as any other phase: no lever turned round, no duel tweaks"},
};

const EnvSwitch* find(const char* name) {
    for (const EnvSwitch& s : kSwitches)
        if (strcmp(s.name, name) == 0 || (s.old_name && strcmp(s.old_name, name) == 0)) return &s;
    return nullptr;
}

}   // namespace

const char* swt_env(const char* name) {
    const EnvSwitch* s = find(name);
    if (!s) {   // a switch read in the code but missing above
        fprintf(stderr, "[env] %s is not listed in env_switches.cpp\n", name);
        return getenv(name);
    }
    if (const char* v = getenv(s->name)) return v;
    return s->old_name ? getenv(s->old_name) : nullptr;
}

void swt_env_log() {
    char** env = _environ;
    if (!env) return;
    for (; *env; env++) {
        if (strncmp(*env, "SWT_", 4) != 0 && strncmp(*env, "LW_", 3) != 0) continue;
        const char* eq = strchr(*env, '=');
        char name[96];
        snprintf(name, sizeof(name), "%.*s", eq ? int(eq - *env) : int(strlen(*env)), *env);
        const EnvSwitch* s = find(name);
        if (!s) printf("[env] %s (not a switch: see env_switches.cpp)\n", *env);
        else if (strcmp(s->name, name) != 0) printf("[env] %s (now %s)\n", *env, s->name);
        else printf("[env] %s\n", *env);
    }
}
