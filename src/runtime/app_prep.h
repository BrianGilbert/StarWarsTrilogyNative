// The preparation thread (moved from app.cpp): while interpolating, each new
// game frame is made ready as soon as the game finishes it, its 3D scene and
// its 2D layers side by side, so the presenting thread only draws and keeps
// up with every refresh (the two took ~14 ms here, more than a refresh at
// 120 Hz, and the images went out unevenly). The interpolation delay follows
// when the frames are ready (lateness), so it includes this work
#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app_2d.h"
#include "../video/r3d_scene.h"

struct R3DFrameState;
class R3DRenderer;

// A game frame made ready for the screen ahead of time: its 3D scene and the
// 2D layers of its vblank
struct Prepared {
    std::shared_ptr<const R3DFrameState> state;
    R3DScene scene;
    Layers2D layers;
    uint64_t serial = 0;
    double ready_at = 0;   // when it was ready
};

class FramePrep {
public:
    FramePrep(R3DSceneBuilder& builder, Snapshots2D& snapshots, Compose2D& composer)
        : builder_(builder), snapshots_(snapshots), composer_(composer) {}
    ~FramePrep() { stop(); }
    void start();
    void stop();
    bool stopping() const { return stop_; }

    // One 2D composition and one scene build at a time, here or on the presenting thread
    std::mutex& work_mutex() { return work_mutex_; }
    // Interpolating: frames are prepared. Switched off or on, a frame made
    // earlier is not one to go back to
    void set_on(bool on);
    bool on() const { return on_; }
    // The newest frame made ready, if any
    std::shared_ptr<Prepared> newest();
    // Hands the renderer the new pictures (for its GPU slots) of the frames
    // made ready up to that one, also those of frames never taken
    void give_pictures(uint64_t serial, R3DRenderer& renderer);
    // Its work since the last call: microseconds and frames
    uint64_t take_us() { return us_.exchange(0); }
    uint64_t take_frames() { return frames_.exchange(0); }

private:
    void run();

    R3DSceneBuilder& builder_;
    Snapshots2D& snapshots_;
    Compose2D& composer_;
    std::thread thread_;
    std::mutex work_mutex_;
    std::mutex ready_mutex_;   // newest_ and pictures_
    std::shared_ptr<Prepared> newest_;
    // New pictures for the GPU slots, in order: handed over when their frame
    // (or a later one) is taken
    struct NewPictures {
        uint64_t serial = 0;
        std::vector<RgbaImage> sky;
    };
    std::deque<NewPictures> pictures_;
    std::atomic<bool> on_{false}, stop_{false};
    std::atomic<uint64_t> us_{0}, frames_{0};
};
