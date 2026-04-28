#pragma once
#include "MenuCommand.h"

class ProfileCommand : public MenuCommand {
public:
    const char* label() const override { return "profile"; }
    void execute(IMenuHost& host) override;
};
