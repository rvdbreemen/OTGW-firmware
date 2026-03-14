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
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password)
{
    _server = server;
    _username = username;
    _password = password;
    _updaterError.clear();
    _target = emptyString;

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
      // Always clear flash flag — upload callbacks may have left it set on error paths
      ::state.flash.bESPactive = false;
      if (Update.hasError() || _updaterError.length()) {
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
        delay(3000);
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

        if (upload.name == F("filesystem")) {
          _target = "filesystem";
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          if (_serial_output) {
            Debugf(PSTR("[OTA] Target: filesystem (%u bytes)\r\n"), static_cast<unsigned>(fsSize));
          }
          close_all_fs();
          LittleFS.end();
          if (uploadTotal > 0 && uploadTotal > fsSize) {
            _updaterError = F("filesystem image too large");
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : fsSize, U_FS)){//start with upload size; upper sectors cleaned in UPLOAD_FILE_END
            if (_serial_output) Update.printError(OTGWSerial);
            _setUpdaterError();
          }
        } else {
          _target = "firmware";
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (_serial_output) {
            Debugf(PSTR("[OTA] Target: firmware (%u bytes)\r\n"), static_cast<unsigned>(maxSketchSpace));
          }
          if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
            _updaterError = F("firmware image too large");
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)){
            _setUpdaterError();
          }
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if (_serial_output) {
          blinkLEDnow(LED1);
        }

        // Feed HW watchdog on every chunk
        Wire.beginTransmission(0x26);   Wire.write(0xA5);   Wire.endTransmission();

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          _setUpdaterError();
        }
      } else if(_authenticated && upload.status == UPLOAD_FILE_END){
        if (!_updaterError.length()) {
          if (_serial_output) {
            DebugTln(F("[OTA] End: finalizing flash..."));
          }
          bool updateOk = Update.end(true); //true to set the size to the current progress
          if(updateOk){
            if (_serial_output) Debugf(PSTR("[OTA] End: success (%u bytes)\r\n"), upload.totalSize);

            if (_target == "filesystem") {
              // Erase sectors above the uploaded image to eliminate stale old-filesystem
              // metadata in the upper LittleFS blocks.  The ESP8266 Updater only erases
              // sectors JIT during write(), so sectors above upload.totalSize still contain
              // data from the previous filesystem.  LittleFS mounts with block_count derived
              // from the full partition size and will scan every block; encountering mixed
              // new+old metadata triggers blocking LittleFS traversals that exceed the
              // Software WDT timeout, causing a ctx:cont crash and endless reboot loop.
              //
              // Feed both the HW watchdog (I2C at 0x26) and SW WDT (via Wire.endTransmission
              // which calls yield()) between each 4 KB sector erase to prevent WD resets
              // during what can be a 100-200 ms total erase window.
              if (upload.totalSize > 0) {
                const size_t curFsSize = ((size_t)&_FS_end - (size_t)&_FS_start);
                const uint32_t fsFirstSector = ((uint32_t)(&_FS_start) - 0x40200000U) / 4096U;
                const size_t uploadedSectors = (upload.totalSize + 4095U) / 4096U;
                const size_t totalSectors    = curFsSize / 4096U;
                if (uploadedSectors < totalSectors) {
                  if (_serial_output)
                    Debugf(PSTR("[OTA] Erasing %u stale upper sector(s) (blocks %u-%u)\r\n"),
                           (unsigned)(totalSectors - uploadedSectors),
                           (unsigned)uploadedSectors,
                           (unsigned)(totalSectors - 1U));
                  for (size_t s = uploadedSectors; s < totalSectors; s++) {
                    ESP.flashEraseSector(fsFirstSector + s);
                    // Feed HW watchdog (I2C device at 0x26) and reset SW WDT via yield()
                    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);
                    Wire.write(0xA5);
                    Wire.endTransmission();  // Wire I2C calls yield() internally
                  }
                }
              }

              // Mount filesystem, restore settings, then reboot (HTTP_POST handler above)
              LittleFSmounted = LittleFS.begin();
              if (LittleFSmounted) {
                updateLittleFSStatus(F("/.ota_post"));
                if (_serial_output) Debugln(F("[OTA] Restoring settings to filesystem"));
                writeSettings(false);  // show=false: TelnetStream not connected during OTA
              } else {
                LittleFSmounted = false;
                if (_serial_output) Debugln(F("[OTA] Error: LittleFS mount failed"));
              }
            }

            if (_serial_output) {
              DebugTln(F("[OTA] Preparing to reboot..."));
              DebugFlush();  // Ensure debug output is sent before reboot
            }
          } else {
            _setUpdaterError();
          }
        }
        // Clear flash flag — HTTP_POST handler will restart
        ::state.flash.bESPactive = false;
      } else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
        if (_serial_output) Debugln(F("[OTA] Abort: update cancelled"));
        ::state.flash.bESPactive = false;
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
