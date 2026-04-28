#include "SetCommand.h"
#include "../Theme.h"
#include <stdio.h>

static constexpr int      BRIGHT_VALS[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
static constexpr int      BRIGHT_N      = 10;

static constexpr uint16_t DOFF_VALS[]   = { 0, 5, 10, 15, 30, 60, 120, 180, 300, 600 };
static constexpr int      DOFF_N        = 10;
static const char* const  DOFF_LABELS[] = {
    "off", "5s", "10s", "15s", "30s", "1m", "2m", "3m", "5m", "10m"
};

void SetCommand::execute(IMenuHost& host) {
    _section = Section::Root;
    host.openSubMenu(this);
}

int SetCommand::subCount() const {
    switch (_section) {
    case Section::Root:       return 4;
    case Section::Theme:      return Theme::COUNT + 1;
    case Section::Brightness: return BRIGHT_N + 1;
    case Section::DisplayOff: return DOFF_N + 1;
    }
    return 0;
}

const char* SetCommand::subLabel(int idx) const {
    switch (_section) {

    case Section::Root: {
        static const char* const roots[] = { "theme", "brightness", "displayoff", "back" };
        return (idx < 4) ? roots[idx] : "";
    }

    case Section::Theme:
        if (idx < Theme::COUNT) return Theme::ENTRIES[idx].name;
        return "back";

    case Section::Brightness: {
        static char bufs[BRIGHT_N][8] = {};
        static bool init = false;
        if (!init) {
            for (int i = 0; i < BRIGHT_N; i++)
                snprintf(bufs[i], sizeof(bufs[i]), "%d%%", BRIGHT_VALS[i]);
            init = true;
        }
        if (idx < BRIGHT_N) return bufs[idx];
        return "back";
    }

    case Section::DisplayOff:
        if (idx < DOFF_N) return DOFF_LABELS[idx];
        return "back";
    }
    return "";
}

bool SetCommand::subIsActive(int idx) const {
    switch (_section) {
    case Section::Root:       return false;
    case Section::Theme:
        return idx < Theme::COUNT && idx == Theme::idx();
    case Section::Brightness:
        return idx < BRIGHT_N &&
               (BRIGHT_VALS[idx] * 255 / 100) == (int)Theme::brightness();
    case Section::DisplayOff:
        return idx < DOFF_N && DOFF_VALS[idx] == Theme::displayOffSecs();
    }
    return false;
}

int SetCommand::subItemH() const {
    switch (_section) {
    case Section::Brightness:
    case Section::DisplayOff: return 14;
    default:                  return 18;
    }
}

const char* SetCommand::inputHint() const {
    switch (_section) {
    case Section::Theme:      return "set theme";
    case Section::Brightness: return "set brightness";
    case Section::DisplayOff: return "set displayoff";
    default:                  return "set";
    }
}

void SetCommand::onSubSelect(IMenuHost& host, int idx) {
    switch (_section) {

    case Section::Root:
        if (idx == 0) { _section = Section::Theme;      return; }
        if (idx == 1) { _section = Section::Brightness;  return; }
        if (idx == 2) { _section = Section::DisplayOff;  return; }
        host.menuBack();
        return;

    case Section::Theme:
        if (idx == Theme::COUNT) { _section = Section::Root; return; }
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "set theme %s", Theme::ENTRIES[idx].name);
            host.cmdPush(buf);
            snprintf(buf, sizeof(buf), "theme: %s", Theme::ENTRIES[idx].name);
            host.outPush(buf);
            host.setPendingTheme((int8_t)idx);
            host.menuClose();
        }
        return;

    case Section::Brightness:
        if (idx == BRIGHT_N) { _section = Section::Root; return; }
        {
            int pct = BRIGHT_VALS[idx];
            char buf[32];
            snprintf(buf, sizeof(buf), "set bright %d%%", pct);
            host.cmdPush(buf);
            snprintf(buf, sizeof(buf), "brightness: %d%%", pct);
            host.outPush(buf);
            host.setPendingBrightness((uint8_t)(pct * 255 / 100));
            host.menuClose();
        }
        return;

    case Section::DisplayOff:
        if (idx == DOFF_N) { _section = Section::Root; return; }
        {
            uint16_t secs = DOFF_VALS[idx];
            char buf[32];
            snprintf(buf, sizeof(buf), "set displayoff %s", DOFF_LABELS[idx]);
            host.cmdPush(buf);
            snprintf(buf, sizeof(buf), "displayoff: %s", DOFF_LABELS[idx]);
            host.outPush(buf);
            host.setPendingDisplayOff(secs);
            host.menuClose();
        }
        return;
    }
}
