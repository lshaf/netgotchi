#pragma once
#include <cstdint>

class Stats {
public:
    void load();
    void save() const;

    // Call on each confirmed EAPOL handshake capture.
    void onCapture();

    uint32_t xp()       const { return _xp; }
    uint32_t captures() const { return _captures; }

private:
    static constexpr uint32_t MAGIC           = 0xDEADBEEF;
    static constexpr uint32_t XP_PER_CAPTURE  = 4;

    uint32_t _xp       = 0;
    uint32_t _captures = 0;
};
