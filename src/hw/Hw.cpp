#include "Hw.h"
#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

// ── Pin map (M5Stack CoreS3) ──────────────────────────────────
static constexpr int INTERNAL_SDA = 12;
static constexpr int INTERNAL_SCL = 11;
static constexpr int GROVE_SDA    = 2;
static constexpr int GROVE_SCL    = 1;

static constexpr int LCD_CS       = 3;
static constexpr int SD_CS        = 4;
static constexpr int SPI_SCK_PIN  = 36;
static constexpr int SPI_MISO_PIN = 35;  // shared with TFT DC
static constexpr int SPI_MOSI_PIN = 37;

static constexpr int TOUCH_INT    = 21;

namespace Hw {

TFT_eSPI       tft;
AXP2101        axp;
AW9523B        aw;
TouchFT6336U   touch;
fs::FS         sd(fs::FSImplPtr{});
bool           sdAvailable = false;
MisoDcGuard    dcGuard;
TouchState     touchState;

static SPIClass _sdSpi(HSPI);
static bool     _lastPressed = false;

bool begin() {
    Wire1.begin(INTERNAL_SDA, INTERNAL_SCL, 400000UL);
    Wire.begin(GROVE_SDA, GROVE_SCL);

    axp.begin(Wire1);
    aw.begin(Wire1);

    pinMode(TOUCH_INT, INPUT_PULLUP);
    touch.begin(Wire1);

    pinMode(LCD_CS, OUTPUT);
    digitalWrite(LCD_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    // sdSpi.begin MUST run before tft.init so TFT_eSPI finds HSPI configured
    // with MISO=35. After tft.init, GPIO35 toggles between INPUT (SD MISO) and
    // OUTPUT (TFT DC) via the MisoDcGuard around every SD op.
    _sdSpi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);

    tft.init();
    tft.setRotation(6);
    tft.fillScreen(TFT_BLACK);

    dcGuard.init(&_sdSpi, SPI_MISO_PIN);
    {
        MisoDcGuard::Scope s(dcGuard);
        sdAvailable = SD.begin(SD_CS, _sdSpi, 4000000UL);
    }
    if (sdAvailable) sd = makeGuardedFs(SD, dcGuard);
    return sdAvailable;
}

void update() {
    int16_t x = 0, y = 0;
    bool pressed = touch.read(x, y);
    touchState.wasPressed  = pressed && !_lastPressed;
    touchState.wasReleased = !pressed && _lastPressed;
    touchState.isPressed   = pressed;
    if (pressed) { touchState.x = x; touchState.y = y; }
    _lastPressed = pressed;
}

uint64_t sdTotalBytes() {
    if (!sdAvailable) return 0;
    MisoDcGuard::Scope s(dcGuard);
    return SD.totalBytes();
}

uint64_t sdUsedBytes() {
    if (!sdAvailable) return 0;
    MisoDcGuard::Scope s(dcGuard);
    return SD.usedBytes();
}

}  // namespace Hw
