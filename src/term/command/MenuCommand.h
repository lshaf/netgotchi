#pragma once
#include <cstdint>
#include "../Virus.h"

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
    virtual void setPendingDisplayOff(uint16_t secs) = 0;
    virtual void setPendingPowerOff() = 0;
    virtual bool     typingIdle()      const = 0;
    virtual bool     menuIsOpen()      const = 0;
    virtual void startService(MenuCommand* cmd)      = 0;
    virtual void setCurrentService(MenuCommand* cmd) = 0;
    virtual void onCapture()  = 0;
    virtual void onCracked()  = 0;
    virtual void onDiscover() = 0;
};

class MenuCommand {
public:
    virtual ~MenuCommand() = default;
    virtual const char* label() const = 0;
    virtual void execute(IMenuHost& host) = 0;

    // Service commands override these
    virtual void         startHardware()                          {}
    virtual void         stopService(IMenuHost& host)             { (void)host; }
    virtual void         update(IMenuHost&, uint32_t)             {}
    virtual void         clearState()                             {}
    virtual Virus::State virusState()                        const { return Virus::State::Idle; }
    virtual bool         progressLine(char*, int, uint32_t)  const { return false; }

    // Sub-menu interface — only overridden by commands that have a sub-menu
    virtual int         subCount()              const { return 0; }
    virtual const char* subLabel(int idx)       const { (void)idx; return ""; }
    virtual bool        subIsActive(int idx)    const { (void)idx; return false; }
    virtual bool        subUseDim()             const { return true; }
    virtual int         subItemH()              const { return 18; }
    virtual const char* inputHint()             const { return label(); }
    virtual void onSubSelect(IMenuHost& host, int idx) { (void)host; (void)idx; }
};
