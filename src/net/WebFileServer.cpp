#include "WebFileServer.h"
#include "WebFiles.h"
#include "../term/Theme.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <cstdarg>
#include <cstring>

void WebFileServer::begin() {
    MDNS.begin(MDNS_HOST);
    _prepareRoutes();
    _server.begin();
}

// ── Activity callback ─────────────────────────────────────────

void WebFileServer::_pushActivity(const char* fmt, ...) {
    if (!_actCb) return;
    char buf[48];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    _actCb(buf);
}

// ── Session auth ──────────────────────────────────────────────

bool WebFileServer::_isAuth(AsyncWebServerRequest* req, bool logout) {
    if (!req->hasHeader("Cookie")) return false;
    String cookies = req->header("cookie");
    int start = cookies.indexOf("session=");
    if (start == -1) return false;
    start += 8;
    int end = cookies.indexOf(';', start);
    String token = (end == -1) ? cookies.substring(start) : cookies.substring(start, end);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i] == token) {
            if (logout) _sessions[i] = "";
            return true;
        }
    }
    return false;
}

// ── Recursive directory delete ────────────────────────────────

bool WebFileServer::_removeDir(const String& path) {
    File dir = SD.open(path.c_str());
    if (!dir || !dir.isDirectory()) return false;
    while (true) {
        File f = dir.openNextFile();
        if (!f) break;
        String fp = path;
        if (!fp.endsWith("/")) fp += "/";
        fp += f.name();
        bool isD = f.isDirectory();
        f.close();
        if (isD) _removeDir(fp);
        else     SD.remove(fp.c_str());
    }
    dir.close();
    return SD.rmdir(path.c_str());
}

// ── Routes ────────────────────────────────────────────────────

void WebFileServer::_prepareRoutes() {

    // ── Static web assets (PROGMEM) ───────────────────────────
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", WEBFILE_HTML, WEBFILE_HTML_LEN);
    });
    _server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", WEBFILE_HTML, WEBFILE_HTML_LEN);
    });
    _server.on("/index.css", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/css", WEBFILE_CSS, WEBFILE_CSS_LEN);
    });
    _server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "application/javascript", WEBFILE_JS, WEBFILE_JS_LEN);
    });

    // ── Dynamic theme colours ──────────────────────────────────
    _server.on("/theme.css", HTTP_GET, [](AsyncWebServerRequest* req) {
        auto hex = [](uint16_t c, char* buf) {
            snprintf(buf, 8, "#%02X%02X%02X",
                     (uint8_t)((c >> 11) & 0x1F) * 255 / 31,
                     (uint8_t)((c >>  5) & 0x3F) * 255 / 63,
                     (uint8_t)(c         & 0x1F)  * 255 / 31);
        };
        char bg[8], fg[8], pale[8], dim[8];
        hex(Theme::BG,   bg);
        hex(Theme::FG,   fg);
        hex(Theme::PALE, pale);
        hex(Theme::DIM,  dim);
        char css[120];
        snprintf(css, sizeof(css),
                 ":root{--color:%s;--background:%s;--pale:%s;--dim:%s;}",
                 fg, bg, pale, dim);
        req->send(200, "text/css", css);
    });

    // ── File download ──────────────────────────────────────────
    _server.on("/download", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!_isAuth(req)) { req->send(401, "text/plain", "not authenticated."); return; }
        if (!req->hasArg("file")) { req->send(400, "text/plain", "No file specified."); return; }
        String path = req->arg("file");
        if (!SD.exists(path.c_str())) { req->send(404, "text/plain", "File not found."); return; }
        const char* base = strrchr(path.c_str(), '/');
        _pushActivity("[web] get %.41s", base ? base + 1 : path.c_str());
        req->send(SD, path.c_str(), "application/octet-stream", true);
    });

    // ── File upload ────────────────────────────────────────────
    _server.on("/upload", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            if (!_isAuth(req)) { req->send(401, "text/plain", "not authenticated."); return; }
            if (!_uploadTempPath.isEmpty()) {
                String folder = req->arg("folder");
                if (!folder.isEmpty()) {
                    if (!folder.startsWith("/")) folder = "/" + folder;
                    if (!folder.endsWith("/"))   folder += "/";
                    int sl = _uploadTempPath.lastIndexOf('/');
                    String target = folder + _uploadTempPath.substring(sl + 1);
                    if (target != _uploadTempPath) {
                        SD.mkdir(folder.substring(0, folder.length() - 1).c_str());
                        SD.rename(_uploadTempPath.c_str(), target.c_str());
                    }
                }
                _uploadTempPath = "";
            }
            req->send(200, "text/plain", "ok.");
        },
        [this](AsyncWebServerRequest* req, String filename, size_t index,
               uint8_t* data, size_t len, bool final) {
            if (!_isAuth(req)) { req->send(401, "text/plain", "not authenticated."); return; }
            if (!index) {
                String folder = req->arg("folder");
                String path = folder.isEmpty() ? "/" : folder;
                if (!path.startsWith("/")) path = "/" + path;
                if (!path.endsWith("/"))   path += "/";
                SD.mkdir(path.substring(0, path.length() - 1).c_str());
                _uploadTempPath = path + filename;
                _fsUpload = SD.open(_uploadTempPath.c_str(), FILE_WRITE);
            }
            if (len && _fsUpload) _fsUpload.write(data, len);
            if (final && _fsUpload) {
                _fsUpload.close();
                _pushActivity("[web] put %.41s", filename.c_str());
            }
        }
    );

    // ── Command dispatch (POST /) ──────────────────────────────
    _server.on("/", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("command", true)) {
            req->send(404, "text/plain", "404");
            return;
        }
        const String cmd = req->getParam("command", true)->value();

        if (cmd != "sudo" && !_isAuth(req)) {
            req->send(401, "text/plain", "not authenticated.");
            return;
        }

        // ls
        if (cmd == "ls") {
            String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "/";
            if (path.isEmpty()) path = "/";
            File dir = SD.open(path.c_str());
            if (!dir || !dir.isDirectory()) {
                req->send(403, "text/plain", "Not a directory.");
                return;
            }
            String resp = "";
            while (true) {
                File f = dir.openNextFile();
                if (!f) break;
                resp += (f.isDirectory() ? "DIR:" : "FILE:");
                resp += f.name();
                resp += ":";
                resp += String(f.size());
                resp += "\n";
                f.close();
            }
            dir.close();
            _pushActivity("[web] ls %.38s", path.c_str());
            req->send(200, "text/plain", resp);

        // sysinfo
        } else if (cmd == "sysinfo") {
            uint64_t total = SD.totalBytes();
            uint64_t used  = SD.usedBytes();
            String resp = "netgotchi File Manager\n";
            resp += "FS:" + String(total - used) + "\n";
            resp += "US:" + String(used) + "\n";
            resp += "TS:" + String(total) + "\n";
            req->send(200, "text/plain", resp);

        // sudo (login)
        } else if (cmd == "sudo") {
            const String pw = req->hasParam("param", true)
                ? req->getParam("param", true)->value() : "";
            if (pw == WEB_PASS) {
                const String token = String(esp_random(), HEX);
                _sessionSlot = (_sessionSlot + 1) % MAX_SESSIONS;
                _sessions[_sessionSlot] = token;
                AsyncWebServerResponse* resp =
                    req->beginResponse(200, "text/plain", "Login successful");
                resp->addHeader("Set-Cookie",
                    "session=" + token + "; HttpOnly; Max-Age=86400");
                req->send(resp);
                _pushActivity("[web] login");
            } else {
                req->send(403, "text/plain", "forbidden");
                _pushActivity("[web] login failed");
            }

        // exit (logout)
        } else if (cmd == "exit") {
            _isAuth(req, true);
            AsyncWebServerResponse* resp =
                req->beginResponse(200, "text/plain", "Logged out");
            resp->addHeader("Set-Cookie",
                "session=; HttpOnly; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/");
            req->send(resp);
            _pushActivity("[web] logout");

        // rm
        } else if (cmd == "rm") {
            const String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "";
            if (path.isEmpty()) { req->send(400, "text/plain", "No path specified."); return; }
            if (!SD.exists(path.c_str())) { req->send(404, "text/plain", "Not found."); return; }
            File f = SD.open(path.c_str());
            bool isD = f.isDirectory();
            f.close();
            bool ok = isD ? _removeDir(path) : SD.remove(path.c_str());
            if (ok) {
                const char* base = strrchr(path.c_str(), '/');
                _pushActivity("[web] rm %.41s", base ? base + 1 : path.c_str());
            }
            req->send(ok ? 200 : 500, "text/plain",
                ok ? (isD ? "Directory deleted." : "File deleted.") : "Delete failed.");

        // mv (rename)
        } else if (cmd == "mv") {
            const String src = req->hasParam("src", true)
                ? req->getParam("src", true)->value() : "";
            const String dst = req->hasParam("dst", true)
                ? req->getParam("dst", true)->value() : "";
            if (src.isEmpty() || dst.isEmpty()) {
                req->send(400, "text/plain", "src or dst not specified.");
                return;
            }
            if (!SD.exists(src.c_str())) { req->send(404, "text/plain", "Source not found."); return; }
            bool ok = SD.rename(src.c_str(), dst.c_str());
            if (ok) {
                const char* bs = strrchr(src.c_str(), '/');
                const char* bd = strrchr(dst.c_str(), '/');
                _pushActivity("[web] mv %.20s>%.19s",
                              bs ? bs + 1 : src.c_str(),
                              bd ? bd + 1 : dst.c_str());
            }
            req->send(ok ? 200 : 500, "text/plain", ok ? "Moved." : "Move failed.");

        // mkdir
        } else if (cmd == "mkdir") {
            const String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "";
            if (path.isEmpty()) { req->send(400, "text/plain", "No path specified."); return; }
            bool ok = SD.mkdir(path.c_str());
            if (ok) _pushActivity("[web] mkdir %.40s", path.c_str());
            req->send(ok ? 200 : 500, "text/plain",
                ok ? "Directory created." : "Failed to create directory.");

        // touch
        } else if (cmd == "touch") {
            const String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "";
            if (path.isEmpty()) { req->send(400, "text/plain", "No path specified."); return; }
            File f = SD.open(path.c_str(), FILE_WRITE);
            if (f) {
                f.close();
                const char* base = strrchr(path.c_str(), '/');
                _pushActivity("[web] touch %.39s", base ? base + 1 : path.c_str());
                req->send(200, "text/plain", "File created.");
            } else {
                req->send(500, "text/plain", "Failed to create file.");
            }

        // cat
        } else if (cmd == "cat") {
            const String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "";
            if (path.isEmpty()) { req->send(400, "text/plain", "No path specified."); return; }
            if (!SD.exists(path.c_str())) { req->send(404, "text/plain", "Not found."); return; }
            const char* base = strrchr(path.c_str(), '/');
            _pushActivity("[web] cat %.38s", base ? base + 1 : path.c_str());
            req->send(SD, path.c_str(), "text/plain");

        // echo (write file)
        } else if (cmd == "echo") {
            const String path = req->hasParam("path", true)
                ? req->getParam("path", true)->value() : "";
            const String content = req->hasParam("content", true)
                ? req->getParam("content", true)->value() : "";
            if (path.isEmpty()) { req->send(400, "text/plain", "No path specified."); return; }
            File f = SD.open(path.c_str(), FILE_WRITE);
            if (!f) { req->send(500, "text/plain", "Failed to open file."); return; }
            f.print(content);
            f.close();
            const char* base = strrchr(path.c_str(), '/');
            _pushActivity("[web] write %.38s", base ? base + 1 : path.c_str());
            req->send(200, "text/plain", "Content written.");

        } else {
            req->send(404, "text/plain", "command not found");
        }
    });

    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "404");
    });
}
