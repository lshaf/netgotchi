#pragma once
#include "MenuCommand.h"
#include "../Stats.h"

class ProfileCommand : public MenuCommand {
public:
    const char* label() const override { return "profile"; }
    void init(const Stats* stats) { _stats = stats; }
    void execute(IMenuHost& host) override;

private:
    const Stats* _stats = nullptr;
};
