//
// CoreS3 hardware bring-up + globals.
// Hw::begin() runs the boot sequence: I2C → AXP2101 → AW9523B → HSPI → TFT.
// After begin(): use Hw::tft for the display, Hw::sd for SD ops (auto-guarded),
// Hw::axp for power/backlight, Hw::touch for capacitive touch.
//

#pragma once

#include <TFT_eSPI.h>
#include <FS.h>
#include "AXP2101.h"
#include "AW9523B.h"
#include "TouchFT6336U.h"
#include "MisoDcGuard.h"

class ExtSpiClass;  // forward; defined in Hw.cpp

namespace Hw {

extern TFT_eSPI       tft;
extern AXP2101        axp;
extern AW9523B        aw;
extern TouchFT6336U   touch;
extern fs::FS         sd;
extern bool           sdAvailable;
extern MisoDcGuard    dcGuard;

struct TouchState {
    bool isPressed   = false;
    bool wasPressed  = false;
    bool wasReleased = false;
    int16_t x = 0, y = 0;
};

extern TouchState touchState;

// Boot the board: power → reset lines → HSPI → TFT → SD.
// Returns true if SD mounted.
bool begin();

// Poll touch hardware; updates Hw::touchState. Call once per frame.
void update();

// SDClass-specific calls (totalBytes/usedBytes are not on fs::FS).
// These take MisoDcGuard internally so callers don't need to.
uint64_t sdTotalBytes();
uint64_t sdUsedBytes();

}  // namespace Hw
