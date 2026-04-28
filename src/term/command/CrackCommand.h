#pragma once
#include "MenuCommand.h"

class CrackCommand : public MenuCommand {
public:
    const char* label() const override { return "crack"; }
    void execute(IMenuHost& host) override;
};
