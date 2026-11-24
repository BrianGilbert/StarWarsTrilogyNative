// macOS and Linux: the pointer kept in the window while it aims
// (mouse_confine.h), through SDL's mouse grab.
#include "mouse_confine.h"

#include <SDL3/SDL.h>

#include "platform_mac.h"

namespace mouse_confine {
namespace {
bool g_grabbed = false;
}

void update(HWND hwnd, bool aiming) {
    bool grab = aiming && GetForegroundWindow() == hwnd;
    if (grab != g_grabbed) SDL_SetWindowMouseGrab(mac_window(), grab);
    g_grabbed = grab;
}

}  // namespace mouse_confine
