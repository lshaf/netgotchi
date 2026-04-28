#include "PowerOffCommand.h"

void PowerOffCommand::execute(IMenuHost& host) {
    host.menuClose();
    host.cmdPush("poweroff");
    host.outPush("shutting down...");
    host.setPendingPowerOff();
}
