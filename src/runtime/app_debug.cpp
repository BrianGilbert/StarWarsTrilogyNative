// Debugging aids of the presenting loop, turned on with environment
// variables (env_switches.cpp; moved from app.cpp)
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "app_internal.h"
#include "env_switches.h"
#include "runtime.h"
#include "../ui/frontend.h"
#include "../video/r3d_renderer.h"
#include "../video/r3d_scene.h"
#include "../video/r3d_state.h"
#include "../video/vk_presenter.h"

// Debugging aid: SWT_MENU_KEYS="40,13,27" presses those keys (one
// every 10 presents; Esc in the game pauses; "cX/Y" clicks at window
// pixel X,Y), then saves out/menu.bmp and quits
void debug_menu_keys(Frontend& frontend, VkPresenter& presenter) {
    static const char* menu_keys = swt_env("SWT_MENU_KEYS");
    static int menu_presents = 0;
    if (menu_keys && ++menu_presents % 10 == 0) {
        if (*menu_keys == 'c') {
            int cx = 0, cy = 0, len = 0;
            sscanf(menu_keys, "c%d/%d%n", &cx, &cy, &len);
            menu_keys += len;
            if (*menu_keys == ',') menu_keys++;
            frontend.click(cx, cy);
        } else if (*menu_keys) {
            char* end = nullptr;
            int vk = int(strtol(menu_keys, &end, 10));
            menu_keys = *end == ',' ? end + 1 : end;
            if (frontend.active()) frontend.key(vk);
            else if (vk == VK_ESCAPE) open_pause(frontend);
        } else if (menu_presents % 30 == 0) {
            presenter.capture_next("out/menu.bmp");
            menu_keys = nullptr;
            presenter.close();
        }
    }
}

// Debugging aid: SWT_SPOTLOG=path writes the world viewport's spotlight every game frame
void debug_spot_log(const R3DScene& scene, const R3DRenderer& renderer, uint64_t frame) {
    static FILE* spot_log = swt_env("SWT_SPOTLOG") ? fopen(swt_env("SWT_SPOTLOG"), "w") : nullptr;
    if (spot_log) {
        int worlds = 0;
        for (const R3DViewport& v : scene.viewports)
            if (v.world) {
                worlds++;
                fprintf(spot_log,
                        "%llu vp%d col %.0f%.0f%.0f c %.4f %.4f r %.4f %.4f start %.3f ext %.2f sun %.3f amb %.3f "
                        "fog %.5f/%.3f scroll %.3f far %.1f black %d night %.2f "
                        "light %.3f %.3f %.3f world %.3f %.3f %.3f\n",
                        (unsigned long long)frame, worlds, v.spot_color[0], v.spot_color[1], v.spot_color[2],
                        v.spot_center[0], v.spot_center[1], v.spot_radius[0], v.spot_radius[1], v.spot_start,
                        v.spot_extent, v.sun_intensity, v.ambient, v.fog_density, v.fog_ambient, v.scroll_fog,
                        v.scenery_far, int(renderer.back_black()), renderer.night_level(), v.sun[0], v.sun[1], v.sun[2],
                        v.world_axes[0] * v.sun[0] + v.world_axes[1] * v.sun[1] + v.world_axes[2] * v.sun[2],
                        v.world_axes[3] * v.sun[0] + v.world_axes[4] * v.sun[1] + v.world_axes[5] * v.sun[2],
                        v.world_axes[6] * v.sun[0] + v.world_axes[7] * v.sun[1] + v.world_axes[8] * v.sun[2]);
            }
        if (!worlds) fprintf(spot_log, "%llu no world\n", (unsigned long long)frame);
        fflush(spot_log);
    }
}

// Debugging aid: SWT_FILM=first,count records that many game frames
// as snapshots in out/film, to replay as a sequence
void debug_film(const R3DFrameState& built, const std::vector<uint8_t>& vram) {
    static const char* film_rec = swt_env("SWT_FILM");
    static int film_saved = 0;
    if (film_rec && g_opt.replay.empty()) {
        unsigned long long first = 0;
        int count = 0, every = 1;   // optional third value: keep one frame in that many
        sscanf(film_rec, "%llu,%d,%d", &first, &count, &every);
        static unsigned long long film_last = 0;
        if (built.frame >= first && film_saved < count && (!film_saved || built.frame >= film_last + every)) {
            film_last = built.frame;
            CreateDirectoryA("out/film", nullptr);
            char path[64];
            snprintf(path, sizeof(path), "out/film/f_%06llu.bin", (unsigned long long)built.frame);
            save_snapshot(path, built, vram);
            film_saved++;
        }
    }
}

// Debugging aid: SWT_SHOTS=N[,FROM[,TO]] saves one game frame in N as out/shots/<frame>.bmp
void debug_shots(uint64_t shown, VkPresenter& presenter) {
    static int shots_every = 0;
    static unsigned long long shots_from = 0, shots_to = ~0ull;
    static bool shots_parsed = false;
    if (!shots_parsed) {
        shots_parsed = true;
        if (const char* e = swt_env("SWT_SHOTS")) sscanf(e, "%d,%llu,%llu", &shots_every, &shots_from, &shots_to);
    }
    static uint64_t last_shot = 0;
    if (shots_every > 0 && shown != ~0ull && shown >= shots_from && shown <= shots_to &&
        shown >= last_shot + shots_every) {
        last_shot = shown;
        CreateDirectoryA("out/shots", nullptr);
        char path[64];
        snprintf(path, sizeof(path), "out/shots/%06llu.bmp", (unsigned long long)shown);
        presenter.capture_next(path);
    }
}
