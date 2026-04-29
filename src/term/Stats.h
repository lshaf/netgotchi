#pragma once
#include <cstdint>

class Stats {
public:
    void load();
    void save() const;

    // Call on each confirmed EAPOL handshake capture.
    void onCapture();
    // Call on each unique cracked WiFi password.
    void onCrack();
    // Call on each new guard discovery event.
    void onDeauthDiscover();
    void onFloodDiscover();
    void onEvilDiscover();

    uint32_t xp()              const { return _xp; }
    uint32_t captures()        const { return _captures; }
    uint32_t cracked()         const { return _cracked; }
    uint32_t deauthDiscovers() const { return _deauthDiscovers; }
    uint32_t floodDiscovers()  const { return _floodDiscovers; }
    uint32_t evilDiscovers()   const { return _evilDiscovers; }
    uint32_t level()           const { return (_xp / 100) + 1; }
    uint32_t xpProgress()      const { return _xp % 100; }
    int      battery()         const;
    bool     isCharging()      const;

private:
    static constexpr uint32_t XP_PER_CAPTURE    = 4;
    static constexpr uint32_t XP_PER_CRACK      = 8;
    static constexpr uint32_t XP_PER_DISCOVER   = 1;

    uint32_t _xp             = 0;
    uint32_t _captures       = 0;
    uint32_t _cracked        = 0;
    uint32_t _deauthDiscovers = 0;
    uint32_t _floodDiscovers  = 0;
    uint32_t _evilDiscovers   = 0;
};
