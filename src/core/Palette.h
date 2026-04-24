#pragma once
#include <cstdint>

namespace Palette {

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(
        ((r & 0xF8u) << 8u) | ((g & 0xFCu) << 3u) | (b >> 3u)
    );
}

// ── Night sky & scene ─────────────────────────────────────────
static constexpr uint16_t SkyTop     = rgb(0, 0, 0);
static constexpr uint16_t SkyBot     = rgb(0, 0, 0);
static constexpr uint16_t Cloud      = rgb( 18, 22, 45);

// ── Ground (dark asphalt) ─────────────────────────────────────
static constexpr uint16_t Ground     = rgb( 20, 22, 32);
static constexpr uint16_t GroundEdge = rgb( 40, 44, 60);
static constexpr uint16_t Shadow     = rgb(  5,  5, 12);

// ── Slime body (undercover purple) ───────────────────────────
static constexpr uint16_t SlimeBody  = rgb( 70, 28, 115);
static constexpr uint16_t SlimeDark  = rgb( 28, 10,  55);
static constexpr uint16_t SlimeLight = rgb(120, 72, 185);

// ── Generic ───────────────────────────────────────────────────
static constexpr uint16_t White      = rgb(255, 255, 255);
static constexpr uint16_t Black      = rgb(  0,   0,   0);
static constexpr uint16_t StarDim    = rgb( 90, 110, 160);

// ── HUD bar strip ─────────────────────────────────────────────
static constexpr uint16_t HudBg      = rgb(  5,  6, 18);
static constexpr uint16_t HudText    = rgb(150, 170, 255);
static constexpr uint16_t BarBg      = rgb( 18, 20, 32);
static constexpr uint16_t BarBdr     = rgb( 50, 54, 72);
static constexpr uint16_t HpFill     = rgb( 22, 110,  38);   // dark green
static constexpr uint16_t BrainFill  = rgb( 88,  20, 130);   // dark violet
static constexpr uint16_t ExpFill    = rgb(160, 100,  10);   // dark gold

// ── Terminal dialog ───────────────────────────────────────────
static constexpr uint16_t TermBg     = rgb(  4,  8,  4);
static constexpr uint16_t TermBdr    = rgb( 30,  90, 30);
static constexpr uint16_t TermText   = rgb( 50, 220, 80);

// ── Speech / think bubble (dark) ─────────────────────────────
static constexpr uint16_t Bubble     = rgb( 16, 20, 48);
static constexpr uint16_t BubbleBdr  = rgb( 75,  98, 178);
static constexpr uint16_t BubbleTxt  = rgb(175, 208, 255);

} // namespace Palette
