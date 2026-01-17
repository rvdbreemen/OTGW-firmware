/* 
***************************************************************************  
**  Program : FSexplorer
**  Version  : v1.0.0-rc4
**
**  Mostly stolen from https://www.arduinoforum.de/User-Fips
**  For more information visit: https://fipsok.de
**  See also https://www.arduinoforum.de/arduino-Thread-SPIFFS-DOWNLOAD-UPLOAD-DELETE-Esp8266-NodeMCU
**
***************************************************************************      
  Copyright (c) 2018 Jens Fleischer. All rights reserved.

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

const char Helper[] =
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
    httpServer.serveStatic("/",           LittleFS, "/FSexplorer.html");
    httpServer.serveStatic("/index",      LittleFS, "/FSexplorer.html");
    httpServer.serveStatic("/index.html", LittleFS, "/FSexplorer.html");
  } else{
    httpServer.serveStatic("/",           LittleFS, "/index.html");
    httpServer.serveStatic("/index",      LittleFS, "/index.html");
    httpServer.serveStatic("/index.html", LittleFS, "/index.html");
  } 
  httpServer.serveStatic("/FSexplorer.png",   LittleFS, "/FSexplorer.png");
  httpServer.serveStatic("/index.css", LittleFS, "/index.css");
  httpServer.serveStatic("/index.js",  LittleFS, "/index.js");
  //otgw pic functions
  httpServer.on("/pic", upgradepic);
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_ANY, processAPI);  //was only HTTP_GET (20210110)

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
    httpServer.serveStatic("/FSexplorer.html", LittleFS, "/FSexplorer.html");
    httpServer.serveStatic("/FSexplorer",      LittleFS, "/FSexplorer.html");
  }
  else 
  {
    httpServer.send(200, "text/html", Helper); //Upload the FSexplorer.html
  }
  httpServer.on("/api/firmwarefilelist", apifirmwarefilelist); 
  httpServer.on("/api/listfiles", apilistfiles);
  // httpServer.on("/LittleFSformat", formatLittleFS);
  httpServer.on("/upload", HTTP_POST, []() {}, handleFileUpload);
  httpServer.on("/ReBoot", reBootESP);
  httpServer.on("/ResetWireless", resetWirelessButton);
 
  httpServer.onNotFound([]() 
  {
    if (bDebugRestAPI) DebugTf(PSTR("in 'onNotFound()'!! [%s] => \r\n"), String(httpServer.uri()).c_str());
    if (httpServer.uri().indexOf("/api/") == 0) 
    {
      if (bDebugRestAPI) DebugTf(PSTR("next: processAPI(%s)\r\n"), String(httpServer.uri()).c_str());
      processAPI();
    }
    // else if (httpServer.uri() == "/")
    // {
    //   DebugTln(F("index requested.."));
    //   sendIndexPage();
    // }
    else
    {
      if (bDebugRestAPI) DebugTf(PSTR("next: handleFile(%s)\r\n")
                      , String(httpServer.urlDecode(httpServer.uri())).c_str());
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
  
  // Use small 256-byte buffer for streaming individual entries
  char entryBuffer[256];
  String version, fwversion;
  Dir dir;
  File f;
  bool firstEntry = true;

  String dirpath = "/" + String(sPICdeviceid);
  DebugTf(PSTR("dirpath=%s\r\n"), dirpath.c_str());
  
  // Start chunked response with JSON array opening
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("application/json"), F(""));
  httpServer.sendContent(F("["));
  
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
      }
      firstEntry = false;
      
      // Stream this entry directly (fits in 256-byte buffer)
      snprintf_P(entryBuffer, sizeof(entryBuffer), 
                 PSTR("{\"name\":\"%s\",\"version\":\"%s\",\"size\":%d}"), 
                 CSTR(dir.fileName()), CSTR(version), dir.fileSize());
      httpServer.sendContent(entryBuffer);
      
      feedWatchDog(); // Feed watchdog during potentially long operation
    }
  }
  
  // Close JSON array
  httpServer.sendContent(F("]\r\n"));
  httpServer.sendContent(F("")); // End chunked response
  
  DebugTln(F("Firmware file list sent via streaming"));
}


//=====================================================================================


void apilistfiles()             // Senden aller Daten an den Client
{   
  FSInfo LittleFSinfo;
  String path = "/";
  if (httpServer.hasArg("path")) {
      path = httpServer.arg("path");
  }

  #define LEN_FILENAME 32
  typedef struct _fileMeta {
    char    Name[LEN_FILENAME];     
    int32_t Size;
    bool    isDir;
  } fileMeta;

  _fileMeta dirMap[MAX_FILES_IN_LIST+1];
  int fileNr = 0;
    
  Dir dir = LittleFS.openDir(path);         // List files on LittleFS
  while (dir.next() && (fileNr < MAX_FILES_IN_LIST))  
  {
    dirMap[fileNr].Name[0] = '\0';
    strlcat(dirMap[fileNr].Name, dir.fileName().c_str(), LEN_FILENAME); 
    dirMap[fileNr].Size = dir.fileSize();
    dirMap[fileNr].isDir = dir.isDirectory();
    fileNr++;
  }
  //DebugTf(PSTR("fileNr[%d], Max[%d]\r\n"), fileNr, MAX_FILES_IN_LIST);

  // -- bubble sort dirMap op .Name--
  for (int8_t y = 0; y < fileNr; y++) {
    yield();
    for (int8_t x = y + 1; x < fileNr; x++)  {
      //DebugTf(PSTR("y[%d], x[%d] => seq[y][%s] / seq[x][%s] "), y, x, dirMap[y].Name, dirMap[x].Name);
      if (strcasecmp(dirMap[x].Name, dirMap[y].Name) <= 0)
      {
        //Debug(F(" !switch!"));
        fileMeta temp = dirMap[y];
        dirMap[y] = dirMap[x];
        dirMap[x] = temp;
      } /* end if */
      //Debugln();
    } /* end for */
  } /* end for */
  
  if (fileNr >= MAX_FILES_IN_LIST)
  {
    fileNr = MAX_FILES_IN_LIST;
    dirMap[fileNr].Name[0] = '\0';
    //--- if you change this message you also have to 
    //--- change FSexplorer.html
    strncat(dirMap[fileNr].Name, "More files not listed ..", 29); 
    dirMap[fileNr].Size = 0;
    dirMap[fileNr].isDir = false;
    fileNr++;
  }

  String temp = "[";
  for (int f=0; f < fileNr; f++)  
  {
    DebugTf(PSTR("[%3d] >> [%s]\r\n"), f, dirMap[f].Name);
    temp += R"({"name":")" + String(dirMap[f].Name) + R"(","size":")" + formatBytes(dirMap[f].Size) + R"(","type":")" + (dirMap[f].isDir ? "dir" : "file") + R"("},)";
  }

  LittleFS.info(LittleFSinfo);
  temp += R"({"usedBytes":")" + formatBytes(LittleFSinfo.usedBytes * 1.05) + R"(",)" +       // Berechnet den verwendeten Speicherplatz + 5% Sicherheitsaufschlag
          R"("totalBytes":")" + formatBytes(LittleFSinfo.totalBytes) + R"(","freeBytes":")" + // Zeigt die Größe des Speichers
          (LittleFSinfo.totalBytes - (LittleFSinfo.usedBytes * 1.05)) + R"("}])";               // Berechnet den freien Speicherplatz + 5% Sicherheitsaufschlag

  httpServer.send(200, "application/json", temp);
  
} // apilistfiles()


//=====================================================================================
bool handleFile(String&& path) 
{
  if (httpServer.hasArg("delete")) 
  {
    DebugTf(PSTR("Delete -> [%s]\n\r"),  httpServer.arg("delete").c_str());
    LittleFS.remove(httpServer.arg("delete"));    // Datei löschen
    httpServer.sendContent(Header);
    return true;
  }
  if (!LittleFS.exists("/FSexplorer.html")) httpServer.send(200, "text/html", Helper); //Upload the FSexplorer.html
  if (path.endsWith("/")) path += F("index.html");
  return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, contentType(path)); f.close(); true;}) : false;

} // handleFile()


//=====================================================================================
void handleFileUpload() 
{
  static File fsUploadFile;
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
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
    DebugT(F("FileUpload Size: ")); Debugln((String)upload.totalSize);
    httpServer.sendContent(Header);
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
  if (filename.endsWith(".htm") || filename.endsWith(".html")) filename = "text/html";
  else if (filename.endsWith(".css")) filename = "text/css";
  else if (filename.endsWith(".js")) filename = "application/javascript";
  else if (filename.endsWith(".json")) filename = "application/json";
  else if (filename.endsWith(".png")) filename = "image/png";
  else if (filename.endsWith(".gif")) filename = "image/gif";
  else if (filename.endsWith(".jpg")) filename = "image/jpeg";
  else if (filename.endsWith(".ico")) filename = "image/x-icon";
  else if (filename.endsWith(".xml")) filename = "text/xml";
  else if (filename.endsWith(".pdf")) filename = "application/x-pdf";
  else if (filename.endsWith(".zip")) filename = "application/x-zip";
  else if (filename.endsWith(".gz")) filename = "application/x-gzip";
  else filename = "text/plain";
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
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("Reboot OTGW firmware ..", 120, "/", true);   
} // reBootESP()

void resetWirelessButton()
{
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

  String redirectHTML = 
  "<!DOCTYPE HTML><html lang='en-US'>"
  "<head>"
  "<meta charset='UTF-8'>"
  "<style type='text/css'>"
  "body {background-color: lightblue;}"
  "</style>"
  "<title>Redirect to ...</title>"
  "</head>"
  "<body><h1>FSexplorer</h1>"
  "<h3>"+safeMsg+"</h3>"
  "<br><div style='width: 500px; position: relative; font-size: 25px;'>"
  "  <div style='float: left;'>Redirect in &nbsp;</div>"
  "  <div style='float: left;' id='counter'>"+String(safeWait)+"</div>"
  "  <div style='float: left;'>&nbsp; seconds ...</div>"
  "  <div style='float: right;'>&nbsp;</div>"
  "</div>"
  "<!-- Note: don't tell people to `click` the link, just tell them that it is a link. -->"
  "<br><br><hr>Wait for the redirect. In case you are not redirected automatically, then click this <a href='"+safeURL+"'>link to continue</a>."
  "  <script>"
  "      setInterval(function() {"
  "          var div = document.querySelector('#counter');"
  "          var count = div.textContent * 1 - 1;"
  "          div.textContent = count;"
  "          if (count <= 0) {"
  "              window.location.replace('"+safeURL+"'); "
  "          } "
  "      }, 1000); "
  "  </script> "
  "</body></html>\r\n";
  
  DebugTln(msg);
  // add non-JS fallback for redirect
  httpServer.sendHeader("Refresh", String(safeWait) + ";url=" + safeURL);
  httpServer.send(200, "text/html", redirectHTML);
  if (reboot) doRestart("Reboot after upgrade");
  
} // doRedirect()
