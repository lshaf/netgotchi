#include "types.h"

// Defaults match CoreS3 (320×240 → scale=4, 80×60 virtual).
// App::init() overwrites these after M5.begin() with real display dimensions.
int SCALE     = 4;
int SCREEN_H  = 60;
int BTN_STRIP = 11;
int GROUND_Y  = 54;
