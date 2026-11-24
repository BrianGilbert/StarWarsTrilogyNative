#include "mouse_confine.h"

namespace mouse_confine {
namespace {
bool g_clipped = false;
}

void update(HWND hwnd, bool aiming) {
    POINT tl{0, 0};
    RECT rc;
    GetClientRect(hwnd, &rc);
    ClientToScreen(hwnd, &tl);
    RECT area{tl.x, tl.y, tl.x + rc.right, tl.y + rc.bottom};
    bool clip = aiming && GetForegroundWindow() == hwnd && rc.right > 0 && rc.bottom > 0;
    if (clip) ClipCursor(&area);
    else if (g_clipped) ClipCursor(nullptr);
    g_clipped = clip;
}

}  // namespace mouse_confine
