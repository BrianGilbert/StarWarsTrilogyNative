// The 2D layers on their way to the renderer (moved from app.cpp): copies of
// the game's layer memory of the last vblanks, and their composition by
// TileGen with the drawn skies, the back layers' scroll and the choice of
// where the HUD sits
#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

#include "../video/image_io.h"
#include "../video/tilegen.h"

class SkyPack;

// The 2D layers of one vblank as the renderer takes them
struct Layers2D {
    uint64_t frame = ~0ull;   // the vblank
    std::vector<uint8_t> vram = std::vector<uint8_t>(TILEGEN_SIZE);   // its layer memory
    std::vector<uint32_t> back = std::vector<uint32_t>(BACK_W * SCREEN_H),
                          front = std::vector<uint32_t>(SCREEN_W * SCREEN_H),
                          sky_coords = std::vector<uint32_t>(TileGen::SKY_COORDS_W * TileGen::SKY_COORDS_H);
    std::vector<RgbaImage> sky_pictures;   // new pictures for the GPU slots
    bool hud_band = false;
    uint64_t shift_from = ~0ull, shift_to = ~0ull;   // the 2D frames of the back layers' shift
};

// The game's 2D layer memory of the last vblanks, kept in a few copies. Any thread
class Snapshots2D {
public:
    // Copies the game's newest layers if they are new; the newest vblank kept (~0 = none)
    uint64_t poll();
    // Copies the newest layers at or before that vblank into `out` (its frame
    // stays ~0 if there are none)
    void take(uint64_t up_to, Layers2D& out);

private:
    void poll_locked();

    struct Snapshot {
        uint64_t frame = ~0ull;
        std::vector<uint8_t> vram = std::vector<uint8_t>(TILEGEN_SIZE);
    };
    std::array<Snapshot, 4> snaps_;
    size_t newest_ = 0;
    std::mutex mutex_;
};

// Composes the 2D layers of a vblank (out.frame, its layer memory in out.vram)
// for the renderer. One composition at a time (the caller's lock): the back
// layers' scroll and the HUD's place keep state between them
class Compose2D {
public:
    explicit Compose2D(SkyPack& sky) : sky_(sky) {}
    void compose(Layers2D& out);

private:
    SkyPack& sky_;
    // Interpolation of the back layers (the sky): where each row's picture was
    // in the last two 2D frames composed, and the vblank of the last one
    std::vector<int32_t> back_rows_ = std::vector<int32_t>(SCREEN_H * 3),
                         back_rows_prev_ = std::vector<int32_t>(SCREEN_H * 3, -1);
    uint64_t back_rows_frame_ = ~0ull;
    // Whether the HUD sits at the widescreen edges, held for the whole phase
    // being played: the stage it was taken in, if it is held, and how many 2D
    // frames in a row the top band has looked like a HUD
    int hud_stage_ = -1;
    bool hud_held_ = false, hud_held_once_ = false;
    int hud_frames_ = 0;
    // ... the phase results screens seen, and how many 2D frames in a row the
    // top band has not looked like a HUD while it was held
    int hud_results_ = 0, hud_misses_ = 0;
};
