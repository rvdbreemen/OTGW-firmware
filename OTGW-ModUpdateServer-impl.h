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
#include <LittleFS.h>
#include "StreamString.h"
#include "Wire.h"
#include "OTGW-ModUpdateServer.h"

// External declarations
extern bool isESPFlashing;          // ESP flashing state flag
extern bool LittleFSmounted;        // LittleFS mount status flag
extern void sendWebSocketJSON(const char *json);
extern FSInfo LittleFSinfo;         // LittleFS filesystem information
extern bool updateLittleFSStatus(const char *probePath);
extern bool updateLittleFSStatus(const __FlashStringHelper *probePath);
extern void writeSettings(bool show); // Write settings from ESP memory to filesystem

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

template <typename ServerType>
ESP8266HTTPUpdateServerTemplate<ServerType>::ESP8266HTTPUpdateServerTemplate(bool serial_debug)
{
  _serial_output = serial_debug;
  _server = NULL;
  _username = emptyString;
  _password = emptyString;
  _authenticated = false;
  _lastDogFeedTime = 0;
  _lastFeedbackBytes = 0;
  _lastFeedbackTime = 0;
  _lastProgressPerc = 0;
  _resetStatus();
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;
    _resetStatus();

    Update.onProgress([this](size_t progress, size_t total) {
      if (_status.phase == UPDATE_ERROR || _status.phase == UPDATE_ABORT || _status.phase == UPDATE_END) {
        return;
      }
      _status.flash_written = progress;
      if (total > 0) {
        _status.flash_total = total;
      }
      _setStatus(UPDATE_WRITE, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
    });

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), _serverIndex);
    });

    _server->on("/status", HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _sendStatusJson();
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError()) {
        _setStatus(UPDATE_ERROR, _status.target, _status.received, _status.total, _status.filename, _updaterError);
        _server->send(200, F("text/html"), String(F("Flash error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        if (_status.target != "filesystem") {
          delay(1000);
          ESP.restart();
          delay(3000);
        } else {
          // Ensure HTTP response is fully sent before unmounting filesystem
          delay(100);
          LittleFS.end();
        }
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = _server->upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();
        if (_serial_output) Debugf(PSTR("Upload Start: %s (size: %s)\r\n"), upload.filename.c_str(), _server->arg("size").c_str());

        _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
        if(!_authenticated){
          if (_serial_output)
            Debugln(F("Unauthenticated Update\n"));
          _status.upload_received = 0;
          _status.upload_total = 0;
          _status.flash_written = 0;
          _status.flash_total = 0;
          _setStatus(UPDATE_ERROR, "unknown", 0, 0, upload.filename, "unauthenticated");
          return;
        }

        // Set global flag to disable background tasks during ESP flash
        ::isESPFlashing = true;
        
        WiFiUDP::stopAll();
        if (_serial_output)
          Debugf(PSTR("Update: %s\r\n"), upload.filename.c_str());

        size_t uploadTotal = 0;
        String sizeArg = _server->arg("size");
        if (sizeArg.length()) {
          long parsedSize = sizeArg.toInt();
          if (parsedSize > 0) {
            uploadTotal = static_cast<size_t>(parsedSize);
          }
        }
        _status.upload_total = uploadTotal;
        _status.upload_received = 0;
        _status.flash_total = uploadTotal;
        _status.flash_written = 0;

        // Initialize throttle variables
        _lastDogFeedTime = millis();
        _lastFeedbackTime = millis();
        _lastFeedbackBytes = 0;
        _lastProgressPerc = 0;

        if (upload.name == F("filesystem")) {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          LittleFS.end();
          if (uploadTotal > 0 && uploadTotal > fsSize) {
            _updaterError = F("filesystem image too large");
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : fsSize, U_FS)){//start with max available size
            if (_serial_output) Update.printError(OTGWSerial);
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
          } else {
            _setStatus(UPDATE_START, "filesystem", 0, uploadTotal, upload.filename, emptyString);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
            _updaterError = F("firmware image too large");
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
          } else {
            _setStatus(UPDATE_START, "firmware", 0, uploadTotal, upload.filename, emptyString);
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {Debug("."); blinkLEDnow(LED1);}

        // Feed the dog on every chunk (Main branch behavior)
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();
        
        size_t written = Update.write(upload.buf, upload.currentSize);
        _status.upload_received = upload.totalSize;
        
        // Manual Status Update for WebSockets/Progress
        // Throttle updates to percentage changes to avoid flooding WebSockets
        if (_status.flash_total > 0) {
           _status.flash_written = _status.upload_received; // approximation based on upload
           int currentPerc = (_status.flash_written * 100) / _status.flash_total;
           if (currentPerc != _lastProgressPerc) {
             _lastProgressPerc = currentPerc;
             _setStatus(UPDATE_WRITE, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
           }
        }

        if (written != upload.currentSize) {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, _updaterError);
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
          if (_serial_output) Debugf(PSTR("\r\nUpdate Success: %u\r\nRebooting...\r\n"), upload.totalSize);
          _status.upload_received = upload.totalSize;
          if (_status.upload_total == 0 && upload.totalSize > 0) {
            _status.upload_total = upload.totalSize;
          }
          if (_status.flash_total == 0 && upload.totalSize > 0) {
            _status.flash_total = upload.totalSize;
          }
          if (_status.flash_written < upload.totalSize) {
            _status.flash_written = upload.totalSize;
          }
          _setStatus(UPDATE_END, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, emptyString);
          
          if (_status.target == "filesystem") {
            LittleFSmounted = LittleFS.begin();
            if (LittleFSmounted) {
              updateLittleFSStatus(F("/.ota_post"));
              // Restore settings from ESP memory to new filesystem
              // Settings are still in RAM, just write them back to the new filesystem.
              // Note: writeSettings() does not expose a success status here; any failures
              // should be logged inside writeSettings() itself.
              Debugln(F("Filesystem update complete; attempting to restore settings from memory..."));
              writeSettings(true);
              Debugln(F("Filesystem update complete; settings restore has been requested"));
            } else {
              // Ensure state is explicitly false and log failure for diagnostics
              LittleFSmounted = false;
              Debugln(F("LittleFS mount failed after filesystem OTA update"));
            }
          }

          // Clear global flag - flash completed successfully
          ::isESPFlashing = false;
        } else {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, _updaterError);
          
          // Clear global flag - flash failed
          ::isESPFlashing = false;
        }
        // if (_serial_output) 
        //   OTGWSerial.setDebugOutput(false);
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        if (_serial_output) Debugln(F("Update was aborted"));
        _status.upload_received = upload.totalSize;
        if (_status.upload_total == 0 && upload.totalSize > 0) {
          _status.upload_total = upload.totalSize;
        }
        if (_status.flash_total == 0 && upload.totalSize > 0) {
          _status.flash_total = upload.totalSize;
        }
        _setStatus(UPDATE_ABORT, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, emptyString);
        
        // Clear global flag - flash aborted
        ::isESPFlashing = false;
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

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_resetStatus()
{
  _status.phase = UPDATE_IDLE;
  _status.target = "unknown";
  _status.received = 0;
  _status.total = 0;
  _status.upload_received = 0;
  _status.upload_total = 0;
  _status.flash_written = 0;
  _status.flash_total = 0;
  _status.filename = emptyString;
  _status.error = emptyString;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_setStatus(uint8_t phase, const String &target, size_t received, size_t total, const String &filename, const String &error)
{
  if (_serial_output) {
      Debugf(PSTR("Update Status: %s (recv: %u, total: %u)\r\n"), _phaseToString(phase), received, total);
  }
  _status.phase = static_cast<UpdatePhase>(phase);
  _status.target = target.length() ? target : "unknown";
  _status.received = received;
  _status.total = total;
  if (_status.flash_total > 0 && _status.flash_written > _status.flash_total) {
    if (_serial_output) {
      Debugf(PSTR("Update warning: flash_written (%u) > flash_total (%u)\r\n"),
             static_cast<unsigned>(_status.flash_written),
             static_cast<unsigned>(_status.flash_total));
    }
    _status.flash_written = _status.flash_total;
  }
  _status.filename = filename;
  _status.error = error;

  // Broadcast via WebSocket
  // Use a static buffer to avoid stack overflow, but protect with interrupt disable? 
  // No, we are in non-interrupt context usually. But strictly speaking static is not thread safe.
  // Stack is better if size is reasonable. 512 bytes is okay for ESP8266 stack (4KB).
  char buf[512];
  char filenameEsc[64];
  char errorEsc[96];
  _jsonEscape(_status.filename, filenameEsc, sizeof(filenameEsc));
  _jsonEscape(_status.error, errorEsc, sizeof(errorEsc));
  int written = snprintf_P(
    buf,
    sizeof(buf),
    PSTR("{\"state\":\"%s\",\"target\":\"%s\",\"received\":%u,\"total\":%u,\"upload_received\":%u,\"upload_total\":%u,\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}"),
    _phaseToString(_status.phase),
    _status.target.length() ? _status.target.c_str() : "unknown",
    static_cast<unsigned>(_status.received),
    static_cast<unsigned>(_status.total),
    static_cast<unsigned>(_status.upload_received),
    static_cast<unsigned>(_status.upload_total),
    static_cast<unsigned>(_status.flash_written),
    static_cast<unsigned>(_status.flash_total),
    filenameEsc,
    errorEsc
  );
  
  if (written > 0 && written < (int)sizeof(buf)) {
      sendWebSocketJSON(buf);
  }
}

template <typename ServerType>
const char *ESP8266HTTPUpdateServerTemplate<ServerType>::_phaseToString(uint8_t phase)
{
  switch (static_cast<UpdatePhase>(phase)) {
    case UPDATE_START: return "start";
    case UPDATE_WRITE: return "write";
    case UPDATE_END: return "end";
    case UPDATE_ERROR: return "error";
    case UPDATE_ABORT: return "abort";
    case UPDATE_IDLE:
    default: return "idle";
  }
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_jsonEscape(const String &in, char *out, size_t outSize)
{
  size_t j = 0;
  for (size_t i = 0; i < in.length() && j + 1 < outSize; ++i) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= outSize) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      if (j + 1 >= outSize) break;
      out[j++] = ' ';
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_sendStatusJson()
{
  constexpr size_t JSON_STATUS_BUFFER_SIZE = 512;
  char buf[JSON_STATUS_BUFFER_SIZE];
  char filenameEsc[64];
  char errorEsc[96];
  _jsonEscape(_status.filename, filenameEsc, sizeof(filenameEsc));
  _jsonEscape(_status.error, errorEsc, sizeof(errorEsc));
  int written = snprintf_P(
    buf,
    sizeof(buf),
    PSTR("{\"state\":\"%s\",\"target\":\"%s\",\"received\":%u,\"total\":%u,\"upload_received\":%u,\"upload_total\":%u,\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}"),
    _phaseToString(_status.phase),
    _status.target.length() ? _status.target.c_str() : "unknown",
    static_cast<unsigned>(_status.received),
    static_cast<unsigned>(_status.total),
    static_cast<unsigned>(_status.upload_received),
    static_cast<unsigned>(_status.upload_total),
    static_cast<unsigned>(_status.flash_written),
    static_cast<unsigned>(_status.flash_total),
    filenameEsc,
    errorEsc
  );
  
  // Check if the output was truncated
  if (written >= (int)sizeof(buf)) {
    if (_serial_output) {
      Debugf(PSTR("Warning: status JSON truncated (%d chars needed, %d available)\r\n"), 
             written, (int)sizeof(buf));
    }
    // Send error response instead of truncated JSON
    _server->send(500, F("text/plain"), F("Status buffer overflow"));
    return;
  }
  
  _server->send(200, F("application/json"), buf);
}

};
