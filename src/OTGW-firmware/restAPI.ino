/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v1.3.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <string.h>
#include <ctype.h>

#define RESTDebugTln(...) ({ if (bDebugRestAPI) DebugTln(__VA_ARGS__);    })
#define RESTDebugln(...)  ({ if (bDebugRestAPI) Debugln(__VA_ARGS__);    })
#define RESTDebugTf(...)  ({ if (bDebugRestAPI) DebugTf(__VA_ARGS__);    })
#define RESTDebugf(...)   ({ if (bDebugRestAPI) Debugf(__VA_ARGS__);    })
#define RESTDebugT(...)   ({ if (bDebugRestAPI) DebugT(__VA_ARGS__);    })
#define RESTDebug(...)    ({ if (bDebugRestAPI) Debug(__VA_ARGS__);    })

//=======================================================================
// RESTful v2 API: Consistent JSON error response helper (ADR-035)
// Returns: {"error":{"status":N,"message":"..."}}
//=======================================================================
static void sendApiError(int httpCode, const __FlashStringHelper* message) {
  char jsonBuff[200];
  snprintf_P(jsonBuff, sizeof(jsonBuff),
    PSTR("{\"error\":{\"status\":%d,\"message\":\"%S\"}}"),
    httpCode, (PGM_P)message);
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(httpCode, F("application/json"), jsonBuff);
}

// T44: 405 responses with RFC 7231 §6.5.5 Allow header
static void sendApiMethodNotAllowed(const __FlashStringHelper* allowedMethods) {
  httpServer.sendHeader(F("Allow"), allowedMethods);
  sendApiError(405, F("Method not allowed"));
}

//=======================================================================

static bool isDigitStr(const char *s) {
  if (s == nullptr || *s == '\0') return false;
  while (*s) {
    if (!isdigit(static_cast<unsigned char>(*s))) return false;
    s++;
  }
  return true;
}

static bool parseMsgId(const char *token, uint8_t &msgId) {
  if (!isDigitStr(token)) return false;
  long val = strtol(token, nullptr, 10);
  if (val < 0 || val > OT_MSGID_MAX) return false;
  msgId = static_cast<uint8_t>(val);
  return true;
}

static bool parseSettingRequestBody(const char* body, char* field, size_t fieldSize, char* value, size_t valueSize) {
  if (!body || body[0] == '\0') return false;
  if (!extractJsonFieldText(body, "name", field, fieldSize)) return false;
  if (field[0] == '\0') return false;
  if (!extractJsonFieldText(body, "value", value, valueSize)) return false;
  if (strcmp_P(value, PSTR("null")) == 0) value[0] = '\0';
  return true;
}

static bool isValidDallasLabelJson(const char* body) {
  if (!body) return false;
  const char* p = body;
  while (*p && isspace(static_cast<unsigned char>(*p))) p++;
  if (*p != '{') return false;
  p++;
  while (*p && isspace(static_cast<unsigned char>(*p))) p++;
  if (*p == '}') return true;

  while (*p) {
    if (*p != '"') return false;
    p++;
    while (*p && *p != '"') {
      if (*p == '\\' && *(p + 1)) p++;
      p++;
    }
    if (*p != '"') return false;
    p++;
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;
    if (*p != ':') return false;
    p++;
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;
    if (*p != '"') return false;
    p++;
    while (*p && *p != '"') {
      if (*p == '\\' && *(p + 1)) p++;
      p++;
    }
    if (*p != '"') return false;
    p++;
    while (*p && isspace(static_cast<unsigned char>(*p))) p++;
    if (*p == ',') {
      p++;
      while (*p && isspace(static_cast<unsigned char>(*p))) p++;
      continue;
    }
    if (*p == '}') {
      p++;
      while (*p && isspace(static_cast<unsigned char>(*p))) p++;
      return (*p == '\0');
    }
    return false;
  }
  return false;
}

//=======================================================================

void processAPI() 
{
  constexpr uint8_t MAX_WORDS = 8;
  constexpr size_t WORD_LEN = 32;
  char URI[50]   = "";
  char words[MAX_WORDS][WORD_LEN] = {{0}};

  const HTTPMethod method = httpServer.method();
  const bool isGet = (method == HTTP_GET);
  const bool isPostOrPut = (method == HTTP_POST || method == HTTP_PUT);

  const size_t uriLen = strlcpy(URI, httpServer.uri().c_str(), sizeof(URI));
  char originalURI[sizeof(URI)];
  strlcpy(originalURI, URI, sizeof(originalURI));

  RESTDebugTf(PSTR("from[%s] URI[%s] method[%s] \r\n"), httpServer.client().remoteIP().toString().c_str(), URI, strHTTPmethod(method).c_str());

  if (uriLen >= sizeof(URI))
  {
    RESTDebugTln(F("==> Bailout due to oversized URI"));
    httpServer.send_P(414, PSTR("text/plain"), PSTR("414: URI too long\r\n"));
    return;
  }

  if (ESP.getFreeHeap() < 4096) // to prevent firmware from crashing!
  {
    // Lowered from 8500 to 4096 in v1.0.0-rc3.
    // The new WebSocket server (port 81) consumes significant heap, establishing a new lower normal baseline.
    // The REST API refactor to C-strings reduces fragmentation, making operation at 4KB safe.
    RESTDebugTf(PSTR("==> Bailout due to low heap (%d bytes))\r\n"), ESP.getFreeHeap() );
    httpServer.send_P(500, PSTR("text/plain"), PSTR("500: internal server error (low heap)\r\n")); 
    return;
  }

  uint8_t wc = 0;
  {
    char *savePtr = nullptr;

    if (URI[0] == '/' && wc < MAX_WORDS) {
      words[wc][0] = '\0';
      wc++;
    }

    for (char *token = strtok_r(URI, "/", &savePtr); token && wc < MAX_WORDS; token = strtok_r(nullptr, "/", &savePtr)) {
      strlcpy(words[wc], token, WORD_LEN);
      wc++;
    }
  }
  
  if (bDebugRestAPI)
  {
    DebugT(F(">>"));
    for (uint_fast8_t  w=0; w<wc; w++)
    {
      Debugf(PSTR("word[%d] => [%s], "), w, words[w]);
    }
    Debugln(F(" "));
  }

  if (wc > 1 && strcmp_P(words[1], PSTR("api")) == 0) {

    if (wc > 2 && strcmp_P(words[2], PSTR("v2")) == 0)
    { //v2 API calls — RESTful compliant (ADR-035)
      // T45: OPTIONS preflight for all v2 endpoints (CORS support)
      if (method == HTTP_OPTIONS) {
        httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
        httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET, OPTIONS"));
        httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
        httpServer.sendHeader(F("Access-Control-Max-Age"), F("86400"));
        httpServer.send(204);
        return;
      }
      if (wc > 3 && strcmp_P(words[3], PSTR("health")) == 0) {
        // GET /api/v2/health
        if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
        sendHealth();
      } else if (wc > 3 && strcmp_P(words[3], PSTR("settings")) == 0) {
        // GET/POST /api/v2/settings
        if (isPostOrPut) {
          postSettings();
        } else if (isGet) {
          sendDeviceSettings();
        } else {
          sendApiMethodNotAllowed(F("GET, POST, PUT"));
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("sensors")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("labels")) == 0) {
          // GET/POST /api/v2/sensors/labels
          if (isGet) {
            getDallasLabels();
          } else if (isPostOrPut) {
            updateAllDallasLabels();
          } else {
            sendApiMethodNotAllowed(F("GET, POST, PUT"));
          }
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("device")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("info")) == 0) {
          // GET /api/v2/device/info (map format)
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendDeviceInfoV2();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("time")) == 0) {
          // GET /api/v2/device/time — RESTful name for devtime (map format)
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendDeviceTimeV2();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("flash")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0) {
          // GET /api/v2/flash/status — RESTful name for flashstatus
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendFlashStatus();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("pic")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("flash-status")) == 0) {
          // GET /api/v2/pic/flash-status — RESTful name for pic/flashstatus
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendPICFlashStatus();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("update-check")) == 0) {
          // GET /api/v2/pic/update-check — on-demand check for available PIC firmware
          // Makes an outbound request; only call when the user opens the PIC firmware tab.
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendPICUpdateCheck();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("firmware")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("files")) == 0) {
          // GET /api/v2/firmware/files — versioned replacement for /api/firmwarefilelist
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          apifirmwarefilelist();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("filesystem")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("files")) == 0) {
          // GET /api/v2/filesystem/files — versioned replacement for /api/listfiles
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          apilistfiles();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("hash-check")) == 0) {
          // GET /api/v2/filesystem/hash-check — compare LittleFS version.hash with firmware hash
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendFilesystemHashCheck();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("otmonitor")) == 0) {
          // GET /api/v2/otgw/otmonitor
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendOTmonitorV2();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("telegraf")) == 0) {
          // GET /api/v2/otgw/telegraf — map format (use otmonitor map, same data)
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          sendOTmonitorV2();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("messages")) == 0) {
          // GET /api/v2/otgw/messages/{id} — RESTful resource name for OT message by ID
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          uint8_t msgId = 0;
          if (wc > 5 && parseMsgId(words[5], msgId)) {
            sendOTGWvalue(msgId);
          } else {
            sendApiError(400, F("Invalid or missing message ID"));
          }
        } else if (wc > 4 && strcmp_P(words[4], PSTR("commands")) == 0) {
          // POST /api/v2/otgw/commands — RESTful: command in body, 202 Accepted
          if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }

          // Read command from request body (JSON: {"command":"TT=20.5"} or plain text)
          const String& body = httpServer.arg(0);
          char cmdBuf[64] = "";
          if (!extractJsonFieldText(body.c_str(), "command", cmdBuf, sizeof(cmdBuf))) {
            strlcpy(cmdBuf, body.c_str(), sizeof(cmdBuf));
          }
          const char* cmdStr = cmdBuf;

          if (!cmdStr || cmdStr[0] == '\0') {
            sendApiError(400, F("Missing command"));
            return;
          }

          constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1;
          const size_t cmdLen = strlen(cmdStr);
          if ((cmdLen < 3) || (cmdStr[2] != '=')) {
            sendApiError(400, F("Invalid command format (expected XX=value)"));
            return;
          }
          if (cmdLen > kMaxCmdLen) {
            sendApiError(413, F("Command too long"));
            return;
          }

          addOTWGcmdtoqueue(cmdStr, static_cast<int>(cmdLen));
          // 202 Accepted — command queued for async processing
          httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
          httpServer.send(202, F("application/json"), F("{\"status\":\"queued\"}"));
        } else if (wc > 4 && strcmp_P(words[4], PSTR("discovery")) == 0) {
          // POST /api/v2/otgw/discovery — RESTful name for MQTT autodiscovery trigger
          if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
          // 202 Accepted — discovery messages sent asynchronously
          httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
          httpServer.send(202, F("application/json"), F("{\"status\":\"accepted\"}"));
          doAutoConfigure();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("id")) == 0) {
          // GET /api/v2/otgw/id/{msgid} — backward compat alias for messages/{id}
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          uint8_t msgId = 0;
          if (wc > 5 && parseMsgId(words[5], msgId)) {
            sendOTGWvalue(msgId);
          } else {
            sendApiError(400, F("Invalid or missing message ID"));
          }
        } else if (wc > 4 && strcmp_P(words[4], PSTR("label")) == 0) {
          // GET /api/v2/otgw/label/{msglabel}
          if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
          if (wc <= 5 || words[5][0] == '\0') { sendApiError(400, F("Missing label")); return; }
          sendOTGWlabel(words[5]);
        } else if (wc > 4 && strcmp_P(words[4], PSTR("command")) == 0) {
          // POST /api/v2/otgw/command/{cmd} — backward compat alias (prefer /commands)
          if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
          if (wc <= 5 || words[5][0] == '\0') {
            sendApiError(400, F("Missing command"));
            return;
          }
          constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1;
          const size_t cmdLen = strlen(words[5]);
          if ((cmdLen < 3) || (words[5][2] != '=')) {
            sendApiError(400, F("Invalid command format (expected XX=value)"));
            return;
          }
          if (cmdLen > kMaxCmdLen) {
            sendApiError(413, F("Command too long"));
            return;
          }
          addOTWGcmdtoqueue(words[5], static_cast<int>(cmdLen));
          httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
          httpServer.send(202, F("application/json"), F("{\"status\":\"queued\"}"));
        } else if (wc > 4 && strcmp_P(words[4], PSTR("autoconfigure")) == 0) {
          // POST /api/v2/otgw/autoconfigure — backward compat alias (prefer /discovery)
          if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
          httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
          httpServer.send(202, F("application/json"), F("{\"status\":\"accepted\"}"));
          doAutoConfigure();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("webhook")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("test")) == 0) {
          // POST /api/v2/webhook/test?state=on|off — fire webhook immediately for testing
          if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST")); return; }
          String stateParam = httpServer.arg(F("state"));
          if (!stateParam.length()) {
            sendApiError(400, F("Missing required 'state' parameter; expected on|1 or off|0"));
            return;
          }
          bool isOn  = (stateParam.equalsIgnoreCase("on")  || stateParam == "1");
          bool isOff = (stateParam.equalsIgnoreCase("off") || stateParam == "0");
          if (!isOn && !isOff) {
            sendApiError(400, F("Invalid state; expected on|1 or off|0"));
            return;
          }
          bool testOn = isOn;
          testWebhook(testOn);
          httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
          httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
        } else {
          sendApiNotFound(originalURI);
        }
      } else {
        sendApiNotFound(originalURI);
      }
    }
    else if (wc > 2 && (strcmp_P(words[2], PSTR("v0")) == 0 || strcmp_P(words[2], PSTR("v1")) == 0))
    { // v0 and v1 APIs removed — use /api/v2 (see ADR-035)
      sendApiError(410, F("API version removed; use /api/v2"));
    } else {
      sendApiNotFound(originalURI);
    }
  } else {
    sendApiNotFound(originalURI);
  }
} // processAPI()


//====[ implementing REST API ]====
static void buildOTGWValueResponse(uint_fast8_t msgid, char *out, size_t outSize) {
  PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
  if (OTlookupitem.type==ot_undef) {  //message is undefined, return error
    strlcpy(out, "{\"error\":\"message undefined: reserved for future use\"}", outSize);
    return;
  }

  RESTDebugTf(PSTR("%s = %s %s\r\n"), OTlookupitem.label, getOTGWValue(msgid), OTlookupitem.unit);
  if (OTlookupitem.type == ot_f88) {
    snprintf_P(out, outSize, PSTR("{\"label\":\"%s\",\"value\":%.3f,\"unit\":\"%s\"}"),
               OTlookupitem.label, atof(getOTGWValue(msgid)), OTlookupitem.unit);
  } else {// all other message types convert to integer
    snprintf_P(out, outSize, PSTR("{\"label\":\"%s\",\"value\":%d,\"unit\":\"%s\"}"),
               OTlookupitem.label, atoi(getOTGWValue(msgid)), OTlookupitem.unit);
  }
}

void sendOTGWvalue(int msgid){
  char sBuff[JSON_ENTRY_BUF];
  if (msgid < 0 || msgid > OT_MSGID_MAX) {
    strlcpy(sBuff, "{\"error\":\"message id: out of range\"}", sizeof(sBuff));
  } else {
    buildOTGWValueResponse(msgid, sBuff, sizeof(sBuff));
  }
  //RESTDebugTf(PSTR("Json = %s\r\n"), sBuff);
  //reply with json
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
}

void sendOTGWlabel(const char *msglabel){
  char sBuff[JSON_ENTRY_BUF];
  uint_fast8_t msgid;
  for (msgid = 0; msgid <= OT_MSGID_MAX; msgid++){
    PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
    if (strcasecmp(OTlookupitem.label, msglabel) == 0) break;
  }
  if (msgid > OT_MSGID_MAX){
    strlcpy(sBuff, "{\"error\":\"message id: reserved for future use\"}", sizeof(sBuff));
  } else {
    buildOTGWValueResponse(msgid, sBuff, sizeof(sBuff));
  }
  //RESTDebugTf(PSTR("Json = %s\r\n"), sBuff);
  //reply with json
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
}

//=======================================================================
// Helper Overloads for sendJsonOTmonObj and others to support PROGMEM functionality
// These ensure aggressive PROGMEM usage by allowing F() macros in calls
// while maintaining compatibility with the underlying implementation.
//=======================================================================

void sendStartJsonObj(const __FlashStringHelper* objName) {
  char buf[33];
  strncpy_P(buf, (PGM_P)objName, sizeof(buf));
  buf[sizeof(buf)-1] = 0;
  sendStartJsonObj(buf);
}

void sendEndJsonObj(const __FlashStringHelper* objName) {
  char buf[33];
  strncpy_P(buf, (PGM_P)objName, sizeof(buf));
  buf[sizeof(buf)-1] = 0;
  sendEndJsonObj(buf);
}

template <typename T>
void sendJsonOTmonObj(const __FlashStringHelper* label, T value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; // Buffer for label (longest ~25 chars)
  char unitBuf[10];  // Buffer for unit
  
  // Copy PROGMEM strings to temporary stack buffers
  strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf));
  labelBuf[sizeof(labelBuf)-1] = 0;
  
  strncpy_P(unitBuf, (PGM_P)unit, sizeof(unitBuf));
  unitBuf[sizeof(unitBuf)-1] = 0;
  
  // Call original function (assuming it takes const char*)
  sendJsonOTmonObj(labelBuf, value, unitBuf, lastupdated);
}

// Overload for mixed arguments (Flash label, dynamic unit)
template <typename T>
void sendJsonOTmonObj(const __FlashStringHelper* label, T value, const char* unit, unsigned long lastupdated) {
  char labelBuf[35];
  strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf));
  labelBuf[sizeof(labelBuf)-1] = 0;
  
  sendJsonOTmonObj(labelBuf, value, unit, lastupdated);
}

// Overload for mixed arguments (Dynamic label, Flash unit)
template <typename T>
void sendJsonOTmonObj(const char* label, T value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char unitBuf[10];
  strncpy_P(unitBuf, (PGM_P)unit, sizeof(unitBuf));
  unitBuf[sizeof(unitBuf)-1] = 0;
  
  sendJsonOTmonObj(label, value, unitBuf, lastupdated);
}

//=======================================================================
// Helpers for NEW Map-based JSON functions (sendJsonOTmonMapEntry)
//=======================================================================

template <typename T>
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, T value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; 
  char unitBuf[10];
  
  strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf));
  labelBuf[sizeof(labelBuf)-1] = 0;
  
  strncpy_P(unitBuf, (PGM_P)unit, sizeof(unitBuf));
  unitBuf[sizeof(unitBuf)-1] = 0;
  
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}

template <typename T>
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, T value, const char* unit, unsigned long lastupdated) {
  char labelBuf[35];
  strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf));
  labelBuf[sizeof(labelBuf)-1] = 0;
  
  sendJsonOTmonMapEntry(labelBuf, value, unit, lastupdated);
}

template <typename T>
void sendJsonOTmonMapEntry(const char* label, T value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char unitBuf[10];
  strncpy_P(unitBuf, (PGM_P)unit, sizeof(unitBuf));
  unitBuf[sizeof(unitBuf)-1] = 0;
  
  sendJsonOTmonMapEntry(label, value, unitBuf, lastupdated);
}

// Helpers for start/end map
void sendStartJsonMap(const __FlashStringHelper* objName) {
  char buf[33];
  strncpy_P(buf, (PGM_P)objName, sizeof(buf));
  buf[sizeof(buf)-1] = 0;
  sendStartJsonMap(buf);
}

void sendEndJsonMap(const __FlashStringHelper* objName) {
  char buf[33];
  strncpy_P(buf, (PGM_P)objName, sizeof(buf));
  buf[sizeof(buf)-1] = 0;
  sendEndJsonMap(buf);
}
//=======================================================================

void sendOTmonitorV2() 
{
  time_t now = time(nullptr); // needed for Dallas sensor display
  RESTDebugTln(F("sending OT monitor values (V2)...\r"));

  sendStartJsonMap(F("otmonitor"));

  // sendJsonOTmonObj(F("status hb"), byte_to_binary((OTcurrentSystemState.Statusflags>>8) & 0xFF),F(""), msglastupdated[OT_Statusflags]);
  // sendJsonOTmonObj(F("status lb"), byte_to_binary(OTcurrentSystemState.Statusflags & 0xFF),F(""), msglastupdated[OT_Statusflags]);

  sendJsonOTmonMapEntry(F("flamestatus"), CONOFF(isFlameStatus()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("chmodus"), CONOFF(isCentralHeatingActive()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("chenable"), CONOFF(isCentralHeatingEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("ch2modus"), CONOFF(isCentralHeating2Active()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("ch2enable"), CONOFF(isCentralHeating2enabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("dhwmode"), CONOFF(isDomesticHotWaterActive()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("dhwenable"), CONOFF(isDomesticHotWaterEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("diagnosticindicator"), CONOFF(isDiagnosticIndicator()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("faultindicator"), CONOFF(isFaultIndicator()),F(""), msglastupdated[OT_Statusflags]);
  
  sendJsonOTmonMapEntry(F("coolingmodus"), CONOFF(isCoolingEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonMapEntry(F("coolingactive"), CONOFF(isCoolingActive()),F(""), msglastupdated[OT_Statusflags]);  
  sendJsonOTmonMapEntry(F("otcactive"), CONOFF(isOutsideTemperatureCompensationActive()),F(""), msglastupdated[OT_Statusflags]);

  sendJsonOTmonMapEntry(F("servicerequest"), CONOFF(isServiceRequest()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonMapEntry(F("lockoutreset"), CONOFF(isLockoutReset()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonMapEntry(F("lowwaterpressure"), CONOFF(isLowWaterPressure()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonMapEntry(F("gasflamefault"), CONOFF(isGasFlameFault()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonMapEntry(F("airtemp"), CONOFF(isAirTemperature()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonMapEntry(F("waterovertemperature"), CONOFF(isWaterOverTemperature()),F(""), msglastupdated[OT_ASFflags]);
  

  sendJsonOTmonMapEntry(F("outsidetemperature"), OTcurrentSystemState.Toutside, F("°C"), msglastupdated[OT_Toutside]);
  sendJsonOTmonMapEntry(F("roomtemperature"), OTcurrentSystemState.Tr, F("°C"), msglastupdated[OT_Tr]);
  sendJsonOTmonMapEntry(F("roomsetpoint"), OTcurrentSystemState.TrSet, F("°C"), msglastupdated[OT_TrSet]);
  sendJsonOTmonMapEntry(F("remoteroomsetpoint"), OTcurrentSystemState.TrOverride, F("°C"), msglastupdated[OT_TrOverride]);
  sendJsonOTmonMapEntry(F("controlsetpoint"), OTcurrentSystemState.TSet,F("°C"), msglastupdated[OT_TSet]);
  sendJsonOTmonMapEntry(F("relmodlvl"), OTcurrentSystemState.RelModLevel,F("%"), msglastupdated[OT_RelModLevel]);
  sendJsonOTmonMapEntry(F("maxrelmodlvl"), OTcurrentSystemState.MaxRelModLevelSetting, F("%"), msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonMapEntry(F("boilertemperature"), OTcurrentSystemState.Tboiler, F("°C"), msglastupdated[OT_Tboiler]);
  sendJsonOTmonMapEntry(F("returnwatertemperature"), OTcurrentSystemState.Tret,F("°C"), msglastupdated[OT_Tret]);
  sendJsonOTmonMapEntry(F("dhwtemperature"), OTcurrentSystemState.Tdhw,F("°C"), msglastupdated[OT_Tdhw]);
  sendJsonOTmonMapEntry(F("dhwsetpoint"), OTcurrentSystemState.TdhwSet,F("°C"), msglastupdated[OT_TdhwSet]);
  sendJsonOTmonMapEntry(F("maxchwatersetpoint"), OTcurrentSystemState.MaxTSet,F("°C"), msglastupdated[OT_MaxTSet]);
  sendJsonOTmonMapEntry(F("chwaterpressure"), OTcurrentSystemState.CHPressure, F("bar"), msglastupdated[OT_CHPressure]);
  sendJsonOTmonMapEntry(F("oemdiagnosticcode"), OTcurrentSystemState.OEMDiagnosticCode, F(""), msglastupdated[OT_OEMDiagnosticCode]);
  sendJsonOTmonMapEntry(F("oemfaultcode"), OTcurrentSystemState.ASFflags & 0xFF, F(""), msglastupdated[OT_ASFflags]);

  if (settingS0COUNTERenabled) 
  {
    sendJsonOTmonMapEntry(F("s0powerkw"), OTGWs0powerkw , F("kW"), OTGWs0lasttime);
    sendJsonOTmonMapEntry(F("s0intervalcount"), OTGWs0pulseCount , F(""), OTGWs0lasttime);
    sendJsonOTmonMapEntry(F("s0totalcount"), OTGWs0pulseCountTot , F(""), OTGWs0lasttime);
  }
  sendJsonOTmonMapEntry(F("sensorsimulation"), bDebugSensorSimulation, F(""), now);
  if (settingGPIOSENSORSenabled || bDebugSensorSimulation) 
  {
    sendJsonOTmonMapEntry(F("numberofsensors"), DallasrealDeviceCount , F(""), now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      sendJsonOTmonMapEntryDallasTemp(strDeviceAddress, DallasrealDevice[i].tempC, F("°C"), DallasrealDevice[i].lasttime);
      // Labels now managed by Web UI via /dallas_labels.ini file (not sent in API)
    }
  }

  sendEndJsonMap(F("otmonitor"));
}

//=======================================================================
void sendDeviceInfo() 
{
  sendStartJsonObj(F("devinfo"));

  sendNestedJsonObj(F("author"), F("Robert van den Breemen"));
  sendNestedJsonObj(F("fwversion"), _SEMVER_FULL);
  sendNestedJsonObj(F("picavailable"), CBOOLEAN(bPICavailable));
  sendNestedJsonObj(F("picfwversion"), sPICfwversion);
  sendNestedJsonObj(F("picdeviceid"), sPICdeviceid);
  sendNestedJsonObj(F("picfwtype"), sPICtype);
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  sendNestedJsonObj(F("compiled"), cMsg);
  sendNestedJsonObj(F("hostname"), CSTR(settingHostname));
  sendNestedJsonObj(F("ipaddress"), CSTR(WiFi.localIP().toString()));
  sendNestedJsonObj(F("macaddress"), CSTR(WiFi.macAddress()));
  sendNestedJsonObj(F("freeheap"), ESP.getFreeHeap());
  sendNestedJsonObj(F("maxfreeblock"), ESP.getMaxFreeBlockSize());
  sendNestedJsonObj(F("chipid"), CSTR(String( ESP.getChipId(), HEX )));
  sendNestedJsonObj(F("coreversion"), CSTR(ESP.getCoreVersion()) );
  sendNestedJsonObj(F("sdkversion"),  ESP.getSdkVersion());
  sendNestedJsonObj(F("cpufreq"), ESP.getCpuFreqMHz());
  sendNestedJsonObj(F("sketchsize"), ESP.getSketchSize() );
  sendNestedJsonObj(F("freesketchspace"),  ESP.getFreeSketchSpace() );

  snprintf_P(cMsg, sizeof(cMsg), PSTR("%08X"), ESP.getFlashChipId());
  sendNestedJsonObj(F("flashchipid"), cMsg);  // flashChipId
  sendNestedJsonObj(F("flashchipsize"), (ESP.getFlashChipSize() / 1024.0f / 1024.0f));
  sendNestedJsonObj(F("flashchiprealsize"), (ESP.getFlashChipRealSize() / 1024.0f / 1024.0f));

  LittleFS.info(LittleFSinfo);
  sendNestedJsonObj(F("LittleFSsize"), floorf((LittleFSinfo.totalBytes / (1024.0f * 1024.0f))));

  sendNestedJsonObj(F("flashchipspeed"), floorf((ESP.getFlashChipSpeed() / 1000.0f / 1000.0f)));

  FlashMode_t ideMode = ESP.getFlashChipMode();
  sendNestedJsonObj(F("flashchipmode"), flashMode[ideMode]);
//   sendNestedJsonObj("boardtype",
// #if defined(ARDUINO_ESP8266_NODEMCU)
//      "ESP8266_NODEMCU"
// #elif defined(ARDUINO_ESP8266_GENERIC)
//      "ESP8266_GENERIC"
// #elif defined(ESP8266_ESP01)
//      "ESP8266_ESP01"
// #elif defined(ESP8266_ESP12)
//      "ESP8266_ESP12"
// #elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
//      "WEMOS_D1MINI"
// #else 
//      "Unknown board"
// #endif

//   );
  sendNestedJsonObj(F("ssid"), CSTR(WiFi.SSID()));
  sendNestedJsonObj(F("wifirssi"), WiFi.RSSI());
  sendNestedJsonObj(F("wifiquality"), signal_quality_perc_quad(WiFi.RSSI()));
  sendNestedJsonObj(F("wifiquality_text"), dBmtoQuality(WiFi.RSSI()));
  sendNestedJsonObj(F("ntpenable"), CBOOLEAN(settingNTPenable));
  sendNestedJsonObj(F("ntptimezone"), CSTR(settingNTPtimezone));
  sendNestedJsonObj(F("uptime"), upTime());
  sendNestedJsonObj(F("lastreset"), lastReset);
  sendNestedJsonObj(F("bootcount"), rebootCount);
  sendNestedJsonObj(F("mqttconnected"), CBOOLEAN(statusMQTTconnection));
  sendNestedJsonObj(F("thermostatconnected"), CBOOLEAN(bOTGWthermostatstate));
  sendNestedJsonObj(F("boilerconnected"), CBOOLEAN(bOTGWboilerstate));      
  sendNestedJsonObj(F("otgwmode"), bOTGWgatewaystateKnown ? CCONOFF(bOTGWgatewaystate) : "detecting");
  sendNestedJsonObj(F("otgwconnected"), CBOOLEAN(bOTGWonline));
  
  sendEndJsonObj(F("devinfo"));

} // sendDeviceInfo()

//=======================================================================
// Sends device info as JSON map (v2 format)
// Returns: {"device":{"author":"...","fwversion":"...",...}}
void sendDeviceInfoV2() 
{
  sendStartJsonMap(F("device"));

  sendJsonMapEntry(F("author"), F("Robert van den Breemen"));
  sendJsonMapEntry(F("fwversion"), _SEMVER_FULL);
  sendJsonMapEntry(F("picavailable"), bPICavailable);
  sendJsonMapEntry(F("picfwversion"), sPICfwversion);
  sendJsonMapEntry(F("picdeviceid"), sPICdeviceid);
  sendJsonMapEntry(F("picfwtype"), sPICtype);
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  sendJsonMapEntry(F("compiled"), cMsg);
  sendJsonMapEntry(F("hostname"), CSTR(settingHostname));
  sendJsonMapEntry(F("ipaddress"), CSTR(WiFi.localIP().toString()));
  sendJsonMapEntry(F("macaddress"), CSTR(WiFi.macAddress()));
  sendJsonMapEntry(F("freeheap"), ESP.getFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), ESP.getMaxFreeBlockSize());
  sendJsonMapEntry(F("chipid"), CSTR(String( ESP.getChipId(), HEX )));
  sendJsonMapEntry(F("coreversion"), CSTR(ESP.getCoreVersion()) );
  sendJsonMapEntry(F("sdkversion"),  ESP.getSdkVersion());
  sendJsonMapEntry(F("cpufreq"), ESP.getCpuFreqMHz());
  sendJsonMapEntry(F("sketchsize"), ESP.getSketchSize() );
  sendJsonMapEntry(F("freesketchspace"),  ESP.getFreeSketchSpace() );

  snprintf_P(cMsg, sizeof(cMsg), PSTR("%08X"), ESP.getFlashChipId());
  sendJsonMapEntry(F("flashchipid"), cMsg);
  sendJsonMapEntry(F("flashchipsize"), (ESP.getFlashChipSize() / 1024.0f / 1024.0f));
  sendJsonMapEntry(F("flashchiprealsize"), (ESP.getFlashChipRealSize() / 1024.0f / 1024.0f));

  LittleFS.info(LittleFSinfo);
  sendJsonMapEntry(F("LittleFSsize"), floorf((LittleFSinfo.totalBytes / (1024.0f * 1024.0f))));

  sendJsonMapEntry(F("flashchipspeed"), floorf((ESP.getFlashChipSpeed() / 1000.0f / 1000.0f)));

  FlashMode_t ideMode = ESP.getFlashChipMode();
  sendJsonMapEntry(F("flashchipmode"), flashMode[ideMode]);
  sendJsonMapEntry(F("ssid"), CSTR(WiFi.SSID()));
  sendJsonMapEntry(F("wifirssi"), WiFi.RSSI());
  sendJsonMapEntry(F("wifiquality"), signal_quality_perc_quad(WiFi.RSSI()));
  sendJsonMapEntry(F("wifiquality_text"), dBmtoQuality(WiFi.RSSI()));
  sendJsonMapEntry(F("ntpenable"), settingNTPenable);
  sendJsonMapEntry(F("ntptimezone"), CSTR(settingNTPtimezone));
  sendJsonMapEntry(F("uptime"), upTime());
  sendJsonMapEntry(F("lastreset"), lastReset);
  sendJsonMapEntry(F("bootcount"), rebootCount);
  sendJsonMapEntry(F("mqttconnected"), statusMQTTconnection);
  sendJsonMapEntry(F("thermostatconnected"), bOTGWthermostatstate);
  sendJsonMapEntry(F("boilerconnected"), bOTGWboilerstate);      
  sendJsonMapEntry(F("otgwmode"), bOTGWgatewaystateKnown ? CCONOFF(bOTGWgatewaystate) : "detecting");
  sendJsonMapEntry(F("otgwconnected"), bOTGWonline);
  
  sendEndJsonMap(F("device"));

} // sendDeviceInfoV2()

//=======================================================================
// Sends health status as JSON object (map format)
// Returns: {"health": {"status": "UP", "uptime": "...", ...}}
void sendHealth() 
{
  sendStartJsonMap(F("health"));

  updateLittleFSStatus(F("/.health"));
  sendJsonMapEntry(F("status"), LittleFSmounted ? F("UP") : F("DEGRADED"));
  sendJsonMapEntry(F("uptime"), upTime());
  sendJsonMapEntry(F("heap"), ESP.getFreeHeap());
  sendJsonMapEntry(F("wifirssi"), WiFi.RSSI());
  sendJsonMapEntry(F("mqttconnected"), CBOOLEAN(statusMQTTconnection));
  sendJsonMapEntry(F("otgwconnected"), CBOOLEAN(bOTGWonline));
  sendJsonMapEntry(F("picavailable"), CBOOLEAN(bPICavailable));
  sendJsonMapEntry(F("littlefsMounted"), CBOOLEAN(LittleFSmounted));
  
  sendEndJsonMap(F("health"));

} // sendHealth()


//=======================================================================
void sendPICFlashStatus()
{
  // Minimal PIC flash status endpoint for polling during flash
  // Returns: {"flashstatus":{"flashing":true|false,"progress":0-100,"filename":"...","error":"..."}}
  sendStartJsonMap(F("flashstatus"));
  sendJsonMapEntry(F("flashing"), isPICFlashing);
  sendJsonMapEntry(F("progress"), currentPICFlashProgress);
  sendJsonMapEntry(F("filename"), currentPICFlashFile);
  sendJsonMapEntry(F("error"), errorupgrade);
  sendEndJsonMap(F("flashstatus"));
} // sendPICFlashStatus()

//=======================================================================
void sendPICUpdateCheck()
{
  // On-demand PIC firmware update check.
  // Only called when the user opens the PIC firmware tab — never on a timer.
  // Makes an outbound HTTP HEAD request to otgw.tclcode.com.
  String latest = "";
  if (strcmp_P(sPICdeviceid, PSTR("unknown")) != 0 && sPICdeviceid[0] != '\0') {
    String picFile;
    if (strcmp_P(sPICtype, PSTR("diagnose")) == 0) {
      picFile = F("diagnose.hex");
    } else if (strcmp_P(sPICtype, PSTR("interface")) == 0) {
      picFile = F("interface.hex");
    } else {
      picFile = F("gateway.hex");
    }
    latest = checkforupdatepic(picFile);
  }
  bool updateAvailable = (latest.length() > 0 && latest != String(sPICfwversion));
  sendStartJsonMap(F("pic_update"));
  sendJsonMapEntry(F("current"), sPICfwversion);
  sendJsonMapEntry(F("latest"), latest.c_str());
  sendJsonMapEntry(F("update_available"), updateAvailable);
  sendEndJsonMap(F("pic_update"));
} // sendPICUpdateCheck()

//=======================================================================
void sendFilesystemHashCheck()
{
  // Read the hash stored in LittleFS and compare with the compiled-in firmware hash.
  // Uses the cached getFilesystemHash() — safe to call from an HTTP handler.
  String fsHash = getFilesystemHash();
  bool match = (fsHash.length() > 0 &&
                strcasecmp(fsHash.c_str(), _VERSION_GITHASH) == 0);
  sendStartJsonMap(F("filesystem_check"));
  sendJsonMapEntry(F("match"), match);
  sendJsonMapEntry(F("fw_hash"), _VERSION_GITHASH);
  sendJsonMapEntry(F("fs_hash"), fsHash.c_str());
  sendEndJsonMap(F("filesystem_check"));
} // sendFilesystemHashCheck()

//=======================================================================
void sendFlashStatus()
{
  // Unified flash status endpoint - minimal response with only fields used by frontend
  // Returns: {"flashstatus":{"flashing":bool,"pic_flashing":bool,"pic_progress":0-100,"pic_filename":"...","pic_error":"..."}}
  sendStartJsonMap(F("flashstatus"));
  sendJsonMapEntry(F("flashing"), isFlashing());
  sendJsonMapEntry(F("pic_flashing"), isPICFlashing);
  sendJsonMapEntry(F("pic_progress"), currentPICFlashProgress);
  sendJsonMapEntry(F("pic_filename"), currentPICFlashFile);
  sendJsonMapEntry(F("pic_error"), errorupgrade);
  sendEndJsonMap(F("flashstatus"));
} // sendFlashStatus()


//=======================================================================
void sendDeviceTime() 
{
  char buf[50];
  
  sendStartJsonObj(F("devtime"));
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf_P(buf, sizeof(buf), PSTR("%04d-%02d-%02d %02d:%02d:%02d"), myTime.year(), myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second());
  sendNestedJsonObj(F("dateTime"), buf); 
  sendNestedJsonObj(F("epoch"), (int)now);
  sendNestedJsonObj(F("message"), sMessage);
  sendNestedJsonObj(F("psmode"), CBOOLEAN(bPSmode));

  sendEndJsonObj(F("devtime"));

} // sendDeviceTime()

//=======================================================================
void sendDeviceTimeV2() 
{
  char buf[50];
  
  sendStartJsonMap(F("devtime"));
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf_P(buf, sizeof(buf), PSTR("%04d-%02d-%02d %02d:%02d:%02d"), myTime.year(), myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second());
  sendJsonMapEntry(F("dateTime"), buf); 
  sendJsonMapEntry(F("epoch"), (int)now);
  sendJsonMapEntry(F("message"), sMessage);
  sendJsonMapEntry(F("psmode"), bPSmode);
  sendJsonMapEntry(F("freeheap"), ESP.getFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), ESP.getMaxFreeBlockSize());

  sendEndJsonMap(F("devtime"));

} // sendDeviceTimeV2()

//=======================================================================
void sendDeviceSettings() 
{
  RESTDebugTln(F("sending device settings ...\r"));

  sendStartJsonMap(F("settings"));

  //sendJsonSettingObj("string",   settingString,   "p", sizeof(settingString)-1);  
  //sendJsonSettingObj("string",   settingString,   "s", sizeof(settingString)-1);
  //sendJsonSettingObj("float",    settingFloat,    "f", 0, 10,  5);
  //sendJsonSettingObj("intager",  settingInteger , "i", 2, 60);

  sendJsonSettingObj(F("hostname"), CSTR(settingHostname), "s", 32);
  sendJsonSettingObj(F("mqttenable"), settingMQTTenable, "b");
  sendJsonSettingObj(F("mqttbroker"), CSTR(settingMQTTbroker), "s", 32);
  sendJsonSettingObj(F("mqttbrokerport"), settingMQTTbrokerPort, "i", 0, 65535);
  sendJsonSettingObj(F("mqttuser"), CSTR(settingMQTTuser), "s", 32);
  sendJsonSettingObj(F("mqttpasswd"), "notthepassword", "p", 100);
  sendJsonSettingObj(F("mqtttoptopic"), CSTR(settingMQTTtopTopic), "s", 15);
  sendJsonSettingObj(F("mqtthaprefix"), CSTR(settingMQTThaprefix), "s", 20);
  sendJsonSettingObj(F("mqttharebootdetection"), settingMQTTharebootdetection, "b");
  sendJsonSettingObj(F("mqttuniqueid"), CSTR(settingMQTTuniqueid), "s", 20);
  sendJsonSettingObj(F("mqttotmessage"), settingMQTTOTmessage, "b");
  sendJsonSettingObj(F("mqttinterval"), settingMQTTinterval, "i", 0, 3600);
  sendJsonSettingObj(F("mqttseparatesources"), settingMQTTSeparateSources, "b");
  sendJsonSettingObj(F("ntpenable"), settingNTPenable, "b");
  sendJsonSettingObj(F("ntptimezone"), CSTR(settingNTPtimezone), "s", 50);
  sendJsonSettingObj(F("ntphostname"), CSTR(settingNTPhostname), "s", 50);
  sendJsonSettingObj(F("ntpsendtime"), settingNTPsendtime, "b");
  sendJsonSettingObj(F("ledblink"), settingLEDblink, "b");
  sendJsonSettingObj(F("darktheme"), settingDarkTheme, "b");
  sendJsonSettingObj(F("ui_autoscroll"), settingUIAutoScroll, "b");
  sendJsonSettingObj(F("ui_timestamps"), settingUIShowTimestamp, "b");
  sendJsonSettingObj(F("ui_capture"), settingUICaptureMode, "b");
  sendJsonSettingObj(F("ui_autoscreenshot"), settingUIAutoScreenshot, "b");
  sendJsonSettingObj(F("ui_autodownloadlog"), settingUIAutoDownloadLog, "b");
  sendJsonSettingObj(F("ui_autoexport"), settingUIAutoExport, "b");
  sendJsonSettingObj(F("ui_graphtimewindow"), settingUIGraphTimeWindow, "i", 0, 1440);
  sendJsonSettingObj(F("gpiosensorsenabled"), settingGPIOSENSORSenabled, "b");
  sendJsonSettingObj(F("gpiosensorslegacyformat"), settingGPIOSENSORSlegacyformat, "b");
  sendJsonSettingObj(F("gpiosensorspin"), settingGPIOSENSORSpin, "i", 0, 16);
  sendJsonSettingObj(F("gpiosensorsinterval"), settingGPIOSENSORSinterval, "i", 5, 65535);
  sendJsonSettingObj(F("s0counterenabled"), settingS0COUNTERenabled, "b");
  sendJsonSettingObj(F("s0counterpin"), settingS0COUNTERpin, "i", 1, 16);
  sendJsonSettingObj(F("s0counterdebouncetime"), settingS0COUNTERdebouncetime, "i", 0, 1000);
  sendJsonSettingObj(F("s0counterpulsekw"), settingS0COUNTERpulsekw, "i", 1, 5000);
  sendJsonSettingObj(F("s0counterinterval"), settingS0COUNTERinterval, "i", 5, 65535);
  sendJsonSettingObj(F("gpiooutputsenabled"), settingGPIOOUTPUTSenabled, "b");
  sendJsonSettingObj(F("gpiooutputspin"), settingGPIOOUTPUTSpin, "i", 0, 16);
  sendJsonSettingObj(F("gpiooutputstriggerbit"), settingGPIOOUTPUTStriggerBit, "i", 0, 16);
  sendJsonSettingObj(F("otgwcommandenable"), settingOTGWcommandenable, "b");
  sendJsonSettingObj(F("otgwcommands"), CSTR(settingOTGWcommands), "s", 128);
  sendJsonSettingObj(F("webhookenable"), settingWebhookEnabled, "b");
  sendJsonSettingObj(F("webhookurlon"), CSTR(settingWebhookURLon), "s", 100);
  sendJsonSettingObj(F("webhookurloff"), CSTR(settingWebhookURLoff), "s", 100);
  sendJsonSettingObj(F("webhooktriggerbit"), settingWebhookTriggerBit, "i", 0, 15);
  sendJsonSettingObj(F("webhookpayload"), CSTR(settingWebhookPayload), "s", 200);
  sendJsonSettingObj(F("webhookcontenttype"), CSTR(settingWebhookContentType), "s", 31);

  sendEndJsonMap(F("settings"));

} // sendDeviceSettings()


//=======================================================================
void postSettings()
{
  //------------------------------------------------------------
  // json string: {"name":"settingInterval","value":9}
  // json string: {"name":"settingHostname","value":"abc"}
  // json string: {"name":"darktheme","value":true}
  //------------------------------------------------------------
  char field[64] = "";
  char newValue[150] = "";
  const String& body = httpServer.arg(0);
  if (!parseSettingRequestBody(body.c_str(), field, sizeof(field), newValue, sizeof(newValue))) {
    RESTDebugTln(F("postSettings JSON parse error"));
    httpServer.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  if (newValue[0] != '\0') {
    RESTDebugTf(PSTR("--> field[%s] => newValue[%s]\r\n"), field, newValue);
    updateSetting(field, newValue);
    // Synchronous flush: persist to flash NOW so the 200 OK is truthful.
    // The deferred timer still handles MQTT/NTP command updates, but HTTP
    // saves must be durable before we confirm success to the browser.
    flushSettings();
    httpServer.send(200, F("application/json"), httpServer.arg(0));
  } else {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Missing value\"}"));
  }

} // postSettings()


//====[ Dallas sensor label file operations ]====

// Get single Dallas sensor label from file by address
// GET /api/v1/sensors/label?address=28FF64D1841703F1
// Get all Dallas sensor labels from file (bulk read)
void getDallasLabels() {
  File labelsFile = LittleFS.open(F("/dallas_labels.ini"), "r");
  
  if (!labelsFile) {
    // No file exists - return empty JSON object
    httpServer.send(200, F("application/json"), F("{}"));
    return;
  }
  
  // Stream the file content directly to response
  httpServer.streamFile(labelsFile, F("application/json"));
  labelsFile.close();
}

// Update all Dallas sensor labels in file (bulk operation)
void updateAllDallasLabels() {
  // Parse JSON body from request
  const size_t MAX_DALLAS_LABELS_BODY_SIZE = 4096;
  const String& body = httpServer.arg(F("plain"));

  if (body.length() == 0) {
    httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Empty request body\"}"));
    return;
  }
  if (body.length() > MAX_DALLAS_LABELS_BODY_SIZE) {
    httpServer.send(413, F("application/json"), F("{\"success\":false,\"error\":\"Request body too large\"}"));
    return;
  }

  if (!isValidDallasLabelJson(body.c_str())) {
    httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON format\"}"));
    return;
  }

  // Write directly to file
  File labelsFile = LittleFS.open(F("/dallas_labels.ini"), "w");
  if (!labelsFile) {
    httpServer.send(500, F("application/json"), F("{\"success\":false,\"error\":\"Failed to open file for writing\"}"));
    return;
  }

  labelsFile.print(body);
  labelsFile.close();

  httpServer.send(200, F("application/json"), F("{\"success\":true}"));
}

//====================================================
void sendApiNotFound(const char *URI)
{
  // For API routes, return JSON 404 (ADR-035: RESTful compliance)
  if (strncmp_P(URI, PSTR("/api/"), 5) == 0) {
    sendApiError(404, F("Endpoint not found"));
    return;
  }

  // For non-API routes, return HTML 404 (legacy behavior)
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send_P(404, PSTR("text/html; charset=UTF-8"), PSTR("<!DOCTYPE HTML><html><head>"));

  httpServer.sendContent_P(PSTR("<style>body { background-color: lightgray; font-size: 15pt;}</style></head><body>"));
  httpServer.sendContent_P(PSTR("<h1>OTGW firmware</h1><b1>"));
  httpServer.sendContent_P(PSTR("<br>[<b>"));
  // HTML-escape URI to prevent reflected XSS
  String escapedURI = String(URI);
  escapedURI.replace(F("&"), F("&amp;"));
  escapedURI.replace(F("<"), F("&lt;"));
  escapedURI.replace(F(">"), F("&gt;"));
  escapedURI.replace(F("\""), F("&quot;"));
  escapedURI.replace(F("'"), F("&#39;"));
  httpServer.sendContent(escapedURI);
  httpServer.sendContent_P(PSTR("</b>] is not a valid "));
  httpServer.sendContent_P(PSTR("</body></html>\r\n"));

} // sendApiNotFound()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
