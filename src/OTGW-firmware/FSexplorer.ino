/* 
***************************************************************************  
**  Program : FSexplorer
**  Version  : v2.0.0-alpha.84
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
**        setupFSexplorer();
**        httpServer.serveStatic("/FSexplorer.png",   LittleFS, "/FSexplorer.png");
**        httpServer.on("/",          sendIndexPage);
**        httpServer.on("/index",     sendIndexPage);
**        httpServer.on("/index.html",sendIndexPage);
**        httpServer.begin();
**      }
**      
**      loop()
**      {
**        httpServer.handleClient();
**        .
**        .
**      }
*/
// forward declaration — contentType is defined later in this file (line ~565)
const String &contentType(String& filename);

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



//=====================================================================================
void startWebserver(){
  if (!LittleFS.exists("/index.html")) {
    httpServer.on("/", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
    httpServer.on("/index", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
    httpServer.on("/index.html", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
  } else{
    // Serve index.html with ETag-based caching:
    //  - Browser caches index.html but always revalidates via If-None-Match (ETag = fsHash).
    //  - Unchanged FS → server replies 304 Not Modified (headers only, no body re-download).
    //  - FS upgraded → ETag changes → server replies 200 with fresh content.
    //  - JS assets use ?v=<fsHash> versioned URLs for independent long-term caching.
    auto sendIndex = []() {
      // Auth guard: if a password is configured, require it upfront so the browser
      // caches credentials before any API calls are made — avoids mid-session popup.
      if (!checkHttpAuth()) return;

      File f = LittleFS.open("/index.html", "r");
      if (!f) {
        httpServer.send(404, F("text/plain"), F("File not found"));
        return;
      }

      const char* fsHash = getFilesystemHash();
      bool hasHash = (fsHash && fsHash[0] != '\0');

      if (hasHash) {
        // ETags must be double-quoted per RFC 7232.
        char etag[24];
        snprintf_P(etag, sizeof(etag), PSTR("\"%s\""), fsHash);

        // Conditional GET: return 304 if browser's cached copy is still current.
        if (httpServer.hasHeader(F("If-None-Match")) &&
            strcmp(httpServer.header(F("If-None-Match")).c_str(), etag) == 0) {
          f.close();
          httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
          httpServer.sendHeader(F("ETag"), etag);
          httpServer.send(304);
          return;
        }

        httpServer.sendHeader(F("ETag"), etag);
      }

      // no-cache + ETag: browser stores the response but revalidates each visit.
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));

      // Stream line-by-line to inject ?v=<hash> into JS asset URLs.
      httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      httpServer.send(200, F("text/html; charset=UTF-8"), F(""));

      // Line buffer: 512 B is sized for the longest line in index.html plus the
      // ?v=<hash> injection slack. Stack-local (not static) because httpServer.
      // sendContent() can yield via feedWatchDog(), which would let a re-entered
      // sendIndex() clobber a shared static buffer mid-line. Stack peak of one
      // extra 512 B frame per concurrent (re-entered) request is within the
      // ESP8266 ~4 KB cooperative-scheduling budget. Saves 512 B BSS vs the
      // previous function-static placement.
      char lineBuf[512];
      while (f.available()) {
        int n = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[n] = '\0';
        // Strip trailing CR if present
        if (n > 0 && lineBuf[n - 1] == '\r') lineBuf[--n] = '\0';

        // In chunked mode an empty sendContent() marks end-of-response.
        // Blank HTML lines must therefore emit only a newline chunk.
        if (n == 0) {
          httpServer.sendContent(F("\n"));
          continue;
        }

        // Inject ?v=<hash> into JS asset URLs for cache-busting.
        if (hasHash && strstr_P(lineBuf, PSTR("src=\"./index.js\""))) {
          char* pos = strstr(lineBuf, "src=\"./index.js\"");
          *pos = '\0'; // terminate prefix
          if (lineBuf[0] != '\0') {
            httpServer.sendContent(lineBuf);
          }
          httpServer.sendContent(F("src=\"./index.js?v="));
          httpServer.sendContent(fsHash);
          httpServer.sendContent(F("\""));
          if (*(pos + 16) != '\0') {
            httpServer.sendContent(pos + 16);
          }
        } else if (hasHash && strstr_P(lineBuf, PSTR("src=\"./graph.js\""))) {
          char* pos = strstr(lineBuf, "src=\"./graph.js\"");
          *pos = '\0';
          if (lineBuf[0] != '\0') {
            httpServer.sendContent(lineBuf);
          }
          httpServer.sendContent(F("src=\"./graph.js?v="));
          httpServer.sendContent(fsHash);
          httpServer.sendContent(F("\""));
          if (*(pos + 16) != '\0') {
            httpServer.sendContent(pos + 16);
          }
        } else {
          httpServer.sendContent(lineBuf);
        }
        httpServer.sendContent(F("\n"));
      }
      httpServer.sendContent(F("")); // End chunked stream
      f.close();
    };
    
    httpServer.on("/", sendIndex);
    httpServer.on("/index", sendIndex);
    httpServer.on("/index.html", sendIndex);
  } 
  httpServer.serveStatic("/FSexplorer.png",   LittleFS, "/FSexplorer.png");
  
  // Serve CSS and JS files with appropriate caching headers

  // TASK-304: prefer the .gz sibling (pre-gzipped at build time) with
  // Content-Encoding: gzip when present. All target browsers (Chrome/FF/
  // Safari latest +2) accept gzip unconditionally; no Accept-Encoding
  // negotiation needed.
  httpServer.on("/index.js", []() {
    const char* fsHash = getFilesystemHash();
    if (httpServer.hasArg("v") && fsHash[0] != '\0' && strcmp(httpServer.arg("v").c_str(), fsHash) == 0) {
      httpServer.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
    } else {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    }
    if (LittleFS.exists("/index.js.gz")) {
      // streamFile() auto-detects the .gz suffix and emits Content-Encoding: gzip
      // itself; do NOT also send it manually or the browser receives it twice and
      // decompression silently produces an empty body (TASK-433).
      File f = LittleFS.open("/index.js.gz", "r");
      httpServer.streamFile(f, F("application/javascript"));
      f.close();
    } else {
      File f = LittleFS.open("/index.js", "r");
      httpServer.streamFile(f, F("application/javascript"));
      f.close();
    }
  });

  httpServer.on("/graph.js", []() {
    // Same versioned-URL caching + gzip-preference strategy as /index.js (see above).
    const char* fsHash = getFilesystemHash();
    if (httpServer.hasArg("v") && fsHash[0] != '\0' && strcmp(httpServer.arg("v").c_str(), fsHash) == 0) {
      httpServer.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
    } else {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    }
    if (LittleFS.exists("/graph.js.gz")) {
      // streamFile() auto-detects the .gz suffix and emits Content-Encoding: gzip
      // itself; manually adding it again sends it twice and breaks the browser's
      // decompression flow (TASK-433).
      File f = LittleFS.open("/graph.js.gz", "r");
      httpServer.streamFile(f, F("application/javascript"));
      f.close();
    } else {
      File f = LittleFS.open("/graph.js", "r");
      httpServer.streamFile(f, F("application/javascript"));
      f.close();
    }
  });
#if HAS_PIC
  //otgw pic functions
  httpServer.on("/pic", upgradepic);
#endif
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_ANY, processAPI);  //was only HTTP_GET (20210110)

  // Enable collection of headers the API layer needs:
  //   - If-None-Match: ETag conditional requests (index.html cache)
  //   - Origin / Referer: CSRF same-origin check in restAPI.ino (ADR-056 §2).
  //     Without these, httpServer.header("Origin") always returns empty and
  //     isSameOriginRequest() treats every browser request as "non-browser",
  //     silently disabling CSRF protection.
  // Use the array+count form: portable across ESP8266 Core 2.7.4 (LTS), Core 3.x,
  // and ESP32 WebServer. The variadic template overload exists only on ESP8266
  // Core 3.x — keeping the array form is the lowest common denominator.
  // ESP8266WebServer stores these pointers by value (no PROGMEM copy-out),
  // so plain RAM string literals are correct here — not F()/PSTR().
  {
    static const char* collectHeaderKeys[] = {"If-None-Match", "Origin", "Referer"};
    httpServer.collectHeaders(collectHeaderKeys, 3);
  }

  httpServer.begin();
  // Set up first message as the IP address
  DebugTln(F("\nHTTP Server started\r"));  
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%03d.%03d.%d.%d"), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DebugTf(PSTR("\nAssigned IP=%s\r\n"), cMsg);
}
//=====================================================================================
void setupFSexplorer(){    
  LittleFS.begin();
  if (LittleFS.exists("/FSexplorer.html")) 
  {
    httpServer.on("/FSexplorer.html", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
    httpServer.on("/FSexplorer", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
  }
  else 
  {
    httpServer.send_P(200, PSTR("text/html; charset=UTF-8"), Helper); //Upload the FSexplorer.html
  }
  httpServer.on("/api/firmwarefilelist", apifirmwarefilelist);  // DEPRECATED: unversioned, will be removed in v1.3.0 (see ADR-035)
  httpServer.on("/api/listfiles", apilistfiles);               // DEPRECATED: unversioned, will be removed in v1.3.0 (see ADR-035)
  // httpServer.on("/LittleFSformat", formatLittleFS);
  httpServer.on("/upload", HTTP_POST, []() {
    // If auth failed, handleFileUpload skipped the write; send 401 challenge here.
    // If auth succeeded, the 303 redirect was already sent by handleFileUpload.
    checkHttpAuth();
  }, handleFileUpload);
  httpServer.on("/ReBoot", reBootESP);
  httpServer.on("/ResetWireless", resetWirelessButton);
 
  httpServer.onNotFound([]()
  {
    if (httpServer.uri().indexOf("/api/") == 0)
    {
      processAPI();
      return;
    }
    // TASK-683 port: emit one outcome-bearing line per static request — 200
    // gated on bRestAPI (chatty debug surface), 404 always-on (actionable).
    String path = httpServer.urlDecode(httpServer.uri());
    const bool served = handleFile(String(path));
    if (!served) {
      httpServer.send_P(404, PSTR("text/plain"), PSTR("FileNotFound\r\n"));
      DebugTf(PSTR("http GET %s => 404\r\n"), path.c_str());
    } else if (state.debug.bRestAPI) {
      DebugTf(PSTR("http GET %s => 200 (file)\r\n"), path.c_str());
    }
  });
  
} // setupFSexplorer()

//=====================================================================================
void apifirmwarefilelist() {
  // TASK-683 port: drop JSON-mirror-to-telnet noise. Function-entry trace and
  // per-file GetVersion result lines are gated on bRestAPI; one always-on
  // summary line is emitted at the end.
  const unsigned long startMs = millis();
  unsigned int entryCount = 0;
  if (state.debug.bRestAPI) DebugTf(PSTR("API: apifirmwarefilelist()\r\n"));

  // 150 bytes covers longest entry: path (~30) + version (~32) + fwversion (~32) + JSON overhead
  char entryBuffer[150];
  // ADR-004: stack char[] instead of String to avoid per-iteration heap churn.
  char version[32]   = {0};
  char fwversion[32] = {0};
  File f;
  bool firstEntry = true;

  String dirpath = "/" + String(state.pic.sDeviceid);
  if (state.debug.bRestAPI) DebugTf(PSTR("dirpath=%s\r\n"), dirpath.c_str());

  // Start chunked response with JSON array opening
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("application/json"), F(""));
  httpServer.sendContent(F("["));

  PlatformDir dir(dirpath.c_str());
  if (!dir.valid()) {
    httpServer.sendContent(F("]\r\n"));
    httpServer.sendContent(F(""));
    DebugTf(PSTR("api firmware/files: 0 entries (%lums)\r\n"),
            (unsigned long)(millis() - startMs));
    return;
  }
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

      // Add comma separator after first entry
      if (!firstEntry) {
        httpServer.sendContent(F(","));
      }
      firstEntry = false;

      // Stream this entry directly (fits in 256-byte buffer)
      // CSTR() macro handles null safety globally - returns "" if null
      snprintf_P(entryBuffer, sizeof(entryBuffer),
                 PSTR("{\"name\":\"%s\",\"version\":\"%s\",\"size\":%d}"),
                 CSTR(entryName), CSTR(version), (int)entrySize);
      httpServer.sendContent(entryBuffer);

      feedWatchDog(); // Feed watchdog during potentially long operation
      entryCount++;
    }
  }

  // Close JSON array
  httpServer.sendContent(F("]\r\n"));
  httpServer.sendContent(F("")); // End chunked response

  DebugTf(PSTR("api firmware/files: %u entries (%lums)\r\n"),
          entryCount, (unsigned long)(millis() - startMs));
}


//=====================================================================================


void apilistfiles()
{
  // --- Delete handler: local buffer instead of global cMsg ---
  if (httpServer.hasArg("delete")) {
    if (!checkHttpAuth()) return;  // 401 already sent
    char deletePath[34];  // LittleFS paths are max 31 chars + '/' prefix + '\0'
    strlcpy(deletePath, httpServer.arg("delete").c_str(), sizeof(deletePath));
    // Normalize: LittleFS paths must start with '/'
    if (deletePath[0] != '/' && deletePath[0] != '\0') {
      size_t len = strnlen(deletePath, sizeof(deletePath) - 2);
      memmove(deletePath + 1, deletePath, len + 1);
      deletePath[0] = '/';
    }
    DebugTf(PSTR("Delete -> [%s]\r\n"), deletePath);
    if (!LittleFS.exists(deletePath)) {
      httpServer.send(404, F("text/plain"), F("File not found"));
    } else if (LittleFS.remove(deletePath)) {
      httpServer.send(200, F("text/plain"), F("File deleted"));
    } else {
      httpServer.send(500, F("text/plain"), F("Delete failed"));
    }
    return;
  }

  // --- File listing: stream directly from LittleFS, no buffering/sorting ---
  // Sorting and size formatting are handled by the frontend (FSexplorer.html).
  String path = "/";
  if (httpServer.hasArg("path")) {
    path = httpServer.arg("path");
  }

  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("application/json"), F(""));
  httpServer.sendContent(F("["));

  char buf[100];
  bool first = true;
  int fileCount = 0;
  bool truncated = false;

  PlatformDir dir(path.c_str());
  if (!dir.valid()) {
    httpServer.sendContent(F("]\r\n"));
    httpServer.sendContent(F(""));
    return;
  }
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
    if (!first) httpServer.sendContent(F(","));
    first = false;

    snprintf_P(buf, sizeof(buf),
      PSTR("{\"name\":\"%s\",\"size\":%ld,\"type\":\"%s\"}"),
      fname.c_str(), fsize,
      isDir ? "dir" : "file");
    httpServer.sendContent(buf);
    fileCount++;
  }

  // Storage info as last entry (raw bytes — frontend formats for display)
  FSInfo fsInfo;
  platformFSInfo(fsInfo);
  unsigned long totalBytes = fsInfo.totalBytes;
  unsigned long usedBytesRaw = fsInfo.usedBytes;
  if (!first) httpServer.sendContent(F(","));
  unsigned long usedBytes = (unsigned long)(usedBytesRaw * 1.05);
  unsigned long freeBytes = totalBytes - usedBytes;
  snprintf_P(buf, sizeof(buf),
    PSTR("{\"usedBytes\":%lu,\"totalBytes\":%lu,\"freeBytes\":%lu,\"truncated\":%s}"),
    usedBytes, totalBytes, freeBytes,
    truncated ? "true" : "false");
  httpServer.sendContent(buf);

  httpServer.sendContent(F("]\r\n"));
  httpServer.sendContent(F(""));

} // apilistfiles()


//=====================================================================================
bool handleFile(String&& path) 
{
  if (httpServer.hasArg("delete")) 
  {
    if (!checkHttpAuth()) return false;
    DebugTf(PSTR("Delete -> [%s]\n\r"),  httpServer.arg("delete").c_str());
    LittleFS.remove(httpServer.arg("delete"));    // Datei löschen
    httpServer.sendContent(Header);
    return true;
  }
  if (!LittleFS.exists("/FSexplorer.html")) httpServer.send_P(200, PSTR("text/html; charset=UTF-8"), Helper); //Upload the FSexplorer.html
  if (path.endsWith("/")) path += F("index.html");
  return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, contentType(path)); f.close(); true;}) : false;

} // handleFile()


//=====================================================================================
void handleFileUpload() 
{
  static File fsUploadFile;
  static bool uploadAuthorized = true;
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
    // Check auth by reading headers only - does NOT send a response.
    // If auth is required and fails, skip the file open so nothing is written.
    uploadAuthorized = (settings.sHTTPpasswd[0] == '\0' || httpServer.authenticate("admin", settings.sHTTPpasswd));
    if (!uploadAuthorized) return;

    if (upload.filename.length() > 30) 
    {
      upload.filename = upload.filename.substring(upload.filename.length() - 30, upload.filename.length());  // Dateinamen auf 30 Zeichen kürzen
    }
    String path = "/";
    if (httpServer.hasArg("path")) {
        path = httpServer.arg("path");
        if (!path.endsWith("/")) path += F("/");
    }
    String filename = path + httpServer.urlDecode(upload.filename);
    if(filename.startsWith("//")) filename = filename.substring(1);
    
    DebugT(F("FileUpload Name: ")); Debugln(filename);
    fsUploadFile = LittleFS.open(filename, "w");
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) 
  {
    DebugT(F("FileUpload Data: ")); Debugln((String)upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) 
  {
    if (fsUploadFile)
      fsUploadFile.close();
    if (uploadAuthorized) {
      DebugT(F("FileUpload Size: ")); Debugln((String)upload.totalSize);
      httpServer.sendContent(Header);
    }
  }
  
} // handleFileUpload() 


//=====================================================================================
void formatLittleFS() 
{       //Formatiert den Speicher
  if (!LittleFS.exists("/!format")) return;
  DebugTln(F("Format LittleFS"));
  LittleFS.format();
  httpServer.sendContent(Header);
  
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
void reBootESP()
{
  if (!checkHttpAuth()) return;
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("Reboot OTGW firmware ..", 120, "/", true);   
} // reBootESP()

void resetWirelessButton()
{
  if (!checkHttpAuth()) return;
  DebugTln(F("Reset Wireless settings.."));
  resetWiFiSettings();
  doRedirect("Reboot OTGW firmware with reset wireless settings..", 120, "/", true);   
}
//=====================================================================================
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
  // add non-JS fallback for redirect
  httpServer.sendHeader(F("Refresh"), String(safeWait) + F(";url=") + safeURL);
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("text/html; charset=UTF-8"), F(""));

  char waitBuf[12];
  snprintf_P(waitBuf, sizeof(waitBuf), PSTR("%d"), safeWait);

  httpServer.sendContent_P(PSTR("<!DOCTYPE HTML><html lang='en-US'><head><meta charset='UTF-8'>"));
  httpServer.sendContent_P(PSTR("<style type='text/css'>body {background-color: lightblue;}</style>"));
  httpServer.sendContent_P(PSTR("<title>Redirect to ...</title></head><body><h1>FSexplorer</h1><h3>"));
  httpServer.sendContent(safeMsg);
  httpServer.sendContent_P(PSTR("</h3><br><div style='width: 500px; position: relative; font-size: 25px;'>"));
  httpServer.sendContent_P(PSTR("<div style='float: left;'>Redirect in &nbsp;</div><div style='float: left;' id='counter'>"));
  httpServer.sendContent(waitBuf);
  httpServer.sendContent_P(PSTR("</div><div style='float: left;'>&nbsp; seconds ...</div><div style='float: right;'>&nbsp;</div></div>"));
  httpServer.sendContent_P(PSTR("<br><br><hr>Wait for the redirect. In case you are not redirected automatically, then click this <a href='"));
  httpServer.sendContent(safeURL);
  httpServer.sendContent_P(PSTR("'>link to continue</a>."));
  httpServer.sendContent_P(PSTR("<script>setInterval(function(){var div=document.querySelector('#counter');var count=div.textContent*1-1;div.textContent=count;if(count<=0){window.location.replace('"));
  httpServer.sendContent(safeURL);
  httpServer.sendContent_P(PSTR("');}},1000);</script></body></html>\r\n"));
  httpServer.sendContent(F(""));
  if (reboot) doRestart("Reboot after upgrade");
  
} // doRedirect()
