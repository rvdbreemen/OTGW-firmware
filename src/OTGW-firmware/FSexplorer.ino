/* 
***************************************************************************  
**  Program : FSexplorer
**  Version  : v2.0.0-alpha.334
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

#define MAX_FILES_IN_LIST   40

const char Helper[] PROGMEM =
  "<br>You first need to upload these two files:\n"
  "<ul>\n"
  "  <li>FSexplorer.html</li>\n"
  "  <li>FSexplorer.css</li>\n"
  "</ul>\n"
  "<form method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">\n"
  "  <input type=\"file\" name=\"upload\">\n"
  "  <input type=\"submit\" value=\"Upload\">\n"
  "</form>\n"
  "<br/><b>or</b> you can use the <i>Flash Utility</i> to flash firmware or LittleFS!\n"
  "<form action='/update' method='GET'>\n"
  "  <input type='submit' name='SUBMIT' value='Flash Utility'/>\n"
  "</form>\n";
const char Header[] PROGMEM = "HTTP/1.1 303 OK\r\nLocation:FSexplorer.html\r\nCache-Control: no-cache\r\n";


// Serve a static webui asset with ETag-based cache validation (ADR-139, amended
// TASK-958). Stable URLs, no ?v= query versioning: the ETag is the filesystem
// hash. Cache-Control: no-cache means the browser MAY store the asset but MUST
// revalidate (conditional GET) before every use — it never serves from cache
// blind. Unchanged FS -> 304 (headers only, no body); a reflash/OTA changes the
// hash -> 200 with the new asset on the very next load. This replaces the earlier
// max-age=60 window: that window let the browser serve stale assets without asking
// for up to 60s, so a filesystem OTA (which DOES replace the assets) still showed
// the old UI on the immediate post-reboot reload (window.location.href='/' does
// not bypass a fresh cache entry). no-cache costs one tiny 304 revalidation per
// asset per load, negligible on the trusted LAN. Assets are
// stored as plain readable files on LittleFS (no build-time gzip, maintainer
// directive); streamed straight from flash via AsyncFileResponse, never buffered
// whole on the fragmented S3 heap.
static void serveVersionedAsset(const char* path, const __FlashStringHelper* mime) {
  // Existence first, before any ETag / max-age is staged and before the 304
  // short-circuit. A missing asset must NOT be cached: the ETag is the filesystem
  // hash, which only changes on a reflash, so a 404 tagged with it (or answered
  // 304 against it) would be reused for the whole firmware session and would even
  // survive a manual FSexplorer re-upload of the file (an upload does not bump the
  // hash). So a miss returns a no-cache 404 with no validator, matching the
  // pre-ETag behaviour and refetching every time.
  if (!LittleFS.exists(path)) {
    webPushHeader(F("Cache-Control"), F("no-cache"));
    webSend(404, F("text/plain"), F("File not found"));
    return;
  }
  const char* fsHash = getFilesystemHash();
  if (fsHash && fsHash[0] != '\0') {
    // The ETag must identify the RESOURCE REPRESENTATION, not merely the
    // filesystem build. The root route ("/", "/index", "/index.html") serves
    // index.html OR v2.html depending on settings.ui.bUseV2 (sendIndex), so a
    // bare fsHash ETag is identical for both shells. The browser then gets a
    // false 304 when the UI toggle flips and keeps showing the stale shell until
    // a hard reload (TASK-960: "need CTRL-R between UI switches"). Include the
    // served path so each shell, and each asset, has a distinct validator.
    char etag[64];
    snprintf_P(etag, sizeof(etag), PSTR("\"%s-%s\""), fsHash, path);
    if (hasHeaderCompat(F("If-None-Match")) &&
        strcmp(headerCompat(F("If-None-Match")), etag) == 0) {
      webPushHeader(F("Cache-Control"), F("no-cache"));
      webPushHeader(F("ETag"), etag);
      webSendStatus(304);
      return;
    }
    webPushHeader(F("ETag"), etag);
  }
  webPushHeader(F("Cache-Control"), F("no-cache"));
  webSendFile(path, mime, /*gzip=*/false);
}

// TASK-989: fonts are large immutable binaries referenced from ds-tokens.css
// (@font-face). Unlike the HTML/CSS/JS shell they never change between releases,
// so serve them with a long immutable max-age: after the first visit the browser
// never asks again (not even a 304 probe), removing three parallel connections
// from every page load. They previously fell through to the onNotFound catch-all
// (no cache headers -> refetched per load, plus String churn on the async task).
static void serveImmutableAsset(const char* path, const __FlashStringHelper* mime) {
  if (!LittleFS.exists(path)) {
    webPushHeader(F("Cache-Control"), F("no-cache"));
    webSend(404, F("text/plain"), F("File not found"));
    return;
  }
  webPushHeader(F("Cache-Control"), F("max-age=31536000, immutable"));
  webSendFile(path, mime, /*gzip=*/false);
}

// Serve /FSexplorer.html for the no-index fallback routes (/, /index, /index.html).
static void sendFSexplorerFallback(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  webSendFile("/FSexplorer.html", F("text/html; charset=UTF-8"), /*gzip=*/false);
}

// Serve the web UI shell (index.html). ADR-139: the bespoke chunked
// beginChunkedResponse emitter that rewrote ?v=<fsHash> into asset URLs line by
// line is retired. The shell is now a plain static file served from flash via
// AsyncFileResponse with ETag validation, identical to every other asset, so it
// is never buffered whole on the fragmented S3 heap and shares one cache policy.
static void sendIndex(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  // Auth guard: if a password is configured, require it upfront so the browser
  // caches credentials before any API call is made (ADR-056), avoiding a
  // mid-session popup. checkHttpAuth() sends the 401 challenge on failure.
  if (!checkHttpAuth()) return;
  // TASK-908: device-wide default UI selector. When settings.ui.bUseV2 is set,
  // the root path serves the redesigned v2 shell; otherwise the classic UI.
  // Both shells stay individually reachable at /index.html and /v2.html so the
  // in-page toggle can always navigate explicitly regardless of the flag.
  if (settings.ui.bUseV2 && LittleFS.exists("/v2.html")) {
    serveVersionedAsset("/v2.html", F("text/html; charset=UTF-8"));
  } else {
    serveVersionedAsset("/index.html", F("text/html; charset=UTF-8"));
  }
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
  // Versioned-asset helpers register one handler per asset; the lambda binds the
  // request context before delegating to serveVersionedAsset().
  if (!LittleFS.exists("/index.html")) {
    server.on("/",           HTTP_GET, sendFSexplorerFallback);
    server.on("/index",      HTTP_GET, sendFSexplorerFallback);
    server.on("/index.html", HTTP_GET, sendFSexplorerFallback);
  } else {
    // Serve index.html with ETag-based caching (ADR-139, amended TASK-958):
    //  - Cache-Control: no-cache — browser may store but MUST revalidate via If-None-Match (ETag = fsHash) every load.
    //  - Unchanged FS → server replies 304 Not Modified (headers only, no body re-download).
    //  - FS upgraded/OTA → ETag changes → server replies 200 with fresh content on the very next load (no stale window).
    //  - JS/CSS assets share the same stable-path + ETag policy (no ?v= query versioning).
    server.on("/",           HTTP_GET, sendIndex);
    server.on("/index",      HTTP_GET, sendIndex);
    server.on("/index.html", HTTP_GET, sendIndex);
  }
  server.serveStatic("/FSexplorer.png", LittleFS, "/FSexplorer.png");

  // ETag + bounded max-age caching for every static webui asset (ADR-139). Plain
  // readable files served from flash via AsyncFileResponse; no gzip, no ?v= query.
  server.on("/index.js",         HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/index.js",         F("application/javascript")); });
  server.on("/graph.js",         HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/graph.js",         F("application/javascript")); });
  server.on("/sat.js",           HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/sat.js",           F("application/javascript")); });
  server.on("/sat-slider.js",    HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/sat-slider.js",    F("application/javascript")); });
  server.on("/echarts-theme.js", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/echarts-theme.js", F("application/javascript")); });
  server.on("/theme-toggle.js",  HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/theme-toggle.js",  F("application/javascript")); });
  server.on("/components.css",   HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/components.css",   F("text/css")); });
  server.on("/ds-tokens.css",    HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/ds-tokens.css",    F("text/css")); });
  // TASK-908: coexisting v2 Web UI bundle (served next to the classic assets).
  server.on("/v2.html",          HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); if (!checkHttpAuth()) return; serveVersionedAsset("/v2.html", F("text/html; charset=UTF-8")); });
  server.on("/v2.js",            HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/v2.js",            F("application/javascript")); });
  server.on("/v2.css",           HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/v2.css",           F("text/css")); });
  // TASK-989: ds-tokens.css + v2.css concatenated at build time (build.py,
  // generate_v2_css_bundle) -- v2.html loads this instead of the two separate
  // files, cutting one request off its critical path. /v2.css and
  // /ds-tokens.css stay routed above for design.html/index.html/FSexplorer.html.
  server.on("/v2-bundle.css",    HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/v2-bundle.css",    F("text/css")); });
  // TASK-989: immutable-cached font routes (see serveImmutableAsset above).
  server.on("/fonts/inter-400.woff2",          HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveImmutableAsset("/fonts/inter-400.woff2",          F("font/woff2")); });
  server.on("/fonts/inter-700.woff2",          HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveImmutableAsset("/fonts/inter-700.woff2",          F("font/woff2")); });
  server.on("/fonts/jetbrains-mono-400.woff2", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveImmutableAsset("/fonts/jetbrains-mono-400.woff2", F("font/woff2")); });
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
  // Set up first message as the IP address
  DebugTln(F("\nHTTP Server started\r"));
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%03d.%03d.%d.%d"), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DebugTf(PSTR("\nAssigned IP=%s\r\n"), cMsg);
}
// Serve /FSexplorer.html (static file).
static void sendFSexplorerHtml(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  if (LittleFS.exists("/FSexplorer.html")) {
    webSendFile("/FSexplorer.html", F("text/html; charset=UTF-8"), /*gzip=*/false);
  } else {
    webSendP(200, PSTR("text/html; charset=UTF-8"), Helper);  // prompt to upload FSexplorer.html
  }
}

//=====================================================================================
void setupFSexplorer(){
  LittleFS.begin();
  server.on("/FSexplorer.html", HTTP_GET, sendFSexplorerHtml);
  server.on("/FSexplorer",      HTTP_GET, sendFSexplorerHtml);

  server.on("/api/firmwarefilelist", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); apifirmwarefilelist(); });  // DEPRECATED: unversioned (ADR-035)
  server.on("/api/listfiles",        HTTP_ANY, [](AsyncWebServerRequest *r){ webBeginRequest(r); apilistfiles(); });         // DEPRECATED: unversioned (ADR-035)
  // server.on("/LittleFSformat", HTTP_GET, formatLittleFS);
  // /upload: the body completion handler runs after the upload finished. If
  // auth failed, handleFileUpload skipped the write; send the 401 challenge here.
  // If auth succeeded, handleFileUpload already queued the 303 redirect body.
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      webBeginRequest(request);
      // checkHttpAuth() sends a 401 challenge when auth is required and failed.
      // On success, redirect back to FSexplorer.html (303 + Location) — the same
      // outcome the sync server produced with the raw `Header` literal.
      if (!checkHttpAuth()) return;
      if (!g_responseSent) sendFSexplorerRedirect();
    },
    handleFileUpload);
  server.on("/ReBoot",        HTTP_ANY, reBootESP);
  server.on("/ResetWireless", HTTP_ANY, resetWirelessButton);

  // TASK-865.11: register the OTA /update endpoint on the AsyncWebServer here,
  // alongside the other routes and before startWebserver()'s server.begin().
  // setup() is idempotent (guarded by _routesRegistered), so the per-connect
  // updateCredentials() calls in startWiFi() never re-register the route.
  httpUpdater.setup(&server);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
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
    String path = request->urlDecode(request->url());
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
    char deletePath[34];  // LittleFS paths are max 31 chars + '/' prefix + '\0'
    strlcpy(deletePath, argCompat("delete"), sizeof(deletePath));
    // Normalize: LittleFS paths must start with '/'
    if (deletePath[0] != '/' && deletePath[0] != '\0') {
      size_t len = strnlen(deletePath, sizeof(deletePath) - 2);
      memmove(deletePath + 1, deletePath, len + 1);
      deletePath[0] = '/';
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
  if (!LittleFS.exists("/FSexplorer.html")) { webSendP(200, PSTR("text/html; charset=UTF-8"), (PGM_P)Helper); return true; }
  if (path.endsWith("/")) path += F("index.html");
  if (!LittleFS.exists(path)) return false;
  // contentType() mutates its argument into the mime string, so snapshot the
  // file path first, then derive the mime from a throwaway copy.
  String filePath = path;
  String mime = path;            // contentType() rewrites this copy in place
  webSendFile(filePath.c_str(), contentType(mime).c_str(), /*gzip=*/false);
  return true;

} // handleFile()


//=====================================================================================
// Async upload handler. ESPAsyncWebServer calls this per chunk:
//   (request, filename, index, data, len, final)
// index==0 marks the first chunk (== UPLOAD_FILE_START), final==true the last
// (== UPLOAD_FILE_END). The 303 redirect body is queued by the /upload POST
// completion handler in setupFSexplorer(), not here.
void handleFileUpload(AsyncWebServerRequest *request, const String &filename,
                      size_t index, uint8_t *data, size_t len, bool final)
{
  static File fsUploadFile;
  static bool uploadAuthorized = true;

  if (index == 0)
  {
    // Auth: do NOT send a response here (the POST completion handler does).
    // If auth fails, skip the open so nothing is written.
    uploadAuthorized = (settings.sHTTPpasswd[0] == '\0' ||
                        request->authenticate("admin", settings.sHTTPpasswd));
    if (!uploadAuthorized) return;

    String fn = filename;
    if (fn.length() > 30) fn = fn.substring(fn.length() - 30, fn.length());  // trim to 30 chars
    String path = "/";
    const AsyncWebParameter *pp = request->getParam("path", true);
    if (!pp) pp = request->getParam("path", false);
    if (pp) { path = pp->value(); if (!path.endsWith("/")) path += F("/"); }
    String fullname = path + request->urlDecode(fn);
    if (fullname.startsWith("//")) fullname = fullname.substring(1);

    DebugT(F("FileUpload Name: ")); Debugln(fullname);
    fsUploadFile = LittleFS.open(fullname, "w");
  }

  if (uploadAuthorized && len && fsUploadFile)
  {
    fsUploadFile.write(data, len);
  }

  if (final)
  {
    if (fsUploadFile) fsUploadFile.close();
    if (uploadAuthorized) {
      DebugT(F("FileUpload Size: ")); Debugln((String)(index + len));
    }
  }

} // handleFileUpload()


//=====================================================================================
void formatLittleFS(AsyncWebServerRequest *request)
{       //Formatiert den Speicher
  webBeginRequest(request);
  if (!LittleFS.exists("/!format")) return;
  DebugTln(F("Format LittleFS"));
  LittleFS.format();
  sendFSexplorerRedirect();

} // formatLittleFS()

//=====================================================================================
const String formatBytes(size_t const& bytes) 
{ 
  return (bytes < 1024) ? String(bytes) + " Byte" : (bytes < (1024 * 1024)) ? String(bytes / 1024.0) + " KB" : String(bytes / 1024.0 / 1024.0) + " MB";

} //formatBytes()

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
bool freeSpace(uint16_t const& printsize)
{
  FSInfo fsInfo;
  platformFSInfo(fsInfo);
  unsigned long totalB = fsInfo.totalBytes;
  unsigned long usedB  = fsInfo.usedBytes;
  Debugln(formatBytes(totalB - (unsigned long)(usedB * 1.05)) + " im LittleFS frei");
  return (totalB - (unsigned long)(usedB * 1.05) > printsize);

} // freeSpace()

//=====================================================================================
void reBootESP(AsyncWebServerRequest *request)
{
  webBeginRequest(request);
  if (!checkHttpAuth()) return;
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("Reboot OTGW firmware ..", 120, "/", true);
} // reBootESP()

void resetWirelessButton(AsyncWebServerRequest *request)
{
  webBeginRequest(request);
  if (!checkHttpAuth()) return;
  DebugTln(F("Reset Wireless settings.."));
  resetWiFiSettings();
  doRedirect("Reboot OTGW firmware with reset wireless settings..", 120, "/", true);
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
