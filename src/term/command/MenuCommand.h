#pragma once
#include <cstdint>

class MenuCommand;

class IMenuHost {
public:
    virtual ~IMenuHost() = default;
    virtual void cmdPush(const char* text) = 0;
    virtual void outPush(const char* text) = 0;
    virtual void menuClose() = 0;
    virtual void menuBack()  = 0;
    virtual void openSubMenu(MenuCommand* cmd) = 0;
    virtual void setPendingTheme(int8_t idx) = 0;
    virtual void setPendingBrightness(uint8_t val255) = 0;
    virtual void setPendingPowerOff() = 0;
    virtual void     startCrack(const char* pcapPath, const char* dictPath) = 0;
    virtual void     startNetgotchi() = 0;
    virtual uint32_t statsXp()         const = 0;
    virtual uint32_t statsCaptures()   const = 0;
    virtual uint32_t statsLevel()      const = 0;
    virtual uint32_t statsXpProgress() const = 0;
    virtual int      statsBattery()    const = 0;
    virtual bool     statsCharging()   const = 0;
};

class MenuCommand {
public:
    virtual ~MenuCommand() = default;
    virtual const char* label() const = 0;
    virtual void execute(IMenuHost& host) = 0;

    // Sub-menu interface — only overridden by commands that have a sub-menu
    virtual int         subCount()              const { return 0; }
    virtual const char* subLabel(int idx)       const { (void)idx; return ""; }
    virtual bool        subIsActive(int idx)    const { (void)idx; return false; }
    virtual int         subItemH()              const { return 18; }
    virtual const char* inputHint()             const { return label(); }
    virtual void onSubSelect(IMenuHost& host, int idx) { (void)host; (void)idx; }
};
