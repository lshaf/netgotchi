#include "DisplayOffCommand.h"
#include "../Theme.h"
#include <stdio.h>

static constexpr uint16_t DOFF_VALS[] = { 0, 5, 10, 15, 30, 60, 120, 180, 300, 600 };
static constexpr int      DOFF_N      = 10;

static const char* doffLabel(int idx) {
    static const char* const labels[DOFF_N] = {
        "off", "5s", "10s", "15s", "30s", "1m", "2m", "3m", "5m", "10m"
    };
    return labels[idx];
}

int DisplayOffCommand::subCount() const { return DOFF_N + 1; }

const char* DisplayOffCommand::subLabel(int idx) const {
    if (idx < DOFF_N) return doffLabel(idx);
    return "back";
}

bool DisplayOffCommand::subIsActive(int idx) const {
    if (idx >= DOFF_N) return false;
    return DOFF_VALS[idx] == Theme::displayOffSecs();
}

void DisplayOffCommand::onSubSelect(IMenuHost& host, int idx) {
    if (idx < DOFF_N) {
        uint16_t secs = DOFF_VALS[idx];
        char buf[32];
        snprintf(buf, sizeof(buf), "displayoff %s", doffLabel(idx));
        host.cmdPush(buf);
        snprintf(buf, sizeof(buf), "displayoff: %s", doffLabel(idx));
        host.outPush(buf);
        host.setPendingDisplayOff(secs);
        host.menuClose();
    } else {
        host.menuBack();
    }
}
