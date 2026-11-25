// The preparation thread (app_prep.h, moved from app.cpp)
#include "app_prep.h"

#include <algorithm>
#include <chrono>
#include <future>

#include "app_internal.h"
#include "runtime.h"
#include "../video/r3d_renderer.h"

void FramePrep::start() { thread_ = std::thread([this] { run(); }); }

void FramePrep::stop() {
    stop_ = true;
    g_r3d_cv.notify_all();
    if (thread_.joinable()) thread_.join();
}

void FramePrep::set_on(bool on) {
    on_ = on;
    std::lock_guard<std::mutex> lock(ready_mutex_);
    newest_.reset();
}

std::shared_ptr<Prepared> FramePrep::newest() {
    std::lock_guard<std::mutex> lock(ready_mutex_);
    return newest_;
}

void FramePrep::give_pictures(uint64_t serial, R3DRenderer& renderer) {
    std::vector<NewPictures> due;
    {
        std::lock_guard<std::mutex> lock(ready_mutex_);
        while (!pictures_.empty() && pictures_.front().serial <= serial) {
            due.push_back(std::move(pictures_.front()));
            pictures_.pop_front();
        }
    }
    for (NewPictures& n : due) {
        if (!n.sky.empty()) renderer.add_sky_pictures(n.sky);
    }
}

void FramePrep::run() {
    std::vector<std::shared_ptr<Prepared>> pool;   // reused once nobody else holds them
    std::shared_ptr<const R3DFrameState> done;
    uint64_t serial = 0;
    while (!stop_) {
        std::shared_ptr<const R3DFrameState> state;
        {
            std::unique_lock<std::mutex> lock(g_r3d_mutex);
            g_r3d_cv.wait_for(lock, std::chrono::milliseconds(20), [&] { return stop_ || (on_ && g_r3d_latest != done); });
            state = g_r3d_latest;
        }
        if (!on_) done.reset();
        if (stop_ || !on_ || !state || state == done) continue;
        done = state;
        std::shared_ptr<Prepared> p;
        for (auto& q : pool)
            if (q.use_count() == 1) {
                p = q;
                break;
            }
        if (!p) pool.push_back(p = std::make_shared<Prepared>());
        p->state = state;
        p->serial = ++serial;
        NewPictures pictures;
        pictures.serial = p->serial;
        {
            std::lock_guard<std::mutex> lock(work_mutex_);
            snapshots_.take(state->vblank, p->layers);   // the 2D layers of the frame's vblank
            std::future<void> layers;
            if (p->layers.frame != ~0ull) layers = std::async(std::launch::async, [&] { composer_.compose(p->layers); });
            double from = qpc_seconds();
            builder_.set_fov_widened(g_wide_applied);
            builder_.set_view_wide(g_opt.max_aspect > 4.0f / 3.0f + 0.01f ? std::max(1.0f, g_wide_view.load()) : 1.0f);
            builder_.build(*state, p->scene);
            collect_scene_targets(builder_, p->scene);
            double built_at = qpc_seconds();
            if (layers.valid()) layers.get();
            double took = qpc_seconds() - from;
            us_ += uint64_t(took * 1e6);
            frames_++;
            const float* st = builder_.stage_ms();
            frame_log("R %llu %.6f %.3f %.3f  nodes %.2f track %.2f screen %.2f pair %.2f  polys %zu\n",
                      (unsigned long long)state->frame, qpc_seconds(), (built_at - from) * 1000, took * 1000, st[0], st[1],
                      st[2], st[3], p->scene.polygons);
            pictures.sky.swap(p->layers.sky_pictures);
            p->layers.sky_pictures.clear();
        }
        std::lock_guard<std::mutex> lock(ready_mutex_);
        pictures_.push_back(std::move(pictures));
        p->ready_at = qpc_seconds();
        if (on_) newest_ = p;
    }
}
