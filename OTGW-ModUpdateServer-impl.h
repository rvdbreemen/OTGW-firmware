/*
***************************************************************************  
**  Program  : MonUpdateServer-impl.h
**  Modified to work with OTGW Nodoshop Hardware Watchdog
** 
**  This is the ESP8266HTTPUpdateServer.h file 
**  Created and modified by Ivan Grokhotkov, Miguel Angel Ajo, Earle Philhower and many others 
**  see: https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPUpdateServer
**
**  ... and then modified by Willem Aandewiel
**
** License and credits
** Arduino IDE is developed and maintained by the Arduino team. The IDE is licensed under GPL.
**
** ESP8266 core includes an xtensa gcc toolchain, which is also under GPL.
**
***************************************************************************
*/

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <flash_hal.h>
#include <FS.h>
#include "StreamString.h"
#include "Wire.h"
#include "OTGW-ModUpdateServer.h"

#ifndef Debug
  //#warning Debug() was not defined!
	#define Debug(...)		({ OTGWSerial.print(__VA_ARGS__); })  
	#define Debugln(...)	({ OTGWSerial.println(__VA_ARGS__); })  
	#define Debugf(...)		({ OTGWSerial.printf(__VA_ARGS__); })  
//#else
//  #warning Seems Debug() is already defined!
#endif

namespace esp8266httpupdateserver {
using namespace esp8266webserver;
namespace {
// Use static storage for update status
UpdateStatus gUpdateStatus = {0, 0, 0, 0, "Idle"};
WiFiClient gSseClient;
bool gSseActive = false;
UpdateStatus gSseLastSent = {0, 0, 0, 0, ""};
uint32_t gSseLastSendMs = 0;

void setUpdateStatus(uint8_t state, uint8_t percent, uint32_t transferred, uint32_t total, const char *message) {
  // Disable interrupts for atomic access on ESP8266
  noInterrupts();
  
  gUpdateStatus.state = state;
  gUpdateStatus.percent = percent;
  gUpdateStatus.transferred = transferred;
  gUpdateStatus.total = total;
  if (message && *message) {
    strlcpy(gUpdateStatus.message, message, sizeof(gUpdateStatus.message));
  }
  
  interrupts();
}

void sanitizeJsonString(const char *src, char *dst, size_t len) {
  if (!dst || len == 0) return;
  size_t w = 0;
  for (size_t r = 0; src && src[r] != '\0'; r++) {
    unsigned char c = static_cast<unsigned char>(src[r]);
    // Escape control characters and JSON special characters
    if (c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\b') {
      if (w + 1 >= len) break;  // Space check for single char
      dst[w++] = ' ';  // Replace control chars with space
    } else if (c == '"' || c == '\\') {
      if (w + 2 >= len) break;  // Space check for escape sequence
      dst[w++] = '\\';
      dst[w++] = c;
    } else if (c < 0x20 || c == 0x7F) {
      if (w + 1 >= len) break;  // Space check for single char
      dst[w++] = ' ';  // Replace other control chars with space
    } else {
      if (w + 1 >= len) break;  // Space check for single char
      dst[w++] = static_cast<char>(c);
    }
  }
  dst[w] = '\0';
}
} // namespace

void getUpdateStatus(UpdateStatus &out) {
  // Disable interrupts for atomic access on ESP8266
  noInterrupts();
  
  out.state = gUpdateStatus.state;
  out.percent = gUpdateStatus.percent;
  out.transferred = gUpdateStatus.transferred;
  out.total = gUpdateStatus.total;
  strlcpy(out.message, gUpdateStatus.message, sizeof(out.message));
  
  interrupts();
}

size_t updateStatusToJson(char *buf, size_t len) {
  UpdateStatus s{};
  getUpdateStatus(s);
  char msg[sizeof(s.message)] = {0};
  sanitizeJsonString(s.message, msg, sizeof(msg));
  int written = snprintf(buf, len,
           "{\"state\":%u,\"percent\":%u,\"transferred\":%lu,\"total\":%lu,\"message\":\"%s\"}",
           static_cast<unsigned>(s.state),
           static_cast<unsigned>(s.percent),
           static_cast<unsigned long>(s.transferred),
           static_cast<unsigned long>(s.total),
           msg);
  // Return actual size written only if successful (no truncation)
  // snprintf returns number of chars that would be written (excluding null)
  if (written > 0 && static_cast<size_t>(written) < len) {
    return static_cast<size_t>(written);
  }
  return 0;  // Indicate failure if truncation occurred or error
}

static bool sseStatusChanged(const UpdateStatus &a, const UpdateStatus &b) {
  if (a.state != b.state) return true;
  if (a.percent != b.percent) return true;
  if (a.transferred != b.transferred) return true;
  if (a.total != b.total) return true;
  return (strncmp(a.message, b.message, sizeof(a.message)) != 0);
}

void beginUpdateEventStream(ESP8266WebServer &server) {
  if (gSseClient && gSseClient.connected()) {
    gSseClient.stop();
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Connection", "keep-alive");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/event-stream", "");
  gSseClient = server.client();
  gSseClient.setNoDelay(true);
  gSseActive = true;
  gSseLastSendMs = millis();  // Initialize with current time to avoid underflow
}

void pumpUpdateEventStream() {
  if (!gSseActive) return;
  if (!gSseClient || !gSseClient.connected()) {
    gSseActive = false;
    return;
  }

  UpdateStatus now{};
  getUpdateStatus(now);
  const uint32_t tick = millis();
  const bool changed = sseStatusChanged(now, gSseLastSent);
  
  // Use rollover-safe time difference calculation
  uint32_t timeSinceLastSend = tick - gSseLastSendMs;

  if (changed) {
    // Send update immediately on status change
    char json[512];  // Increased buffer size
    size_t jsonLen = updateStatusToJson(json, sizeof(json));
    
    // Validate JSON was generated successfully
    if (jsonLen == 0) {
      gSseActive = false;
      return;
    }
    
    // Check for write errors
    if (gSseClient.print("event: update\n") <= 0) {
      gSseActive = false;
      return;
    }
    if (gSseClient.print("data: ") <= 0) {
      gSseActive = false;
      return;
    }
    if (gSseClient.print(json) <= 0) {
      gSseActive = false;
      return;
    }
    if (gSseClient.print("\n\n") <= 0) {
      gSseActive = false;
      return;
    }
    
    gSseLastSent = now;
    gSseLastSendMs = tick;
  } else if (timeSinceLastSend > 10000) {
    // Send keepalive ping every 10 seconds when no changes
    if (gSseClient.print(": ping\n\n") <= 0) {
      gSseActive = false;
      return;
    }
    gSseLastSendMs = tick;
  }
}
/**
static const char serverIndex2[] PROGMEM =
  R"(<html charset="UTF-8">
     <body>
     <h1>ESP8266 Flash utility</h1>
     <form method='POST' action='?cmd=0' enctype='multipart/form-data'>
        <input type='hidden' name='cmd' value='0'>
                  <input type='file' accept='ino.bin' name='update'>
                  <input type='submit' value='Flash Firmware'>
      </form>
      <form method='POST' action='?cmd=100' enctype='multipart/form-data'> 
        <input type='hidden' name='cmd' value='100'>
                  <input type='file' accept='LittleFS.bin' name='update'>
                  <input type='submit' value='Flash LittleFS'>
      </form>
     </html>)";

static const char successResponse[] PROGMEM = 
  "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update <b>Success</b>!<br>Wait for DSMR-logger to reboot...";
**/

template <typename ServerType>
ESP8266HTTPUpdateServerTemplate<ServerType>::ESP8266HTTPUpdateServerTemplate(bool serial_debug)
{
  _serial_output = serial_debug;
  _server = NULL;
  _username = emptyString;
  _password = emptyString;
  _authenticated = false;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      setUpdateStatus(0, 0, 0, 0, "Idle");
      _server->send_P(200, PSTR("text/html"), _serverIndex);
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError()) {
        setUpdateStatus(3, gUpdateStatus.percent, gUpdateStatus.transferred, gUpdateStatus.total, _updaterError.c_str());
        _server->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
        // Status already set to success in UPLOAD_FILE_END handler
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        delay(1000);
        ESP.restart();
        delay(3000);
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = _server->upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();
        // if (_serial_output)
        //   OTGWSerial.setDebugOutput(true);

        _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
        if(!_authenticated){
          if (_serial_output)
            Debugln("Unauthenticated Update\n");
          return;
        }

        WiFiUDP::stopAll();
        if (_serial_output)
          Debugf("Update: %s\r\n", upload.filename.c_str());
        if (upload.name == "filesystem") {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          if (!Update.begin(fsSize, U_FS)){//start with max available size
            if (_serial_output) Update.printError(OTGWSerial);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
          }
        }
        setUpdateStatus(1, 0, 0, Update.size(), "Upload started");
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {Debug("."); blinkLEDnow(LED1);}
        // Feed the dog before it bites!
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();
        // End of feeding hack
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          _setUpdaterError();
          setUpdateStatus(3, gUpdateStatus.percent, gUpdateStatus.transferred, gUpdateStatus.total, _updaterError.c_str());
        } else {
          uint32_t total = Update.size();
          uint32_t progress = Update.progress();
          uint8_t pct = (total > 0) ? static_cast<uint8_t>((progress * 100U) / total) : 0;
          setUpdateStatus(1, pct, progress, total, "Uploading");
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
          if (_serial_output) Debugf("\r\nUpdate Success: %u\r\nRebooting...\r\n", upload.totalSize);
          setUpdateStatus(2, 100, Update.progress(), Update.size(), "Update finished, rebooting");
        } else {
          _setUpdaterError();
          setUpdateStatus(3, gUpdateStatus.percent, gUpdateStatus.transferred, gUpdateStatus.total, _updaterError.c_str());
        }
        // if (_serial_output) 
        //   OTGWSerial.setDebugOutput(false);
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        setUpdateStatus(3, gUpdateStatus.percent, gUpdateStatus.transferred, gUpdateStatus.total, "Update aborted");
        if (_serial_output) Debugln("Update was aborted");
      }
      delay(0);
    });
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setIndexPage(const char *indexPage)
{
	_serverIndex = indexPage;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setSuccessPage(const char *successPage)
{
	_serverSuccess = successPage;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_setUpdaterError()
{
  if (_serial_output) Update.printError(OTGWSerial);
  StreamString str;
  Update.printError(str);
  _updaterError = str.c_str();
}

};
