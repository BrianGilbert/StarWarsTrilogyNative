// The 2D layers on their way to the renderer (app_2d.h, moved from app.cpp)
#include "app_2d.h"

#include <cstdlib>
#include <cstring>

#include "inputs.h"
#include "runtime.h"
#include "../video/r3d_screen.h"
#include "../video/sky_pack.h"

void Snapshots2D::poll_locked() {
    std::lock_guard<std::mutex> lock(g_video_mutex);
    if (g_video_frame != snaps_[newest_].frame && !g_boot_skip) {
        newest_ = (newest_ + 1) % snaps_.size();
        snaps_[newest_].frame = g_video_frame;
        memcpy(snaps_[newest_].vram.data(), g_video_vram.data(), TILEGEN_SIZE);
    }
}

uint64_t Snapshots2D::poll() {
    std::lock_guard<std::mutex> lock(mutex_);
    poll_locked();
    return snaps_[newest_].frame;
}

void Snapshots2D::take(uint64_t up_to, Layers2D& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    poll_locked();
    const Snapshot* pick = nullptr;
    for (const Snapshot& s : snaps_)
        if (s.frame != ~0ull && s.frame <= up_to && (!pick || s.frame > pick->frame)) pick = &s;
    out.frame = pick ? pick->frame : ~0ull;
    if (pick) memcpy(out.vram.data(), pick->vram.data(), TILEGEN_SIZE);
}

void Compose2D::compose(Layers2D& out) {
    bool sky_on = sky_.active();
    TileGen::compose(out.vram.data(),
                     {.back = out.back.data(),
                      .front = out.front.data(),
                      .sky_coords = sky_on ? out.sky_coords.data() : nullptr,
                      .back_rows = back_rows_.data()},
                     sky_on ? &sky_ : nullptr);
    // How far each row of the back layers scrolled since the previous
    // 2D frame composed, for drawing them in between (renderer.set_back_shift)
    {
        uint32_t* shift_row = &out.sky_coords[(SCREEN_H + 1) * TileGen::SKY_COORDS_W];
        for (int y = 0; y < SCREEN_H; y++) {
            const int32_t* now_row = &back_rows_[3 * y];
            const int32_t* was = &back_rows_prev_[3 * y];
            int dx = ((now_row[1] - was[1] + 256) & 511) - 256, dy = ((now_row[2] - was[2] + 256) & 511) - 256;
            bool known = now_row[0] >= 0 && now_row[0] == was[0] && std::abs(dx) < 100 && std::abs(dy) < 100;
            shift_row[y] = known ? 0xFF000000u | 0xFFu | uint32_t(dx + 128) << 16 | uint32_t(dy + 128) << 8 : 0;
        }
        out.shift_from = back_rows_frame_;
        out.shift_to = back_rows_frame_ = out.frame;
        back_rows_prev_.swap(back_rows_);
    }
    out.sky_pictures = sky_.take_new_images();
    // The top band only moves to the screen edges when it looks like a
    // HUD: the score on the left, a gap, the level on the right
    auto lit = [&](int x0, int x1) {
        int n = 0;
        for (int y = 8; y < 38; y++)
            for (int x = x0; x < x1; x++) n += (out.front[y * SCREEN_W + x] >> 24) != 0;
        return n;
    };
    out.hud_band = lit(303, 318) < 8 && lit(20, 290) > 300 && lit(325, 460) > 150;
    // The lightsaber duels: SUCCESS / FAILURE at the left (x 13..138),
    // the enemy's bar and name at the right (316..479)
    // (its HUD is 3D in fact: r3d_anchor_swt_hud tags it, this lets it move)
    // (not on the duel's briefing: 2D text at the top)
    if (g_swt_duel && lit(20, 460) < 20) out.hud_band = true;
    // While playing, the choice holds for the whole phase: the game blinks
    // the top HUD off when the player is hit or blocks a shot, and blinks
    // SUCCESS / FAILURE after a duel, and those frames must not send the
    // whole HUD back to 4:3 (issues #57, #82). Text in the top band's
    // centre (a briefing in the middle of a phase; the HUD leaves it
    // empty) lets it go, and so do the phase's results screen and a top
    // band that has not looked like a HUD for a second and a half: the
    // results' text is all over the 4:3 screen, and moving it with the
    // HUD tore it apart (issue #90). It comes back after half a second of HUD
    const int stage = g_swt_top_state;
    const int results = g_swt_phase_results;
    if (stage != hud_stage_) hud_stage_ = stage, hud_held_ = hud_held_once_ = false, hud_frames_ = hud_misses_ = 0;
    if (results != hud_results_) hud_results_ = results, hud_held_ = false, hud_frames_ = hud_misses_ = 0;
    if (stage >= 0) {
        if (lit(303, 318) >= 8) {
            hud_held_ = false, hud_frames_ = hud_misses_ = 0;
        } else if (out.hud_band) {
            hud_frames_++, hud_misses_ = 0;
            if (!hud_held_once_ || hud_frames_ >= 30) hud_held_ = hud_held_once_ = true;
        } else {
            hud_frames_ = 0;
            if (hud_held_ && ++hud_misses_ >= 90) hud_held_ = false, hud_misses_ = 0;
        }
        out.hud_band = hud_held_;
    }
    r3d_set_swt_duel(g_swt_duel);
    r3d_set_swt_stage(g_swt_top_state);
}
