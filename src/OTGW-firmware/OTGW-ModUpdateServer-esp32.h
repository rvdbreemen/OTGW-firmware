/*
***************************************************************************
**  Program  : OTGW-ModUpdateServer-esp32.h
**  Version  : v2.0.0-alpha.40
**
**  ESP32 OTA update server — functional equivalent of the ESP8266
**  OTGW-ModUpdateServer with Nodoshop hardware watchdog feeding.
**
**  Uses the ESP32 WebServer + Update libraries directly (no template
**  indirection like the ESP8266 variant).
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef OTGW_MOD_UPDATE_SERVER_ESP32_H
#define OTGW_MOD_UPDATE_SERVER_ESP32_H

#include <WebServer.h>
#include <Update.h>
#include <StreamString.h>
#include <LittleFS.h>
#include "Wire.h"

// External declarations (same as ESP8266 variant)
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
      _serverIndex(nullptr), _serverSuccess(nullptr),
      _uploadTarget(UploadTarget::None),
      _uploadExpectedBytes(0), _uploadWrittenBytes(0), _uploadBlockIndex(0),
      _mergedSkipBytes(0), _mergedWriteLimit(0) {
    _updaterError[0] = '\0';
  }

  void setup(WebServer *server) {
    setup(server, emptyString, emptyString);
  }

  void setup(WebServer *server, const String& path) {
    setup(server, path, emptyString, emptyString);
  }

  void setup(WebServer *server, const String& username, const String& password) {
    setup(server, "/update", username, password);
  }

  void setup(WebServer *server, const String& path, const String& username, const String& password) {
    _server = server;
    _username = username;
    _password = password;
    _resetUploadTracking();

    // Handler for the update form page (GET)
    _server->on(path.c_str(), HTTP_GET, [&]() {
      if (_username.length() && _password.length() &&
          !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), _serverIndex);
    });

    // Handler for the update form POST (after upload completes)
    _server->on(path.c_str(), HTTP_POST, [&]() {
      if (!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError() || _updaterError[0] != '\0') {
        ::state.flash.bESPactive = false;
        if (!LittleFSmounted && _uploadTarget == UploadTarget::Filesystem) {
          LittleFSmounted = LittleFS.begin();
        }
        char errMsg[192];
        snprintf_P(errMsg, sizeof(errMsg), PSTR("Flash error: %s"), _updaterError);
        _server->send(200, F("text/html"), errMsg);
      } else {
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        // Defer the reboot to the next loop() tick instead of calling doRestart()
        // directly from inside the HTTP callback. Rationale (mirrors ESP8266 at
        // OTGW-ModUpdateServer-impl.h): doRestart() calls prepareForReboot()
        // which tears down MQTT/WS/OTGWstream while we are still inside the
        // request handler. On slow/congested links the HTTP 200 success body
        // may not have fully drained. requestDeferredReboot() sets a flag that
        // loop() observes via isRebootPending() && !isFlashing() and then fires
        // performDeferredReboot() from the main loop — giving lwIP 10-100ms to
        // finish the response before cleanup begins. The service-cleanup path
        // is identical to the direct-call variant, just invoked from loop().
        logBootSignature("[OTA] pre-reboot");    // fourth/final OTA probe, parity with ESP8266
        requestDeferredReboot("[OTA] Rebooting...");
      }
    }, [&]() {
      // Upload handler
      HTTPUpload& upload = _server->upload();
      if (upload.status == UPLOAD_FILE_START) {
        _handleUploadStart(upload);
      } else if (_authenticated && upload.status == UPLOAD_FILE_WRITE && _updaterError[0] == '\0') {
        _handleUploadWrite(upload);
      } else if (_authenticated && upload.status == UPLOAD_FILE_END && _updaterError[0] == '\0') {
        _handleUploadEnd(upload);
      } else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
        _handleUploadAbort(upload);
      }
      delay(0);
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
  // 4B-H3: app0 slot size MUST match the active partition table value
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

  size_t _parseUploadTotalSize() const {
    size_t uploadTotal = 0;
    String sizeArg = _server->arg("size");
    if (sizeArg.length()) {
      long parsedSize = sizeArg.toInt();
      if (parsedSize > 0) uploadTotal = static_cast<size_t>(parsedSize);
    }
    return uploadTotal;
  }

  void _handleUploadStart(HTTPUpload& upload) {
    _updaterError[0] = '\0';
    _resetUploadTracking();
    _authenticated = (_username.length() == 0 || _password.length() == 0 ||
                      _server->authenticate(_username.c_str(), _password.c_str()));
    if (!_authenticated) {
      if (_serial_output) DebugTln(F("Unauthenticated Update"));
      return;
    }

    if (_serial_output) {
      DebugTf(PSTR("[OTA] Flash start: %s (size: %s)\r\n"),
              upload.filename.c_str(), _server->arg("size").c_str());
    }

    logBootSignature("[OTA] pre-begin");    // first of four OTA lifecycle probes, parity with ESP8266
    ::state.flash.bESPactive = true;

    size_t uploadTotal = _parseUploadTotalSize();
    _uploadExpectedBytes = uploadTotal;

    if (upload.name == F("filesystem")) {
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

  void _handleUploadWrite(HTTPUpload& upload) {
    if (_serial_output) blinkLEDnow(PIN_LED1);

    // Feed hardware watchdog (I2C address 0x26)
    Wire.beginTransmission(0x26);
    Wire.write(0xA5);
    Wire.endTransmission();

    uint8_t *buf = upload.buf;
    size_t   len = upload.currentSize;

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

  void _handleUploadEnd(HTTPUpload& upload) {
    if (_serial_output) DebugTln(F("[OTA] End: finalizing flash..."));

    bool updateOk = Update.end(true);
    if (updateOk) {
      if (_serial_output) DebugTf(PSTR("[OTA] End: success (%u bytes)\r\n"), upload.totalSize);
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
    ::state.flash.bESPactive = false;
    _authenticated = false;  // force re-authentication on next upload
    _resetUploadTracking();
  }

  void _handleUploadAbort(HTTPUpload& upload) {
    Update.end();
    if (_serial_output) {
      DebugTf(PSTR("[OTA] Abort: after %u bytes\r\n"), static_cast<unsigned>(_uploadWrittenBytes));
    }
    // Remount LittleFS if a filesystem upload was aborted mid-flight
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
  WebServer *_server;
  String _username;
  String _password;
  bool _authenticated;
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
