// macOS and Linux: VkPresenter's MODE and RESOLUTION through SDL3 (Windows':
// src/video/vk_presenter.cpp). Sizes here are in pixels, as on Windows: SDL's
// window and display sizes are in points (Retina) or the desktop's scaled
// units (Wayland), so they are turned into pixels with the pixel density.
#include "vk_presenter.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "platform_mac.h"
#include "../../runtime/env_switches.h"

namespace {

// SWT_BACKGROUND (test runs): the hidden window stays as it is, as a dry run
bool background() {
    static const bool on = swt_env_on("SWT_BACKGROUND");
    return on;
}

SDL_DisplayID display_of(SDL_Window* w) {
    SDL_DisplayID id = w ? SDL_GetDisplayForWindow(w) : 0;
    return id ? id : SDL_GetPrimaryDisplay();
}

int pixels(float v, float density) { return int(std::lround(v * density)); }

}  // namespace

void VkPresenter::toggle_fullscreen() {
    if (fullscreen_) set_mode(0, 0, 0);
    else set_mode(full_mode_, full_w_, full_h_);
}

void VkPresenter::set_mode(int mode, int w, int h) {
    SDL_Window* win = mac_window();
    mode = std::clamp(mode, 0, 2);
    if (mode) full_mode_ = mode, full_w_ = w, full_h_ = h;
    if (!win || background()) {
        printf("[display] dry run: mode %d, %dx%d\n", mode, w, h);
        mode_ = mode;
        fullscreen_ = mode != 0;
        return;
    }
    SDL_DisplayID id = display_of(win);
    if (mode == 0) {
        SDL_SetWindowFullscreen(win, false);
        SDL_SyncWindow(win);
        if (w > 0 && h > 0) {   // RESOLUTION: the window's inside; smaller (same shape) where it does not fit, centred
            if (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(win);
            float density = std::max(SDL_GetWindowPixelDensity(win), 0.5f);
            SDL_Rect work{};
            SDL_GetDisplayUsableBounds(id, &work);
            int top = 0, left = 0, bottom = 0, right = 0;   // the borders and the title bar (none known on Wayland)
            SDL_GetWindowBordersSize(win, &top, &left, &bottom, &right);
            float ww = float(work.w - left - right) * density, wh = float(work.h - top - bottom) * density;
            float k = std::min({1.0f, ww / float(w), wh / float(h)});
            SDL_SetWindowSize(win, std::max(320, int(float(w) * k / density + 0.5f)), std::max(240, int(float(h) * k / density + 0.5f)));
            SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id));
            SDL_SyncWindow(win);
        }
        int gw = 0, gh = 0;
        SDL_GetWindowSizeInPixels(win, &gw, &gh);
        printf("[display] window: asked %dx%d, inside now %dx%d\n", w, h, gw, gh);
    } else {
        const SDL_DisplayMode* chosen = nullptr;   // none: borderless at the desktop's resolution
        SDL_DisplayMode** modes = nullptr;
        if (mode == 2 && w > 0 && h > 0) {   // the monitor's mode of that size (the highest refresh rate first)
            int count = 0;
            modes = SDL_GetFullscreenDisplayModes(id, &count);
            for (int i = 0; modes && i < count && !chosen; i++)
                if (pixels(modes[i]->w, modes[i]->pixel_density) == w && pixels(modes[i]->h, modes[i]->pixel_density) == h)
                    chosen = modes[i];
            if (!chosen || !SDL_SetWindowFullscreenMode(win, chosen)) {   // BORDERLESS instead (the menu shows it)
                printf("[display] the monitor did not take %dx%d: BORDERLESS instead\n", w, h);
                chosen = nullptr;
                mode = 1;
            }
        }
        if (!chosen) SDL_SetWindowFullscreenMode(win, nullptr);
        SDL_SetWindowFullscreen(win, true);
        SDL_SyncWindow(win);
        if (const SDL_DisplayMode* cur = SDL_GetCurrentDisplayMode(id))
            printf("[display] %s %dx%d @%.0f Hz\n", mode == 2 ? "fullscreen" : "borderless", pixels(cur->w, cur->pixel_density),
                   pixels(cur->h, cur->pixel_density), double(cur->refresh_rate));
        SDL_free(modes);
    }
    mode_ = mode;
    fullscreen_ = mode != 0;
    resized_ = true;
}

RECT VkPresenter::monitor_rect() const {
    RECT r{0, 0, 0, 0};
    if (const SDL_DisplayMode* d = SDL_GetDesktopDisplayMode(display_of(mac_window()))) {
        r.right = pixels(d->w, d->pixel_density);
        r.bottom = pixels(d->h, d->pixel_density);
    }
    return r;
}

std::vector<std::pair<int, int>> VkPresenter::display_modes() const {
    SDL_DisplayID id = display_of(mac_window());
    std::vector<std::pair<int, int>> list;
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(id, &count);
    for (int i = 0; modes && i < count; i++)
        list.push_back({pixels(modes[i]->w, modes[i]->pixel_density), pixels(modes[i]->h, modes[i]->pixel_density)});
    SDL_free(modes);
    RECT desk = monitor_rect();
    return usual_modes(std::move(list), {int(desk.right), int(desk.bottom)});
}
