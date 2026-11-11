/* 
***************************************************************************  
**  Program : FSexplorer
**  Version  : v2.0.0-beta.184
**
**  Mostly stolen from https://www.arduinoforum.de/User-Fips
**  For more information visit: https://fipsok.de
**  See also https://www.arduinoforum.de/arduino-Thread-SPIFFS-DOWNLOAD-UPLOAD-DELETE-Esp8266-NodeMCU
**
***************************************************************************      
  Copyright (c) 2018 Jens Fleischer. All rights reserved.
  Copyright (c) 2026 Robert van den Breemen. All rights reserved.
    Extended the code with functions to upgrade OTGW firmware and PIC firmware and include directory support.
  
  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
*******************************************************************
**      Usage:
**      
**      setup()
**      {
**        setupFSexplorer();   // registers routes on the AsyncWebServer `server`
**        startWebserver();    // serveStatic + index routes + server.begin()
**      }
**
**      loop()
**      {
**        // No handleClient() — ESPAsyncWebServer serves on the AsyncTCP task.
**      }
*/
// forward declaration — contentType is defined later in this file (line ~565)
const String &contentType(String& filename);
// forward declaration — used in setupFSexplorer() before its definition below.
static void sendFSexplorerRedirect();

// Outcome of the upload currently being received: true only once the target file
// opened AND every chunk written full-length. handleFileUpload() latches it; the
// /upload POST completion handler reads and clears it to pick 303 vs 507. It must
// live at file scope because the response is emitted from that completion handler,
// which cannot see handleFileUpload()'s locals.
// KNOWN RESIDUAL: unlike the file handle (now per-request, on request->_tempFile),
// this single flag is shared. Two concurrent uploads can therefore have the status
// code attributed to the wrong request — while the file CONTENTS stay correct per
// request, which is the part that used to corrupt.
static bool s_uploadOk = false;

#define MAX_FILES_IN_LIST   40

const char Helper[] PROGMEM =
  "<br>You first need to upload this file (from the filesystem build, .pio/data-staging):\n"
  "<ul>\n"
  "  <li>FSexplorer.html.gz</li>\n"
  "</ul>\n"
  "<form method=\"POST\" action=\"upload\" enctype=\"multipart/form-data\">\n"
  "  <input type=\"file\" name=\"upload\">\n"
  "  <input type=\"submit\" value=\"Upload\">\n"
  "</form>\n"
  "<br/><b>or</b> you can use the <i>Flash Utility</i> to flash firmware or LittleFS!\n"
  "<form action='update' method='GET'>\n"
  "  <input type='submit' name='SUBMIT' value='Flash Utility'/>\n"
  "</form>\n";
const char Header[] PROGMEM = "HTTP/1.1 303 OK\r\nLocation:FSexplorer.html\r\nCache-Control: no-cache\r\n";


// Serve a gzipped web page with ETag-based cache validation (ADR-139, amended
// TASK-958). Stable URLs, no ?v= query versioning: the ETag is the filesystem
// hash. Cache-Control: no-cache means the browser MAY store the asset but MUST
// revalidate (conditional GET) before every use — it never serves from cache
// blind. Unchanged FS -> 304 (headers only, no body); a reflash/OTA changes the
// hash -> 200 with the new asset on the very next load. This replaces the earlier
// max-age=60 window: that window let the browser serve stale assets without asking
// for up to 60s, so a filesystem OTA (which DOES replace the assets) still showed
// the old UI on the immediate post-reboot reload (window.location.href='/' does
// not bypass a fresh cache entry). no-cache costs one tiny 304 revalidation per
// asset per load, negligible on the trusted LAN. TASK-1640 (ADR-215): each web
// page is one gzipped file (scripts/webui_pages.py inlines its CSS, JavaScript
// and fonts at build time), streamed straight from flash via AsyncFileResponse
// with Content-Encoding: gzip, never buffered whole on the fragmented S3 heap.
// Mixed state (ADR-215): new firmware on a filesystem from before the gzipped pages
// still has the plain page (plainPath, e.g. /v2.html). It is served as-is, without
// Content-Encoding, when the .gz file is absent. Returns false, without sending
// anything, when neither exists, so the caller can fall back to the recovery page.
// mime: the Content-Type to send; nullptr means an HTML page (TASK-1689 adds the
// language packs, served as application/json).
static bool serveGzipPage(const char* gzPath, const char* plainPath,
                          const __FlashStringHelper* mime = nullptr) {
  // Existence first, before any ETag is staged and before the 304 short-circuit:
  // the ETag is the filesystem hash, which only changes on a reflash, so a miss
  // must not be answered with it.
  const char* path = gzPath;
  bool gzip = true;
  if (!LittleFS.exists(path)) {
    if (!plainPath || !LittleFS.exists(plainPath)) return false;
    path = plainPath;
    gzip = false;
  }
  const char* fsHash = getFilesystemHash();
  if (fsHash && fsHash[0] != '\0') {
    // The ETag must identify the RESOURCE REPRESENTATION, not merely the
    // filesystem build. A bare fsHash ETag would be identical for every file,
    // so a browser could get a false 304 for one path from the validator of
    // another (TASK-960 saw this when the root route still switched between two
    // UI shells). Include the served path so each page has a distinct validator.
    char etag[64];
    snprintf_P(etag, sizeof(etag), PSTR("\"%s-%s\""), fsHash, path);
    if (hasHeaderCompat(F("If-None-Match")) &&
        strcmp(headerCompat(F("If-None-Match")), etag) == 0) {
      webPushHeader(F("Cache-Control"), F("no-cache"));
      webPushHeader(F("ETag"), etag);
      webSendStatus(304);
      return true;
    }
    webPushHeader(F("ETag"), etag);
  }
  webPushHeader(F("Cache-Control"), F("no-cache"));
  const __FlashStringHelper* type = mime ? mime : F("text/html; charset=UTF-8");
  webSendFile(path, type, gzip);
  return true;
}

// The recovery page: FSexplorer, or the PROGMEM upload prompt when it is missing too.
static void sendFSexplorerPage() {
  if (!serveGzipPage("/FSexplorer.html.gz", "/FSexplorer.html")) {
    webSendP(200, PSTR("text/html; charset=UTF-8"), Helper);  // prompt to upload FSexplorer.html.gz
  }
}

// TASK-1294: static text that lives in the firmware image instead of on LittleFS, so
// the 4 MB boards keep their LittleFS headroom. Same cache contract as
// serveGzipPage(): Cache-Control no-cache plus an ETag, 304 on a matching
// If-None-Match. The validator is the FIRMWARE git hash plus the path, because the
// body changes only with the firmware image, not with the LittleFS image: a
// filesystem-only update leaves it alone, a firmware update changes it. The body
// streams straight from flash (webSendPStream, no heap copy). The path is a
// compile-time constant of the caller, always a plain ASCII URL.
#include "settingsHelp.h"
#include "faviconData.h"   // TASK-1696: /favicon.ico from app flash
static void serveProgmemAsset(const char* path, PGM_P mime, PGM_P body) {
  char etag[64];
  snprintf_P(etag, sizeof(etag), PSTR("\"%s-%s\""), _VERSION_GITHASH, path);
  webPushHeader(F("Cache-Control"), F("no-cache"));
  webPushHeader(F("ETag"), etag);
  if (hasHeaderCompat(F("If-None-Match")) &&
      strcmp(headerCompat(F("If-None-Match")), etag) == 0) {
    webSendStatus(304);
    return;
  }
  webSendPStream(200, mime, body);
}

// Serve the web UI (v2.html.gz). ADR-139: the bespoke chunked
// beginChunkedResponse emitter that rewrote ?v=<fsHash> into asset URLs line by
// line is retired; the page is a plain static file with ETag validation.
static void sendIndex(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  // Auth guard: if a password is configured, require it upfront so the browser
  // caches credentials before any API call is made (ADR-056), avoiding a
  // mid-session popup. checkHttpAuth() sends the 401 challenge on failure.
  if (!checkHttpAuth()) return;
  // TASK-1223.12: v2 is the only Web UI; the classic shell (index.html) is gone.
  // Checked per request, not at boot: when the UI page is deleted or lost at runtime
  // the root still lands on FSexplorer, the page to repair the filesystem with.
  if (!serveGzipPage("/v2.html.gz", "/v2.html")) sendFSexplorerPage();
}

// TASK-989 NOTE: a server-level heap-guard middleware (shed/abort on low maxblock,
// ahead of every handler) was implemented here and REMOVED after on-device A/B
// load-ramps falsified the approach: a floor above idle maxblock (~11.7K on the
// bench S3 with BLE on) sheds at rest; a floor below it fires only AFTER the
// admitted handlers have already collapsed the heap (reactive = one burst too
// late); and a hard request->abort() tier (tcp_abort from the async task) killed
// the entire network stack under a conc>=12 burst (RST storm corrupting LWIP —
// port 80 AND telnet dead, esptool-reset-only). The existing per-domain gates
// (webFileGateTryAdmit, REST in-flight cap) already bound handler concurrency;
// the effective levers against the burst wedge are ASYNC_TCP_STACK_SIZE=16384
// (platformio.ini) and reducing the offered load (serialized asset loading,
// staggered API polls, gzip). Full evidence: TASK-989, buglog bug-145..147.

//=====================================================================================
void startWebserver(){
  // The root paths keep /index, /index.html and /v2.html so old bookmarks still land on
  // the UI. ETag-based caching (ADR-139, amended TASK-958):
  //  - Cache-Control: no-cache — browser may store but MUST revalidate via If-None-Match (ETag = fsHash) every load.
  //  - Unchanged FS → server replies 304 Not Modified (headers only, no body re-download).
  //  - FS upgraded/OTA → ETag changes → server replies 200 with fresh content on the very next load (no stale window).
  // TASK-1640: the page carries its CSS, JavaScript and fonts, so there are no asset routes.
  // sendIndex() falls back to FSexplorer when v2.html.gz is missing.
  server.on("/",           HTTP_GET, sendIndex);
  server.on("/index",      HTTP_GET, sendIndex);
  server.on("/index.html", HTTP_GET, sendIndex);
  server.on("/v2.html",    HTTP_GET, sendIndex);
  // TASK-1281: the settings help texts, fetched by v2.js the first time an info icon is used.
  // TASK-1294: served from the firmware image (settingsHelp.h), not from LittleFS.
  server.on("/settings-help.json", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveProgmemAsset("/settings-help.json", PSTR("application/json"), SETTINGS_HELP_JSON); });
  // TASK-1696: the favicon comes from app flash, so a page load opens no LittleFS file for it and
  // takes no web file-gate slot (WEB_FILE_MAX_INFLIGHT); same ETag/no-cache/304 rule as the pages.
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); webSendFlashBody(PSTR("image/x-icon"), FAVICON_ICO, FAVICON_ICO_LEN, "/favicon.ico", false); });
#if HAS_PIC
  //otgw pic functions
  server.on("/pic", HTTP_ANY, upgradepic);
#endif
  // all other api calls are caught in the FSexplorer onNotFound catch-all; the
  // explicit /api prefix is also routed to processAPI for any method.
  //
  // TASK-879: the POST/PUT body MUST be captured on THIS handler. The global
  // server.onRequestBody() below is the CATCH-ALL handler's onBody
  // (AsyncWebServer::onRequestBody -> _catchAllHandler->onBody, WebServer.cpp), so
  // it fires ONLY for requests that fall through to the catch-all. /api/* matches
  // this explicit handler first, so without an onBody here the body was silently
  // dropped and every POST /api/v2/settings returned 400 "Invalid JSON" (the async
  // web UI's settings save was broken). Attach the same capture as this handler's
  // own onBody.
  server.on("/api", HTTP_ANY, processAPI, nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      webCaptureBody(request, data, len, index, total);
    });

  // ESPAsyncWebServer keeps ALL request headers available on the request object
  // (no collectHeaders() allowlist needed, unlike the sync WebServer). The
  // If-None-Match / Origin / Referer headers the cache + CSRF logic reads are
  // therefore always present via headerCompat().

  // Global POST/PUT body capture (TASK-865.9): the sync WebServer surfaced the
  // raw body as arg("plain")/arg(0); the async server streams it through this
  // hook. webCaptureBody() accumulates it into g_requestBody, read back by the
  // REST handlers via bodyCompat(). Fires before the matching route handler runs.
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    webCaptureBody(request, data, len, index, total);
  });

  server.begin();
  // TASK-1130: begin() is void and fails silently when lwIP refuses the bind.
  // After the WiFiManager config portal, its closed HTTP connections sit in
  // TIME_WAIT on port 80 for 2*TCP_MSL (120 s); AsyncTCP binds without
  // SOF_REUSEADDR, so tcp_bind() returns ERR_USE and no listener exists.
  // handleWebserverListener() retries until the bind succeeds.
  if (server.state() != LISTEN) {
    DebugTln(F("HTTP Server: bind on port 80 failed, retrying every 5s"));
  }
  // Set up first message as the IP address
  DebugTln(F("\nHTTP Server started\r"));
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%03d.%03d.%d.%d"), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DebugTf(PSTR("\nAssigned IP=%s\r\n"), cMsg);
}

// TASK-1130: re-attempt the port-80 bind while the listener is missing (see
// startWebserver()). A failed begin() leaves AsyncServer without a pcb, so a
// later begin() starts from scratch; routes stay registered on the server.
void handleWebserverListener(){
  static uint16_t retries = 0;
  if (server.state() == LISTEN) return;
  server.begin();
  retries++;
  if (server.state() == LISTEN) {
    DebugTf(PSTR("HTTP Server: listening on port 80 after %u retries\r\n"), (unsigned)retries);
    retries = 0;
  }
}
// Serve FSexplorer (FSexplorer.html.gz), the recovery page.
static void sendFSexplorerHtml(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  sendFSexplorerPage();
}

//=====================================================================================
void setupFSexplorer(){
  LittleFS.begin();
  server.on("/FSexplorer.html", HTTP_GET, sendFSexplorerHtml);
  server.on("/FSexplorer",      HTTP_GET, sendFSexplorerHtml);

  server.on("/api/firmwarefilelist", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); apifirmwarefilelist(); });  // DEPRECATED: unversioned (ADR-035)
  server.on("/api/listfiles",        HTTP_ANY, [](AsyncWebServerRequest *r){ webBeginRequest(r); apilistfiles(); });         // DEPRECATED: unversioned (ADR-035)
  // /upload: the body completion handler runs after the upload finished. If auth
  // or the same-origin check failed, handleFileUpload skipped the open and wrote
  // nothing; send the 401/403 here. This handler owns the whole response.
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      webBeginRequest(request);
      // Latch and clear the outcome handleFileUpload() recorded, so a later POST
      // to /upload cannot inherit this request's result.
      const bool uploadOk = s_uploadOk;
      s_uploadOk = false;
      // checkHttpAuth() sends a 401 challenge when auth is required and failed,
      // or a 403 when the origin check failed.
      if (!checkHttpAuth()) return;
      if (g_responseSent) return;
      // Only report success when the file actually opened and every chunk was
      // written. A 303 here used to be sent unconditionally, so an upload onto a
      // full filesystem (or onto a directory name) reported success with no bytes
      // stored. The client already alerts on any non-200 (FSexplorer.html).
      if (!uploadOk) { webSendStatus(507); return; }
      // Redirect back to FSexplorer.html (303 + Location) — the same outcome the
      // sync server produced with the raw `Header` literal.
      sendFSexplorerRedirect();
    },
    handleFileUpload);
  server.on("/ReBoot",        HTTP_ANY, reBootESP);
  server.on("/ResetWireless", HTTP_ANY, resetWirelessButton);

  // TASK-865.11: register the OTA /update endpoint on the AsyncWebServer here,
  // alongside the other routes and before startWebserver()'s server.begin().
  // setup() is idempotent (guarded by _routesRegistered), so the per-connect
  // updateCredentials() calls in startWiFi() never re-register the route.
  httpUpdater.setup(&server);
  httpUpdater.setIndexPage(UpdateServerIndex, UpdateServerIndexLen);
  httpUpdater.setSuccessPage(UpdateServerSuccess, UpdateServerSuccessLen);
  if (settings.sHTTPpasswd[0] != '\0') {
    httpUpdater.updateCredentials("admin", settings.sHTTPpasswd);
  }

  server.onNotFound([](AsyncWebServerRequest *request) {
    webBeginRequest(request);
    if (strncmp_P(request->url().c_str(), PSTR("/api/"), 5) == 0) {
      processAPI(request);
      return;
    }
    // TASK-683 port: emit one outcome-bearing line per static request — 200
    // gated on bRestAPI (chatty debug surface), 404 always-on (actionable).
    // ESPAsyncWebServer already URL-decoded _url in _parseReqHead() (WebRequest.cpp),
    // so decoding a second time here corrupts any '%' or '+' in a stored filename.
    // The sync ESP8266WebServer does NOT decode, which is why the 1.x tree keeps its
    // own urlDecode at the same spot — do not "restore" the symmetry on this tree.
    String path = request->url();
    const bool served = handleFile(String(path));
    if (!served) {
      webSendP(404, PSTR("text/plain"), PSTR("FileNotFound\r\n"));
      DebugTf(PSTR("http GET %s => 404\r\n"), path.c_str());
    } else if (state.debug.bRestAPI) {
      DebugTf(PSTR("http GET %s => 200 (file)\r\n"), path.c_str());
    }
  });

} // setupFSexplorer()

//=====================================================================================
// Parameterless: reads the already-bound currentRequest. Reached both from the
// /api/firmwarefilelist route (wrapped to bind the request) and from the v2
// dispatch handleFirmware() where the context is already bound.
void apifirmwarefilelist() {
  // TASK-683 port: drop JSON-mirror-to-telnet noise. Function-entry trace and
  // per-file GetVersion result lines are gated on bRestAPI; one always-on
  // summary line is emitted at the end.
  const unsigned long startMs = millis();
  unsigned int entryCount = 0;
  if (state.debug.bRestAPI) DebugTf(PSTR("API: apifirmwarefilelist()\r\n"));

  // Heap floor (mirrors processAPI's pre-check at restAPI.ino:2221). This
  // deprecated route is registered directly on the async server, so it bypasses
  // that guard; replicate it here so a low-heap entry into restBeginStream's
  // throwing beginResponseStream() can never reach this path (TASK-886 review M1).
  if (platformFreeHeap() < 4096) {
    webSendP(500, PSTR("text/plain"), PSTR("500: internal server error (low heap)\r\n"));
    return;
  }

  // ADR-004: stack char[] instead of String to avoid per-iteration heap churn.
  char version[32]   = {0};
  char fwversion[32] = {0};
  File f;

  String dirpath = "/" + String(state.pic.sDeviceid);
  if (state.debug.bRestAPI) DebugTf(PSTR("dirpath=%s\r\n"), dirpath.c_str());

  // ADR-141 revert / TASK-885: streaming JsonEmit replaces the JsonDocument path.
  // Bounded array of hex entries emitted field-by-field directly into the per-
  // request response stream — no materialized document. Each entry holds the file
  // name, its stored .ver version and size. CORS header pushed before
  // restBeginStream() so webApplyHeaders() picks it up (ADR-035 unversioned).
  webPushHeader(F("Access-Control-Allow-Origin"), F("*"));
  AsyncResponseStream* strm = restBeginStream("application/json");
  if (strm) {
    JsonEmit je(*strm);
    je.beginArray();                       // root [

    PlatformDir dir(dirpath.c_str());
    if (dir.valid()) {
      while (dir.next()) {
        String entryName = dir.fileName();
        size_t entrySize = dir.fileSize();
        if (entryName.endsWith(".hex")) {
          version[0]   = '\0';
          fwversion[0] = '\0';
          String hexfile = dirpath + "/" + entryName;
          String verfile = hexfile;
          verfile.replace(".hex", ".ver");
          f = LittleFS.open(verfile, "r");
          if (f) {
            // ADR-004: readBytesUntil into stack buffer (pattern: helperStuff.ino:690).
            size_t n = f.readBytesUntil('\n', (uint8_t*)version, sizeof(version) - 1);
            version[n] = '\0';
            // Trim trailing CR (Windows-style lines) and whitespace.
            while (n > 0 && (version[n-1] == '\r' || version[n-1] == ' ' || version[n-1] == '\t')) {
              version[--n] = '\0';
            }
            f.close();
          }

          GetVersion(hexfile.c_str(), fwversion, sizeof(fwversion));

          if (state.debug.bRestAPI) DebugTf(PSTR("GetVersion(%s) returned [%s]\r\n"), hexfile.c_str(), fwversion);
          if (fwversion[0] != '\0' && strcmp(fwversion, version) != 0) {
            strlcpy(version, fwversion, sizeof(version));
            if (f = LittleFS.open(verfile, "w")) {
              if (state.debug.bRestAPI) DebugTf(PSTR("writing %s to %s\r\n"), version, verfile.c_str());
              f.print(version);
              f.print('\n');
              f.close();
            }
          }

          je.beginObject();                 // entry {
          je.field(F("name"),    entryName);          // String
          je.field(F("version"), version);            // char[]
          je.field(F("size"),    (int)entrySize);
          je.endObject();                   // close entry

          feedWatchDog(); // Feed watchdog during potentially long operation
          entryCount++;
        }
      }
    }

    je.endArray();                         // close root
  }
  restFinalize();

  DebugTf(PSTR("api firmware/files: %u entries (%lums)\r\n"),
          entryCount, (unsigned long)(millis() - startMs));
}


//=====================================================================================


// Parameterless: reads the already-bound currentRequest (see apifirmwarefilelist).
void apilistfiles()
{
  // Heap floor (mirrors processAPI's pre-check at restAPI.ino:2221). Deprecated
  // route registered directly on the async server -> bypasses that guard; replicate
  // it so the throwing beginResponseStream() on the listing path can never be
  // reached under memory pressure (TASK-886 review M1).
  if (platformFreeHeap() < 4096) {
    webSendP(500, PSTR("text/plain"), PSTR("500: internal server error (low heap)\r\n"));
    return;
  }

  // --- Delete handler: local buffer instead of global cMsg ---
  if (hasArgCompat("delete")) {
    if (!checkHttpAuth()) return;  // 401 already sent
    char deletePath[64];  // filename is max 31 chars, but a subdirectory prefix adds to that
    strlcpy(deletePath, argCompat("delete"), sizeof(deletePath));
    // Normalize: LittleFS paths must start with '/'
    if (deletePath[0] != '/' && deletePath[0] != '\0') {
      size_t len = strnlen(deletePath, sizeof(deletePath) - 2);
      memmove(deletePath + 1, deletePath, len + 1);
      deletePath[0] = '/';
      // strnlen caps len at 62, so exactly at the strlcpy truncation point the byte
      // moved into the last slot is a real character, not the terminator — leaving
      // all 64 bytes non-zero and sending strlen() off the end of the array. Force
      // the terminator. No-op for every path of 62 characters or fewer.
      deletePath[sizeof(deletePath) - 1] = '\0';
    }
    DebugTf(PSTR("Delete -> [%s]\r\n"), deletePath);
    if (!LittleFS.exists(deletePath)) {
      webSend(404, F("text/plain"), F("File not found"));
    } else if (LittleFS.remove(deletePath)) {
      webSend(200, F("text/plain"), F("File deleted"));
    } else {
      webSend(500, F("text/plain"), F("Delete failed"));
    }
    return;
  }

  // --- File listing: bounded JSON array into the per-request response stream ---
  // Sorting and size formatting are handled by the frontend (FSexplorer.html).
  String path = "/";
  if (hasArgCompat("path")) {
    path = argCompat("path");
  }

  // ADR-141 revert / TASK-885: streaming JsonEmit replaces the JsonDocument path.
  // Heterogeneous array — up to MAX_FILES_IN_LIST file objects {name,size,type}
  // followed by ONE trailing storage-summary object. Order preserved: files first,
  // summary last. "dir"/"file" are genuine string labels (not bools).
  int fileCount = 0;
  bool truncated = false;

  webPushHeader(F("Access-Control-Allow-Origin"), F("*"));
  AsyncResponseStream* strm = restBeginStream("application/json");
  if (strm) {
    JsonEmit je(*strm);
    je.beginArray();                       // root [

    PlatformDir dir(path.c_str());
    if (dir.valid()) {
      while (dir.next()) {
        String fname = dir.fileName();
        long   fsize = (long)dir.fileSize();
        bool   isDir = dir.isDirectory();
        feedWatchDog();
        // Skip hidden files/directories (names starting with '.')
        if (fname.charAt(0) == '.') {
          continue;
        }
        if (fileCount >= MAX_FILES_IN_LIST) { truncated = true; break; }

        je.beginObject();                   // file entry {
        je.field(F("name"), fname);                    // String
        je.field(F("size"), (int32_t)fsize);           // long
        je.field(F("type"), isDir ? F("dir") : F("file")); // genuine string label
        je.endObject();                     // close file entry
        fileCount++;
      }
    }

    // Storage info as last entry (raw bytes — frontend formats for display)
    FSInfo fsInfo;
    platformFSInfo(fsInfo);
    unsigned long totalBytes = fsInfo.totalBytes;
    unsigned long usedBytesRaw = fsInfo.usedBytes;
    unsigned long usedBytes = (unsigned long)(usedBytesRaw * 1.05);
    // Clamp BEFORE the subtraction: past 95.238% full the 5% margin pushes usedBytes
    // over totalBytes and the 32-bit subtraction wraps to ~4 GB, which defeats the
    // frontend's only "not enough space!" guard (nBytes > freeBytes). Clamping
    // usedBytes (rather than flooring freeBytes) also keeps used + free == total, so
    // the panel cannot read "used 2.03 MB of 2.00 MB".
    if (usedBytes > totalBytes) usedBytes = totalBytes;
    unsigned long freeBytes = totalBytes - usedBytes;
    je.beginObject();                       // summary {
    je.field(F("usedBytes"),  (uint32_t)usedBytes);    // unsigned long
    je.field(F("totalBytes"), (uint32_t)totalBytes);   // unsigned long
    je.field(F("freeBytes"),  (uint32_t)freeBytes);    // unsigned long
    je.field(F("truncated"),  truncated);              // real JSON bool
    je.endObject();                         // close summary

    je.endArray();                         // close root
  }
  restFinalize();

} // apilistfiles()


// 303 redirect back to FSexplorer.html (replaces the raw `Header` HTTP literal
// the sync server wrote with sendContent()).
static void sendFSexplorerRedirect() {
  webPushHeader(F("Location"), F("FSexplorer.html"));
  webPushHeader(F("Cache-Control"), F("no-cache"));
  webSendStatus(303);
}

//=====================================================================================
// Called from the onNotFound catch-all; currentRequest is already bound there.
bool handleFile(String&& path)
{
  if (hasArgCompat("delete"))
  {
    if (!checkHttpAuth()) return false;
    DebugTf(PSTR("Delete -> [%s]\n\r"), argCompat("delete"));
    LittleFS.remove(argCompat("delete"));    // Datei löschen
    sendFSexplorerRedirect();
    return true;
  }
  // The upload prompt only when no recovery page exists at all: a filesystem from before
  // the gzipped pages has the plain /FSexplorer.html and keeps serving settings.ini etc.
  if (!LittleFS.exists("/FSexplorer.html.gz") && !LittleFS.exists("/FSexplorer.html")) { webSendP(200, PSTR("text/html; charset=UTF-8"), (PGM_P)Helper); return true; }
  // ADR-056 §1: settings.ini stores httppasswd and MQTTpasswd in cleartext, so this
  // read path sits INSIDE the protected admin boundary — it is not one of the "read
  // paths that do not expose secret values" the open carve-out covers. Every sibling
  // operation on this handler already gates; only the read did not, so setting an
  // admin password created the secret that an unauthenticated GET then handed out.
  {
    const char *sp = path.c_str();
    while (sp[0] == '/' && sp[1] == '/') sp++;   // collapse a "//settings.ini" alias
    if (strcasecmp_P(sp, PSTR(SETTINGS_FILE)) == 0 && !checkHttpAuth()) {
      // true, not false: checkHttpAuth() already sent the 401/403, and the
      // onNotFound catch-all sends its own 404 when handleFile() returns false.
      return true;
    }
  }
  // TASK-1689 (ADR-221): a web UI language pack, /i18n/<lang>.json with a 2-3 letter
  // lowercase tag, exists only as <lang>.json.gz and is sent with Content-Encoding:
  // gzip and the path ETag, like the pages. No plain fallback.
  {
    const char *p = path.c_str();
    if (strncmp_P(p, PSTR("/i18n/"), 6) == 0) {
      size_t n = 0;
      while (p[6 + n] >= 'a' && p[6 + n] <= 'z') n++;
      if (n < 2 || n > 3 || strcmp_P(p + 6 + n, PSTR(".json")) != 0) return false;
      char gz[24];
      snprintf_P(gz, sizeof(gz), PSTR("%s.gz"), p);
      return serveGzipPage(gz, nullptr, F("application/json"));
    }
  }
  if (!LittleFS.exists(path)) return false;
  // contentType() mutates its argument into the mime string, so snapshot the
  // file path first, then derive the mime from a throwaway copy.
  String filePath = path;
  String mime = path;            // contentType() rewrites this copy in place
  webSendFile(filePath.c_str(), contentType(mime).c_str(), /*gzip=*/false);
  return true;

} // handleFile()


//=====================================================================================
// Same-origin (CSRF) test for the upload body callback (ADR-056 §2). Mirrors
// isSameOriginRequest() in restAPI.ino exactly, including its deliberate
// permissiveness when the client sends no Origin/Referer at all (curl, Home
// Assistant, OTmonitor) and its rejection of a malformed origin — the start gate
// and the completion gate MUST agree, or a request passes one and fails the other.
// It reads the headers straight off `request` instead of going through
// headerCompat()/hostHeaderCompat(): those read the global currentRequest, which is
// not bound to THIS request during body parsing, and webBeginRequest() would clear
// g_responseSent / g_pendingHeaders mid-request. getHeader() matches the header name
// case-insensitively (WebRequest.cpp), same as the compat shim.
static bool uploadSameOrigin(AsyncWebServerRequest *request) {
  const AsyncWebHeader *o = request->getHeader("Origin");
  if (!o || o->value().length() == 0) o = request->getHeader("Referer");
  if (!o || o->value().length() == 0) return true;   // no origin header — allow
  const String &host = request->host();
  if (host.length() == 0) return true;               // cannot validate without Host
  const String &origin = o->value();
  int schemeSep = origin.indexOf("://");
  if (schemeSep < 0) return false;                   // malformed Origin/Referer
  String originHost = origin.substring(schemeSep + 3);
  int slash = originHost.indexOf('/');
  if (slash >= 0) originHost = originHost.substring(0, slash);
  return originHost.equalsIgnoreCase(host);
}

//=====================================================================================
// Async upload handler. ESPAsyncWebServer calls this per chunk:
//   (request, filename, index, data, len, final)
// index==0 marks the first chunk (== UPLOAD_FILE_START), final==true the last
// (== UPLOAD_FILE_END). The response (303 / 401 / 403 / 507) is owned entirely by
// the /upload POST completion handler in setupFSexplorer(); never send from here.
//
// Per-upload state lives on request->_tempFile, NOT in function-local statics. The
// sync ESP8266WebServer serialised whole requests, so one static File slot was safe;
// ESPAsyncWebServer invokes this callback per chunk DURING body reception, so two
// simultaneous uploads interleave and a static File would be reassigned out from
// under the first one. _tempFile is a public member of AsyncWebServerRequest
// (ESPAsyncWebServer.h) that ~AsyncWebServerRequest closes (WebRequest.cpp), so an
// aborted upload cleans itself up. No collision with the library's own use of the
// slot: only AsyncStaticWebHandler touches _tempFile, and its canHandle() requires
// HTTP_GET (WebHandlers.cpp), so it can never see this POST.
void handleFileUpload(AsyncWebServerRequest *request, const String &filename,
                      size_t index, uint8_t *data, size_t len, bool final)
{
  if (index == 0)
  {
    s_uploadOk = false;   // static: reset per upload or a stale result carries over

    // Authorize BEFORE the open. The same-origin check used to run only in the POST
    // completion handler, i.e. after LittleFS.open(...,"w") had already truncated the
    // target and every chunk had been written — the 403 was emitted after the
    // filesystem had changed. Deciding here means a cross-origin POST writes nothing.
    // Do NOT send a response from this callback; the completion handler owns it, and
    // its own checkHttpAuth() re-runs both tests to pick 401 vs 403.
    if (settings.sHTTPpasswd[0] != '\0')
    {
      if (!request->authenticate("admin", settings.sHTTPpasswd)) return;
      if (!uploadSameOrigin(request)) return;
    }

    String fn = filename;
    if (fn.length() > 30) fn = fn.substring(fn.length() - 30, fn.length());  // trim to 30 chars
    String path = "/";
    const AsyncWebParameter *pp = request->getParam("path", true);
    if (!pp) pp = request->getParam("path", false);
    if (pp) { path = pp->value(); if (!path.endsWith("/")) path += F("/"); }
    // No urlDecode: RFC 7578 §4.2 filename= is a raw basename and browsers do not
    // percent-encode it. Decoding it turned '+' into a space and truncated the name
    // at any literal '%' (invalid hex made strtol yield an embedded NUL), so
    // '50%.csv' was stored as '/50'.
    String fullname = path + fn;
    if (fullname.startsWith("//")) fullname = fullname.substring(1);

    DebugT(F("FileUpload Name: ")); Debugln(fullname);
    request->_tempFile = LittleFS.open(fullname, "w");
    s_uploadOk = (bool)request->_tempFile;
  }

  if (len && request->_tempFile)
  {
    // A short write means ENOSPC (or a name colliding with a directory, where the
    // impl returns 0). lfs_file_write returns the full count on success and the
    // wrapper returns 0 on error, so there are no benign short writes to tolerate.
    if (request->_tempFile.write(data, len) != len) s_uploadOk = false;
  }

  if (final)
  {
    if (request->_tempFile) {
      request->_tempFile.close();
      DebugT(F("FileUpload Size: ")); Debugln((String)(index + len));
    }
  }

} // handleFileUpload()


//=====================================================================================
const String &contentType(String& filename) 
{       
  if (filename.endsWith(".htm") || filename.endsWith(".html")) filename = F("text/html; charset=UTF-8");
  else if (filename.endsWith(".css")) filename = F("text/css");
  else if (filename.endsWith(".js")) filename = F("application/javascript");
  else if (filename.endsWith(".json")) filename = F("application/json");
  else if (filename.endsWith(".png")) filename = F("image/png");
  else if (filename.endsWith(".gif")) filename = F("image/gif");
  else if (filename.endsWith(".jpg")) filename = F("image/jpeg");
  else if (filename.endsWith(".ico")) filename = F("image/x-icon");
  else if (filename.endsWith(".xml")) filename = F("text/xml");
  else if (filename.endsWith(".pdf")) filename = F("application/x-pdf");
  else if (filename.endsWith(".zip")) filename = F("application/x-zip");
  else if (filename.endsWith(".gz")) filename = F("application/x-gzip");
  else filename = F("text/plain");
  return filename;
  
} // &contentType()

//=====================================================================================
void reBootESP(AsyncWebServerRequest *request)
{
  webBeginRequest(request);
  if (!checkHttpAuth()) return;
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("Reboot OTGW firmware ..", 120, "./", true);   // TASK-1445: page-relative, also behind a proxy prefix
} // reBootESP()

void resetWirelessButton(AsyncWebServerRequest *request)
{
  webBeginRequest(request);
  if (!checkHttpAuth()) return;
  DebugTln(F("Reset Wireless settings.."));
  resetWiFiSettings();
  doRedirect("Reboot OTGW firmware with reset wireless settings..", 120, "./", true);   // TASK-1445
}
//=====================================================================================
// Emits the bounded redirect/countdown page into the per-request response stream
// and queues the Refresh header. When reboot==true the restart is DEFERRED to the
// next loop() tick (requestDeferredReboot): the async response is only queued —
// not yet drained to the socket — when this returns, so tearing the services down
// here (doRestart) would truncate the body. loop() fires the reboot once
// !isFlashing(), giving lwIP time to flush (mirrors the OTA path).
void doRedirect(String msg, int wait, const char* URL, bool reboot)
{
  // clamp wait to sane bounds
  int safeWait = max(0, min(wait, 3600)); // 1 hour max

  // escape dynamic parts
  String safeMsg = msg;
  safeMsg.replace("&", "&amp;");
  safeMsg.replace("<", "&lt;");
  safeMsg.replace(">", "&gt;");
  safeMsg.replace("\"", "&quot;");
  safeMsg.replace("'", "&#39;");

  String safeURL = String(URL);
  safeURL.replace("'", "%27");
  safeURL.replace("\"", "%22");

  DebugTln(msg);
  // non-JS fallback for redirect
  webPushHeader(F("Refresh"), (String(safeWait) + F(";url=") + safeURL).c_str());

  AsyncResponseStream *s = restBeginStream("text/html; charset=UTF-8");
  if (!s) return;

  char waitBuf[12];
  snprintf_P(waitBuf, sizeof(waitBuf), PSTR("%d"), safeWait);

  s->print(F("<!DOCTYPE HTML><html lang='en-US'><head><meta charset='UTF-8'>"));
  s->print(F("<style type='text/css'>body {background-color: lightblue;}</style>"));
  s->print(F("<title>Redirect to ...</title></head><body><h1>FSexplorer</h1><h3>"));
  s->print(safeMsg);
  s->print(F("</h3><br><div style='width: 500px; position: relative; font-size: 25px;'>"));
  s->print(F("<div style='float: left;'>Redirect in &nbsp;</div><div style='float: left;' id='counter'>"));
  s->print(waitBuf);
  s->print(F("</div><div style='float: left;'>&nbsp; seconds ...</div><div style='float: right;'>&nbsp;</div></div>"));
  s->print(F("<br><br><hr>Wait for the redirect. In case you are not redirected automatically, then click this <a href='"));
  s->print(safeURL);
  s->print(F("'>link to continue</a>."));
  s->print(F("<script>setInterval(function(){var div=document.querySelector('#counter');var count=div.textContent*1-1;div.textContent=count;if(count<=0){window.location.replace('"));
  s->print(safeURL);
  s->print(F("');}},1000);</script></body></html>\r\n"));
  restFinalize();
  if (reboot) requestDeferredReboot("Reboot after upgrade");

} // doRedirect()
