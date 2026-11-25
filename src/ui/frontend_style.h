// The menus' look (frontend.cpp, frontend_draw.cpp): the colours, the fonts
// and colour mixing
#pragma once
#include <algorithm>
#include <cstdint>

#include "frontend.h"

namespace frontend_style {

// Star Wars: the title's yellow (bright at the top, deeper below) with a dark
// outline, cool blue-white letters, and lightsaber blue for the lines and the
// glow behind the selected entry
inline constexpr uint32_t GOLD_TOP = 0xFFF06A, GOLD_BOTTOM = 0xF5C400, EDGE = 0x020308;
inline constexpr uint32_t PALE_TOP = 0xEAF2FF, PALE_BOTTOM = 0xAFC2DA, DIM = 0x56606E;
inline constexpr uint32_t HINT = 0x9DB8D8, FOOT = 0x6A7A90;
inline constexpr uint32_t SABER = 0x2E9BFF, PANEL = 0x040914;

inline float chan(uint32_t rgb, int shift) { return float((rgb >> shift) & 0xFF) / 255.0f; }

inline uint32_t mix(uint32_t a, uint32_t b, float t) {
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        float v = chan(a, s) + (chan(b, s) - chan(a, s)) * t;
        r |= uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f) << s;
    }
    return r;
}

// Frontend::Family: titles, entries, typewriter text; the settings page's rows, group headings and descriptions
inline const wchar_t* const FACES[6] = {L"Saira ExtraCondensed ExCond", L"Barlow Condensed", L"Share Tech Mono",
                                        L"Barlow Condensed SemiBold", L"Barlow Condensed Medium", L"Barlow"};
inline const int WEIGHTS[6] = {FW_BOLD, FW_BOLD, FW_NORMAL, FW_SEMIBOLD, FW_MEDIUM, FW_NORMAL};

}  // namespace frontend_style
