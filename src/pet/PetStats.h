#pragma once
#include <cstdint>

class PetStats {
public:
    struct Achievement {
        uint32_t    threshold;
        const char* msg;
    };
    static const Achievement ACHIEVEMENTS[];
    static constexpr int     ACH_N = 5;

    // Load from /netgotchi/pet; silently no-ops if file absent or corrupt
    void load();
    // Persist current state to /netgotchi/pet
    void save() const;

    // Called on each confirmed handshake capture.
    // Updates totalCaptures, exp, level, and checks achievements.
    // Returns true if the level increased.
    bool onCapture();

    uint32_t    totalCaptures() const { return _totalCaptures; }
    uint32_t    exp()           const { return _exp; }
    uint8_t     level()         const { return _level; }
    uint8_t     nextAch()       const { return _nextAch; }

    // Valid only immediately after onCapture() returns
    bool        leveledUp()     const { return _leveledUp; }
    const char* achMsg()        const { return _achMsg; }

private:
    static constexpr uint32_t MAGIC = 0xBABEFACE;

    uint32_t    _totalCaptures = 0;
    uint32_t    _exp           = 0;
    uint8_t     _level         = 1;
    uint8_t     _nextAch       = 0;

    // set by onCapture(), consumed by caller
    bool        _leveledUp = false;
    const char* _achMsg    = nullptr;
};
