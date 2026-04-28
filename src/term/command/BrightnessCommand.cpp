#include "BrightnessCommand.h"
#include "../Theme.h"
#include <stdio.h>

static constexpr int BRIGHT_VALS[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static constexpr int BRIGHT_N      = 10;

int BrightnessCommand::subCount() const { return BRIGHT_N + 1; }

const char* BrightnessCommand::subLabel(int idx) const {
    static char bufs[BRIGHT_N][8] = {};
    static bool s_init = false;
    if (!s_init) {
        for (int i = 0; i < BRIGHT_N; i++)
            snprintf(bufs[i], sizeof(bufs[i]), "%d%%", BRIGHT_VALS[i]);
        s_init = true;
    }
    if (idx < BRIGHT_N) return bufs[idx];
    return "back";
}

bool BrightnessCommand::subIsActive(int idx) const {
    if (idx >= BRIGHT_N) return false;
    return (BRIGHT_VALS[idx] * 255 / 100) == (int)Theme::brightness();
}

void BrightnessCommand::onSubSelect(IMenuHost& host, int idx) {
    if (idx < BRIGHT_N) {
        int pct = BRIGHT_VALS[idx];
        char buf[32];
        snprintf(buf, sizeof(buf), "bright %d%%", pct);
        host.cmdPush(buf);
        snprintf(buf, sizeof(buf), "brightness: %d%%", pct);
        host.outPush(buf);
        host.setPendingBrightness((uint8_t)(pct * 255 / 100));
        host.menuClose();
    } else {
        host.menuBack();
    }
}
