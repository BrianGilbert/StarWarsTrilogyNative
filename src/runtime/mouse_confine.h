// The pointer kept inside the window while it aims: with the sight under the
// pointer (pointer_sight), a pointer that left the window for a second
// monitor would leave the sight stuck at the edge.
#pragma once
#include <windows.h>

namespace mouse_confine {

// Each poll: whether the pointer aims in the game now (the window must also
// be in front); the window's client area is read again each time
void update(HWND hwnd, bool aiming);

}  // namespace mouse_confine
