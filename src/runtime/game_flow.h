// The game's flow under the menu (game_flow.cpp), run by rt_event in
// runtime.cpp once a frame
#pragma once
#include <cstdint>

class DriveBoard;
extern uint64_t g_frame;    // the game's vblanks (runtime.cpp)
extern DriveBoard g_drive;  // the lever's force feedback board (runtime.cpp)

// The game is in its attract (title, rankings, demo, INSERT COIN)
bool in_attract();
// PLAY, QUIT TO MAIN MENU and DEBUG (g_arcade_entry): one frame of the run on
// to the mission select (and on to a phase); true once it is done
bool arcade_entry_frame();
// A game that ends goes on to the mission select unseen, not into the attract
void attract_guard_frame();
// The drive board's motors as pad rumble (g_ffb_shake)
void ffb_frame();
