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
        _setStatus(UPDATE_ERROR, _status.target);
        // Restore normal operation regardless of how the error arose.
        // Not clearing bESPactive here would permanently disable loopWifi().
        ::state.flash.bESPactive = false;
        // If we unmounted LittleFS in UPLOAD_FILE_START but never remounted it
        // (error/abort before or during flash), attempt recovery now so the web UI
        // remains functional without a reboot.
        if (!LittleFSmounted && _status.target == "filesystem") {
          LittleFSmounted = LittleFS.begin();
          if (!LittleFSmounted) {
            LittleFS.format();
            LittleFSmounted = LittleFS.begin();
          }
        }
        _server->send(200, F("text/html"), String(F("Flash error: ")) + _updaterError);
      } else {
        _server->client().setNoDelay(true);
        _server->send_P(200, PSTR("text/html"), _serverSuccess);
        _server->client().stop();
        // Reboot for BOTH firmware and filesystem
        if (_serial_output) {
          DebugTln(F("[OTA] Rebooting..."));
        }
        delay(1000);
        ESP.restart();
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = _server->upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();

        _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
        if(!_authenticated){
          if (_serial_output)
            Debugln(F("Unauthenticated Update\n"));
          _status.flash_written = 0;
          _status.flash_total = 0;
          _setStatus(UPDATE_ERROR, emptyString);
          return;
        }

        if (_serial_output) {
          Debugf(PSTR("[OTA] Flash start: %s (size: %s)\r\n"), upload.filename.c_str(), _server->arg("size").c_str());
        }

        // Set global flag to disable background tasks during ESP flash
        ::state.flash.bESPactive = true;
        
        WiFiUDP::stopAll();

        size_t uploadTotal = 0;
        String sizeArg = _server->arg("size");
        if (sizeArg.length()) {
          long parsedSize = sizeArg.toInt();
          if (parsedSize > 0) {
            uploadTotal = static_cast<size_t>(parsedSize);
          }
        }
        _status.flash_total = uploadTotal;
        _status.flash_written = 0;

        _lastProgressPerc = 0;

        if (upload.name == F("filesystem")) {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          if (_serial_output) {
            Debugf(PSTR("[OTA] Target: filesystem (%u bytes)\r\n"), static_cast<unsigned>(fsSize));
          }
          close_all_fs();
          LittleFS.end();
          if (uploadTotal > 0 && uploadTotal > fsSize) {
            _updaterError = F("filesystem image too large");
            _setStatus(UPDATE_ERROR, "filesystem");
          } else if (!Update.begin(fsSize, U_FS)){//always use full partition size to erase stale old-filesystem data in upper blocks
            if (_serial_output) Update.printError(OTGWSerial);
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "filesystem");
          } else {
            _setStatus(UPDATE_START, "filesystem");
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (_serial_output) {
            Debugf(PSTR("[OTA] Target: firmware (%u bytes)\r\n"), static_cast<unsigned>(maxSketchSpace));
          }
          if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
            _updaterError = F("firmware image too large");
            _setStatus(UPDATE_ERROR, "firmware");
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "firmware");
          } else {
            _setStatus(UPDATE_START, "firmware");
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {
          blinkLEDnow(LED1);
        }

        // Feed the dog on every chunk (Main branch behavior)
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();
        
        // Update flash progress counters (used for debug log output below)
        if (_status.flash_total > 0) {
           _status.flash_written = upload.totalSize; // approximation based on upload bytes received
           int currentPerc = (_status.flash_written * 100) / _status.flash_total;
           if (currentPerc != _lastProgressPerc) {
             _lastProgressPerc = currentPerc;
             if (_serial_output) {
               Debugf(PSTR("[OTA] Write: %d%% (%u/%u bytes)\r\n"), currentPerc,
                      static_cast<unsigned>(_status.flash_written),
                      static_cast<unsigned>(_status.flash_total));
             }
             _setStatus(UPDATE_WRITE, _status.target);
           }
        }
        
        // Now perform the blocking flash write
        size_t written = Update.write(upload.buf, upload.currentSize);

        if (written != upload.currentSize) {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target);
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if (_serial_output) {
          DebugTln(F("[OTA] End: finalizing flash..."));
        }
        bool updateOk = Update.end(true); //true to set the size to the current progress
        if(updateOk){
          if (_status.flash_total == 0 && upload.totalSize > 0) {
            _status.flash_total = upload.totalSize;
          }
          if (_status.flash_written < upload.totalSize) {
            _status.flash_written = upload.totalSize;
          }
          _setStatus(UPDATE_END, _status.target);
          if (_serial_output) Debugf(PSTR("[OTA] End: success (%u bytes)\r\n"), upload.totalSize);
          
          if (_status.target == "filesystem") {
            // Mount filesystem, restore settings, then reboot (HTTP_POST handler below)
            LittleFSmounted = LittleFS.begin();
            if (!LittleFSmounted) {
              // Mount failed (e.g. partial write left stale metadata). Attempt a clean
              // format so the device boots with defaults rather than staying unrecoverable.
              if (_serial_output) Debugln(F("[OTA] LittleFS mount failed; formatting for recovery"));
              LittleFS.format();
              LittleFSmounted = LittleFS.begin();
            }
            if (LittleFSmounted) {
              updateLittleFSStatus(F("/.ota_post"));
              if (_serial_output) Debugln(F("[OTA] Restoring settings to filesystem"));
              writeSettings(false);  // show=false: TelnetStream not connected during OTA
              // Clear the deferred-flush dirty flag so the 2-second timer does not trigger
              // MQTT/NTP/MDNS service restarts during the 1-second window before reboot.
              settingsMarkClean();
            } else {
              LittleFSmounted = false;
              if (_serial_output) Debugln(F("[OTA] Error: LittleFS mount failed even after format"));
            }
          }
          
          if (_serial_output) {
            DebugTln(F("[OTA] Preparing to reboot..."));
            DebugFlush();  // Ensure debug output is sent before reboot
          }

          // Clear global flag - flash completed successfully
          ::state.flash.bESPactive = false;
        } else {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target);

          // Clear global flag - flash failed
          ::state.flash.bESPactive = false;
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        if (_serial_output) Debugln(F("[OTA] Abort: update cancelled"));
        _setStatus(UPDATE_ABORT, _status.target);
        ::state.flash.bESPactive = false;
        // LittleFS was unmounted at UPLOAD_FILE_START; HTTP_POST error handler
        // will remount it (with format+recover if needed) when it runs next.
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
  _status.target = emptyString;
  _status.flash_written = 0;
  _status.flash_total = 0;
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_setStatus(uint8_t phase, const String &target)
{
  _status.phase = static_cast<UpdatePhase>(phase);
  _status.target = target;
  if (_status.flash_total > 0 && _status.flash_written > _status.flash_total) {
    if (_serial_output) {
      Debugf(PSTR("[OTA] Warning: flash_written (%u) > flash_total (%u); clamping\r\n"),
             static_cast<unsigned>(_status.flash_written),
             static_cast<unsigned>(_status.flash_total));
    }
    _status.flash_written = _status.flash_total;
  }
}

};
