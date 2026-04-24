#pragma once
#include <cstdint>

// CoreS3: 320×240 native, no scaling layer
static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
static constexpr int BTN_STRIP = 36;   // HUD height in physical px
static constexpr int GROUND_Y  = 216;  // ground y in physical px

// ── Animation states ──────────────────────────────────────────
enum class Anim : uint8_t {
    Walk  = 0,
    Talk  = 1,
    Think = 2,
    Idle  = 3,
    Count = 4,
};
