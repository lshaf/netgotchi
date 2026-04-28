#include "CrackCommand.h"

void CrackCommand::execute(IMenuHost& host) {
    host.menuClose();
    host.startCrack();
}
