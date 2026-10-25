/*
***************************************************************************
**  Program  : OTGW-ModUpdateServer-esp32.h
**  Version  : v2.0.0-alpha.501
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
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

#ifndef OTGW_MOD_UPDATE_SERVER_ESP32_H
#define OTGW_MOD_UPDATE_SERVER_ESP32_H

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>       // esp_ota_get_running/next_update_partition — single-app-slot OTA guard
#include <esp_partition.h>     // esp_partition_find_first — full LittleFS partition size
#include <StreamString.h>
#include <LittleFS.h>
#include <Preferences.h>       // ADR-191: hostname/password mirror for the safeboot updater
#include "Wire.h"
#include "webServerCompat.h"   // webBeginRequest / webSendP / webSend / g_responseSent

// External declarations (same as the sync variant)
extern OTGWState state;
extern bool LittleFSmounted;
extern bool updateLittleFSStatus(const char *probePath);
extern bool updateLittleFSStatus(const __FlashStringHelper *probePath);
extern void writeSettings(bool show);
extern void settingsMarkClean();
extern void satPersistAllToFs();           // TASK-1286: SAT learned state, RAM -> LittleFS
extern void saveOtSupportFilesForced();    // TASK-1286: OT profile files, RAM -> LittleFS
extern void blinkLEDnow(uint8_t);
extern bool checkHttpAuth();
extern void requestDeferredReboot(const char *reason);

#ifndef Debug
  #define Debug(...)      ({ OTGWSerial.print(__VA_ARGS__); })
  #define Debugln(...)    ({ OTGWSerial.println(__VA_ARGS__); })
  #define Debugf(...)     ({ OTGWSerial.printf_P(__VA_ARGS__); })
#endif

// TASK-959: shared with restAPI.ino's device/info emit so the v2/Classic Flash
// Utility UI can hide the firmware-upload option before the user ever tries it,
// rather than only rejecting the upload after the fact (see _handleUploadStart
// below, which independently re-checks this — the UI hint is a courtesy, not
// the safety boundary). Dynamic: an 8MB dual-slot table flips this to true.
inline bool hasSpareAppOtaSlot() {
  const esp_partition_t *running  = esp_ota_get_running_partition();
  const esp_partition_t *nextSlot = esp_ota_get_next_update_partition(nullptr);
  return nextSlot != nullptr && nextSlot != running;
}

// ---------------------------------------------------------------------------
// ADR-191: firmware updates over WiFi on 4MB boards through the safeboot
// updater in the app/factory partition (safeboot/src/main.cpp).
// ---------------------------------------------------------------------------

// NVS namespace shared with the safeboot updater.
static const char SAFEBOOT_NVS_NS[] = "otgw-sb";

inline const esp_partition_t *safebootPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
}

// True when a factory partition holds a safeboot app and we are not running
// from it. Checks the app descriptor (magic word), so the handover never sends
// the bootloader to an empty factory partition.
inline bool hasSafeboot() {
  const esp_partition_t *sb = safebootPartition();
  if (sb == nullptr || sb == esp_ota_get_running_partition()) return false;
  esp_app_desc_t desc;
  return esp_ota_get_partition_description(sb, &desc) == ESP_OK;
}

// Mirror the hostname and the admin password into NVS for safeboot, which
// cannot read settings.ini (LittleFS). Writes only on change, so calling it on
// every settings write costs no flash wear. NVS is not encrypted; the password
// is already stored as plain text in settings.ini, so exposure does not grow.
inline void safebootSyncNvs(const char *host, const char *pw) {
  if (safebootPartition() == nullptr) return;   // old layout: nothing to mirror for
  Preferences prefs;
  if (!prefs.begin(SAFEBOOT_NVS_NS, false)) return;
  char cur[41];
  prefs.getString("host", cur, sizeof(cur));
  if (strcmp(cur, host) != 0) prefs.putString("host", host);
  cur[0] = '\0';
  prefs.getString("pw", cur, sizeof(cur));
  if (strcmp(cur, pw) != 0) prefs.putString("pw", pw);
  prefs.end();
}

// Hand over to safeboot: erase the WHOLE otadata partition, so the bootloader
// starts the factory app at the next boot. Erasing one sector is not enough:
// with a single OTA slot a leftover valid entry would point back at ota_0.
inline bool safebootHandover() {
  if (!hasSafeboot()) return false;
  const esp_partition_t *otadata = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (otadata == nullptr) return false;
  return esp_partition_erase_range(otadata, 0, otadata->size) == ESP_OK;
}

// Bootloader app rollback (ADR-191): a new image starts as PENDING_VERIFY
// (verifyRollbackLater() in OTGW-firmware.ino) and must confirm itself, or the
// next reset rolls back to safeboot. Called after the health gate and before any
// planned restart. A no-op when the image is already valid.
inline void safebootMarkAppValid() {
  esp_ota_img_states_t st;
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running && esp_ota_get_state_partition(running, &st) == ESP_OK &&
      st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

class OTGWUpdateServer {
public:
  enum class UploadTarget : uint8_t { None, Firmware, Filesystem };

  OTGWUpdateServer(bool serial_debug = false)
    : _serial_output(serial_debug), _server(nullptr), _authenticated(false),
      _routesRegistered(false), _updateFinalized(false),
      _serverIndex(nullptr), _serverSuccess(nullptr),
      _uploadTarget(UploadTarget::None),
      _uploadExpectedBytes(0), _uploadWrittenBytes(0), _uploadBlockIndex(0) {
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
      webSendPStream(200, PSTR("text/html"), _serverIndex);   // 47 KB page: stream from flash, no heap copy
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
          _dropKeptFiles();   // clearing bESPactive disables the abort cleanup, so free the copies here
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

    // ADR-191: POST /safeboot hands over to the safeboot updater, which then
    // serves the firmware upload page at the same address. Not under /update:
    // the /update handlers also match /update/... by prefix. Admin-protected
    // like every other side-effect route (ADR-056).
    srv.on("/safeboot", HTTP_POST, [](AsyncWebServerRequest *request) {
      webBeginRequest(request);
      if (!checkHttpAuth()) return;
      if (!safebootHandover()) {
        webSendP(409, PSTR("text/plain"), PSTR("No safeboot updater on this board"));
        return;
      }
      webSendP(200, PSTR("text/plain"), PSTR("Restarting into the firmware updater..."));
      requestDeferredReboot("[OTA] handover to safeboot");
    });
  }

  void updateCredentials(const String& username, const String& password) {
    _username = username;
    _password = password;
  }

  void setIndexPage(const char *indexPage) { _serverIndex = indexPage; }
  void setSuccessPage(const char *successPage) { _serverSuccess = successPage; }

private:
  void _resetUploadTracking() {
    _uploadTarget = UploadTarget::None;
    _uploadExpectedBytes = 0;
    _uploadWrittenBytes = 0;
    _uploadBlockIndex = 0;
  }

  // Auth helper. Empty credentials == open (no auth configured).
  bool _uploadAuthorized(AsyncWebServerRequest *request) const {
    return (_username.length() == 0 || _password.length() == 0 ||
            request->authenticate(_username.c_str(), _password.c_str()));
  }

  // Total upload size, taken ONLY from the `size=<file.size>` query param the
  // form posts (see updateServerHtml.h). Do NOT fall back to
  // request->contentLength(): that is the whole multipart body length (file +
  // boundary/header bytes), which for a full-partition filesystem image exceeds
  // the partition and would trip a spurious "filesystem image too large". When
  // the param is absent, return 0 and let the target begin() size the region
  // from the partition itself — matching the 1.x ESP8266 path.
  size_t _parseUploadTotalSize(AsyncWebServerRequest *request) const {
    const AsyncWebParameter *p = request->getParam("size", false);  // query string
    if (p) {
      long parsedSize = p->value().toInt();
      if (parsedSize > 0) return static_cast<size_t>(parsedSize);
    }
    return 0;
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
      // Use the FULL LittleFS partition size, not LittleFS.totalBytes(): the
      // latter reports USABLE bytes (< partition — LittleFS reserves metadata
      // blocks), while a mklittlefs image always spans the WHOLE partition.
      // Comparing an image against totalBytes() spuriously rejects it, and
      // Update.begin() needs the full partition as its size budget so a
      // partition-spanning image is accepted (no UPDATE_ERROR_SPACE); the
      // Updater erases lazily per-sector as bytes are written. Mirrors the
      // 1.x ESP8266 path (`_FS_end - _FS_start`).
      const esp_partition_t *fsPart = esp_partition_find_first(
          ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
      size_t fsSize = fsPart ? fsPart->size : LittleFS.totalBytes();
      if (_serial_output) {
        DebugTf(PSTR("[OTA] Target: filesystem (%u bytes)\r\n"), static_cast<unsigned>(fsSize));
      }
      _keepFiles();   // TASK-1266/1286: the image lacks these files; copy them to the heap, restore after the remount
      LittleFS.end();
      if (uploadTotal > 0 && uploadTotal > fsSize) {
        strlcpy(_updaterError, "filesystem image too large", sizeof(_updaterError));
        _dropKeptFiles();   // the old filesystem is untouched; the copies are not needed
      } else if (!Update.begin(fsSize, U_SPIFFS)) {  // U_SPIFFS covers LittleFS on ESP32
        _setUpdaterError();
        _dropKeptFiles();
      }
    } else {
      _uploadTarget = UploadTarget::Firmware;
      // App/firmware OTA needs a SECOND app partition to receive the image:
      // esp_ota writes to the inactive OTA slot, then otadata flips the boot
      // pointer. On a single-app-slot table (the 4MB combo — app0/ota_0 only;
      // the web-asset LittleFS leaves no room for a second ~1.9MB slot, and the
      // only 4MB dual-slot scheme ships a 190KB FS too small for the web UI),
      // esp_ota_get_next_update_partition() returns the RUNNING partition, so
      // the write would overwrite the executing app and brick the device
      // (empirically confirmed — TASK-959). Reject before any Update.begin();
      // app flashing is USB-only here (flash_otgw.bat / merged-full over USB).
      // The check is dynamic: an 8MB dual-slot (ota_0+ota_1) table would make
      // nextSlot != running and re-enable OTA app updates automatically.
      // The 4MB merged image is deliberately NOT accepted over OTA (USB-only);
      // a plain firmware.bin app image is the only firmware upload we handle.
      if (!hasSpareAppOtaSlot()) {
        strlcpy(_updaterError,
                hasSafeboot()
                  ? "app update runs in the safeboot updater on this board; use 'Restart into the firmware updater' on /update"
                  : "app update is USB-only on this board (single app slot); use flash_otgw.bat over USB",
                sizeof(_updaterError));
        if (_serial_output) {
          DebugTln(F("[OTA] Firmware OTA rejected: single app slot (no ota_1) — app flash is USB-only"));
        }
        return;
      }
      uint32_t maxSketchSpace = (platformFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (_serial_output) {
        DebugTf(PSTR("[OTA] Target: firmware (%u bytes)\r\n"), static_cast<unsigned>(maxSketchSpace));
      }
      if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)) {
        _setUpdaterError();
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

    size_t written = Update.write(data, len);
    if (written != len) {
      _setUpdaterError();
      _dropKeptFiles();   // the end handler does not run after a write error
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
        // TASK-958: Update.end(true) finalizes even a truncated write, so a short
        // upload could report "success" with a broken/old filesystem. A complete
        // image always carries /version.hash; its absence after remount means the
        // write did NOT take. Surface it as an error instead of a silent success.
        if (LittleFSmounted && !LittleFS.exists(F("/version.hash"))) {
          strlcpy(_updaterError, "incomplete filesystem image (version.hash missing)", sizeof(_updaterError));
          if (_serial_output) DebugTln(F("[OTA] Error: filesystem image incomplete (no version.hash)"));
        } else if (LittleFSmounted) {
          updateLittleFSStatus(F("/.ota_post"));
          logBootSignature("[OTA] post-remount");    // third probe (FS-OTA only): after LittleFS.begin() remount
          if (_serial_output) DebugTln(F("[OTA] Restoring settings to filesystem"));
          writeSettings(false);
          settingsMarkClean();
          _restoreKeptFiles();           // TASK-1266/1286: files without a RAM copy, from the heap
          saveOtSupportFilesForced();    // TASK-1286: the OT profiles live in RAM, so no heap copy is needed
          satPersistAllToFs();           // TASK-1286: SAT learned state from RAM, even when it was saved less than an hour ago
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
    _dropKeptFiles();          // no-op after a restore; frees the copies on every other path
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
    _dropKeptFiles();
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

  // A filesystem update writes a fresh image over the whole partition. Two kinds
  // of runtime file survive it:
  //  - files with a RAM model are written back from RAM after the remount:
  //    settings.ini (writeSettings), the SAT learned state (satPersistAllToFs,
  //    TASK-1286), the OpenTherm profiles (saveOtSupportFilesForced) and the DHW
  //    water total (flushDHWWaterMeter() in doRestart);
  //  - files without one are copied to the heap before the image is written and
  //    written back after the remount (KEEP_LIST below). dallas_labels.ini
  //    (TASK-1266) was the first; TASK-1286 added the rest.
  // A missing file, an oversized one, a low heap or a failed allocation only
  // skips that file's restore; the filesystem update itself goes on. The copy
  // is made and restored on the AsyncTCP task (the upload callbacks), which
  // serializes the uploads, so the per-instance buffers need no further guard.
  struct KeepEntry { const char *path; uint16_t maxLen; };
  static constexpr KeepEntry KEEP_LIST[] = {
    {"/dallas_labels.ini", 2048},   // the user's Dallas sensor names
    {"/sat_markers.json",  2560},   // at most 20 heating-curve markers of at most 110 B
    {"/reboot_count.txt",    16},   // one decimal number
    {"/reboot_log.txt",    3072},   // 20 lines of at most 140 B
  };
  static constexpr size_t   KEEP_COUNT        = sizeof(KEEP_LIST) / sizeof(KEEP_LIST[0]);
  static constexpr size_t   KEEP_TOTAL_MAX    = 8192;   // all copies together
  static constexpr uint32_t KEEP_HEAP_RESERVE = 8192;   // largest free block must stay this far above a copy

  void _keepFiles() {
    _dropKeptFiles();
    if (!LittleFSmounted) return;
    size_t total = 0;
    for (size_t i = 0; i < KEEP_COUNT; i++) {
      File f = LittleFS.open(KEEP_LIST[i].path, "r");
      if (!f) continue;
      const size_t len = f.size();
      if (len == 0) {
        f.close();
        continue;
      }
      if (len > KEEP_LIST[i].maxLen) {
        if (_serial_output) {
          DebugTf(PSTR("[OTA] %s too large to keep (%u bytes, cap %u)\r\n"),
                  KEEP_LIST[i].path, static_cast<unsigned>(len), static_cast<unsigned>(KEEP_LIST[i].maxLen));
        }
        f.close();
        continue;
      }
      if (total + len > KEEP_TOTAL_MAX) {
        if (_serial_output) {
          DebugTf(PSTR("[OTA] %s not kept: total cap %u reached (%u kept, file %u)\r\n"),
                  KEEP_LIST[i].path, static_cast<unsigned>(KEEP_TOTAL_MAX), static_cast<unsigned>(total), static_cast<unsigned>(len));
        }
        f.close();
        continue;
      }
      if (platformMaxFreeBlock() < len + KEEP_HEAP_RESERVE) {
        if (_serial_output) {
          DebugTf(PSTR("[OTA] %s not kept: low heap (largest block %u)\r\n"),
                  KEEP_LIST[i].path, static_cast<unsigned>(platformMaxFreeBlock()));
        }
        f.close();
        continue;
      }
      uint8_t *buf = static_cast<uint8_t *>(malloc(len));
      if (buf != nullptr) {
        if (f.read(buf, len) == len) {
          _keepBuf[i] = buf;
          _keepLen[i] = static_cast<uint16_t>(len);
          total += len;
        } else {
          free(buf);
        }
      }
      f.close();
    }
    if (_serial_output) {
      DebugTf(PSTR("[OTA] kept %u bytes of runtime files on the heap\r\n"), static_cast<unsigned>(total));
    }
  }

  void _restoreKeptFiles() {
    for (size_t i = 0; i < KEEP_COUNT; i++) {
      if (_keepBuf[i] == nullptr) continue;
      File f = LittleFS.open(KEEP_LIST[i].path, "w");
      const bool ok = f && f.write(_keepBuf[i], _keepLen[i]) == _keepLen[i];
      if (f) f.close();
      if (_serial_output) {
        DebugTf(PSTR("[OTA] Restoring %s (%u bytes): %s\r\n"),
                KEEP_LIST[i].path, static_cast<unsigned>(_keepLen[i]), ok ? "OK" : "FAILED");
      }
    }
    _dropKeptFiles();
  }

  void _dropKeptFiles() {
    for (size_t i = 0; i < KEEP_COUNT; i++) {
      free(_keepBuf[i]);
      _keepBuf[i] = nullptr;
      _keepLen[i] = 0;
    }
  }

  uint8_t *_keepBuf[KEEP_COUNT] = {};
  uint16_t _keepLen[KEEP_COUNT] = {};

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
  const char *_serverIndex;
  const char *_serverSuccess;
};

/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/

#endif // OTGW_MOD_UPDATE_SERVER_ESP32_H
