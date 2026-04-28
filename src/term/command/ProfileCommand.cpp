#include "ProfileCommand.h"
#include <SD.h>
#include <esp_heap_caps.h>
#include <stdio.h>

void ProfileCommand::execute(IMenuHost& host) {
    host.menuClose();
    host.cmdPush("profile");

    char buf[56];

    snprintf(buf, sizeof(buf), "battery: %d%%%s",
             host.statsBattery(), host.statsCharging() ? " [chg]" : "");
    host.outPush(buf);

    uint32_t lvCur  = host.statsLevel();
    uint32_t lvNext = lvCur + 1;
    uint32_t prog   = host.statsXpProgress();
    snprintf(buf, sizeof(buf), "lv:    %lu → %lu", (unsigned long)lvCur, (unsigned long)lvNext);
    host.outPush(buf);
    snprintf(buf, sizeof(buf), "xp:    %lu / 100 (%lu%%)", (unsigned long)prog, (unsigned long)prog);
    host.outPush(buf);

    snprintf(buf, sizeof(buf), "brain: %lu cap", (unsigned long)host.statsCaptures());
    host.outPush(buf);

    uint32_t fH = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
                + (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t tH = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL)
                + (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    snprintf(buf, sizeof(buf), "ram:   %d%% used",
             tH ? (int)((uint64_t)(tH - fH) * 100 / tH) : 0);
    host.outPush(buf);

    uint64_t sdU = SD.usedBytes(), sdT = SD.totalBytes();
    if (sdT > 0)
        snprintf(buf, sizeof(buf), "sd:    %lu mb / %lu mb",
                 (unsigned long)(sdU / (1024*1024)), (unsigned long)(sdT / (1024*1024)));
    else
        snprintf(buf, sizeof(buf), "sd:    no card");
    host.outPush(buf);
}
