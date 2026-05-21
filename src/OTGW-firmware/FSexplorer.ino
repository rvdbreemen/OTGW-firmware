/* 
***************************************************************************  
**  Program : FSexplorer
**  Version  : v1.6.0-beta.16
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
    // LittleFS not mounted or index.html missing — show the upload helper page.
    // Helper is a PROGMEM string with a form to upload FSexplorer.html and a
    // link to the Flash Utility at /update.
    auto sendHelper = []() {
      httpServer.send_P(200, PSTR("text/html; charset=UTF-8"), Helper);
    };
    httpServer.on("/", sendHelper);
    httpServer.on("/index", sendHelper);
    httpServer.on("/index.html", sendHelper);
  } else{
    // Serve index.html with ETag-based caching:
    //  - Browser caches index.html but always revalidates via If-None-Match (ETag = fsHash).
    //  - Unchanged FS → server replies 304 Not Modified (headers only, no body re-download).
    //  - FS upgraded → ETag changes → server replies 200 with fresh content.
    //  - JS assets use ?v=<fsHash> versioned URLs for independent long-term caching.
    auto sendIndex = []() {
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

      // Use a fixed-size line buffer instead of String to avoid heap fragmentation.
      static char lineBuf[512];
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
  httpServer.on("/index.css", []() {
    File f = LittleFS.open("/index.css", "r");
    if (!f) { httpServer.send(404, F("text/plain"), F("File not found")); return; }
    // CSS can be cached for longer periods (1 day)
    httpServer.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
    httpServer.streamFile(f, F("text/css"));
    f.close();
  });
  
  httpServer.on("/index.js", []() {
    File f = LittleFS.open("/index.js", "r");
    if (!f) { httpServer.send(404, F("text/plain"), F("File not found")); return; }
    // ?v=<hash> versioned requests get long-term cache; bare /index.js gets no-cache.
    const char* fsHash = getFilesystemHash();
    // httpServer.arg() returns String by value — compare directly to avoid dangling c_str()
    if (httpServer.hasArg("v") && fsHash[0] != '\0' && strcmp(httpServer.arg("v").c_str(), fsHash) == 0) {
      httpServer.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
    } else {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    }
    httpServer.streamFile(f, F("application/javascript"));
    f.close();
  });

  httpServer.on("/graph.js", []() {
    File f = LittleFS.open("/graph.js", "r");
    if (!f) { httpServer.send(404, F("text/plain"), F("File not found")); return; }
    // Same versioned-URL caching strategy as index.js (see above).
    const char* fsHash = getFilesystemHash();
    if (httpServer.hasArg("v") && fsHash[0] != '\0' && strcmp(httpServer.arg("v").c_str(), fsHash) == 0) {
      httpServer.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
    } else {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    }
    httpServer.streamFile(f, F("application/javascript"));
    f.close();
  });
  //otgw pic functions
  httpServer.on("/pic", upgradepic);
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_ANY, processAPI);  //was only HTTP_GET (20210110)

  // Enable collection of If-None-Match so index.html ETag conditional requests work.
  // TASK-398: use array form (works on both Core 2.7.4 and 3.1.2; single-arg overload is 3.x-only).
  {
    static const char *otaHeaderKeys[] = {"If-None-Match"};
    httpServer.collectHeaders(otaHeaderKeys, 1);
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
      if (!f) { httpServer.send(404, F("text/plain"), F("File not found")); return; }
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
    httpServer.on("/FSexplorer", []() {
      File f = LittleFS.open("/FSexplorer.html", "r");
      if (!f) { httpServer.send(404, F("text/plain"), F("File not found")); return; }
      httpServer.streamFile(f, F("text/html; charset=UTF-8"));
      f.close();
    });
  }
  else
  {
    // FSexplorer.html not on filesystem (FS not mounted or file missing).
    // Register routes that serve the Helper page, which includes a link to the
    // Flash Utility at /update so the user can upload the filesystem image.
    auto sendHelper = []() {
      httpServer.send_P(200, PSTR("text/html; charset=UTF-8"), Helper);
    };
    httpServer.on("/FSexplorer.html", sendHelper);
    httpServer.on("/FSexplorer", sendHelper);
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
    }
    else
    {
      if (state.debug.bRestAPI) DebugTf(PSTR("onNotFound: handleFile(%s)\r\n"),
                      String(httpServer.urlDecode(httpServer.uri())).c_str());
      if (!handleFile(httpServer.urlDecode(httpServer.uri())))
      {
        httpServer.send_P(404, PSTR("text/plain"), PSTR("FileNotFound\r\n"));
      }
    }
  });
  
} // setupFSexplorer()

//=====================================================================================
void apifirmwarefilelist() {
  DebugTf(PSTR("API: apifirmwarefilelist()\r\n"));
  
  // 150 bytes covers longest entry: path (~30) + version (~32) + fwversion (~32) + JSON overhead
  char entryBuffer[150];
  String version, fwversion;
  Dir dir;
  File f;
  bool firstEntry = true;

  String dirpath = "/" + String(state.pic.sDeviceid);
  DebugTf(PSTR("dirpath=%s\r\n"), dirpath.c_str());
  
  // Start chunked response with JSON array opening
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("application/json"), F(""));
  httpServer.sendContent(F("["));
  
  // Also stream to debug telnet
  DebugTln(F("--- Firmware File List (streamed) ---"));
  DebugTln(F("["));
  
  dir = LittleFS.openDir(dirpath);	
  while (dir.next()) {
    DebugTf(PSTR("dir.fileName()=%s\r\n"), dir.fileName().c_str());
    if (dir.fileName().endsWith(".hex")) {
      version="";
      fwversion="";
      String hexfile = dirpath + "/" + dir.fileName();   
      String verfile = hexfile;
      verfile.replace(".hex", ".ver");
      f = LittleFS.open(verfile, "r");
      if (f) {
        version = f.readStringUntil('\n');
        version.trim();
        f.close();
      } 
      
      char fwversionBuf[32] = {0};
      GetVersion(hexfile.c_str(), fwversionBuf, sizeof(fwversionBuf));
      fwversion = fwversionBuf;

      DebugTf(PSTR("GetVersion(%s) returned [%s]\r\n"), hexfile.c_str(), fwversion.c_str());  
      if (fwversion.length() && strcmp(fwversion.c_str(),version.c_str())) {
        version=fwversion;
        if (f = LittleFS.open(verfile, "w")) {
          DebugTf(PSTR("writing %s to %s\r\n"),version.c_str(),verfile.c_str());
          f.print(version + "\n");
          f.close();
        } 
      }
      Debugln();
      
      // Add comma separator after first entry
      if (!firstEntry) {
        httpServer.sendContent(F(","));
        DebugTln(F(",")); // Also to debug telnet
      }
      firstEntry = false;
      
      // Stream this entry directly (fits in 256-byte buffer)
      // CSTR() macro handles null safety globally - returns "" if null
      snprintf_P(entryBuffer, sizeof(entryBuffer), 
                 PSTR("{\"name\":\"%s\",\"version\":\"%s\",\"size\":%d}"), 
                 CSTR(dir.fileName()), CSTR(version), dir.fileSize());
      httpServer.sendContent(entryBuffer);
      
      // Also stream entry to debug telnet
      DebugTf(PSTR("  %s\r\n"), entryBuffer);
      
      feedWatchDog(); // Feed watchdog during potentially long operation
    }
  }
  
  // Close JSON array
  httpServer.sendContent(F("]\r\n"));
  httpServer.sendContent(F("")); // End chunked response
  
  // Also close JSON array in debug telnet
  DebugTln(F("]"));
  DebugTln(F("--- End of Firmware File List ---"));
}


//=====================================================================================
// Stack-based formatBytes — writes into caller-supplied buffer, no heap allocation.
static void formatBytesTo(size_t bytes, char *buf, size_t bufSize)
{
  if (bytes < 1024) {
    snprintf_P(buf, bufSize, PSTR("%u Byte"), (unsigned)bytes);
  } else if (bytes < (1024 * 1024)) {
    snprintf_P(buf, bufSize, PSTR("%.1f KB"), bytes / 1024.0);
  } else {
    snprintf_P(buf, bufSize, PSTR("%.1f MB"), bytes / 1024.0 / 1024.0);
  }
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

  Dir dir = LittleFS.openDir(path);
  while (dir.next()) {
    feedWatchDog();
    // Skip hidden files/directories (names starting with '.')
    if (dir.fileName().charAt(0) == '.') continue;
    if (fileCount >= MAX_FILES_IN_LIST) { truncated = true; break; }
    if (!first) httpServer.sendContent(F(","));
    first = false;

    snprintf_P(buf, sizeof(buf),
      PSTR("{\"name\":\"%s\",\"size\":%ld,\"type\":\"%s\"}"),
      dir.fileName().c_str(), (long)dir.fileSize(),
      dir.isDirectory() ? "dir" : "file");
    httpServer.sendContent(buf);
    fileCount++;
  }

  // Storage info as last entry (raw bytes — frontend formats for display)
  FSInfo fsInfo;
  LittleFS.info(fsInfo);
  if (!first) httpServer.sendContent(F(","));
  unsigned long usedBytes = (unsigned long)(fsInfo.usedBytes * 1.05);
  unsigned long freeBytes = fsInfo.totalBytes - usedBytes;
  snprintf_P(buf, sizeof(buf),
    PSTR("{\"usedBytes\":%lu,\"totalBytes\":%lu,\"freeBytes\":%lu,\"truncated\":%s}"),
    usedBytes, fsInfo.totalBytes, freeBytes,
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
  FSInfo LittleFSinfo;
  LittleFS.info(LittleFSinfo);
  Debugln(formatBytes(LittleFSinfo.totalBytes - (LittleFSinfo.usedBytes * 1.05)) + " im LittleFS frei");
  return (LittleFSinfo.totalBytes - (LittleFSinfo.usedBytes * 1.05) > printsize) ? true : false;
  
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
