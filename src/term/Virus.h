#pragma once
#include <M5GFX.h>
#include <cstdint>

class Virus {
public:
    enum class State : uint8_t { Idle, Active, Guard, Decrypting, Sleep };

    static void draw(M5Canvas& c, uint32_t ms, State state);

    static constexpr int BAR_H = 15;
    static constexpr int BAR_GAP = 2;
    static constexpr int SIDE  = 2 * BAR_H + BAR_GAP;    // 32
    static constexpr int X0    = 320 - 4 - SIDE;         // 284
    static constexpr int Y0    = 4;

private:
    static constexpr int R = 10;
};
