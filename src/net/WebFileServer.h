#pragma once
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <functional>

class WebFileServer {
public:
    static constexpr const char* AP_SSID   = "netgotchi";
    static constexpr const char* MDNS_HOST = "netgotchi";
    static constexpr const char* WEB_PASS  = "netgotchi";

    using ActivityCb = std::function<void(const char*)>;

    void begin();
    void setActivityCallback(ActivityCb cb)             { _actCb      = std::move(cb); }
    void setOnCrackSaved(std::function<void()> cb)      { _crackSaveCb = std::move(cb); }

private:
    static constexpr int PORT         = 80;
    static constexpr int MAX_SESSIONS = 4;

    AsyncWebServer _server{PORT};
    File           _fsUpload;
    String         _uploadTempPath;
    int            _sessionSlot = 0;
    String         _sessions[MAX_SESSIONS];
    ActivityCb             _actCb;
    std::function<void()>  _crackSaveCb;

    void _pushActivity(const char* fmt, ...);
    void _prepareRoutes();
    bool _isAuth(AsyncWebServerRequest* req, bool logout = false);
    bool _removeDir(const String& path);
};
