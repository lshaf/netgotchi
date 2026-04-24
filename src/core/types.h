#pragma once
#include <cstdint>

// Virtual canvas: always 80px wide. Height and scale are set at runtime
// by App::init() once M5.Display dimensions are known.
static constexpr int SCREEN_W = 80;

extern int SCALE;      // physW / SCREEN_W
extern int SCREEN_H;   // physH / SCALE
extern int BTN_STRIP;  // HUD bar height (~18% of SCREEN_H)
extern int GROUND_Y;   // ground surface y   (~90% of SCREEN_H)

// ── Animation states ──────────────────────────────────────────
enum class Anim : uint8_t {
    Walk  = 0,
    Talk  = 1,
    Think = 2,
    Idle  = 3,
    Count = 4,
};
