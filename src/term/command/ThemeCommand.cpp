#include "ThemeCommand.h"
#include <stdio.h>

const char* ThemeCommand::subLabel(int idx) const {
    if (idx < Theme::COUNT) return Theme::ENTRIES[idx].name;
    return "";
}

bool ThemeCommand::subIsActive(int idx) const {
    return idx < Theme::COUNT && idx == Theme::idx();
}

void ThemeCommand::onSubSelect(IMenuHost& host, int idx) {
    if (idx < Theme::COUNT) {
        char buf[48];
        snprintf(buf, sizeof(buf), "theme %s", Theme::ENTRIES[idx].name);
        host.cmdPush(buf);
        snprintf(buf, sizeof(buf), "theme: %s", Theme::ENTRIES[idx].name);
        host.outPush(buf);
        host.setPendingTheme((int8_t)idx);
        host.menuClose();
    }
}
