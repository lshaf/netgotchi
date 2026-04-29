#pragma once
#include "MenuCommand.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

struct CrackHandshake {
    char     ssid[33]     = {};
    uint8_t  ssid_len     = 0;
    uint8_t  ap[6]        = {};
    uint8_t  sta[6]       = {};
    uint8_t  anonce[32]   = {};
    uint8_t  snonce[32]   = {};
    uint8_t  mic[16]      = {};
    uint8_t  eapol[300]   = {};
    uint16_t eapol_len    = 0;
    uint8_t  prf_data[76] = {};
};

class CrackCommand : public MenuCommand {
public:
    const char* label() const override { return "crack"; }
    void execute(IMenuHost& host) override;

    int         subCount()           const override;
    const char* subLabel(int idx)    const override;
    bool        subIsActive(int idx) const override { (void)idx; return false; }
    int         subItemH()           const override { return 14; }
    const char* inputHint()          const override;
    void        onSubSelect(IMenuHost& host, int idx) override;

    void         stopService(IMenuHost& host)                        override { (void)host; stop(); }
    bool         isRunning()                                   const { return _crackState == CrackState::Running; }
    Virus::State virusState()                                  const override { return Virus::State::Decrypting; }
    bool         progressLine(char* buf, int len, uint32_t ms) const override;
    uint32_t tested()     const { return _crackCtx.tested; }
    uint32_t bytesDone()  const { return _crackCtx.bytesDone; }
    uint32_t fileSize()   const { return _crackCtx.fileSize; }
    uint32_t startMs()    const { return _crackStartMs; }

    void update(IMenuHost& host, uint32_t ms) override;
    void stop();

private:
    // ── Sub-menu state ─────────────────────────────────────────
    enum SubState : uint8_t { kPcap, kDict };
    static constexpr int MAX_FILES = 10;

    SubState _subState   = kPcap;
    bool     _pcapLoaded = false;
    char     _selPcap[64]              = {};
    char     _filePaths[MAX_FILES][64] = {};
    char     _fileNames[MAX_FILES][52] = {};
    int      _fileCount  = 0;
    mutable char _hint[52] = {};

    void _loadPcapList();
    void _loadDictList();
    static const char* _basename(const char* path) {
        const char* s = strrchr(path, '/');
        return s ? s + 1 : path;
    }

    // ── Crack engine ───────────────────────────────────────────
    static constexpr int CRACK_QUEUE_DEPTH = 16;
    static constexpr int CRACK_PASS_MAX    = 64;

    struct CrackPwEntry { char pw[CRACK_PASS_MAX]; uint8_t len; };

    struct CrackCtx {
        CrackHandshake    hs;
        char              wordlistPath[64] = {};
        QueueHandle_t     queue              = nullptr;
        SemaphoreHandle_t doneSem            = nullptr;
        TaskHandle_t      workerHandles[2]   = {};
        volatile bool     stop             = false;
        volatile bool     done             = false;
        volatile bool     found            = false;
        char              foundPass[64]    = {};
        volatile uint32_t tested           = 0;
        volatile uint32_t bytesDone        = 0;
        volatile uint32_t fileSize         = 0;
    };

    enum class CrackState : uint8_t { Idle, Running };
    CrackState   _crackState      = CrackState::Idle;
    CrackCtx     _crackCtx        = {};
    TaskHandle_t _crackProdHandle = nullptr;
    char         _crackPcapPath[64] = {};
    uint32_t     _crackStartMs    = 0;
    bool         _pendingStart    = false;

    void _startCrack(IMenuHost& host);
    static void _crackWorkerTask(void* param);
    static void _crackProdTask  (void* param);
};
