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
#include <Hash.h>
#include <base64.h>

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

// RFC 6455 WebSocket Protocol - supported version
static const char WEBSOCKET_VERSION[] = "13";

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
  _isWebSocket = false;
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

    // Collect headers needed for WebSocket handshake
    const char * headerkeys[] = {"Upgrade", "Sec-WebSocket-Key", "Sec-WebSocket-Version"};
    size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
    _server->collectHeaders(headerkeys, headerkeyssize);

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

    _server->on("/events", HTTP_GET, [&](){
      if(_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();

      // Check for WebSocket Upgrade
      if (_server->header("Upgrade").equalsIgnoreCase("websocket")) {
          // RFC 6455: Validate Sec-WebSocket-Version header
          String version = _server->header("Sec-WebSocket-Version");
          if (version.isEmpty()) {
              // Missing version header
              _server->sendHeader("Sec-WebSocket-Version", WEBSOCKET_VERSION);
              _server->send(400, "text/plain", "Missing Sec-WebSocket-Version header");
              return;
          }
          if (version != WEBSOCKET_VERSION) {
              // Unsupported version
              _server->sendHeader("Sec-WebSocket-Version", WEBSOCKET_VERSION);
              _server->send(400, "text/plain", "WebSocket version not supported");
              return;
          }

          String keyHeader = _server->header("Sec-WebSocket-Key");
          // RFC 6455 requires WebSocket key to be exactly 24 base64 characters (16 bytes encoded)
          if (keyHeader.length() != 24) {
              // Invalid or missing key
              _server->send(400, "text/plain", "Invalid Sec-WebSocket-Key header");
              return;
          }
          
          // Calculate Sec-WebSocket-Accept
          // Use char buffer to avoid heap fragmentation from String concatenation
          // Buffer size: 24 (validated key) + 36 (magic string) + 1 (null) = 61 bytes
          // Allocated 64 bytes for alignment and safety margin
          char keyWithMagic[64];
          snprintf(keyWithMagic, sizeof(keyWithMagic), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", keyHeader.c_str());
          
          uint8_t hash[20];
          sha1(keyWithMagic, &hash[0]);
          String accept = base64::encode(hash, 20);

          _server->sendHeader("Upgrade", "websocket");
          _server->sendHeader("Connection", "Upgrade");
          _server->sendHeader("Sec-WebSocket-Accept", accept);
          _server->send(101);

          // Clean up any previously active event client before assigning a new one
          if (_eventClientActive) {
            if (_eventClient.connected()) {
              _eventClient.stop();
            }
            _eventClientActive = false;
          }

          _eventClient = _server->client();
          _eventClientActive = true;
          _isWebSocket = true;
          if (_serial_output) Debugln("WS: Connected");
          return;
      }

      // Fallback to SSE
      _server->sendHeader(F("Cache-Control"), F("no-cache"));
      _server->sendHeader(F("Connection"), F("keep-alive"));
      _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
      _server->send(200, F("text/event-stream"), "");

      // Clean up any previously active event client before assigning a new one
      if (_eventClientActive) {
        if (_eventClient.connected()) {
          _eventClient.stop();
        }
        _eventClientActive = false;
      }

      _eventClient = _server->client();
      _eventClient.setNoDelay(true);
      _isWebSocket = false;

      // Only mark the client active and send an event if the connection is valid
      if (_eventClient.connected()) {
        _eventClientActive = true;
        _sendStatusEvent();
      }
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
          if (uploadTotal > 0 && uploadTotal > fsSize) {
            _updaterError = F("filesystem image too large");
            _setStatus(UPDATE_ERROR, "filesystem", 0, uploadTotal, upload.filename, _updaterError);
            _sendStatusEvent();
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : fsSize, U_FS)){//start with max available size
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
          if (uploadTotal > 0 && uploadTotal > maxSketchSpace) {
            _updaterError = F("firmware image too large");
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
            _sendStatusEvent();
          } else if (!Update.begin(uploadTotal > 0 ? uploadTotal : maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
            _setStatus(UPDATE_ERROR, "firmware", 0, uploadTotal, upload.filename, _updaterError);
            _sendStatusEvent();
          } else {
            _setStatus(UPDATE_START, "firmware", 0, uploadTotal, upload.filename, emptyString);
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
          _setStatus(UPDATE_ERROR, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, _updaterError);
          _sendStatusEvent();
        } else {
          _setStatus(UPDATE_WRITE, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, emptyString);
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
          _setStatus(UPDATE_END, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, emptyString);
          _sendStatusEvent();
        } else {
          _setUpdaterError();
          _setStatus(UPDATE_ERROR, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, _updaterError);
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
        _setStatus(UPDATE_ABORT, _status.target.c_str(), _status.flash_written, _status.flash_total, _status.filename, emptyString);
        _sendStatusEvent();
      }
      // Delay of 1ms to prevent network starvation (needed when no Telnet/Debug is active)
      delay(1);
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
void ESP8266HTTPUpdateServerTemplate<ServerType>::_setStatus(uint8_t phase, const String &target, size_t received, size_t total, const String &filename, const String &error)
{
  _status.phase = static_cast<UpdatePhase>(phase);
  _status.target = target.length() ? target : "unknown";
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
  int written = snprintf(
    buf,
    sizeof(buf),
    "{\"state\":\"%s\",\"target\":\"%s\",\"received\":%u,\"total\":%u,\"upload_received\":%u,\"upload_total\":%u,\"flash_written\":%u,\"flash_total\":%u,\"filename\":\"%s\",\"error\":\"%s\"}",
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
      Debugf("Warning: status JSON truncated (%d chars needed, %d available)\r\n", 
             written, (int)sizeof(buf));
    }
    // Send error response instead of truncated JSON
    _server->send(500, F("text/plain"), F("Status buffer overflow"));
    return;
  }
  
  _server->send(200, F("application/json"), buf);
}

template <typename ServerType>
void ESP8266HTTPUpdateServerTemplate<ServerType>::_sendStatusEvent()
{
  if (!_eventClientActive) return;
  if (!_eventClient || !_eventClient.connected()) {
    if (_serial_output) Debugln("SSE: Client disconnected");
    _eventClientActive = false;
    return;
  }
  unsigned long now = millis();
  // Use int32_t cast to handle millis() overflow correctly (wraps every ~49 days)
  // Throttle to 100ms to be responsive but not flood
  if (_status.phase == _lastEventPhase && (int32_t)(now - _lastEventMs) < 100) {
    return;
  }
  // Note: _lastEventMs and _lastEventPhase are updated ONLY after successful send

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
      Debugf("Warning: Status JSON truncated (%d chars needed, %d available)\r\n", 
             written, (int)sizeof(buf));
    }
    // Don't send truncated/malformed JSON
    return;
  }
  
  // Build WebSocket frame header or calculate SSE message length
  char wsHeader[4];  // Max 4 bytes for WebSocket frame header
  size_t headerLen = 0;
  size_t msgLen;
  
  if (_isWebSocket) {
      // WebSocket Frame: FIN + Text (0x81)
      // Build frame header directly in a buffer to avoid String heap fragmentation
      size_t payloadLen = written;
      
      // Note: JSON payload is max 512 bytes (JSON_STATUS_BUFFER_SIZE), so we only need
      // to handle payloadLen < 65536. Payloads >= 65536 would need 8-byte extended length.
      wsHeader[0] = 0x81;  // FIN + Text opcode
      if (payloadLen < 126) {
          wsHeader[1] = (char)payloadLen;
          headerLen = 2;
      } else {  // 126 <= payloadLen < 65536
          wsHeader[1] = 126;
          wsHeader[2] = (char)((payloadLen >> 8) & 0xFF);
          wsHeader[3] = (char)(payloadLen & 0xFF);
          headerLen = 4;
      }
      
      msgLen = headerLen + payloadLen;
  } else {
      // SSE Format: "event: status\n" (14) + "data: " (6) + JSON + "\n\n" (2) = 22 overhead
      msgLen = written + 22;
  }
  
  // Robustness: Check if we can write without blocking
  // If the send buffer is full (e.g. network congestion), skip this update
  // to prioritize the file upload process.
  size_t available = _eventClient.availableForWrite();

  // If this is a final state, we really want to send it, so we can afford to wait a bit
  // This "speed reads" the buffer by yielding to the network stack
  bool isFinalState = (_status.phase == UPDATE_END || _status.phase == UPDATE_ERROR || _status.phase == UPDATE_ABORT);
  
  if (available < msgLen && isFinalState) {
      // Try to wait for buffer to drain (max 1000ms)
      unsigned long startWait = millis();
      // Cast the millis() difference to int32_t so the timeout remains correct even when
      // millis() wraps around (standard rollover-safe timing idiom on 32-bit counters).
      while (available < msgLen && (int32_t)(millis() - startWait) < 1000) {
          yield(); // Allow network stack to process and drain buffer
          available = _eventClient.availableForWrite();
      }
  }

  if (available >= msgLen) {
      if (_isWebSocket) {
          // Send WebSocket frame header followed by JSON payload
          _eventClient.write((const uint8_t*)wsHeader, headerLen);
          _eventClient.write((const uint8_t*)buf, written);
      } else {
          // SSE Format - build and send as String
          String msg;
          msg.reserve(msgLen);  // Already calculated as written + 22
          msg = F("event: status\n");
          msg += F("data: ");
          msg += buf;
          msg += F("\n\n");
          _eventClient.print(msg);
      }
      _lastEventMs = now;
      _lastEventPhase = _status.phase;
      yield(); 
  } else {
      if (_serial_output) {
          Debugf("WS: Buffer full (avail: %u, need: %u). Skipping update.\r\n", available, msgLen);
      }
  }
}

};
