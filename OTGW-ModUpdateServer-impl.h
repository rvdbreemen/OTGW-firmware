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
#include "safeTimers.h"
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
      if (_serial_output) {
        unsigned long statusStartMs = millis();
        Debugf(PSTR("[%lu] Status request start heap=%u bytes\r\n"),
               statusStartMs, static_cast<unsigned>(ESP.getFreeHeap()));
        if (::isESPFlashing) {
          DebugTln(F("Update status requested during flash (polling active)"));
        }
        _sendStatusJson();
        Debugf(PSTR("[%lu] Status request end duration=%lu ms\r\n"),
               millis(), static_cast<unsigned long>(millis() - statusStartMs));
        return;
      }
      _sendStatusJson();
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(path.c_str(), HTTP_POST, [&](){
      if(!_authenticated)
        return _server->requestAuthentication();
      if (Update.hasError()) {
        if (_serial_output) {
          DebugTln(F("Update POST complete: Update.hasError() true"));
        }
        _setStatus(UPDATE_ERROR, _status.target, _status.received, _status.total, _status.filename, _updaterError);
        _server->send(200, F("text/html"), String(F("Flash error: ")) + _updaterError);
      } else {
        if (_serial_output) {
          DebugTln(F("Update POST complete: success response sent"));
        }
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        // Reboot for BOTH firmware and filesystem
        if (_serial_output) {
          Debugf(PSTR("[%lu] OTA POST complete, rebooting...\r\n"), millis());
        }
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

        if (_serial_output) {
          DebugTln(F("Update authenticated; starting flash session"));
          Debugf(PSTR("Update heap at start: %u bytes\r\n"), static_cast<unsigned>(ESP.getFreeHeap()));
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
          if (_serial_output) {
            DebugTln(F("Update target: filesystem"));
          }
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          if (_serial_output) {
            Debugf(PSTR("Filesystem size: %u bytes\r\n"), static_cast<unsigned>(fsSize));
          }
          close_all_fs();
          LittleFS.end();
          if (uploadTotal > 0 && uploadTotal > fsSize) {
            _updaterError = F("filesystem image too large");
            if (_serial_output) {
              Debugf(PSTR("Filesystem image too large: %u > %u\r\n"), static_cast<unsigned>(uploadTotal), static_cast<unsigned>(fsSize));
            }
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : fsSize, U_FS)){//start with max available size
            if (_serial_output) Update.printError(OTGWSerial);
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
          } else {
            if (_serial_output) {
              Debugf(PSTR("Filesystem update begin OK (size: %u)\r\n"), static_cast<unsigned>(uploadTotal > 0 ? uploadTotal : fsSize));
            }
            _setStatus(UPDATE_START, "filesystem", 0, uploadTotal, upload.filename, emptyString);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (_serial_output) {
            DebugTln(F("Update target: firmware"));
            Debugf(PSTR("Max sketch space: %u bytes\r\n"), static_cast<unsigned>(maxSketchSpace));
          }
          if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
            _updaterError = F("firmware image too large");
            if (_serial_output) {
              Debugf(PSTR("Firmware image too large: %u > %u\r\n"), static_cast<unsigned>(uploadTotal), static_cast<unsigned>(maxSketchSpace));
            }
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
          } else {
            if (_serial_output) {
              Debugf(PSTR("Firmware update begin OK (size: %u)\r\n"), static_cast<unsigned>(uploadTotal > 0 ? uploadTotal : maxSketchSpace));
            }
            _setStatus(UPDATE_START, "firmware", 0, uploadTotal, upload.filename, emptyString);
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {Debug("."); blinkLEDnow(LED1);}

        // Feed the dog on every chunk (Main branch behavior)
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();
        if (_serial_output) {
          static unsigned long lastDogLogMs = 0;
          unsigned long nowMs = millis();
          if (lastDogLogMs == 0) {
            lastDogLogMs = nowMs;
          } else if (nowMs - lastDogLogMs >= 1000) {
            Debugf(PSTR("Watchdog feed OK (chunk: %u bytes)\r\n"),
                   static_cast<unsigned>(upload.currentSize));
            lastDogLogMs = nowMs;
          }
        }
        
        // Update status and send WebSocket BEFORE the blocking flash write
        // This ensures progress updates reach the browser even if the write blocks for 10+ seconds
        _status.upload_received = upload.totalSize;
        if (_status.flash_total > 0) {
           _status.flash_written = _status.upload_received; // approximation based on upload
           int currentPerc = (_status.flash_written * 100) / _status.flash_total;
           if (currentPerc != _lastProgressPerc) {
             _lastProgressPerc = currentPerc;
             if (_serial_output) {
               Debugf(PSTR("Update progress: %d%% (upload %u/%u)\r\n"), currentPerc,
                      static_cast<unsigned>(_status.upload_received),
                      static_cast<unsigned>(_status.flash_total));
             }
             _setStatus(UPDATE_WRITE, _status.target, _status.flash_written, _status.flash_total, _status.filename, emptyString);
           }
        }
        
        // Now perform the blocking flash write
        unsigned long writeStartMs = millis();
        size_t written = Update.write(upload.buf, upload.currentSize);
        unsigned long writeEndMs = millis();
        if (_serial_output) {
          unsigned long writeMs = (writeEndMs >= writeStartMs) ? (writeEndMs - writeStartMs) : 0;
          static unsigned long lastWriteLogMs = 0;
          if (lastWriteLogMs == 0 || writeMs > 200 || (writeEndMs - lastWriteLogMs) >= 1000) {
            Debugf(PSTR("Update write duration: %u ms (chunk %u bytes)\r\n"),
                   static_cast<unsigned>(writeMs),
                   static_cast<unsigned>(upload.currentSize));
            lastWriteLogMs = writeEndMs;
          }
        }

        if (_serial_output) {
          static unsigned long lastRateLogMs = 0;
          static size_t lastRateBytes = 0;
          unsigned long nowMs = millis();
          if (lastRateLogMs == 0) {
            lastRateLogMs = nowMs;
            lastRateBytes = _status.upload_received;
          } else if (nowMs - lastRateLogMs >= 1000) {
            size_t delta = _status.upload_received - lastRateBytes;
            Debugf(PSTR("Update throughput: %u bytes/s (uploaded %u/%u)\r\n"),
                   static_cast<unsigned>(delta),
                   static_cast<unsigned>(_status.upload_received),
                   static_cast<unsigned>(_status.flash_total));
            lastRateLogMs = nowMs;
            lastRateBytes = _status.upload_received;
          }
        }

        if (written != upload.currentSize) {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, _updaterError);
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if (_serial_output) {
          Debugf(PSTR("[%lu] Update end begin\r\n"), millis());
        }
        bool updateOk = Update.end(true); //true to set the size to the current progress
        if (_serial_output) {
          if (updateOk) {
            Debugf(PSTR("[%lu] Update end OK\r\n"), millis());
          } else {
            Debugf(PSTR("[%lu] Update end FAILED\r\n"), millis());
          }
        }
        if(updateOk){
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
          if (_serial_output) Debugf(PSTR("\r\nUpdate Success: %u\r\n"), upload.totalSize);
          
          if (_status.target == "filesystem") {
            LittleFSmounted = LittleFS.begin();
            if (LittleFSmounted) {
              updateLittleFSStatus(F("/.ota_post"));
              // Restore settings from ESP memory to new filesystem
              // During filesystem-only OTA, only the LittleFS partition is erased/written.
              // The ESP8266 continues running the current firmware and RAM remains intact.
              // All settings loaded at boot (global variables like settingHostname, etc.)
              // are still valid in RAM, so we write them back to the fresh filesystem.
              writeSettings(true);
              if (_serial_output) Debugln(F("\r\nFilesystem update complete; settings restored from memory"));
            } else {
              // Ensure state is explicitly false and log failure for diagnostics
              LittleFSmounted = false;
              if (_serial_output) Debugln(F("LittleFS mount failed after filesystem OTA update"));
            }
          }
          
          if (_serial_output) {
            Debugln(F("Rebooting..."));
            DebugFlush();  // Ensure debug output is sent before reboot
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
      Debugf(PSTR("Update Status detail: target=%s filename=%s error=%s\r\n"),
             target.c_str(), filename.c_str(), error.c_str());
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
  // Stack is better if size is reasonable. 320 bytes provides safety margin for edge cases.
  // Max size: state(5) + flash_written(10) + flash_total(10) + filename(64) + error(96) + overhead(69) = 254 bytes
  char buf[320];
  char filenameEsc[64];
  char errorEsc[96];
  _jsonEscape(_status.filename, filenameEsc, sizeof(filenameEsc));
  _jsonEscape(_status.error, errorEsc, sizeof(errorEsc));
  int written = snprintf_P(
    buf,
    sizeof(buf),
    PSTR("{\"state\":\"%s\",\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}"),
    _phaseToString(_status.phase),
    static_cast<unsigned>(_status.flash_written),
    static_cast<unsigned>(_status.flash_total),
    filenameEsc,
    errorEsc
  );
  
  if (written > 0 && written < (int)sizeof(buf)) {
      // Throttle WebSocket broadcasts during flash to prevent network stack interference
      // Send updates max twice per second (500ms) to avoid overwhelming ESP8266's limited buffers
      // EXCEPT for final/error states - always send those immediately
      DECLARE_TIMER_MS(timerWSJSONThrottle, 500, SKIP_MISSED_TICKS);
      if (_status.phase == UPDATE_END || _status.phase == UPDATE_ERROR || _status.phase == UPDATE_ABORT || DUE(timerWSJSONThrottle)) {
        sendWebSocketJSON(buf);
      }
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
  constexpr size_t JSON_STATUS_BUFFER_SIZE = 320;
  char buf[JSON_STATUS_BUFFER_SIZE];
  char filenameEsc[64];
  char errorEsc[96];
  _jsonEscape(_status.filename, filenameEsc, sizeof(filenameEsc));
  _jsonEscape(_status.error, errorEsc, sizeof(errorEsc));
  int written = snprintf_P(
    buf,
    sizeof(buf),
    PSTR("{\"state\":\"%s\",\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}"),
    _phaseToString(_status.phase),
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

  if (_serial_output) {
    Debugf(PSTR("Status JSON sent: %s\r\n"), buf);
  }
  
  _server->send(200, F("application/json"), buf);
}

};
