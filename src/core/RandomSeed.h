#pragma once
#include <cstdint>

class RandomSeed {
public:
    static void init();
    static void reseed();

private:
    static uint32_t _prev;
    static uint32_t _build();
};
