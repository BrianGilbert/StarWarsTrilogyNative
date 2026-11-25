// Debugging and testing switches read from the environment (SWT_*).
//
// Every switch is listed, with what it does, in env_switches.cpp: the one
// place to add a new one. The log lists the ones set when the game starts.
// The names inherited from The Lost World (LW_FILM, LW_SHOTS...) still work
// for the switches that had them.
#pragma once

// The value of a switch listed in env_switches.cpp, nullptr = not set
const char* swt_env(const char* name);
inline bool swt_env_on(const char* name) { return swt_env(name) != nullptr; }

// Logs the SWT_ and LW_ variables set, naming any that is not a switch
void swt_env_log();
