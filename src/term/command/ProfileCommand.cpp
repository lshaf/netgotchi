#include "ProfileCommand.h"
#include <SD.h>
#include <esp_heap_caps.h>
#include <stdio.h>

static const char* kSep = "+----------+----------------+";

static void rowPush(IMenuHost& host, const char* key, const char* val) {
    char buf[56];
    snprintf(buf, sizeof(buf), "| %-8s | %-14s |", key, val);
    host.outPush(buf);
}

void ProfileCommand::execute(IMenuHost& host) {
    host.menuClose();
    host.cmdPush("profile");

    char val[24];

    host.outPush(kSep);

    snprintf(val, sizeof(val), "%d%%%s",
             host.statsBattery(), host.statsCharging() ? " chg" : "");
    rowPush(host, "batt", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsLevel());
    rowPush(host, "lv", val);

    snprintf(val, sizeof(val), "%lu/100", (unsigned long)host.statsXpProgress());
    rowPush(host, "xp", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsCaptures());
    rowPush(host, "captured", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsCracked());
    rowPush(host, "cracked", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsDeauthDiscovers());
    rowPush(host, "deauth", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsFloodDiscovers());
    rowPush(host, "flood", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)host.statsEvilDiscovers());
    rowPush(host, "evil", val);

    uint32_t fH = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
                + (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t tH = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL)
                + (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    snprintf(val, sizeof(val), "%d%% used",
             tH ? (int)((uint64_t)(tH - fH) * 100 / tH) : 0);
    rowPush(host, "ram", val);

    uint64_t sdU = SD.usedBytes(), sdT = SD.totalBytes();
    if (sdT > 0)
        snprintf(val, sizeof(val), "%lu/%lu mb",
                 (unsigned long)(sdU / (1024*1024)), (unsigned long)(sdT / (1024*1024)));
    else
        strncpy(val, "no card", sizeof(val));
    rowPush(host, "sd", val);

    host.outPush(kSep);
}
