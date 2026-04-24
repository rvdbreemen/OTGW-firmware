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
extern OTGWState state;             // Global state object (provides flash.bESPactive flag)
extern bool LittleFSmounted;        // LittleFS mount status flag
extern bool updateLittleFSStatus(const char *probePath);
extern bool updateLittleFSStatus(const __FlashStringHelper *probePath);
extern void writeSettings(bool show); // Write settings from ESP memory to filesystem
extern void settingsMarkClean();      // Clear dirty flag without writing or restarting services

#ifndef Debug
  //#warning Debug() was not defined!
	#define Debug(...)		({ OTGWSerial.print(__VA_ARGS__); })  
	#define Debugln(...)	({ OTGWSerial.println(__VA_ARGS__); })  
  #define Debugf(...)		({ OTGWSerial.printf_P(__VA_ARGS__); })  
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
  _resetUploadTracking();
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;
  _resetUploadTracking();

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), _serverIndex);
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      // Check both Update's internal error flag AND our pre-begin error string.
      // Update.hasError() returns false when Update.begin() was never called
      // (e.g. "image too large"), so _updaterError must be tested independently.
      if (Update.hasError() || _updaterError.length()) {
        // Restore normal operation regardless of how the error arose.
        // Not clearing bESPactive here would permanently disable loopWifi().
        ::state.flash.bESPactive = false;
        // If we unmounted LittleFS in UPLOAD_FILE_START but never remounted it
        // (error/abort before or during flash), attempt recovery now so the web UI
        // remains functional without a reboot.
        if (!LittleFSmounted && _uploadTarget == "filesystem") {
          LittleFSmounted = LittleFS.begin();
        }
        _server->send(200, F("text/html"), String(F("Flash error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        // Reboot for BOTH firmware and filesystem.
        // TASK-396 Phase 3: switched from immediate doRestart() to the deferred
        // mechanism. The rationale: doRestart() calls prepareForReboot() which
        // stops telnet and closes MQTT/WS/OTGWstream — all while we're still
        // inside the HTTP handler callback. On fast networks the HTTP 200
        // response body is typically flushed by the client().stop() above, but
        // on slow/congested links we used to see browser hangs where the
        // success HTML never fully arrived. Deferring the reboot to the next
        // loop() tick gives lwIP 10-100ms to finish draining the response
        // socket. The existing service-cleanup sequence (critical on Arduino
        // Core 3.1.0+ per PR esp8266/Arduino#8598) still fires, just from
        // performDeferredReboot() in loop() rather than here.
        logBootSignature("[OTA] pre-reboot");    // TASK-396 Phase 4: fourth/final OTA probe
        requestDeferredReboot("[OTA] Rebooting...");
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = _server->upload();

      if(upload.status == UPLOAD_FILE_START){
        _handleUploadStart(upload);
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        _handleUploadWrite(upload);
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        _handleUploadEnd(upload);
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        _handleUploadAbort(upload);
      }
      delay(0);
    });
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_resetUploadTracking()
{
  _uploadTarget = emptyString;
  _uploadExpectedBytes = 0;
  _uploadWrittenBytes = 0;
  _uploadBlockIndex = 0;
}

template <typename ServerType>
size_t ESP8266HTTPUpdateServerTemplate<ServerType>::_parseUploadTotalSize() const
{
  size_t uploadTotal = 0;
  String sizeArg = _server->arg("size");
  if (sizeArg.length()) {
    long parsedSize = sizeArg.toInt();
    if (parsedSize > 0) {
      uploadTotal = static_cast<size_t>(parsedSize);
    }
  }
  return uploadTotal;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_beginFilesystemUpload(HTTPUpload& upload, size_t uploadTotal)
{
  _uploadTarget = "filesystem";
  logBootSignature("[OTA] pre-begin");    // TASK-396 Phase 4: first of four OTA lifecycle probes
  size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
  if (_serial_output) {
    DebugTf(PSTR("[OTA] Target: filesystem (%u bytes)\r\n"), static_cast<unsigned>(fsSize));
    DebugTf(PSTR("[OTA] XHR upload start: target=filesystem file=%s expected=%u bytes\r\n"),
           upload.filename.c_str(),
           static_cast<unsigned>(_uploadExpectedBytes));
  }
  close_all_fs();
  LittleFS.end();
  if (uploadTotal > 0 && uploadTotal > fsSize) {
    _updaterError = F("filesystem image too large");
  } else if (!Update.begin(fsSize, U_FS)) { // always use full partition size to erase stale upper-block data
    if (_serial_output) Update.printError(OTGWSerial);
    _setUpdaterError();
  }
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_beginFirmwareUpload(HTTPUpload& upload, size_t uploadTotal)
{
  _uploadTarget = "firmware";
  logBootSignature("[OTA] pre-begin");    // TASK-396 Phase 4: first of four OTA lifecycle probes
  uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (_serial_output) {
    DebugTf(PSTR("[OTA] Target: firmware (%u bytes)\r\n"), static_cast<unsigned>(maxSketchSpace));
    DebugTf(PSTR("[OTA] XHR upload start: target=firmware file=%s expected=%u bytes\r\n"),
           upload.filename.c_str(),
           static_cast<unsigned>(_uploadExpectedBytes));
  }
  if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
    _updaterError = F("firmware image too large");
  } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)) { // start with max available size
    _setUpdaterError();
  }
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_handleUploadStart(HTTPUpload& upload)
{
  _updaterError.clear();
  _resetUploadTracking();

  _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
  if(!_authenticated){
    if (_serial_output)
      DebugTln(F("Unauthenticated Update"));
    return;
  }

  if (_serial_output) {
    DebugTf(PSTR("[OTA] Flash start: %s (size: %s)\r\n"), upload.filename.c_str(), _server->arg("size").c_str());
  }

  ::state.flash.bESPactive = true;
  WiFiUDP::stopAll();

  size_t uploadTotal = _parseUploadTotalSize();
  _uploadExpectedBytes = uploadTotal;

  if (upload.name == F("filesystem")) {
    _beginFilesystemUpload(upload, uploadTotal);
  } else {
    _beginFirmwareUpload(upload, uploadTotal);
  }
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_handleUploadWrite(HTTPUpload& upload)
{
  if (_serial_output) {
    blinkLEDnow(LED1);
  }

  Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();

  size_t written = Update.write(upload.buf, upload.currentSize);

  if (written != upload.currentSize) {
    _setUpdaterError();
    return;
  }

  _uploadBlockIndex++;
  _uploadWrittenBytes += written;

  if (_serial_output) {
    if (_uploadExpectedBytes > 0) {
      unsigned long pct = static_cast<unsigned long>((_uploadWrittenBytes * 100UL) / _uploadExpectedBytes);
      if (pct > 100UL) pct = 100UL;
      DebugTf(PSTR("[OTA] XHR upload progress: target=%s block=%lu chunk=%u total=%u/%u (%lu%%)\r\n"),
             _uploadTarget.c_str(),
             static_cast<unsigned long>(_uploadBlockIndex),
             static_cast<unsigned>(written),
             static_cast<unsigned>(_uploadWrittenBytes),
             static_cast<unsigned>(_uploadExpectedBytes),
             pct);
    } else {
      DebugTf(PSTR("[OTA] XHR upload progress: target=%s block=%lu chunk=%u total=%u bytes\r\n"),
             _uploadTarget.c_str(),
             static_cast<unsigned long>(_uploadBlockIndex),
             static_cast<unsigned>(written),
             static_cast<unsigned>(_uploadWrittenBytes));
    }
    DebugFlush();
  }
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_handleUploadEnd(HTTPUpload& upload)
{
  if (_serial_output) {
    if (_uploadExpectedBytes > 0) {
      DebugTf(PSTR("[OTA] XHR upload complete: target=%s file=%s received=%u/%u bytes in %lu block(s)\r\n"),
             _uploadTarget.c_str(),
             upload.filename.c_str(),
             static_cast<unsigned>(_uploadWrittenBytes),
             static_cast<unsigned>(_uploadExpectedBytes),
             static_cast<unsigned long>(_uploadBlockIndex));
    } else {
      DebugTf(PSTR("[OTA] XHR upload complete: target=%s file=%s received=%u bytes in %lu block(s)\r\n"),
             _uploadTarget.c_str(),
             upload.filename.c_str(),
             static_cast<unsigned>(_uploadWrittenBytes),
             static_cast<unsigned long>(_uploadBlockIndex));
    }
    DebugTln(F("[OTA] End: finalizing flash..."));
  }

  bool updateOk = Update.end(true);
  if(updateOk){
    if (_serial_output) DebugTf(PSTR("[OTA] End: success (%u bytes)\r\n"), upload.totalSize);
    logBootSignature("[OTA] post-end");   // TASK-396 Phase 4: second probe, after Update.end(true) commits the image

    if (_uploadTarget == "filesystem") {
      LittleFSmounted = LittleFS.begin();
      if (LittleFSmounted) {
        updateLittleFSStatus(F("/.ota_post"));
        if (_serial_output) DebugTln(F("[OTA] Restoring settings to filesystem"));
        writeSettings(false);
        settingsMarkClean();
        logBootSignature("[OTA] post-remount");   // TASK-396 Phase 4: third probe, FS-OTA only
      } else {
        LittleFSmounted = false;
        if (_serial_output) DebugTln(F("[OTA] Error: LittleFS mount failed"));
      }
    }

    if (_serial_output) {
      DebugTln(F("[OTA] Preparing to reboot..."));
      DebugFlush();
    }

    ::state.flash.bESPactive = false;
  } else {
    _setUpdaterError();
    ::state.flash.bESPactive = false;
  }

  _resetUploadTracking();
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_handleUploadAbort(HTTPUpload& upload)
{
  Update.end();
  if (_serial_output) {
    DebugTf(PSTR("[OTA] XHR upload aborted: target=%s file=%s after %u bytes in %lu block(s)\r\n"),
           _uploadTarget.c_str(),
           upload.filename.c_str(),
           static_cast<unsigned>(_uploadWrittenBytes),
           static_cast<unsigned long>(_uploadBlockIndex));
    DebugTln(F("[OTA] Abort: update cancelled"));
  }
  ::state.flash.bESPactive = false;
  _resetUploadTracking();
  // LittleFS was unmounted at UPLOAD_FILE_START; HTTP_POST error handler
  // will remount it if needed when it runs next.
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
