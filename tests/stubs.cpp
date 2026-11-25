// What tilegen.cpp needs from the rest of the program and the tests do not
// exercise: no backdrop pictures, so compose() takes the plain paths.
#include "sky_pack.h"

const SkyPack::Picture* SkyPack::find(const uint8_t*, int, bool) { return nullptr; }
