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
  _eventClientActive = false;
  _lastEventMs = 0;
  _lastEventPhase = UPDATE_IDLE;
  _resetStatus();
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;
    _resetStatus();

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), _serverIndex);
    });

    _server->on("/update/status", HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _sendStatusJson();
    });

    _server->on("/update/events", HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->sendHeader(F("Cache-Control"), F("no-cache"));
      _server->sendHeader(F("Connection"), F("keep-alive"));
      _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
      _server->send(200, F("text/event-stream"), "");
      _eventClient = _server->client();
      _eventClient.setNoDelay(true);
      _eventClientActive = true;
      _sendStatusEvent();
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError()) {
        _setStatus(UPDATE_ERROR, _status.target, _status.received, _status.total, _status.filename, _updaterError);
        _sendStatusEvent();
        _server->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
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
          _status.upload_received = 0;
          _status.upload_total = 0;
          _status.flash_written = 0;
          _status.flash_total = 0;
          _setStatus(UPDATE_ERROR, "unknown", 0, 0, upload.filename, "unauthenticated");
          _sendStatusEvent();
          return;
        }

        WiFiUDP::stopAll();
        if (_serial_output)
          Debugf("Update: %s\r\n", upload.filename.c_str());

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

        if (upload.name == "filesystem") {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          if (!Update.begin(fsSize, U_FS)){//start with max available size
            if (_serial_output) Update.printError(OTGWSerial);
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
            _sendStatusEvent();
          } else {
            _setStatus(UPDATE_START, "filesystem", 0, uploadTotal, upload.filename, emptyString);
            _sendStatusEvent();
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "flash", 0, uploadTotal, upload.filename, _updaterError);
            _sendStatusEvent();
          } else {
            _setStatus(UPDATE_START, "flash", 0, uploadTotal, upload.filename, emptyString);
            _sendStatusEvent();
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {Debug("."); blinkLEDnow(LED1);}
        // Feed the dog before it bites!
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();
        // End of feeding hack
        size_t written = Update.write(upload.buf, upload.currentSize);
        _status.upload_received = upload.totalSize;
        _status.flash_written += written;
        if (written != upload.currentSize) {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target, _status.flash_written, _status.flash_total, _status.filename, _updaterError);
          _sendStatusEvent();
        } else {
          _setStatus(UPDATE_WRITE, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
          _sendStatusEvent();
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
          if (_serial_output) Debugf("\r\nUpdate Success: %u\r\nRebooting...\r\n", upload.totalSize);
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
          _setStatus(UPDATE_END, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
          _sendStatusEvent();
        } else {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target, _status.flash_written, _status.flash_total, _status.filename, _updaterError);
          _sendStatusEvent();
        }
        // if (_serial_output) 
        //   OTGWSerial.setDebugOutput(false);
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        if (_serial_output) Debugln("Update was aborted");
        _status.upload_received = upload.totalSize;
        if (_status.upload_total == 0 && upload.totalSize > 0) {
          _status.upload_total = upload.totalSize;
        }
        if (_status.flash_total == 0 && upload.totalSize > 0) {
          _status.flash_total = upload.totalSize;
        }
        _setStatus(UPDATE_ABORT, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
        _sendStatusEvent();
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
  _eventClientActive = false;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_setStatus(uint8_t phase, const char *target, size_t received, size_t total, const String &filename, const String &error)
{
  _status.phase = static_cast<UpdatePhase>(phase);
  _status.target = target ? target : "unknown";
  _status.received = received;
  _status.total = total;
  if (_status.flash_total > 0 && _status.flash_written > _status.flash_total) {
    if (_serial_output) {
      Debugf("Update warning: flash_written (%u) > flash_total (%u)\r\n",
             static_cast<unsigned>(_status.flash_written),
             static_cast<unsigned>(_status.flash_total));
    }
    _status.flash_written = _status.flash_total;
  }
  _status.filename = filename;
  _status.error = error;
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
  snprintf(
    buf,
    sizeof(buf),
    "{\"state\":\"%s\",\"target\":\"%s\",\"received\":%u,\"total\":%u,\"upload_received\":%u,\"upload_total\":%u,\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}",
    _phaseToString(_status.phase),
    _status.target ? _status.target : "unknown",
    static_cast<unsigned>(_status.received),
    static_cast<unsigned>(_status.total),
    static_cast<unsigned>(_status.upload_received),
    static_cast<unsigned>(_status.upload_total),
    static_cast<unsigned>(_status.flash_written),
    static_cast<unsigned>(_status.flash_total),
    filenameEsc,
    errorEsc
  );
  _server->send(200, F("application/json"), buf);
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_sendStatusEvent()
{
  if (!_eventClientActive) return;
  if (!_eventClient || !_eventClient.connected()) {
    _eventClientActive = false;
    return;
  }
  unsigned long now = millis();
  if (_status.phase == _lastEventPhase && (now - _lastEventMs) < 250) {
    return;
  }
  _lastEventMs = now;
  _lastEventPhase = _status.phase;

  constexpr size_t JSON_STATUS_BUFFER_SIZE = 512;
  char buf[JSON_STATUS_BUFFER_SIZE];
  char filenameEsc[64];
  char errorEsc[96];
  _jsonEscape(_status.filename, filenameEsc, sizeof(filenameEsc));
  _jsonEscape(_status.error, errorEsc, sizeof(errorEsc));
  int written = snprintf(
    buf,
    sizeof(buf),
    "{\"state\":\"%s\",\"target\":\"%s\",\"received\":%u,\"total\":%u,\"upload_received\":%u,\"upload_total\":%u,\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}",
    _phaseToString(_status.phase),
    _status.target ? _status.target : "unknown",
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
      Debugf("Warning: SSE status JSON truncated (%d chars needed, %d available)\r\n", 
             written, (int)sizeof(buf));
    }
    // Ensure null termination
    buf[sizeof(buf) - 1] = '\0';
    // Don't send truncated/malformed JSON
    return;
  }
  
  _eventClient.print(F("event: status\n"));
  _eventClient.print(F("data: "));
  _eventClient.print(buf);
  _eventClient.print(F("\n\n"));
}

};
