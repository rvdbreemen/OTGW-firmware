/*
***************************************************************************
**  Program  : OTGW-ModUpdateServer-esp32.h
**  Version  : v2.0.0-alpha.263
**
**  ESP32 OTA update server — functional equivalent of the ESP8266
**  OTGW-ModUpdateServer with Nodoshop hardware watchdog feeding.
**
**  TASK-865.11 (ADR-123 Phase 3): rewritten against ESPAsyncWebServer.
**  The OTA endpoint moved from the synchronous upload model (the
**  START/WRITE/END/ABORTED lifecycle polled by the loop() client pump) onto
**  the AsyncWebServer onUpload(filename,index,data,len,final) callback. The
**  callback runs on the single AsyncTCP service task; handlers are serialized
**  on that task, so the per-request member state below is safe (no concurrency,
**  no overlap between two uploads). The load-bearing flash logic — merged-binary
**  app-slot extraction, the I2C hardware-watchdog feed per write chunk, the
**  filesystem-vs-firmware target split, and the deferred-reboot-from-loop()
**  contract — is carried over unchanged; only the argument plumbing differs.
**
**  Async vs sync mapping notes:
**    - index==0 is UPLOAD_FILE_START; final==true is UPLOAD_FILE_END. The final
**      chunk carries BOTH data and final==true, so we write first, then finalize.
**    - There is no UPLOAD_FILE_ABORTED callback. request->onDisconnect() runs the
**      abort cleanup, guarded so it does not double-act after a clean upload.
**    - The async upload callback delivers the UPLOADED FILE's name (firmware.bin /
**      littlefs.bin), NOT the multipart form-field name the sync server exposed via
**      upload.name. Target detection therefore keys off the `cmd` query param the
**      form posts (cmd=0 firmware, cmd=100 filesystem) — reliably present at
**      index==0 — instead of the field name. The uploaded filename's
**      ".littlefs.bin" suffix is used as a fallback signal.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef OTGW_MOD_UPDATE_SERVER_ESP32_H
#define OTGW_MOD_UPDATE_SERVER_ESP32_H

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <StreamString.h>
#include <LittleFS.h>
#include "Wire.h"
#include "webServerCompat.h"   // webBeginRequest / webSendP / webSend / g_responseSent

// External declarations (same as the sync variant)
extern OTGWState state;
extern bool LittleFSmounted;
extern bool updateLittleFSStatus(const char *probePath);
extern bool updateLittleFSStatus(const __FlashStringHelper *probePath);
extern void writeSettings(bool show);
extern void settingsMarkClean();
extern void blinkLEDnow(uint8_t);

#ifndef Debug
  #define Debug(...)      ({ OTGWSerial.print(__VA_ARGS__); })
  #define Debugln(...)    ({ OTGWSerial.println(__VA_ARGS__); })
  #define Debugf(...)     ({ OTGWSerial.printf_P(__VA_ARGS__); })
#endif

class OTGWUpdateServer {
public:
  enum class UploadTarget : uint8_t { None, Firmware, Filesystem };

  OTGWUpdateServer(bool serial_debug = false)
    : _serial_output(serial_debug), _server(nullptr), _authenticated(false),
      _routesRegistered(false), _updateFinalized(false),
      _serverIndex(nullptr), _serverSuccess(nullptr),
      _uploadTarget(UploadTarget::None),
      _uploadExpectedBytes(0), _uploadWrittenBytes(0), _uploadBlockIndex(0),
      _mergedSkipBytes(0), _mergedWriteLimit(0) {
    _updaterError[0] = '\0';
  }

  void setup(AsyncWebServer *server) {
    setup(server, emptyString, emptyString);
  }

  void setup(AsyncWebServer *server, const String& path) {
    setup(server, path, emptyString, emptyString);
  }

  void setup(AsyncWebServer *server, const String& username, const String& password) {
    setup(server, "/update", username, password);
  }

  void setup(AsyncWebServer *server, const String& path, const String& username, const String& password) {
    _server = server;
    _username = username;
    _password = password;
    _resetUploadTracking();

    // Idempotent: AsyncWebServer::on() APPENDS handlers, so registering more than
    // once would stack duplicate /update routes (the wiring is re-touched on every
    // WiFi/Ethernet transition). Mirror startWebSocket(): register exactly once,
    // and let updateCredentials() carry credential changes on subsequent calls.
    if (_routesRegistered) return;
    _routesRegistered = true;

    AsyncWebServer &srv = *_server;

    // GET /update — serve the upload form page.
    srv.on(path.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
      webBeginRequest(request);
      if (_username.length() && _password.length() &&
          !request->authenticate(_username.c_str(), _password.c_str())) {
        webRequestAuth();
        return;
      }
      webSendP(200, PSTR("text/html"), _serverIndex);
    });

    // POST /update — completion handler (runs after the upload finished) + the
    // per-chunk upload callback. Auth is re-checked independently in each: the
    // sync model's _authenticated handoff from END into the done-lambda does not
    // map onto the async ordering, so we follow FSexplorer's /upload pattern of
    // two independent checks.
    srv.on(path.c_str(), HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        webBeginRequest(request);
        if (!_uploadAuthorized(request)) {
          webRequestAuth();
          return;
        }
        if (Update.hasError() || _updaterError[0] != '\0') {
          ::state.flash.bESPactive = false;
          if (!LittleFSmounted && _uploadTarget == UploadTarget::Filesystem) {
            LittleFSmounted = LittleFS.begin();
          }
          char errMsg[192];
          snprintf_P(errMsg, sizeof(errMsg), PSTR("Flash error: %s"), _updaterError);
          webSend(200, F("text/html"), errMsg);
        } else {
          webSendP(200, PSTR("text/html"), _serverSuccess);
          // Defer the reboot to the next loop() tick instead of calling doRestart()
          // directly from inside the async callback. Rationale (mirrors ESP8266 at
          // OTGW-ModUpdateServer-impl.h): doRestart() calls prepareForReboot()
          // which tears down MQTT/WS/OTGWstream while we are still inside the
          // request handler — and here we are on the AsyncTCP task, which must not
          // block. On slow/congested links the HTTP 200 success body may not have
          // fully drained. requestDeferredReboot() sets a flag that loop() observes
          // via isRebootPending() && !isFlashing() and then fires
          // performDeferredReboot() from the main loop — giving lwIP time to finish
          // the response before cleanup begins.
          logBootSignature("[OTA] pre-reboot");    // fourth/final OTA probe, parity with ESP8266
          requestDeferredReboot("[OTA] Rebooting...");
        }
      },
      [this](AsyncWebServerRequest *request, const String &filename,
             size_t index, uint8_t *data, size_t len, bool final) {
        _handleUpload(request, filename, index, data, len, final);
      });
  }

  void updateCredentials(const String& username, const String& password) {
    _username = username;
    _password = password;
  }

  void setIndexPage(const char *indexPage) { _serverIndex = indexPage; }
  void setSuccessPage(const char *successPage) { _serverSuccess = successPage; }

private:
  // Merged binary layout (from partitions_otgw_esp32.csv)
  // app0 slot size MUST match the active partition table value
  // (`app, ota_0, 0x10000, 0x1E0000` in partitions_otgw_esp32.csv).
  // Earlier value (0x2E0000) would trip Update.begin() range-check on
  // merged-binary OTA uploads since the writeable region is smaller.
  static constexpr size_t MERGED_APP_OFFSET = 0x10000;   // bootloader + partition table
  static constexpr size_t MERGED_APP_SIZE   = 0x1E0000;  // app0 partition size (1.875 MB)

  void _resetUploadTracking() {
    _uploadTarget = UploadTarget::None;
    _uploadExpectedBytes = 0;
    _uploadWrittenBytes = 0;
    _uploadBlockIndex = 0;
    _mergedSkipBytes = 0;
    _mergedWriteLimit = 0;
  }

  // Auth helper. Empty credentials == open (no auth configured).
  bool _uploadAuthorized(AsyncWebServerRequest *request) const {
    return (_username.length() == 0 || _password.length() == 0 ||
            request->authenticate(_username.c_str(), _password.c_str()));
  }

  // Total upload size. The form posts `size=<file.size>` on the query string
  // (see updateServerHtml.h); fall back to the multipart content length when the
  // param is absent so merged-binary detection still has a figure to compare.
  size_t _parseUploadTotalSize(AsyncWebServerRequest *request) const {
    const AsyncWebParameter *p = request->getParam("size", false);  // query string
    if (p) {
      long parsedSize = p->value().toInt();
      if (parsedSize > 0) return static_cast<size_t>(parsedSize);
    }
    size_t cl = request->contentLength();
    return cl;
  }

  // Target detection. The form action carries cmd=0 (firmware) / cmd=100
  // (filesystem) on the query string — reliably present at index==0, unlike the
  // multipart field name the async callback does not deliver. Fall back to the
  // uploaded filename's ".littlefs.bin" suffix when cmd is absent (e.g. a direct
  // curl upload without the form).
  bool _isFilesystemTarget(AsyncWebServerRequest *request, const String &filename) const {
    const AsyncWebParameter *p = request->getParam("cmd", false);  // query string
    if (p) return p->value().toInt() == 100;
    return filename.endsWith(F("littlefs.bin")) || filename.endsWith(F("filesystem.bin"));
  }

  // The async upload lifecycle dispatcher. Carries the START/WRITE/END logic of
  // the sync handler; abort is handled separately via request->onDisconnect().
  void _handleUpload(AsyncWebServerRequest *request, const String &filename,
                     size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
      _handleUploadStart(request, filename);
    }

    // Stop early on auth failure or a sticky error from begin(); the completion
    // handler emits the 401 / error page.
    if (_authenticated && _updaterError[0] == '\0') {
      _handleUploadWrite(data, len);
    }

    if (final && _authenticated && _updaterError[0] == '\0') {
      _handleUploadEnd(index + len);
    }
  }

  void _handleUploadStart(AsyncWebServerRequest *request, const String &filename) {
    _updaterError[0] = '\0';
    _resetUploadTracking();
    _updateFinalized = false;
    _authenticated = _uploadAuthorized(request);
    if (!_authenticated) {
      if (_serial_output) DebugTln(F("Unauthenticated Update"));
      return;
    }

    if (_serial_output) {
      DebugTf(PSTR("[OTA] Flash start: %s\r\n"), filename.c_str());
    }

    logBootSignature("[OTA] pre-begin");    // first of four OTA lifecycle probes, parity with ESP8266
    ::state.flash.bESPactive = true;

    // Run the abort cleanup if the client disconnects before the upload finalizes.
    // onDisconnect fires on normal close too, so the handler guards on
    // _updateFinalized to avoid double-acting after a clean upload.
    request->onDisconnect([this]() { _handleUploadAbort(); });

    size_t uploadTotal = _parseUploadTotalSize(request);
    _uploadExpectedBytes = uploadTotal;

    if (_isFilesystemTarget(request, filename)) {
      _uploadTarget = UploadTarget::Filesystem;
      size_t fsSize = LittleFS.totalBytes();
      if (_serial_output) {
        DebugTf(PSTR("[OTA] Target: filesystem (%u bytes)\r\n"), static_cast<unsigned>(fsSize));
      }
      LittleFS.end();
      if (uploadTotal > 0 && uploadTotal > fsSize) {
        strlcpy(_updaterError, "filesystem image too large", sizeof(_updaterError));
      } else if (!Update.begin(fsSize, U_SPIFFS)) {  // U_SPIFFS covers LittleFS on ESP32
        _setUpdaterError();
      }
    } else {
      _uploadTarget = UploadTarget::Firmware;
      uint32_t maxSketchSpace = (platformFreeSketchSpace() - 0x1000) & 0xFFFFF000;

      if (uploadTotal > maxSketchSpace) {
        // Merged binary (bootloader + partitions + app + filesystem).
        // Extract only the app portion: skip MERGED_APP_OFFSET bytes, write up to MERGED_APP_SIZE.
        _mergedSkipBytes  = MERGED_APP_OFFSET;
        _mergedWriteLimit = MERGED_APP_SIZE;
        _uploadExpectedBytes = MERGED_APP_SIZE;  // progress bar vs. app portion only
        if (_serial_output) {
          DebugTf(PSTR("[OTA] Merged binary detected (%u bytes) — extracting app at 0x%x (%u bytes)\r\n"),
                  static_cast<unsigned>(uploadTotal),
                  static_cast<unsigned>(MERGED_APP_OFFSET),
                  static_cast<unsigned>(MERGED_APP_SIZE));
        }
        if (!Update.begin(MERGED_APP_SIZE, U_FLASH)) {
          _setUpdaterError();
        }
      } else {
        if (_serial_output) {
          DebugTf(PSTR("[OTA] Target: firmware (%u bytes)\r\n"), static_cast<unsigned>(maxSketchSpace));
        }
        if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)) {
          _setUpdaterError();
        }
      }
    }
  }

  void _handleUploadWrite(uint8_t *data, size_t len) {
    if (len == 0) return;
    if (_serial_output) blinkLEDnow(PIN_LED1);

    // Feed the SECONDARY external 0x26 watchdog (Classic PCB) per write chunk
    // through a multi-MB upload. The PRIMARY ESP32 TWDT (ADR-135) watches the
    // loop task, which keeps resetting via doBackgroundTasks()->feedWatchDog()
    // while this async chunk write runs, so the TWDT needs no reset here. Short
    // Wire transaction, safe on the AsyncTCP task; a NACKed no-op on an OTGW32
    // where the 0x26 pins float.
    Wire.beginTransmission(0x26);
    Wire.write(0xA5);
    Wire.endTransmission();

    uint8_t *buf = data;

    // Merged binary: skip the bootloader + partition table header
    if (_mergedSkipBytes > 0) {
      if (len <= _mergedSkipBytes) {
        _mergedSkipBytes -= len;
        return;  // entire chunk is header — discard
      }
      buf += _mergedSkipBytes;
      len -= _mergedSkipBytes;
      _mergedSkipBytes = 0;
    }

    // Merged binary: stop writing once we've covered the full app partition;
    // the remainder of the upload is the filesystem image which must not
    // be written into the app OTA slot.
    if (_mergedWriteLimit > 0) {
      if (len > _mergedWriteLimit) len = _mergedWriteLimit;
      _mergedWriteLimit -= len;
    }

    if (len == 0) return;

    size_t written = Update.write(buf, len);
    if (written != len) {
      _setUpdaterError();
      return;
    }
    _uploadBlockIndex++;
    _uploadWrittenBytes += written;

    if (_serial_output && _uploadExpectedBytes > 0) {
      unsigned long pct = static_cast<unsigned long>((_uploadWrittenBytes * 100UL) / _uploadExpectedBytes);
      if (pct > 100UL) pct = 100UL;
      DebugTf(PSTR("[OTA] progress: %s block=%lu %u/%u (%lu%%)\r\n"),
              _uploadTargetStr(),
              static_cast<unsigned long>(_uploadBlockIndex),
              static_cast<unsigned>(_uploadWrittenBytes),
              static_cast<unsigned>(_uploadExpectedBytes), pct);
    }
  }

  void _handleUploadEnd(size_t totalSize) {
    if (_serial_output) DebugTln(F("[OTA] End: finalizing flash..."));

    bool updateOk = Update.end(true);
    if (updateOk) {
      if (_serial_output) DebugTf(PSTR("[OTA] End: success (%u bytes)\r\n"), static_cast<unsigned>(totalSize));
      logBootSignature("[OTA] post-end");    // second probe: after Update.end(true) commits the image
      if (_uploadTarget == UploadTarget::Filesystem) {
        LittleFSmounted = LittleFS.begin();
        if (LittleFSmounted) {
          updateLittleFSStatus(F("/.ota_post"));
          logBootSignature("[OTA] post-remount");    // third probe (FS-OTA only): after LittleFS.begin() remount
          if (_serial_output) DebugTln(F("[OTA] Restoring settings to filesystem"));
          writeSettings(false);
          settingsMarkClean();
        } else {
          LittleFSmounted = false;
          if (_serial_output) DebugTln(F("[OTA] Error: LittleFS mount failed"));
        }
      }
      if (_serial_output) {
        DebugTln(F("[OTA] Preparing to reboot..."));
        DebugFlush();
      }
    } else {
      _setUpdaterError();
    }
    _updateFinalized = true;   // suppress the onDisconnect abort cleanup on the clean close
    ::state.flash.bESPactive = false;
    _authenticated = false;  // force re-authentication on next upload
    _resetUploadTracking();
  }

  // Abort path. Async has no UPLOAD_FILE_ABORTED; request->onDisconnect() drives
  // this. onDisconnect also fires on a normal close after a clean upload, so the
  // _updateFinalized guard makes this a no-op in that case.
  void _handleUploadAbort() {
    if (_updateFinalized) return;   // clean upload already finalized — nothing to abort
    if (!::state.flash.bESPactive) return;  // never started / already cleaned up

    Update.end();
    if (_serial_output) {
      DebugTf(PSTR("[OTA] Abort: after %u bytes\r\n"), static_cast<unsigned>(_uploadWrittenBytes));
    }
    // Remount LittleFS if a filesystem upload was aborted mid-flight (we called
    // LittleFS.end() at start, so a dropped FS upload leaves assets gone).
    if (_uploadTarget == UploadTarget::Filesystem && !LittleFSmounted) {
      LittleFSmounted = LittleFS.begin();
      if (_serial_output) {
        DebugTf(PSTR("[OTA] Abort: LittleFS remount %s\r\n"), LittleFSmounted ? "OK" : "FAILED");
      }
    }
    ::state.flash.bESPactive = false;
    _authenticated = false;  // force re-authentication on next upload
    _resetUploadTracking();
  }

  const char* _uploadTargetStr() const {
    switch (_uploadTarget) {
      case UploadTarget::Firmware:   return "firmware";
      case UploadTarget::Filesystem: return "filesystem";
      default:                       return "none";
    }
  }

  void _setUpdaterError() {
    StreamString str;
    Update.printError(str);
    strlcpy(_updaterError, str.c_str(), sizeof(_updaterError));
  }

  bool _serial_output;
  AsyncWebServer *_server;
  String _username;
  String _password;
  bool _authenticated;
  bool _routesRegistered;   // guard: register /update routes exactly once
  bool _updateFinalized;    // set in END; suppresses onDisconnect abort on clean close
  char _updaterError[128];
  UploadTarget _uploadTarget;
  size_t _uploadExpectedBytes;
  size_t _uploadWrittenBytes;
  uint32_t _uploadBlockIndex;
  size_t _mergedSkipBytes;   // bytes remaining to skip (merged binary header)
  size_t _mergedWriteLimit;  // bytes remaining to write (0 = no limit); stops at app/fs boundary
  const char *_serverIndex;
  const char *_serverSuccess;
};

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/

#endif // OTGW_MOD_UPDATE_SERVER_ESP32_H
