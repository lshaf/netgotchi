#pragma once
#include "MenuCommand.h"
#include "../../net/WiFiHunter.h"
#include "../../hw/Hw.h"
#include <SD.h>
#include <cstring>
#include <cstdio>

class CleanEapolCommand : public MenuCommand {
public:
    const char* label() const override { return "cleaneapol"; }

    void execute(IMenuHost& host) override {
        host.cmdPush("cleaneapol");
        if (!_hunter) return;

        File dir = Hw::sd.open("/netgotchi/eapol");
        if (!dir) { host.outPush("err: no eapol dir"); return; }

        int deleted = 0;
        boolean isDir = false;
        String fname;
        while (!(fname = dir.getNextFileName(&isDir)).isEmpty()) {
            if (isDir || !fname.endsWith(".pcap")) continue;

            if (!_hunter->pcapIsComplete(fname.c_str())) {
                Hw::sd.remove(fname.c_str());
                int slash = fname.lastIndexOf('/');
                String base = (slash >= 0) ? fname.substring(slash + 1) : fname;
                char buf[52];
                snprintf(buf, sizeof(buf), "del %.44s", base.c_str());
                host.outPush(buf);
                deleted++;
            }
        }
        dir.close();

        if (deleted == 0) host.outPush("nothing to clean");
        host.menuClose();
    }

    void init(WiFiHunter* hunter) { _hunter = hunter; }

private:
    WiFiHunter* _hunter = nullptr;
};
