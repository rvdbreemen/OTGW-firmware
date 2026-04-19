/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v2.0.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <string.h>
#include <ctype.h>

#define RESTDebugTln(...) ({ if (state.debug.bRestAPI) DebugTln(__VA_ARGS__);    })
#define RESTDebugln(...)  ({ if (state.debug.bRestAPI) Debugln(__VA_ARGS__);    })
#define RESTDebugTf(...)  ({ if (state.debug.bRestAPI) DebugTf(__VA_ARGS__);    })
#define RESTDebugf(...)   ({ if (state.debug.bRestAPI) Debugf(__VA_ARGS__);    })
#define RESTDebugT(...)   ({ if (state.debug.bRestAPI) DebugT(__VA_ARGS__);    })
#define RESTDebug(...)    ({ if (state.debug.bRestAPI) Debug(__VA_ARGS__);    })

// REST API debug: tracks last response status for the access-log line.
// Set by sendApiError/sendApiOptions; defaults to 200 before each handler.
static int16_t restResponseStatus = 0;

// Zero-allocation HTTP method to string (returns PROGMEM pointer).
// Replaces strHTTPmethod() which returned String (heap allocation per call).
static const char* httpMethodToStr(HTTPMethod m) {
  switch (m) {
    case HTTP_GET:     return "GET";
    case HTTP_POST:    return "POST";
    case HTTP_PUT:     return "PUT";
    case HTTP_PATCH:   return "PATCH";
    case HTTP_DELETE:  return "DELETE";
    case HTTP_OPTIONS: return "OPTIONS";
    case HTTP_HEAD:    return "HEAD";
    default:           return "?";
  }
}

// H3: Send dynamic CORS Allow-Origin header echoing the request Origin,
// instead of a wildcard. Only sends the header when Origin is present.
static void sendCorsOriginHeader() {
  // httpServer.header() returns const String& (no allocation); strlcpy into a
  // static buffer keeps this allocation-free on the request hot path.
  static char originBuf[128];
  strlcpy(originBuf, httpServer.header(F("Origin")).c_str(), sizeof(originBuf));
  if (originBuf[0] != '\0') {
    httpServer.sendHeader(F("Access-Control-Allow-Origin"), originBuf);
  }
}

//=======================================================================
// RESTful v2 API: Consistent JSON error response helper (ADR-035)
// Returns: {"error":{"status":N,"message":"..."}}
//=======================================================================
static void sendApiError(int httpCode, const __FlashStringHelper* message) {
  restResponseStatus = httpCode;
  char jsonBuff[200];
  snprintf_P(jsonBuff, sizeof(jsonBuff),
    PSTR("{\"error\":{\"status\":%d,\"message\":\"%S\"}}"),
    httpCode, (PGM_P)message);
  sendCorsOriginHeader();
  httpServer.send(httpCode, F("application/json"), jsonBuff);
}

// T44: 405 responses with RFC 7231 §6.5.5 Allow header
static void sendApiMethodNotAllowed(const __FlashStringHelper* allowedMethods) {
  httpServer.sendHeader(F("Allow"), allowedMethods);
  sendApiError(405, F("Method not allowed"));
}

//=======================================================================
// CSRF same-origin helper for admin operations (ADR-054)
// Returns true if the request appears to come from the same origin.
// Permissive for legacy/non-browser clients that don't send Origin/Referer.
static bool isSameOriginRequest() {
  // Use static buffers to avoid String heap allocations (H6).
  // Sizes: Origin/Referer up to 128 chars, Host up to 64 chars.
  static char originBuf[128];
  static char hostBuf[64];

  strlcpy(originBuf, httpServer.header(F("Origin")).c_str(), sizeof(originBuf));
  if (originBuf[0] == '\0') {
    strlcpy(originBuf, httpServer.header(F("Referer")).c_str(), sizeof(originBuf));
  }
  if (originBuf[0] == '\0') return true;  // no origin header — allow (non-browser client)

  strlcpy(hostBuf, httpServer.hostHeader().c_str(), sizeof(hostBuf));
  if (hostBuf[0] == '\0') return true;  // can't validate without Host header

  const char* schemeSep = strstr(originBuf, "://");
  if (schemeSep == nullptr) {
    RESTDebugTln(F("REST CSRF: malformed Origin/Referer, rejecting"));
    return false;
  }
  const char* hostStart = schemeSep + 3;
  const char* pathStart = strchr(hostStart, '/');
  size_t hostLen = pathStart ? (size_t)(pathStart - hostStart) : strlen(hostStart);
  size_t reqHostLen = strlen(hostBuf);
  if (hostLen != reqHostLen || strncasecmp(hostStart, hostBuf, hostLen) != 0) {
    RESTDebugTf(PSTR("REST CSRF: origin host != request host '%s'\r\n"), hostBuf);
    return false;
  }
  return true;
}

// HTTP Basic Auth helper (ADR-054)
// Returns true if request is authorized (no password set, or valid credentials
// AND same-origin CSRF check passes).
// Sends 401 or 403 and returns false if auth/CSRF fails.
bool checkHttpAuth() {
  if (settings.sHTTPpasswd[0] == '\0') return true;  // auth disabled

  if (httpServer.method() == HTTP_OPTIONS) return true;  // allow CORS preflight

  if (!httpServer.authenticate("admin", settings.sHTTPpasswd)) {
    httpServer.requestAuthentication();
    return false;
  }

  if (!isSameOriginRequest()) {
    sendApiError(403, F("CSRF protection: invalid origin"));
    return false;
  }

  return true;
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

//=======================================================================
// v2 API Route Dispatch Table (ADR-050)
// Each resource gets its own handler function. Adding a new endpoint
// requires: (1) add handler function, (2) add one entry to kV2Routes[].
//=======================================================================

#define API_MAX_WORDS  8
#define API_WORD_LEN   32

static void handleHealth(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleSettings(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleSensors(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleDevice(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleFlash(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handlePic(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleFirmware(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleFilesystem(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleSimulate(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleOtgw(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleWebhook(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleSAT(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);

void sendOTValue(int msgid);
void sendOTLabel(const char *msglabel);
void sendOTmonitorV2();
void sendFilesystemHashCheck();
void sendApiNotFound(const char *URI);

// Common OPTIONS/CORS preflight response for all v2 endpoints
static void sendApiOptions() {
  sendCorsOriginHeader();
  httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, OPTIONS"));
  httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
  httpServer.sendHeader(F("Access-Control-Max-Age"), F("86400"));
  httpServer.send(204);
}

// Helper to queue a command from URL segment or request body, with validation
static void handleCommandSubmit(const char* cmdStr) {
  if (!hasOTCommandInterface()) {
    sendApiError(503, F("No OT command interface detected - commands disabled"));
    return;
  }
  if (!cmdStr || cmdStr[0] == '\0') {
    sendApiError(400, F("Missing command"));
    return;
  }
  constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1;
  const size_t cmdLen = strlen(cmdStr);
  // OTGW commands are two letters followed by '=' and a value.
  // Reject non-alphabetic prefixes to prevent malformed bytes reaching the command queue (review I2).
  if ((cmdLen < 3) || (cmdStr[2] != '=') ||
      !isalpha(static_cast<unsigned char>(cmdStr[0])) ||
      !isalpha(static_cast<unsigned char>(cmdStr[1]))) {
    sendApiError(400, F("Invalid command format (expected LL=value)"));
    return;
  }
  if (cmdLen > kMaxCmdLen) {
    sendApiError(413, F("Command too long"));
    return;
  }
  addCommandToQueue(cmdStr, static_cast<int>(cmdLen));
  sendCorsOriginHeader();
  httpServer.send(202, F("application/json"), F("{\"status\":\"queued\"}"));
}

static void sendSimulationStatus() {
  static const char kOTGWSimulationFile[] PROGMEM = "/otgw_simulation.log";
  char jsonBuf[160];
  snprintf_P(jsonBuf, sizeof(jsonBuf),
             PSTR("{\"simulation\":{\"active\":%s,\"file\":\"%S\",\"interval_ms\":%lu}}"),
             state.debug.bOTGWSimulation ? "true" : "false",
             reinterpret_cast<PGM_P>(kOTGWSimulationFile),
             static_cast<unsigned long>(state.debug.iOTGWSimulationIntervalMs));
  sendCorsOriginHeader();
  httpServer.send(200, F("application/json"), jsonBuf);
}

static void setOTGWSimulationEnabled(bool enabled) {
  state.debug.bOTGWSimulation = enabled;
  state.debug.iOTGWSimulationNextDueMs = 0;
  if (enabled) {
    sendEventToWebSocket_P('*', PSTR("OTGW simulation enabled [replay active]"));
  } else {
    sendEventToWebSocket_P('*', PSTR("OTGW simulation disabled [live serial resumed]"));
  }
}

//=== Resource handler functions ===

static void handleHealth(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  sendHealth();
}

static void handleSettings(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  // Auth required for all methods (GET includes sensitive config)
  if (!checkHttpAuth()) return;
  if (method == HTTP_POST || method == HTTP_PUT) {
    postSettings();
  } else if (method == HTTP_GET) {
    sendDeviceSettings();
  } else {
    sendApiMethodNotAllowed(F("GET, POST, PUT"));
  }
}

static void sendSensorStatus() {
  time_t now = time(nullptr);
  sendStartJsonMap(F("sensors"));

  // Dallas temperature sensors
  sendJsonMapEntry(F("dallas_enabled"), settings.sensors.bEnabled);
  sendJsonMapEntry(F("dallas_detected"), bSensorsDetected);
  sendJsonMapEntry(F("dallas_count"), (int32_t)DallasrealDeviceCount);
  sendJsonMapEntry(F("dallas_gpio"), (int32_t)settings.sensors.iPin);
  sendJsonMapEntry(F("dallas_poll_sec"), (int32_t)settings.sensors.iInterval);
  sendJsonMapEntry(F("simulated"), state.debug.bSensorSim);

  // Individual sensor readings
  if (bSensorsDetected || state.debug.bSensorSim) {
    // Start "devices" sub-object — use chunked JSON
    httpServer.sendContent_P(PSTR(",\r\n  \"devices\": {"));
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char *addr = getDallasAddress(DallasrealDevice[i].addr);
      if (!addr) continue;
      char entry[100];
      snprintf_P(entry, sizeof(entry),
                 PSTR("%s\r\n    \"%s\": {\"temp\": %4.1f, \"epoch\": %u}"),
                 (i > 0) ? "," : "",
                 addr, DallasrealDevice[i].tempC, (uint32_t)DallasrealDevice[i].lasttime);
      httpServer.sendContent(entry);
    }
    httpServer.sendContent_P(PSTR("\r\n  }"));
  }

  // S0 pulse counter
  httpServer.sendContent_P(PSTR(",\r\n  \"s0\": {"));
  {
    char s0buf[120];
    snprintf_P(s0buf, sizeof(s0buf),
               PSTR("\r\n    \"enabled\": %s, \"gpio\": %d, \"poll_sec\": %d,"
                    "\r\n    \"pulses\": %u, \"total\": %lu, \"power_kw\": %4.3f, \"epoch\": %u"
                    "\r\n  }"),
               settings.s0.bEnabled ? "true" : "false",
               settings.s0.iPin, settings.s0.iInterval,
               OTGWs0pulseCount, (unsigned long)OTGWs0pulseCountTot,
               OTGWs0powerkw, (uint32_t)OTGWs0lasttime);
    httpServer.sendContent(s0buf);
  }

  sendEndJsonMap(F("sensors"));
}

static void handleSensors(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (wc == 4 || (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0)) {
    // GET /api/v2/sensors or GET /api/v2/sensors/status
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    sendSensorStatus();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("labels")) == 0) {
    if (method == HTTP_GET) {
      getDallasLabels();
    } else if (method == HTTP_POST || method == HTTP_PUT) {
      updateAllDallasLabels();
    } else {
      sendApiMethodNotAllowed(F("GET, POST, PUT"));
    }
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleDevice(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (wc > 4 && strcmp_P(words[4], PSTR("info")) == 0) {
    sendDeviceInfoV2();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("time")) == 0) {
    sendDeviceTimeV2();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("crashlog")) == 0) {
    sendDeviceCrashLog();
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleFlash(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0) {
    sendFlashStatus();
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handlePic(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (!isPICEnabled()) { sendApiError(503, F("No PIC detected - PIC functions disabled")); return; }
  if (wc > 4 && strcmp_P(words[4], PSTR("flash-status")) == 0) {
    sendPICFlashStatus();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("update-check")) == 0) {
    sendPICUpdateCheck();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("settings")) == 0) {
    sendPICsettings();
  } else {
    sendApiNotFound(originalURI);
  }
}

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
static void handleOTDirect(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (!isOTDirectEnabled()) { sendApiError(503, F("No OT-direct hardware - OTGW32 functions disabled")); return; }

  if (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0) {
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    sendOTDirectStatus();
  }
  // POST /api/v2/otdirect/mode?mode=gateway|monitor|bypass
  else if (wc > 4 && strcmp_P(words[4], PSTR("mode")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST")); return; }
    if (!httpServer.hasArg("mode")) { sendApiError(400, F("Missing 'mode' parameter")); return; }
    String modeStr = httpServer.arg("mode");
    if (modeStr == F("gateway"))       addCommandToQueue("GW=1", 4, true);
    else if (modeStr == F("monitor"))  addCommandToQueue("GW=M", 4, true);
    else if (modeStr == F("bypass"))   addCommandToQueue("GW=0", 4, true);
    else if (modeStr == F("master"))   addCommandToQueue("GW=S", 4, true);
    else if (modeStr == F("loopback")) addCommandToQueue("GW=L", 4, true);
    else { sendApiError(400, F("Invalid mode. Use: gateway, monitor, bypass, master, loopback")); return; }
    // Return current status (mode will be updated by the time response renders)
    sendOTDirectStatus();
  }
  // GET /api/v2/otdirect/settings — read OTD settings
  // POST /api/v2/otdirect/settings?... — update
  else if (wc > 4 && strcmp_P(words[4], PSTR("settings")) == 0) {
    if (method == HTTP_GET) {
      sendStartJsonMap(F("otdirect_settings"));
      sendJsonMapEntry(F("mode"),            (int)settings.otd.iMode);
      sendJsonMapEntry(F("setback_temp"),    settings.otd.fSetbackTemp);
      sendJsonMapEntry(F("setback_timeout"), (int)settings.otd.iSetbackTimeout);
      // TASK-183: PI room compensation + heating curve
      sendJsonMapEntry(F("ch_mode"),         (int)settings.otd.iCHMode);
      sendJsonMapEntry(F("flow_temp"),       settings.otd.fFlowTemp);
      sendJsonMapEntry(F("flow_max"),        settings.otd.fFlowMax);
      sendJsonMapEntry(F("room_setpoint"),   settings.otd.fRoomSetpoint);
      sendJsonMapEntry(F("gradient"),        settings.otd.fGradient);
      sendJsonMapEntry(F("exponent"),        settings.otd.fExponent);
      sendJsonMapEntry(F("offset"),          settings.otd.fOffset);
      sendJsonMapEntry(F("room_comp"),       settings.otd.bRoomCompEnabled);
      sendJsonMapEntry(F("kp"),              settings.otd.fKp);
      sendJsonMapEntry(F("ki"),              settings.otd.fKi);
      sendJsonMapEntry(F("kboost"),          settings.otd.fKboost);
      sendEndJsonMap(F("otdirect_settings"));
    } else if (method == HTTP_POST || method == HTTP_PUT) {
      if (httpServer.hasArg("setbacktemp"))    updateSetting("OTDsetbacktemp", httpServer.arg("setbacktemp").c_str());
      if (httpServer.hasArg("setbacktimeout")) updateSetting("OTDsetbacktimeout", httpServer.arg("setbacktimeout").c_str());
      // TASK-183: PI room compensation + heating curve settings
      if (httpServer.hasArg("chmode"))       updateSetting("OTDchmode", httpServer.arg("chmode").c_str());
      if (httpServer.hasArg("flowtemp"))     updateSetting("OTDflowtemp", httpServer.arg("flowtemp").c_str());
      if (httpServer.hasArg("flowmax"))      updateSetting("OTDflowmax", httpServer.arg("flowmax").c_str());
      if (httpServer.hasArg("roomsetpoint")) updateSetting("OTDroomsetpoint", httpServer.arg("roomsetpoint").c_str());
      if (httpServer.hasArg("gradient"))     updateSetting("OTDgradient", httpServer.arg("gradient").c_str());
      if (httpServer.hasArg("exponent"))     updateSetting("OTDexponent", httpServer.arg("exponent").c_str());
      if (httpServer.hasArg("offset"))       updateSetting("OTDoffset", httpServer.arg("offset").c_str());
      if (httpServer.hasArg("roomcomp"))     updateSetting("OTDroomcomp", httpServer.arg("roomcomp").c_str());
      if (httpServer.hasArg("kp"))           updateSetting("OTDkp", httpServer.arg("kp").c_str());
      if (httpServer.hasArg("ki"))           updateSetting("OTDki", httpServer.arg("ki").c_str());
      if (httpServer.hasArg("kboost"))       updateSetting("OTDkboost", httpServer.arg("kboost").c_str());
      sendOTDirectStatus();
    } else {
      sendApiMethodNotAllowed(F("GET, POST"));
    }
  }
  // GET /api/v2/otdirect/overrides — list all active overrides
  // POST /api/v2/otdirect/overrides?action=sr&msgid=X&value=HHHH — set stored response
  // POST /api/v2/otdirect/overrides?action=cr&msgid=X — clear stored response
  // POST /api/v2/otdirect/overrides?action=rm&msgid=X&value=HHHH — set response modifier
  // POST /api/v2/otdirect/overrides?action=cm&msgid=X — clear response modifier
  // POST /api/v2/otdirect/overrides?action=ui&msgid=X — mark unknown ID
  // POST /api/v2/otdirect/overrides?action=ki&msgid=X — mark known ID
  else if (wc > 4 && strcmp_P(words[4], PSTR("overrides")) == 0) {
    if (method == HTTP_GET) {
      sendCorsOriginHeader();
      sendOTDirectOverridesJSON();
    } else if (method == HTTP_POST || method == HTTP_PUT) {
      if (!httpServer.hasArg("action") || !httpServer.hasArg("msgid")) {
        sendApiError(400, F("Missing 'action' and/or 'msgid' parameter")); return;
      }
      // Validate msgid is numeric 0-127
      const char* msgidRaw = httpServer.arg("msgid").c_str();
      long msgidVal = strtol(msgidRaw, nullptr, 10);
      if (msgidVal < 0 || msgidVal > 127) {
        sendApiError(400, F("Invalid msgid (0-127)")); return;
      }
      char msgidBuf[4];
      strlcpy(msgidBuf, msgidRaw, sizeof(msgidBuf));

      const char* actionRaw = httpServer.arg("action").c_str();
      char cmdBuf[16];
      if (strcmp_P(actionRaw, PSTR("sr")) == 0 || strcmp_P(actionRaw, PSTR("rm")) == 0) {
        if (!httpServer.hasArg("value")) { sendApiError(400, F("Missing 'value' parameter")); return; }
        const char* valRaw = httpServer.arg("value").c_str();
        // Validate value is hex, max 4 chars
        size_t vLen = strlen(valRaw);
        if (vLen == 0 || vLen > 4) { sendApiError(400, F("Invalid value (hex, 1-4 chars)")); return; }
        for (size_t i = 0; i < vLen; i++) {
          if (!isxdigit((unsigned char)valRaw[i])) { sendApiError(400, F("Invalid value (hex chars only)")); return; }
        }
        const char* prefix = (strcmp_P(actionRaw, PSTR("sr")) == 0) ? "SR=" : "RM=";
        snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("%s%s:%s"), prefix, msgidBuf, valRaw);
      } else if (strcmp_P(actionRaw, PSTR("cr")) == 0 || strcmp_P(actionRaw, PSTR("cm")) == 0 ||
                 strcmp_P(actionRaw, PSTR("ui")) == 0 || strcmp_P(actionRaw, PSTR("ki")) == 0) {
        const char* prefix;
        if (strcmp_P(actionRaw, PSTR("cr")) == 0)      prefix = "CR=";
        else if (strcmp_P(actionRaw, PSTR("cm")) == 0)  prefix = "CM=";
        else if (strcmp_P(actionRaw, PSTR("ui")) == 0)  prefix = "UI=";
        else                                            prefix = "KI=";
        snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("%s%s"), prefix, msgidBuf);
      } else {
        sendApiError(400, F("Invalid action. Use: sr, cr, rm, cm, ui, ki")); return;
      }
      addCommandToQueue(cmdBuf, strlen(cmdBuf), true);
      // Return updated override list
      sendCorsOriginHeader();
      sendOTDirectOverridesJSON();
    } else {
      sendApiMethodNotAllowed(F("GET, POST"));
    }
  }
  else {
    sendApiNotFound(originalURI);
  }
}
#endif

static void handleFirmware(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (wc > 4 && strcmp_P(words[4], PSTR("files")) == 0) {
    apifirmwarefilelist();
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleFilesystem(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (wc > 4 && strcmp_P(words[4], PSTR("files")) == 0) {
    apilistfiles();
  } else if (wc > 4 && strcmp_P(words[4], PSTR("hash-check")) == 0) {
    sendFilesystemHashCheck();
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleSimulate(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  const bool isGet = (method == HTTP_GET);
  const bool isPostOrPut = (method == HTTP_POST || method == HTTP_PUT);

  if (wc == 4) {
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendSimulationStatus();
    return;
  }

  if (wc > 4 && strcmp_P(words[4], PSTR("start")) == 0) {
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    setOTGWSimulationEnabled(true);
    sendSimulationStatus();
    return;
  }

  if (wc > 4 && strcmp_P(words[4], PSTR("stop")) == 0) {
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    setOTGWSimulationEnabled(false);
    sendSimulationStatus();
    return;
  }

  sendApiNotFound(originalURI);
}

static void handleOtgw(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  const bool isGet = (method == HTTP_GET);
  const bool isPostOrPut = (method == HTTP_POST || method == HTTP_PUT);

  if (wc <= 4) { sendApiNotFound(originalURI); return; }

  if (strcmp_P(words[4], PSTR("otmonitor")) == 0 || strcmp_P(words[4], PSTR("telegraf")) == 0) {
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendOTmonitorV2();
  } else if (strcmp_P(words[4], PSTR("messages")) == 0 || strcmp_P(words[4], PSTR("id")) == 0) {
    // GET /api/v2/otgw/messages/{id} or /api/v2/otgw/id/{id} (compat alias)
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    uint8_t msgId = 0;
    if (wc > 5 && parseMsgId(words[5], msgId)) {
      sendOTValue(msgId);
    } else {
      sendApiError(400, F("Invalid or missing message ID"));
    }
  } else if (strcmp_P(words[4], PSTR("commands")) == 0) {
    // POST /api/v2/otgw/commands — command in body, 202 Accepted
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    const String& body = httpServer.arg(0);
    char cmdBuf[64] = "";
    if (!extractJsonField(body, F("command"), cmdBuf, sizeof(cmdBuf))) {
      strlcpy(cmdBuf, body.c_str(), sizeof(cmdBuf));
    }
    handleCommandSubmit(cmdBuf);
  } else if (strcmp_P(words[4], PSTR("command")) == 0) {
    // POST /api/v2/otgw/command/{cmd} — backward compat alias (prefer /commands)
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    if (wc <= 5 || words[5][0] == '\0') { sendApiError(400, F("Missing command")); return; }
    handleCommandSubmit(words[5]);
  } else if (strcmp_P(words[4], PSTR("discovery")) == 0 || strcmp_P(words[4], PSTR("autoconfigure")) == 0) {
    // POST /api/v2/otgw/discovery (or /autoconfigure compat alias)
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    sendCorsOriginHeader();
    httpServer.send(202, F("application/json"), F("{\"status\":\"accepted\"}"));
    doAutoConfigure();
  } else if (strcmp_P(words[4], PSTR("label")) == 0) {
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    if (wc <= 5 || words[5][0] == '\0') { sendApiError(400, F("Missing label")); return; }
    sendOTLabel(words[5]);
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleWebhook(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (wc > 4 && strcmp_P(words[4], PSTR("test")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST")); return; }
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
    testWebhook(isOn);
    sendCorsOriginHeader();
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  } else {
    sendApiNotFound(originalURI);
  }
}

// Extract value from POST body: accepts raw "21.5" or JSON {"value":"21.5"}
// Returns pointer into buf (null-terminated), or nullptr if no value found.
static const char* satExtractPostValue(const char* body, char* buf, size_t bufSize)
{
  if (!body || !*body) return nullptr;
  // Try to find "value" key in JSON: crude but avoids pulling in a JSON library
  const char* vp = strstr_P(body, PSTR("\"value\""));
  if (vp) {
    // Skip past "value" and any : and whitespace/quotes
    vp += 7; // strlen("\"value\"")
    while (*vp == ':' || *vp == ' ' || *vp == '"') vp++;
    // Copy until quote, comma, brace, or end
    size_t i = 0;
    while (vp[i] && vp[i] != '"' && vp[i] != ',' && vp[i] != '}' && i < bufSize - 1) {
      buf[i] = vp[i];
      i++;
    }
    buf[i] = '\0';
    return buf;
  }
  // No JSON structure found — treat body as raw value
  strlcpy(buf, body, bufSize);
  return buf;
}

//=== SAT extended health summary (detail=full) ===
// Sends a comprehensive JSON with health booleans, pressure, cycle, error,
// and auto-tune diagnostics.  Uses chunked transfer via sendStartJsonMap /
// sendJsonMapEntry / satSendJsonFloat (defined in SATcontrol.ino).
// Called instead of satSendStatusJSON() when ?detail=full is present.
static void satSendHealthJSON()
{
  // --- Derive health booleans ---
  bool syncSetpoint    = state.sat.bSetpointMismatch;
  bool syncModulation  = !state.sat.bModulationReliable;
  // CH sync: computed the same way as the MQTT publisher (SATcontrol.ino)
  bool boilerCHActive  = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;
  bool syncCH          = (state.sat.bActive != boilerCHActive);
  bool flameHealth     = !state.sat.bSafetyTripped;
  bool deviceHealth    = (state.sat.eBoilerStatus != SAT_BS_OFF)
                      && (state.sat.eBoilerStatus != SAT_BS_STALLED_IGNITION);
  bool cycleHealth     = (state.sat.eLastCycleClass != SAT_CYCLE_OVERSHOOT)
                      && (state.sat.eLastCycleClass != SAT_CYCLE_UNDERHEAT)
                      && (state.sat.eLastCycleClass != SAT_CYCLE_SHORT);

  // --- Cycle class / kind names (kept in PROGMEM via static const) ---
  static const char* const ccNames[] = {
    "none", "good", "overshoot", "underheat", "short", "uncertain"
  };
  int ccIdx = (int)state.sat.eLastCycleClass;
  if (ccIdx < 0 || ccIdx > 5) ccIdx = 0;

  static const char* const ckNames[] = {
    "unknown", "ch", "dhw", "mixed"
  };
  int ckIdx = (int)state.sat.eLastCycleKind;
  if (ckIdx < 0 || ckIdx > 3) ckIdx = 0;

  // --- Build chunked JSON response ---
  sendStartJsonMap("");

  // Synchronization problem indicators (AC#3)
  sendJsonMapEntry(F("sync_setpoint"),       syncSetpoint);
  sendJsonMapEntry(F("sync_modulation"),     syncModulation);
  sendJsonMapEntry(F("sync_ch"),             syncCH);

  // Pressure diagnostics (AC#4)
  satSendJsonFloat(F("pressure_smoothed"),   state.sat.fSmoothedPressure, 2);
  satSendJsonFloat(F("pressure_drop_rate"),  state.sat.fPressureDropRate, 3);
  sendJsonMapEntry(F("pressure_alarm"),      state.sat.bPressureAlarm);

  // Cycle diagnostics (AC#5)
  sendJsonMapEntry(F("cycle_kind"),          ckNames[ckIdx]);
  satSendJsonFloat(F("cycle_duration"),      state.sat.fLastCycleDuration, 0);
  sendJsonMapEntry(F("cycle_count"),         state.sat.iCycleCount);
  satSendJsonFloat(F("cycle_fraction_ch"),   state.sat.fLastCycleFractionCH, 3);
  satSendJsonFloat(F("cycle_fraction_dhw"),  state.sat.fLastCycleFractionDHW, 3);

  // Error / curve statistics (AC#6)
  satSendJsonFloat(F("error_mean"),          state.sat.fMeanError, 2);
  satSendJsonFloat(F("error_stddev"),        state.sat.fErrorStdDev, 3);
  sendJsonMapEntry(F("error_samples"),       (int32_t)state.sat.iErrorSampleCount);
  sendJsonMapEntry(F("heating_curve_recommendation"), state.sat.sHeatCurveRec);

  // Health booleans (AC#7)
  sendJsonMapEntry(F("flame_health"),        flameHealth);
  sendJsonMapEntry(F("device_health"),       deviceHealth);
  sendJsonMapEntry(F("cycle_health"),        cycleHealth);
  sendJsonMapEntry(F("cycle_class"),         ccNames[ccIdx]);

  // Auto-tune and OVP calibration (AC#8)
  satSendJsonFloat(F("auto_tune_score"),     state.sat.fAutoTuneScore, 2);
  sendJsonMapEntry(F("auto_tune_cycles"),    (int32_t)state.sat.iAutoTuneCycles);
  sendJsonMapEntry(F("ovp_phase"),           (int32_t)state.sat.eCalibPhase);

  sendEndJsonMap("");
}

// Check whether the current request carries ?detail=full
static bool satRequestHasDetailFull()
{
  return httpServer.hasArg(F("detail"))
      && httpServer.arg(F("detail")) == F("full");
}

//=== SAT API handler ===
// GET  /api/v2/sat/status               — returns full SAT runtime state
// GET  /api/v2/sat/status?detail=full   — returns extended health diagnostics
// POST /api/v2/sat/target               — set target temperature (body: "21.0" or {"value":"21.0"})
// POST /api/v2/sat/externaltemp         — push indoor temp
// POST /api/v2/sat/externaloutdoor      — push outdoor temp
// POST /api/v2/sat/humidity             — push indoor humidity (0-100%)
// POST /api/v2/sat/area/<0-3>           — push area temperature (multi-area)
// POST /api/v2/sat/flush                — flush short-lived data (PID integral + cycle window)
// POST /api/v2/sat/settings/<name>      — update any SAT setting (mirrors all MQTT sat/* commands)
static void handleSAT(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)
{
  if (!checkHttpAuth()) return;

  if (wc <= 4) {
    // GET /api/v2/sat — default to status
    if (method == HTTP_GET) {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
      if (satRequestHasDetailFull()) { satSendHealthJSON(); }
      else                           { satSendStatusJSON(); }
    } else {
      sendApiMethodNotAllowed(F("GET"));
    }
    return;
  }

  const char* sub = words[4];
  if (strcasecmp_P(sub, PSTR("status")) == 0) {
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    if (satRequestHasDetailFull()) { satSendHealthJSON(); }
    else                           { satSendStatusJSON(); }
  }
  else if (strcasecmp_P(sub, PSTR("target")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleTargetTemp(val)) {
      sendApiError(400, F("Invalid or missing value (5.0-30.0)"));
      return;
    }
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("externaltemp")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleExternalTemp(val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("externaloutdoor")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleExternalOutdoor(val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("reset_integral")) == 0) {
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    satResetIntegral();
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\",\"integral\":0}"));
  }
  else if (strcasecmp_P(sub, PSTR("flush")) == 0) {
    // POST /api/v2/sat/flush — clear short-lived SAT data (PID integral + cycle window) (Task #237)
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    satFlushShortLivedData();
    httpServer.send(200, F("application/json"), F("{\"result\":\"ok\",\"flushed\":[\"pid\",\"cycles\"]}"));
  }
  else if (strcasecmp_P(sub, PSTR("window")) == 0) {
    if (method == HTTP_POST || method == HTTP_PUT) {
      char valBuf[16];
      const char* val = nullptr;
      if (httpServer.hasArg(F("plain"))) {
        val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
      } else if (wc > 5) {
        val = words[5];
      }
      if (!val) { sendApiError(400, F("Missing value (open/closed)")); return; }
      bool isOpen = (strcasecmp_P(val, PSTR("open")) == 0 ||
                    strcasecmp_P(val, PSTR("1")) == 0 ||
                    strcasecmp_P(val, PSTR("ON")) == 0);
      satHandleWindow(isOpen);
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    } else {
      sendApiMethodNotAllowed(F("POST, PUT"));
    }
  }
  else if (strcasecmp_P(sub, PSTR("preset")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing preset name (away/eco/comfort/sleep/activity/home)")); return; }
    satHandlePreset(val);
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("enable")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing value (0/1)")); return; }
    satHandleEnabled(val);
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("mode")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing mode (continuous/pwm)")); return; }
    satHandleControlMode(val);
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("humidity")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleHumidity(val)) {
      sendApiError(400, F("Invalid or missing value (0-100)"));
      return;
    }
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("weather")) == 0) {
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
    weatherSendStatusJSON();
  }
  else if (strcasecmp_P(sub, PSTR("area")) == 0) {
    // POST /api/v2/sat/area/<0-3> — push area temperature
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    if (wc <= 5) { sendApiError(400, F("Missing area index (0-3)")); return; }
    int area = atoi(words[5]);
    if (area < 0 || area >= 4) { sendApiError(400, F("Area index must be 0-3")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 6) {
      val = words[6];
    }
    if (!val || !satHandleAreaTemp((uint8_t)area, val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("settings")) == 0) {
    // POST/PUT /api/v2/sat/settings/<setting-name> — mirrors all MQTT sat/<sub-command> handlers
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    if (wc <= 5) { sendApiError(400, F("Missing setting name")); return; }
    const char* settingName = words[5];
    char valBuf[64];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 6) {
      val = words[6];
    }
    bool handled = false;
    bool needsVal = true;
    // No-value commands
    if (strcasecmp_P(settingName, PSTR("reset_integral")) == 0) {
      satResetIntegral();
      handled = true; needsVal = false;
    } else if (strcasecmp_P(settingName, PSTR("ovp_start")) == 0) {
      satOvpStartCalibration();
      handled = true; needsVal = false;
    } else if (strcasecmp_P(settingName, PSTR("ovp_stop")) == 0) {
      satOvpStopCalibration();
      handled = true; needsVal = false;
    }
    if (handled && !needsVal) {
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
      return;
    }
    // All remaining commands require a value
    if (!val) { sendApiError(400, F("Missing value")); return; }
    // Special handlers
    if (strcasecmp_P(settingName, PSTR("control_mode")) == 0) {
      satHandleControlMode(val);
      handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset")) == 0) {
      satHandlePreset(val);
      handled = true;
    } else if (strcasecmp_P(settingName, PSTR("heating_mode")) == 0) {
      satHandleHeatingMode(val);
      handled = true;
    // Direct updateSetting() calls
    } else if (strcasecmp_P(settingName, PSTR("overshoot_margin")) == 0) {
      updateSetting("SATovershootmargin", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("heating_system")) == 0) {
      updateSetting("SATsystem", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("manufacturer")) == 0) {
      updateSetting("SATmanufacturer", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("max_modulation")) == 0) {
      updateSetting("SATmaxmodulation", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("dhw_setpoint")) == 0) {
      updateSetting("SATdhwsetpoint", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("dhw_enabled")) == 0) {
      updateSetting("SATdhwenabled", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("interval")) == 0) {
      updateSetting("SATinterval", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("ovp_value")) == 0) {
      updateSetting("SATovpvalue", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("ovp_enabled")) == 0) {
      updateSetting("SATovpenabled", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("push_setpoint")) == 0) {
      updateSetting("SATpushsetpoint", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("flame_off_offset")) == 0) {
      updateSetting("SATflameoffset", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("force_pwm")) == 0) {
      updateSetting("SATforcepwm", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("flow_offset")) == 0) {
      updateSetting("SATflowoffset", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("summer_simmer")) == 0) {
      updateSetting("SATsummersimmer", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("summer_threshold")) == 0) {
      updateSetting("SATsummerthreshold", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("summer_min_hours")) == 0) {
      updateSetting("SATsummerminhours", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("comfort_adjust")) == 0) {
      updateSetting("SATcomfortadjust", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("comfort_humidity")) == 0) {
      updateSetting("SATcomforthumidity", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("comfort_max_offset")) == 0) {
      updateSetting("SATcomfortmaxoffset", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("simulation")) == 0) {
      updateSetting("SATsimulation", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("ble_enable")) == 0) {
      updateSetting("SATbleenable", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("ble_mac")) == 0) {
      updateSetting("SATblemac", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("ble_interval")) == 0) {
      updateSetting("SATbleinterval", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_sync")) == 0) {
      updateSetting("SATpresetsync", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_sync_topic")) == 0) {
      updateSetting("SATpresetsynctopic", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("multi_area")) == 0) {
      updateSetting("SATmultiarea", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("auto_tune")) == 0) {
      updateSetting("SATautotune", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("auto_tune_rate")) == 0) {
      updateSetting("SATautotunerate", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("multi_area_count")) == 0) {
      updateSetting("SATmultiareacount", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("heating_curve")) == 0) {
      updateSetting("SATcoefficient", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("deadband")) == 0) {
      updateSetting("SATdeadband", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("mod_sup_delay")) == 0) {
      updateSetting("SATmodsupdelay", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("mod_sup_offset")) == 0) {
      updateSetting("SATmodsupoffset", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("boiler_capacity")) == 0) {
      updateSetting("SATboilercapacity", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("target_temp_step")) == 0) {
      updateSetting("SATtempstep", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("min_pressure")) == 0) {
      updateSetting("SATminpressure", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("max_pressure")) == 0) {
      updateSetting("SATmaxpressure", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("max_pressure_drop")) == 0) {
      updateSetting("SATmaxpressdrop", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_comfort")) == 0) {
      updateSetting("SATpresetcomfort", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_eco")) == 0) {
      updateSetting("SATpreseteco", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_away")) == 0) {
      updateSetting("SATpresetaway", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_sleep")) == 0) {
      updateSetting("SATpresetsleep", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_activity")) == 0) {
      updateSetting("SATpresetactivity", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("preset_home")) == 0) {
      updateSetting("SATpresethome", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("solar_gain")) == 0) {
      updateSetting("SATsolargain", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("window_detection")) == 0) {
      updateSetting("SATwindowdetect", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("pwm_auto_switch")) == 0) {
      updateSetting("SATpwmautoswitch", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("sensor_max_age")) == 0) {
      updateSetting("SATsensormaxage", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("error_monitoring")) == 0) {
      updateSetting("SATerrormon", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("auto_gains_value")) == 0) {
      updateSetting("SATautogains", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("cycles_per_hour")) == 0) {
      updateSetting("SATcyclesperhour", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("valve_offset")) == 0) {
      updateSetting("SATvalveoffset", val); handled = true;
    } else if (strcasecmp_P(settingName, PSTR("solar_freeze_integral")) == 0) {
      updateSetting("SATsolarfreezeint", val); handled = true;
    }
    if (!handled) {
      sendApiError(404, F("Unknown setting name"));
      return;
    }
    char respBuf[96];
    snprintf_P(respBuf, sizeof(respBuf), PSTR("{\"status\":\"ok\",\"setting\":\"%s\",\"value\":\"%s\"}"), settingName, val);
    httpServer.send(200, F("application/json"), respBuf);
  }
  else {
    sendApiNotFound(originalURI);
  }
}

//=== Route dispatch table (ADR-050) ===
// Adding a new v2 resource: (1) write handler function above, (2) add entry below.
typedef void (*ApiResourceHandler)(const char[][API_WORD_LEN], uint8_t, HTTPMethod, const char*);

struct ApiRoute {
  PGM_P              segment;
  ApiResourceHandler handler;
};

static const char kRouteHealth[]     PROGMEM = "health";
static const char kRouteSettings[]   PROGMEM = "settings";
static const char kRouteSensors[]    PROGMEM = "sensors";
static const char kRouteDevice[]     PROGMEM = "device";
static const char kRouteFlash[]      PROGMEM = "flash";
static const char kRoutePic[]        PROGMEM = "pic";
static const char kRouteFirmware[]   PROGMEM = "firmware";
static const char kRouteFilesystem[] PROGMEM = "filesystem";
static const char kRouteSimulate[]   PROGMEM = "simulate";
static const char kRouteOtgw[]       PROGMEM = "otgw";
static const char kRouteWebhook[]    PROGMEM = "webhook";
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
static const char kRouteOtdirect[]   PROGMEM = "otdirect";
#endif
static const char kRouteSat[]        PROGMEM = "sat";

static const ApiRoute kV2Routes[] = {
  { kRouteHealth,     handleHealth },
  { kRouteSettings,   handleSettings },
  { kRouteSensors,    handleSensors },
  { kRouteDevice,     handleDevice },
  { kRouteFlash,      handleFlash },
  { kRoutePic,        handlePic },
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  { kRouteOtdirect,   handleOTDirect },
#endif
  { kRouteFirmware,   handleFirmware },
  { kRouteFilesystem, handleFilesystem },
  { kRouteSimulate,   handleSimulate },
  { kRouteOtgw,       handleOtgw },
  { kRouteWebhook,    handleWebhook },
  { kRouteSat,        handleSAT },
  { nullptr,          nullptr }  // sentinel
};

//=======================================================================
void processAPI()
{
  // Static buffers save ~356 bytes of stack (not re-entrant in cooperative scheduler)
  static char URI[50];
  static char words[API_MAX_WORDS][API_WORD_LEN];
  static char originalURI[sizeof(URI)];
  URI[0] = '\0';
  memset(words, 0, sizeof(words));

  const HTTPMethod method = httpServer.method();
  const unsigned long startMs = millis();
  const size_t uriLen = strlcpy(URI, httpServer.uri().c_str(), sizeof(URI));
  strlcpy(originalURI, URI, sizeof(originalURI));

  if (uriLen >= sizeof(URI)) {
    RESTDebugTf(PSTR("REST %s %s => 414 URI too long\r\n"), httpMethodToStr(method), originalURI);
    httpServer.send_P(414, PSTR("text/plain"), PSTR("414: URI too long\r\n"));
    return;
  }

  if (platformFreeHeap() < 4096) {
    RESTDebugTf(PSTR("REST %s %s => 500 low heap (%u)\r\n"), httpMethodToStr(method), originalURI, platformFreeHeap());
    httpServer.send_P(500, PSTR("text/plain"), PSTR("500: internal server error (low heap)\r\n"));
    return;
  }

  // Parse URI into words[] tokens
  uint8_t wc = 0;
  {
    char *savePtr = nullptr;
    if (URI[0] == '/' && wc < API_MAX_WORDS) { words[wc][0] = '\0'; wc++; }
    for (char *token = strtok_r(URI, "/", &savePtr); token && wc < API_MAX_WORDS; token = strtok_r(nullptr, "/", &savePtr)) {
      strlcpy(words[wc], token, API_WORD_LEN);
      wc++;
    }
  }

  // Route: /api/v2/{resource}/...
  if (wc > 1 && strcmp_P(words[1], PSTR("api")) == 0) {
    if (wc > 2 && strcmp_P(words[2], PSTR("v2")) == 0) {
      // OPTIONS preflight for all v2 endpoints (CORS)
      if (method == HTTP_OPTIONS) {
        sendApiOptions();
        RESTDebugTf(PSTR("REST OPTIONS %s => 204 preflight %lums\r\n"), originalURI, millis() - startMs);
        return;
      }

      // H5: Centralized auth check — all POST/PUT mutations require auth
      if (method == HTTP_POST || method == HTTP_PUT) {
        if (!checkHttpAuth()) return;
      }

      // Dispatch via route table
      if (wc > 3) {
        for (const ApiRoute* r = kV2Routes; r->segment != nullptr; r++) {
          if (strcmp_P(words[3], r->segment) == 0) {
            restResponseStatus = 200; // default; overwritten by sendApiError if handler fails
            r->handler(words, wc, method, originalURI);
            RESTDebugTf(PSTR("REST %s %s => %d v2/%S %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, r->segment, millis() - startMs);
            return;
          }
        }
      }
      sendApiNotFound(originalURI);
      RESTDebugTf(PSTR("REST %s %s => %d not found %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, millis() - startMs);
    } else if (wc > 2 && (strcmp_P(words[2], PSTR("v0")) == 0 || strcmp_P(words[2], PSTR("v1")) == 0)) {
      sendApiError(410, F("API version removed; use /api/v2"));
      RESTDebugTf(PSTR("REST %s %s => %d deprecated %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, millis() - startMs);
    } else {
      sendApiNotFound(originalURI);
      RESTDebugTf(PSTR("REST %s %s => %d %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, millis() - startMs);
    }
  } else {
    sendApiNotFound(originalURI);
    RESTDebugTf(PSTR("REST %s %s => %d non-api %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, millis() - startMs);
  }
} // processAPI()


//====[ implementing REST API ]====
void sendOTValue(int msgid){
  if (msgid < 0 || msgid > OT_MSGID_MAX) {
    sendStartJsonMap("");
    sendJsonMapEntry(F("error"), F("message id: out of range"));
    sendEndJsonMap("");
    return;
  }
  PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
  if (OTlookupitem.type == ot_undef) {
    sendStartJsonMap("");
    sendJsonMapEntry(F("error"), F("message undefined: reserved for future use"));
    sendEndJsonMap("");
    return;
  }
  sendStartJsonMap("");
  sendJsonMapEntry(F("label"), OTlookupitem.label);
  if (OTlookupitem.type == ot_f88) {
    sendJsonMapEntry(F("value"), (float)atof(getOTGWValue(msgid)));
  } else {
    sendJsonMapEntry(F("value"), (int32_t)atoi(getOTGWValue(msgid))); // cast selects int32_t overload
  }
  sendJsonMapEntry(F("unit"), OTlookupitem.unit);
  sendEndJsonMap("");
}

void sendOTLabel(const char *msglabel){
  uint_fast8_t msgid;
  for (msgid = 0; msgid <= OT_MSGID_MAX; msgid++){
    PROGMEM_readAnything(&OTmap[msgid], OTlookupitem);
    if (strcasecmp(OTlookupitem.label, msglabel) == 0) break;
  }
  if (msgid > OT_MSGID_MAX){
    sendStartJsonMap("");
    sendJsonMapEntry(F("error"), F("message id: reserved for future use"));
    sendEndJsonMap("");
    return;
  }
  if (OTlookupitem.type == ot_undef) {
    sendStartJsonMap("");
    sendJsonMapEntry(F("error"), F("message undefined: reserved for future use"));
    sendEndJsonMap("");
    return;
  }
  sendStartJsonMap("");
  sendJsonMapEntry(F("label"), OTlookupitem.label);
  if (OTlookupitem.type == ot_f88) {
    sendJsonMapEntry(F("value"), (float)atof(getOTGWValue(msgid)));
  } else {
    sendJsonMapEntry(F("value"), (int32_t)atoi(getOTGWValue(msgid))); // cast selects int32_t overload
  }
  sendJsonMapEntry(F("unit"), OTlookupitem.unit);
  sendEndJsonMap("");
}

//=======================================================================
// Helpers for Map-based JSON functions (sendJsonOTmonMapEntry)
// Concrete overloads instead of templates: ctags (Arduino builder) scans all
// .ino files and generates forward declarations for template functions using
// the template parameter name as a type (e.g. TVal), which GCC rejects because
// TVal is unknown at the point the ctags block is compiled. Concrete overloads
// with real types produce valid ctags forward declarations.
// All call sites use F() for both label and unit, so only the F/value/F overloads
// are needed. The F/value/char* and char*/value/F combinations are unused.
//=======================================================================

// Convert both flash-string label and unit to char buffers, then dispatch to
// the concrete sendJsonOTmonMapEntry(char*, T, char*, time_t) in jsonStuff.ino.
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, const char* value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, float value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, uint16_t value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, uint32_t value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, int value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}
void sendJsonOTmonMapEntry(const __FlashStringHelper* label, bool value, const __FlashStringHelper* unit, unsigned long lastupdated) {
  char labelBuf[35]; strncpy_P(labelBuf, (PGM_P)label, sizeof(labelBuf)); labelBuf[sizeof(labelBuf)-1] = 0;
  char unitBuf[10];  strncpy_P(unitBuf,  (PGM_P)unit,  sizeof(unitBuf));  unitBuf[sizeof(unitBuf)-1]  = 0;
  sendJsonOTmonMapEntry(labelBuf, value, unitBuf, lastupdated);
}

// Helpers for start/end map (non-template)
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

  sendStartJsonMap(F("otmonitor"));

  // sendJsonOTmonObj(F("status hb"), byte_to_binary((OTcurrentSystemState.Statusflags>>8) & 0xFF),F(""), msglastupdated[OT_Statusflags]);
  // sendJsonOTmonObj(F("status lb"), byte_to_binary(OTcurrentSystemState.Statusflags & 0xFF),F(""), msglastupdated[OT_Statusflags]);

  sendJsonOTmonMapEntry(F("flamestatus"), CONOFF(isFlameStatus()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("chmodus"), CONOFF(isCentralHeatingActive()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("chenable"), CONOFF(isCentralHeatingEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("ch2modus"), CONOFF(isCentralHeating2Active()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("ch2enable"), CONOFF(isCentralHeating2enabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("dhwmode"), CONOFF(isDomesticHotWaterActive()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("dhwenable"), CONOFF(isDomesticHotWaterEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("diagnosticindicator"), CONOFF(isDiagnosticIndicator()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("faultindicator"), CONOFF(isFaultIndicator()),F(""), getMsgLastUpdated(OT_Statusflags));
  
  sendJsonOTmonMapEntry(F("coolingmodus"), CONOFF(isCoolingEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  sendJsonOTmonMapEntry(F("coolingactive"), CONOFF(isCoolingActive()),F(""), getMsgLastUpdated(OT_Statusflags));  
  sendJsonOTmonMapEntry(F("otcactive"), CONOFF(isOutsideTemperatureCompensationActive()),F(""), getMsgLastUpdated(OT_Statusflags));

  if (getMsgLastUpdated(OT_ASFflags)) {
    sendJsonOTmonMapEntry(F("servicerequest"), CONOFF(isServiceRequest()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("lockoutreset"), CONOFF(isLockoutReset()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("lowwaterpressure"), CONOFF(isLowWaterPressure()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("gasflamefault"), CONOFF(isGasFlameFault()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("airtemp"), CONOFF(isAirTemperature()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("waterovertemperature"), CONOFF(isWaterOverTemperature()),F(""), getMsgLastUpdated(OT_ASFflags));
    sendJsonOTmonMapEntry(F("oemfaultcode"), OTcurrentSystemState.ASFflags & 0xFF, F(""), getMsgLastUpdated(OT_ASFflags));
  }

  if (getMsgLastUpdated(OT_Toutside))            sendJsonOTmonMapEntry(F("outsidetemperature"), OTcurrentSystemState.Toutside, F("°C"), getMsgLastUpdated(OT_Toutside));
  if (getMsgLastUpdated(OT_Tr))                   sendJsonOTmonMapEntry(F("roomtemperature"), OTcurrentSystemState.Tr, F("°C"), getMsgLastUpdated(OT_Tr));
  if (getMsgLastUpdated(OT_TrSet))                sendJsonOTmonMapEntry(F("roomsetpoint"), OTcurrentSystemState.TrSet, F("°C"), getMsgLastUpdated(OT_TrSet));
  if (getMsgLastUpdated(OT_TrOverride))           sendJsonOTmonMapEntry(F("remoteroomsetpoint"), OTcurrentSystemState.TrOverride, F("°C"), getMsgLastUpdated(OT_TrOverride));
  if (getMsgLastUpdated(OT_TSet))                 sendJsonOTmonMapEntry(F("controlsetpoint"), OTcurrentSystemState.TSet,F("°C"), getMsgLastUpdated(OT_TSet));
  if (getMsgLastUpdated(OT_RelModLevel))          sendJsonOTmonMapEntry(F("relmodlvl"), OTcurrentSystemState.RelModLevel,F("%"), getMsgLastUpdated(OT_RelModLevel));
  if (getMsgLastUpdated(OT_MaxRelModLevelSetting))sendJsonOTmonMapEntry(F("maxrelmodlvl"), OTcurrentSystemState.MaxRelModLevelSetting, F("%"), getMsgLastUpdated(OT_MaxRelModLevelSetting));

  if (getMsgLastUpdated(OT_Tboiler))              sendJsonOTmonMapEntry(F("boilertemperature"), OTcurrentSystemState.Tboiler, F("°C"), getMsgLastUpdated(OT_Tboiler));
  if (getMsgLastUpdated(OT_Tret))                 sendJsonOTmonMapEntry(F("returnwatertemperature"), OTcurrentSystemState.Tret,F("°C"), getMsgLastUpdated(OT_Tret));
  if (getMsgLastUpdated(OT_Tdhw))                 sendJsonOTmonMapEntry(F("dhwtemperature"), OTcurrentSystemState.Tdhw,F("°C"), getMsgLastUpdated(OT_Tdhw));
  if (getMsgLastUpdated(OT_TdhwSet))              sendJsonOTmonMapEntry(F("dhwsetpoint"), OTcurrentSystemState.TdhwSet,F("°C"), getMsgLastUpdated(OT_TdhwSet));
  if (getMsgLastUpdated(OT_MaxTSet))              sendJsonOTmonMapEntry(F("maxchwatersetpoint"), OTcurrentSystemState.MaxTSet,F("°C"), getMsgLastUpdated(OT_MaxTSet));
  if (getMsgLastUpdated(OT_CHPressure))           sendJsonOTmonMapEntry(F("chwaterpressure"), OTcurrentSystemState.CHPressure, F("bar"), getMsgLastUpdated(OT_CHPressure));
  if (getMsgLastUpdated(OT_OEMDiagnosticCode))    sendJsonOTmonMapEntry(F("oemdiagnosticcode"), OTcurrentSystemState.OEMDiagnosticCode, F(""), getMsgLastUpdated(OT_OEMDiagnosticCode));

  if (settings.s0.bEnabled) 
  {
    sendJsonOTmonMapEntry(F("s0powerkw"), OTGWs0powerkw , F("kW"), OTGWs0lasttime);
    sendJsonOTmonMapEntry(F("s0intervalcount"), OTGWs0pulseCount , F(""), OTGWs0lasttime);
    sendJsonOTmonMapEntry(F("s0totalcount"), OTGWs0pulseCountTot , F(""), OTGWs0lasttime);
  }
  sendJsonOTmonMapEntry(F("sensorsimulation"), state.debug.bSensorSim, F(""), now);
  if (settings.sensors.bEnabled || state.debug.bSensorSim) 
  {
    sendJsonOTmonMapEntry(F("numberofsensors"), DallasrealDeviceCount , F(""), now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      if (!strDeviceAddress) continue;
      sendJsonOTmonMapEntryDallasTemp(strDeviceAddress, DallasrealDevice[i].tempC, F("°C"), DallasrealDevice[i].lasttime);
      // Labels now managed by Web UI via /dallas_labels.ini file (not sent in API)
    }
  }

  sendEndJsonMap(F("otmonitor"));
}

//=======================================================================
// Sends device info as JSON map (v2 format)
// Returns: {"device":{"author":"...","fwversion":"...",...}}
void sendDeviceInfoV2() 
{
  sendStartJsonMap(F("device"));

  sendJsonMapEntry(F("author"), F("Robert van den Breemen"));
  sendJsonMapEntry(F("fwversion"), _SEMVER_FULL);
  sendJsonMapEntry(F("picavailable"), state.pic.bAvailable);
  if (isPICEnabled()) {
    sendJsonMapEntry(F("picfwversion"), state.pic.sFwversion);
    sendJsonMapEntry(F("picdeviceid"), state.pic.sDeviceid);
    sendJsonMapEntry(F("picfwtype"), state.pic.sType);
  }
  sendJsonMapEntry(F("otdirectavailable"), isOTDirectEnabled());
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    {
      const char* modeStr = "gateway";
      if (state.otd.eMode == OTD_MODE_MONITOR) modeStr = "monitor";
      else if (state.otd.eMode == OTD_MODE_BYPASS) modeStr = "bypass";
      else if (state.otd.eMode == OTD_MODE_MASTER) modeStr = "master";
      else if (state.otd.eMode == OTD_MODE_LOOPBACK) modeStr = "loopback";
      sendJsonMapEntry(F("otdmode"), modeStr);
    }
    sendJsonMapEntry(F("otdbypass"), state.otd.bBypassActive);
    sendJsonMapEntry(F("otdmonitor"), state.otd.bMonitorMode);
    sendJsonMapEntry(F("otdmaster"), state.otd.bMasterMode);
    sendJsonMapEntry(F("otdstepup"), state.otd.bStepUpEnabled);
    sendJsonMapEntry(F("otdthermostat"), state.otd.bThermostatConnected);
    sendJsonMapEntry(F("otdsetback"), state.otd.bSetbackActive);
    sendJsonMapEntry(F("otdschedtotal"), state.otd.iScheduleTotal);
    sendJsonMapEntry(F("otdschedactive"), state.otd.iScheduleActive);
    sendJsonMapEntry(F("otdscheddisabled"), state.otd.iScheduleDisabled);
    sendJsonMapEntry(F("otdoverrides"), state.otd.iOverrideCount);
  }
#endif
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  sendJsonMapEntry(F("compiled"), cMsg);
  sendJsonMapEntry(F("hostname"), CSTR(settings.sHostname));
  sendJsonMapEntry(F("ipaddress"), CSTR(getActiveIP()));
  sendJsonMapEntry(F("macaddress"), CSTR(getActiveMAC()));
  sendJsonMapEntry(F("platform"), F(PLATFORM_NAME));
  sendJsonMapEntry(F("freeheap"), platformFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), platformMaxFreeBlock());
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%06X"), (unsigned int)platformChipId());
  sendJsonMapEntry(F("chipid"), cMsg);
  sendJsonMapEntry(F("coreversion"), platformCoreVersion());
  sendJsonMapEntry(F("sdkversion"),  platformSdkVersion());
  sendJsonMapEntry(F("cpufreq"), platformCpuFreqMHz());
  sendJsonMapEntry(F("sketchsize"), platformSketchSize() );
  sendJsonMapEntry(F("freesketchspace"),  platformFreeSketchSpace() );

  snprintf_P(cMsg, sizeof(cMsg), PSTR("%08X"), (unsigned int)platformFlashChipId());
  sendJsonMapEntry(F("flashchipid"), cMsg);
  sendJsonMapEntry(F("flashchipsize"), (platformFlashChipSize() / 1024.0f / 1024.0f));
  sendJsonMapEntry(F("flashchiprealsize"), (platformFlashChipRealSize() / 1024.0f / 1024.0f));

  platformFSInfo(LittleFSinfo);
  sendJsonMapEntry(F("LittleFSsize"), floorf((LittleFSinfo.totalBytes / (1024.0f * 1024.0f))));

  sendJsonMapEntry(F("flashchipspeed"), floorf((platformFlashChipSpeed() / 1000.0f / 1000.0f)));

  uint8_t ideMode = platformFlashChipMode();
  sendJsonMapEntry(F("flashchipmode"), flashMode[ideMode < 4 ? ideMode : 4]);
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    sendJsonMapEntry(F("ssid"), CSTR(state.net.sAPSSID));
    sendJsonMapEntry(F("wifirssi"), 0);
    sendJsonMapEntry(F("wifiquality"), 0);
    sendJsonMapEntry(F("wifiquality_text"), F("AP Mode"));
    sendJsonMapEntry(F("apfallback"), true);
  } else
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) {
    sendJsonMapEntry(F("ssid"), F("Wired"));
    sendJsonMapEntry(F("wifirssi"), 0);
    sendJsonMapEntry(F("wifiquality"), 100);
    sendJsonMapEntry(F("wifiquality_text"), F("Wired"));
  } else
#endif
  {
    sendJsonMapEntry(F("ssid"), CSTR(WiFi.SSID()));
    sendJsonMapEntry(F("wifirssi"), WiFi.RSSI());
    sendJsonMapEntry(F("wifiquality"), signal_quality_perc_quad(WiFi.RSSI()));
    sendJsonMapEntry(F("wifiquality_text"), dBmtoQuality(WiFi.RSSI()));
  }
  sendJsonMapEntry(F("ntpenable"), settings.ntp.bEnable);
  sendJsonMapEntry(F("ntptimezone"), CSTR(settings.ntp.sTimezone));
  sendJsonMapEntry(F("uptime"), upTime());
  sendJsonMapEntry(F("lastreset"), lastReset);
  sendJsonMapEntry(F("bootcount"), state.uptime.iRebootCount);
  sendJsonMapEntry(F("mqttconnected"), state.mqtt.bConnected);
  // "otcommandinterface" names which OT interface is active — always one or the other, never both.
  if (isPICEnabled())        sendJsonMapEntry(F("otcommandinterface"), F("PIC"));
  else if (isOTDirectEnabled()) sendJsonMapEntry(F("otcommandinterface"), F("OT-Direct"));
  else                       sendJsonMapEntry(F("otcommandinterface"), F("None"));
  if (hasOTCommandInterface()) {
    sendJsonMapEntry(F("thermostatconnected"), state.otBus.bThermostatState);
    sendJsonMapEntry(F("boilerconnected"), state.otBus.bBoilerState);
    sendJsonMapEntry(F("otgwconnected"), state.otBus.bOnline);
  }
  if (isPICEnabled()) {
    sendJsonMapEntry(F("otgwmode"), !isGatewayFirmware() ? "N/A" : state.otBus.bGatewayModeKnown ? CCONOFF(state.otBus.bGatewayMode) : "detecting");
  }
  sendJsonMapEntry(F("otgwsimulation"), state.debug.bOTGWSimulation);

  // Hardware platform details
  sendJsonMapEntry(F("board"), boardName());
  sendJsonMapEntry(F("hardwaremode"), hardwareModeName());
  sendJsonMapEntry(F("networkmode"), networkModeName());
#if defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE
  sendJsonMapEntry(F("oledpresent"), state.hw.bOLEDPresent);
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  sendJsonMapEntry(F("ethernetpresent"), state.hw.bEthernetPresent);
  sendJsonMapEntry(F("ethernetlink"), state.net.bEthernetLink);
#endif

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
  sendJsonMapEntry(F("heap"), platformFreeHeap());
  sendJsonMapEntry(F("networkmode"), networkModeName());
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  sendJsonMapEntry(F("wifirssi"), (state.net.eMode == NET_ETHERNET) ? 0 : WiFi.RSSI());
#else
  sendJsonMapEntry(F("wifirssi"), WiFi.RSSI());
#endif
  sendJsonMapEntry(F("mqttconnected"), CBOOLEAN(state.mqtt.bConnected));
  sendJsonMapEntry(F("otgwconnected"), CBOOLEAN(state.otBus.bOnline));
  sendJsonMapEntry(F("picavailable"), CBOOLEAN(state.pic.bAvailable));
  sendJsonMapEntry(F("littlefsMounted"), CBOOLEAN(LittleFSmounted));
  
  sendEndJsonMap(F("health"));

} // sendHealth()

//=======================================================================
// Sends latest stored abnormal reboot/crash diagnostics if available.
// Returns: {"crashlog":{"available":true,"summary":"...","details":"..."}}
void sendDeviceCrashLog()
{
  char crashSummary[160] = {0};
  char crashDetails[160] = {0};
  bool hasCrashLog = readLatestCrashLog(crashSummary, sizeof(crashSummary), crashDetails, sizeof(crashDetails));

  sendStartJsonMap(F("crashlog"));
  sendJsonMapEntry(F("available"), hasCrashLog);
  sendJsonMapEntry(F("summary"), hasCrashLog ? crashSummary : "");
  sendJsonMapEntry(F("details"), hasCrashLog ? crashDetails : "");
  sendEndJsonMap(F("crashlog"));
}


//=======================================================================
// GET /api/v2/pic/settings
// Returns the cached PIC settings last queried via PR= commands.
// Triggers a new readout cycle (one PR= every 3s, ~45s full cycle).
// Empty string means "not yet queried" (or not supported by this firmware version).
// Source: Schelte Bron's OTGW firmware docs (https://otgw.tclcode.com/firmware.html)
void sendPICsettings()
{
  triggerPICsettingsReadout();  // re-read all settings from PIC
  sendStartJsonMap(F("pic_settings"));
  // Active settings
  sendJsonMapEntry(F("setpoint_override"),   state.picSettings.sSetpointOverride);
  sendJsonMapEntry(F("setback"),             state.picSettings.sSetback);
  sendJsonMapEntry(F("dhw_override"),        state.picSettings.sDhwOverride);
  // Hardware configuration
  sendJsonMapEntry(F("gpio"),                state.picSettings.sGpio);
  sendJsonMapEntry(F("gpio_states"),         state.picSettings.sGpioStates);
  sendJsonMapEntry(F("led"),                 state.picSettings.sLed);
  sendJsonMapEntry(F("tweaks"),              state.picSettings.sTweaks);
  sendJsonMapEntry(F("temp_sensor"),         state.picSettings.sTempSensor);
  sendJsonMapEntry(F("smart_power"),         state.picSettings.sSmartPower);
  sendJsonMapEntry(F("thermostat_detect"),   state.picSettings.sThermostatDetect);
  // Diagnostics
  sendJsonMapEntry(F("builddate"),           state.picSettings.sBuilddate);
  sendJsonMapEntry(F("clock_mhz"),           state.picSettings.sClockMHz);
  sendJsonMapEntry(F("reset_cause"),         state.picSettings.sResetCause);
  sendJsonMapEntry(F("standalone_interval"), state.picSettings.sStandaloneInterval);
  sendJsonMapEntry(F("voltage_ref"),         state.picSettings.sVoltageRef);
  sendEndJsonMap(F("pic_settings"));
} // sendPICsettings()

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
//=======================================================================
// Sends OT-direct (OTGW32) runtime status as JSON map.
// Parallel to sendPICsettings() — exposes OTGW32-specific diagnostics.
// Returns: {"otdirect_status":{"bypass":false,"stepup":true,...}}
void sendOTDirectStatus()
{
  sendStartJsonMap(F("otdirect_status"));
  // Operating mode
  {
    const char* modeStr = "gateway";
    if (state.otd.eMode == OTD_MODE_MONITOR) modeStr = "monitor";
    else if (state.otd.eMode == OTD_MODE_BYPASS) modeStr = "bypass";
    else if (state.otd.eMode == OTD_MODE_MASTER) modeStr = "master";
    else if (state.otd.eMode == OTD_MODE_LOOPBACK) modeStr = "loopback";
    sendJsonMapEntry(F("mode"),             modeStr);
  }
  // Hardware state
  sendJsonMapEntry(F("bypass"),           state.otd.bBypassActive);
  sendJsonMapEntry(F("stepup"),           state.otd.bStepUpEnabled);
  sendJsonMapEntry(F("monitor_mode"),     state.otd.bMonitorMode);
  sendJsonMapEntry(F("master_mode"),      state.otd.bMasterMode);
  // Thermostat connectivity
  sendJsonMapEntry(F("thermostat_connected"), state.otd.bThermostatConnected);
  sendJsonMapEntry(F("setback_active"),   state.otd.bSetbackActive);
  // Schedule statistics
  sendJsonMapEntry(F("schedule_total"),   state.otd.iScheduleTotal);
  sendJsonMapEntry(F("schedule_active"),  state.otd.iScheduleActive);
  sendJsonMapEntry(F("schedule_disabled"), state.otd.iScheduleDisabled);
  // Override status
  sendJsonMapEntry(F("overrides_active"), state.otd.iOverrideCount);
  // OT bus state
  sendJsonMapEntry(F("ot_online"),        state.otBus.bOnline);
  sendJsonMapEntry(F("thermostat"),       state.otBus.bThermostatState);
  sendJsonMapEntry(F("boiler"),           state.otBus.bBoilerState);
  // TASK-184: flame ratio metrics
  sendJsonMapEntry(F("flame_duty_pct"),         (int)getFlameRatioDuty());
  {
    char freqBuf[8];
    dtostrf(getFlameRatioFreq(), 1, 1, freqBuf);
    sendJsonMapEntry(F("flame_cycles_per_hour"), freqBuf);
  }
  sendEndJsonMap(F("otdirect_status"));
} // sendOTDirectStatus()
#endif

//=======================================================================
void sendPICFlashStatus()
{
  // Minimal PIC flash status endpoint for polling during flash
  // Returns: {"flashstatus":{"flashing":true|false,"progress":0-100,"filename":"...","error":"..."}}
  sendStartJsonMap(F("flashstatus"));
  sendJsonMapEntry(F("flashing"), state.flash.bPICactive);
  sendJsonMapEntry(F("progress"), state.flash.iPICprogress);
  sendJsonMapEntry(F("filename"), state.flash.sPICfile);
  sendJsonMapEntry(F("error"), state.flash.sError);
  sendEndJsonMap(F("flashstatus"));
} // sendPICFlashStatus()

//=======================================================================
void sendPICUpdateCheck()
{
  // On-demand PIC firmware update check.
  // Only called when the user opens the PIC firmware tab — never on a timer.
  // Makes an outbound HTTP HEAD request to otgw.tclcode.com.
  String latest = "";
  if (strcmp_P(state.pic.sDeviceid, PSTR("unknown")) != 0 && state.pic.sDeviceid[0] != '\0') {
    String picFile;
    if (strcmp_P(state.pic.sType, PSTR("diagnose")) == 0) {
      picFile = F("diagnose.hex");
    } else if (strcmp_P(state.pic.sType, PSTR("interface")) == 0) {
      picFile = F("interface.hex");
    } else {
      picFile = F("gateway.hex");
    }
    latest = checkforupdatepic(picFile);
  }
  bool updateAvailable = (latest.length() > 0 && latest != String(state.pic.sFwversion));
  sendStartJsonMap(F("pic_update"));
  sendJsonMapEntry(F("current"), state.pic.sFwversion);
  sendJsonMapEntry(F("latest"), latest.c_str());
  sendJsonMapEntry(F("update_available"), updateAvailable);
  sendEndJsonMap(F("pic_update"));
} // sendPICUpdateCheck()

//=======================================================================
void sendFilesystemHashCheck()
{
  // Read the hash stored in LittleFS and compare with the compiled-in firmware hash.
  // Uses the cached getFilesystemHash() — safe to call from an HTTP handler.
  const char* fsHash = getFilesystemHash();
  bool match = (fsHash[0] != '\0' &&
                strcasecmp(fsHash, _VERSION_GITHASH) == 0);
  sendStartJsonMap(F("filesystem_check"));
  sendJsonMapEntry(F("match"), match);
  sendJsonMapEntry(F("fw_hash"), _VERSION_GITHASH);
  sendJsonMapEntry(F("fs_hash"), fsHash);
  sendEndJsonMap(F("filesystem_check"));
} // sendFilesystemHashCheck()

//=======================================================================
void sendFlashStatus()
{
  // Unified flash status endpoint - minimal response with only fields used by frontend
  // Returns: {"flashstatus":{"flashing":bool,"pic_flashing":bool,"pic_progress":0-100,"pic_filename":"...","pic_error":"..."}}
  sendStartJsonMap(F("flashstatus"));
  sendJsonMapEntry(F("flashing"), isFlashing());
  if (isPICEnabled()) {
    sendJsonMapEntry(F("pic_flashing"), state.flash.bPICactive);
    sendJsonMapEntry(F("pic_progress"), state.flash.iPICprogress);
    sendJsonMapEntry(F("pic_filename"), state.flash.sPICfile);
    sendJsonMapEntry(F("pic_error"), state.flash.sError);
  }
  sendEndJsonMap(F("flashstatus"));
} // sendFlashStatus()


//=======================================================================
void sendDeviceTimeV2() 
{
  char buf[50];
  
  sendStartJsonMap(F("devtime"));
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf_P(buf, sizeof(buf), PSTR("%04d-%02d-%02d %02d:%02d:%02d"), myTime.year(), myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second());
  sendJsonMapEntry(F("dateTime"), buf); 
  sendJsonMapEntry(F("epoch"), (int)now);
  sendJsonMapEntry(F("message"), getStatusMessageText());
  sendJsonMapEntry(F("psmode"), state.otBus.bPSmode);
  sendJsonMapEntry(F("otgwsimulation"), state.debug.bOTGWSimulation);
  sendJsonMapEntry(F("freeheap"), platformFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), platformMaxFreeBlock());
  sendJsonMapEntry(F("networkmode"), networkModeName());
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    sendJsonMapEntry(F("apfallback"), true);
    sendJsonMapEntry(F("wifiquality"), 0);
  } else
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) {
    sendJsonMapEntry(F("wifiquality"), 100);
  } else
#endif
  {
    sendJsonMapEntry(F("wifiquality"), signal_quality_perc_quad(WiFi.RSSI()));
  }

  sendEndJsonMap(F("devtime"));

} // sendDeviceTimeV2()

//=======================================================================
void sendDeviceSettings()
{

  sendStartJsonMap(F("settings"));

  //sendJsonSettingObj("string",   settingString,   "p", sizeof(settingString)-1);  
  //sendJsonSettingObj("string",   settingString,   "s", sizeof(settingString)-1);
  //sendJsonSettingObj("float",    settingFloat,    "f", 0, 10,  5);
  //sendJsonSettingObj("intager",  settingInteger , "i", 2, 60);

  sendJsonSettingObj(F("hostname"), CSTR(settings.sHostname), "s", 32);
  { char ssidBuf[33]; strlcpy(ssidBuf, WiFi.SSID().c_str(), sizeof(ssidBuf)); sendJsonSettingObj(F("ssid"), ssidBuf, "r", 32); }
  sendJsonSettingObj(F("mqttenable"), settings.mqtt.bEnable, "b");
  sendJsonSettingObj(F("mqttbroker"), CSTR(settings.mqtt.sBroker), "s", 32);
  sendJsonSettingObj(F("mqttbrokerport"), settings.mqtt.iBrokerPort, "i", 0, 65535);
  sendJsonSettingObj(F("mqttuser"), CSTR(settings.mqtt.sUser), "s", 32);
  char mqttPasswordPlaceholder[sizeof("password=100")];
  snprintf_P(mqttPasswordPlaceholder,
             sizeof(mqttPasswordPlaceholder),
             PSTR("password=%u"),
             static_cast<unsigned>(strnlen(settings.mqtt.sPasswd, sizeof(settings.mqtt.sPasswd))));
  sendJsonSettingObj(F("mqttpasswd"), mqttPasswordPlaceholder, "p", 100);
  sendJsonSettingObj(F("mqtttoptopic"), CSTR(settings.mqtt.sTopTopic), "s", 15);
  sendJsonSettingObj(F("mqtthaprefix"), CSTR(settings.mqtt.sHaprefix), "s", 20);
  sendJsonSettingObj(F("mqttharebootdetection"), settings.mqtt.bHaRebootDetect, "b");
  sendJsonSettingObj(F("mqttuniqueid"), CSTR(settings.mqtt.sUniqueid), "s", 20);
  sendJsonSettingObj(F("mqttotmessage"), settings.mqtt.bOTmessage, "b");
  sendJsonSettingObj(F("mqttinterval"), settings.mqtt.iInterval, "i", 0, 3600);
  sendJsonSettingObj(F("mqttseparatesources"), settings.mqtt.bSeparateSources, "b");
  sendJsonSettingObj(F("ntpenable"), settings.ntp.bEnable, "b");
  sendJsonSettingObj(F("ntptimezone"), CSTR(settings.ntp.sTimezone), "s", 50);
  sendJsonSettingObj(F("ntphostname"), CSTR(settings.ntp.sHostname), "s", 50);
  sendJsonSettingObj(F("ntpsendtime"), settings.ntp.bSendtime, "b");
  sendJsonSettingObj(F("ledblink"), settings.bLEDblink, "b");
  sendJsonSettingObj(F("darktheme"), settings.bDarkTheme, "b");
  sendJsonSettingObj(F("nightlyrestart"), settings.bNightlyRestart, "b");
  sendJsonSettingObj(F("nightlyrestarthour"), (int)settings.iRestartHour, "i", 0, 23);
  sendJsonSettingObj(F("ui_autoscroll"), settings.ui.bAutoScroll, "b");
  sendJsonSettingObj(F("ui_timestamps"), settings.ui.bShowTimestamp, "b");
  sendJsonSettingObj(F("ui_capture"), settings.ui.bCaptureMode, "b");
  sendJsonSettingObj(F("ui_autoscreenshot"), settings.ui.bAutoScreenshot, "b");
  sendJsonSettingObj(F("ui_autodownloadlog"), settings.ui.bAutoDownloadLog, "b");
  sendJsonSettingObj(F("ui_autoexport"), settings.ui.bAutoExport, "b");
  sendJsonSettingObj(F("ui_graphtimewindow"), settings.ui.iGraphTimeWindow, "i", 0, 1440);
  sendJsonSettingObj(F("gpiosensorsenabled"), settings.sensors.bEnabled, "b");
  sendJsonSettingObj(F("gpiosensorslegacyformat"), settings.sensors.bLegacyFormat, "b");
  sendJsonSettingObj(F("gpiosensorspin"), settings.sensors.iPin, "i", 0, 16);
  sendJsonSettingObj(F("gpiosensorsinterval"), settings.sensors.iInterval, "i", 5, 65535);
  sendJsonSettingObj(F("s0counterenabled"), settings.s0.bEnabled, "b");
  sendJsonSettingObj(F("s0counterpin"), settings.s0.iPin, "i", 1, 16);
  sendJsonSettingObj(F("s0counterdebouncetime"), settings.s0.iDebounceTime, "i", 0, 1000);
  sendJsonSettingObj(F("s0counterpulsekw"), settings.s0.iPulsekw, "i", 1, 5000);
  sendJsonSettingObj(F("s0counterinterval"), settings.s0.iInterval, "i", 5, 65535);
  sendJsonSettingObj(F("gpiooutputsenabled"), settings.outputs.bEnabled, "b");
  sendJsonSettingObj(F("gpiooutputspin"), settings.outputs.iPin, "i", 0, 16);
  sendJsonSettingObj(F("gpiooutputstriggerbit"), settings.outputs.iTriggerBit, "i", 0, 16);
  sendJsonSettingObj(F("otgwcommandenable"), settings.picBoot.bEnable, "b");
  sendJsonSettingObj(F("otgwcommands"), CSTR(settings.picBoot.sCommands), "s", 128);
  sendJsonSettingObj(F("webhookenable"), settings.webhook.bEnabled, "b");
  sendJsonSettingObj(F("webhookurlon"), CSTR(settings.webhook.sURLon), "s", 100);
  sendJsonSettingObj(F("webhookurloff"), CSTR(settings.webhook.sURLoff), "s", 100);
  sendJsonSettingObj(F("webhooktriggerbit"), settings.webhook.iTriggerBit, "i", 0, 15);
  sendJsonSettingObj(F("webhookpayload"), CSTR(settings.webhook.sPayload), "s", 200);
  sendJsonSettingObj(F("webhookcontenttype"), CSTR(settings.webhook.sContentType), "s", 31);
  // --- SAT settings ---
  sendJsonSettingObj(F("satenabled"), settings.sat.bEnabled, "b");
  sendJsonSettingObj(F("satsystem"), settings.sat.iHeatingSystem, "i", 0, 3);  // 0=auto,1=radiators,2=heat_pump,3=underfloor
  sendJsonSettingObj(F("satmanufacturer"), settings.sat.iManufacturer, "i", 0, SAT_MFR_COUNT - 1);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fTargetTemp, 1, 1, tmpBuf);
    sendJsonSettingObj(F("sattargettemp"), tmpBuf, "f", 5, 30);
    dtostrf(settings.sat.fTargetTempStep, 1, 1, tmpBuf);
    sendJsonSettingObj(F("sattempstep"), tmpBuf, "f", 0.1, 1.0);
    dtostrf(settings.sat.fHeatingCurveCoeff, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satcoefficient"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fDeadband, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satdeadband"), tmpBuf, "f", 0, 2);
  }
  sendJsonSettingObj(F("satinterval"), settings.sat.iControlInterval, "i", 10, 300);
  sendJsonSettingObj(F("satexternaltemp"), settings.sat.bUseExternalTemp, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPresetComfort, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpresetcomfort"), tmpBuf, "f", 15, 28);
    dtostrf(settings.sat.fPresetEco, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpreseteco"), tmpBuf, "f", 10, 22);
    dtostrf(settings.sat.fPresetAway, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpresetaway"), tmpBuf, "f", 5, 18);
  }
  sendJsonSettingObj(F("satpwmautoswitch"), settings.sat.bPwmAutoSwitch, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPresetSleep, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpresetsleep"), tmpBuf, "f", 5, 25);
    dtostrf(settings.sat.fPresetActivity, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpresetactivity"), tmpBuf, "f", 5, 20);
    dtostrf(settings.sat.fPresetHome, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satpresethome"), tmpBuf, "f", 10, 25);
  }
  sendJsonSettingObj(F("satmaxmodulation"), settings.sat.iMaxRelModulation, "i", 0, 100);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fOvpValue, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satovpvalue"), tmpBuf, "f", 0, 90);
  }
  sendJsonSettingObj(F("satovpenabled"), settings.sat.bOvpEnabled, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fOvershootMargin, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satovershootmargin"), tmpBuf, "f", 0, 5);
  }
  // --- SAT Weather settings (Task #50) ---
  sendJsonSettingObj(F("satweatherenable"), settings.sat.bWeatherEnable, "b");
  {
    char tmpBuf[12];
    dtostrf(settings.sat.fWeatherLat, 1, 4, tmpBuf);
    sendJsonSettingObj(F("satweatherlat"), tmpBuf, "f", -90, 90);
    dtostrf(settings.sat.fWeatherLon, 1, 4, tmpBuf);
    sendJsonSettingObj(F("satweatherlon"), tmpBuf, "f", -180, 180);
  }
  sendJsonSettingObj(F("satweatherinterval"), settings.sat.iWeatherInterval, "i", 300, 3600);
  // --- SAT Power/Energy settings (Task #45) ---
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fBoilerCapacity, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satboilercapacity"), tmpBuf, "f", 1, 100);
  }
  // --- SAT Preset Sync settings (Task #46) ---
  sendJsonSettingObj(F("satpresetsync"), settings.sat.bPresetSync, "b");
  sendJsonSettingObj(F("satpresetsynctopic"), CSTR(settings.sat.sPresetSyncTopic), "s", 64);
  // --- SAT Simulation settings (Task #37) ---
  sendJsonSettingObj(F("satsimulation"), settings.sat.bSimulation, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSimHeatRate, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satsimheatrate"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fSimCoolRate, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satsimcoolrate"), tmpBuf, "f", 0, 5);
  }
  // --- SAT Thermal Drop Learning (Task #21) ---
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fThermalCoeff, 1, 4, tmpBuf);
    sendJsonSettingObj(F("satthermalcoeff"), tmpBuf, "f", 0, 1);
  }
  // --- SAT Solar Gain settings (Task #23) ---
  sendJsonSettingObj(F("satsolargain"), settings.sat.bSolarGainEnable, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSolarMinRiseRate, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satsolarminrise"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fSolarSetpointOffset, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satsolaroffset"), tmpBuf, "f", 0, 10);
  }
  // --- SAT Summer Simmer settings (Task #24) ---
  sendJsonSettingObj(F("satsummersimmer"), settings.sat.bSummerSimmer, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSummerThreshold, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satsummerthreshold"), tmpBuf, "f", 5, 35);
  }
  sendJsonSettingObj(F("satsummerminhours"), settings.sat.iSummerMinHours, "i", 1, 48);
  // --- SAT Thermal Comfort settings (Task #28/#47) ---
  sendJsonSettingObj(F("satcomfortadjust"), settings.sat.bComfortAdjust, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fComfortHumidity, 1, 0, tmpBuf);
    sendJsonSettingObj(F("satcomforthumidity"), tmpBuf, "f", 10, 90);
    dtostrf(settings.sat.fComfortMaxOffset, 1, 1, tmpBuf);
    sendJsonSettingObj(F("satcomfortmaxoffset"), tmpBuf, "f", 0, 3);
  }
  // --- SAT Multi-area settings (Task #25) ---
  sendJsonSettingObj(F("satmultiarea"), settings.sat.bMultiArea, "b");
  sendJsonSettingObj(F("satmultiareacount"), settings.sat.iMultiAreaCount, "i", 0, 4);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAreaWeight[0], 1, 2, tmpBuf);
    sendJsonSettingObj(F("satareaweight0"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[1], 1, 2, tmpBuf);
    sendJsonSettingObj(F("satareaweight1"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[2], 1, 2, tmpBuf);
    sendJsonSettingObj(F("satareaweight2"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[3], 1, 2, tmpBuf);
    sendJsonSettingObj(F("satareaweight3"), tmpBuf, "f", 0, 10);
  }
  // --- SAT PID Auto-Tuning settings (Task #27) ---
  sendJsonSettingObj(F("satautotune"), settings.sat.bAutoTune, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAutoTuneRate, 1, 3, tmpBuf);
    sendJsonSettingObj(F("satautotunerate"), tmpBuf, "f", 0, 1);
  }
  // --- SAT Python parity settings (Task #82) ---
  sendJsonSettingObj(F("satsensormaxage"), (int32_t)settings.sat.iSensorMaxAgeS, "i", 60, 86400);
  sendJsonSettingObj(F("saterrormon"), settings.sat.bErrorMonitoring, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAutoGainsValue, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satautogains"), tmpBuf, "f", 0, 10);
  }
  sendJsonSettingObj(F("satheatingmode"), settings.sat.iHeatingMode, "i", 0, 1);
  sendJsonSettingObj(F("satcyclesperhour"), settings.sat.iCyclesPerHour, "i", 2, 6);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fValveOffset, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satvalveoffset"), tmpBuf, "f", -1, 1);
  }
  sendJsonSettingObj(F("satsolarfreezeint"), settings.sat.bSolarFreezeIntegral, "b");
#if defined(ESP32)
  // --- SAT BLE Sensor settings (Task #20, ESP32 only) ---
  sendJsonSettingObj(F("satbleenable"), settings.sat.bBleEnable, "b");
  sendJsonSettingObj(F("satblemac"), CSTR(settings.sat.sBleMAC), "s", 17);
  sendJsonSettingObj(F("satbleinterval"), settings.sat.iBleInterval, "i", 10, 300);
#endif
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  // --- OT-Direct settings (OTGW32 only) ---
  sendJsonSettingObj(F("otdmode"), settings.otd.iMode, "i", 0, 4);
  sendJsonSettingObj(F("otdautodetect"), settings.otd.bAutoDetect, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.otd.fSetbackTemp, 1, 1, tmpBuf);
    sendJsonSettingObj(F("otdsetbacktemp"), tmpBuf, "f", 1, 30);
  }
  sendJsonSettingObj(F("otdsetbacktimeout"), settings.otd.iSetbackTimeout, "i", 5, 255);
  sendJsonSettingObj(F("otdenableslave"), settings.otd.bEnableSlave, "b");
  sendJsonSettingObj(F("otdsummermode"), settings.otd.bSummerMode, "b");
  sendJsonSettingObj(F("otdfailsafe"), settings.otd.bFailSafe, "b");
  sendJsonSettingObj(F("otdmsginterval"), settings.otd.iMsgInterval, "i", 100, 1275);
  sendJsonSettingObj(F("otdhasbypassrelay"), settings.otd.bHasBypassRelay, "b");
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // --- Ethernet settings (OTGW32 only) ---
  sendJsonSettingObj(F("ethstaticip"), settings.eth.bStaticIP, "b");
  sendJsonSettingObj(F("ethipaddress"), CSTR(settings.eth.sIPaddress), "s", 15);
  sendJsonSettingObj(F("ethgateway"), CSTR(settings.eth.sGateway), "s", 15);
  sendJsonSettingObj(F("ethsubnet"), CSTR(settings.eth.sSubnet), "s", 15);
  sendJsonSettingObj(F("ethdns"), CSTR(settings.eth.sDNS), "s", 15);
#endif
  char httpPasswordPlaceholder[sizeof("password=40")];
  snprintf_P(httpPasswordPlaceholder,
             sizeof(httpPasswordPlaceholder),
             PSTR("password=%u"),
             static_cast<unsigned>(strnlen(settings.sHTTPpasswd, sizeof(settings.sHTTPpasswd))));
  sendJsonSettingObj(F("httppasswd"), httpPasswordPlaceholder, "p", 40);

  sendEndJsonMap(F("settings"));

} // sendDeviceSettings()


// PROGMEM whitelist of recognised setting field names (canonical lower-case).
// Keep sorted alphabetically for readability; lookup is linear (small list).
static const char* const PROGMEM knownSettings[] = {
  "darktheme",
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  "ethdns", "ethgateway", "ethipaddress", "ethstaticip", "ethsubnet",
#endif
  "gpiooutputsenabled", "gpiooutputspin", "gpiooutputstriggerbit",
  "gpiosensorsenabled", "gpiosensorsinterval", "gpiosensorslegacyformat", "gpiosensorspin",
  "hostname", "httppasswd", "ledblink", "nightlyrestart", "nightlyrestarthour",
  "mqttbroker", "mqttbrokerport", "mqttenable", "mqtthaprefix", "mqttharebootdetection",
  "mqttinterval", "mqttotmessage", "mqttpasswd", "mqttseparatesources",
  "mqtttoptopic", "mqttuniqueid", "mqttuser",
  "ntpenable", "ntphostname", "ntpsendtime", "ntptimezone",
  "otdautodetect", "otdenableslave", "otdfailsafe", "otdmode", "otdmsginterval",
  "otdsetbacktemp", "otdsetbacktimeout", "otdsummermode",
  "otgwcommandenable", "otgwcommands",
  "s0counterdebouncetime", "s0counterenabled", "s0counterinterval", "s0counterpin", "s0counterpulsekw",
  "satareaweight0", "satareaweight1", "satareaweight2", "satareaweight3",
  "satautotune", "satautotunerate",
  "satbleenable", "satbleinterval", "satblemac",
  "satboilercapacity", "satcoefficient", "satcomfortadjust", "satcomforthumidity", "satcomfortmaxoffset",
  "satdeadband", "satenabled", "satexternaltemp",
  "satinterval", "satmanufacturer", "satmultiarea", "satmultiareacount",
  "satovershootmargin", "satpresetaway", "satpresetcomfort", "satpreseteco",
  "satpresetsync", "satpresetsynctopic",
  "satpwmautoswitch", "satsimcoolrate", "satsimheatrate", "satsimulation",
  "satsolargain", "satsolarminrise", "satsolaroffset",
  "satsummerminhours", "satsummersimmer", "satsummerthreshold",
  "satsystem", "sattargettemp", "sattempstep", "satthermalcoeff",
  "satweatherenable", "satweatherinterval", "satweatherlat", "satweatherlon",
  "ui_autodownloadlog", "ui_autoexport", "ui_autoscreenshot", "ui_autoscroll",
  "ui_capture", "ui_graphtimewindow", "ui_timestamps",
  "webhookcontenttype", "webhookenable", "webhookenabled",
  "webhookpayload", "webhooktriggerbit", "webhookurloff", "webhookurlon",
};

static bool isKnownSetting(const char* field) {
  for (size_t i = 0; i < sizeof(knownSettings) / sizeof(knownSettings[0]); i++) {
    if (strcasecmp(field, knownSettings[i]) == 0) return true;
  }
  return false;
}

//=======================================================================
void postSettings()
{
  //------------------------------------------------------------
  // json string: {"name":"settingInterval","value":9}
  // json string: {"name":"settings.sHostname","value":"abc"}
  // json string: {"name":"darktheme","value":true}
  //------------------------------------------------------------
  const String& body = httpServer.arg(0);
  if (body.length() == 0 || !body.startsWith("{")) {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  char field[50];
  if (!extractJsonField(body, F("name"), field, sizeof(field))) {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Missing name\"}"));
    return;
  }

  if (!isKnownSetting(field)) {
    DebugTf(PSTR("postSettings: unknown field [%s], rejected\r\n"), field);
    httpServer.send(400, F("application/json"), F("{\"error\":\"Unknown setting name\"}"));
    return;
  }

  // 150 bytes covers the largest setting value (settingOTGWcommands, max 128 chars).
  char newValue[150];
  if (!extractJsonField(body, F("value"), newValue, sizeof(newValue))) {
    httpServer.send(400, F("application/json"), F("{\"error\":\"Missing value\"}"));
    return;
  }

  // Mask password fields in REST debug log
  if (strcasecmp_P(field, PSTR("httppasswd")) == 0 ||
      strcasecmp_P(field, PSTR("mqttpasswd")) == 0) {
    RESTDebugTf(PSTR("--> field[%s] => newValue[***]\r\n"), field);
  } else {
    RESTDebugTf(PSTR("--> field[%s] => newValue[%s]\r\n"), field, newValue);
  }
  updateSetting(field, newValue);
  // Synchronous flush: persist to flash NOW so the 200 OK is truthful.
  // The deferred timer still handles MQTT/NTP command updates, but HTTP
  // saves must be durable before we confirm success to the browser.
  flushSettings();
  httpServer.send(200, F("application/json"), body);

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
  const String& body = httpServer.arg(F("plain"));

  // Validate: body must be a non-empty JSON object (starts with '{', ends with '}')
  size_t bodyLen = body.length();
  if (bodyLen == 0 || body[0] != '{' || body[bodyLen - 1] != '}') {
    httpServer.send(400, F("application/json"),
      F("{\"success\":false,\"error\":\"Empty or invalid JSON body\"}"));
    return;
  }
  // Limit file size to prevent filling LittleFS (labels are short strings)
  if (bodyLen > 2048) {
    httpServer.send(400, F("application/json"),
      F("{\"success\":false,\"error\":\"Body too large (max 2048 bytes)\"}"));
    return;
  }
  // Basic structural check: all keys and values must be quoted strings.
  // Scan for unquoted colons preceded/followed by quoted strings.
  {
    const char* p = body.c_str() + 1; // skip opening '{'
    while (*p && *p != '}') {
      while (*p == ' ' || *p == '\r' || *p == '\n' || *p == ',') p++;
      if (*p == '}') break;
      if (*p != '"') {
        httpServer.send(400, F("application/json"),
          F("{\"success\":false,\"error\":\"Invalid JSON: expected quoted key\"}"));
        return;
      }
      // skip key string
      p++;
      while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
      if (*p != '"') { httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: unterminated key\"}")); return; }
      p++; // skip closing quote
      while (*p == ' ') p++;
      if (*p != ':') { httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: expected colon\"}")); return; }
      p++; // skip colon
      while (*p == ' ') p++;
      if (*p != '"') { httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: expected quoted value\"}")); return; }
      // skip value string
      p++;
      while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
      if (*p != '"') { httpServer.send(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: unterminated value\"}")); return; }
      p++; // skip closing quote
    }
  }

  // Write validated JSON to file
  File labelsFile = LittleFS.open(F("/dallas_labels.ini"), "w");
  if (!labelsFile) {
    httpServer.send_P(500, PSTR("application/json"),
      PSTR("{\"success\":false,\"error\":\"Failed to open file for writing\"}"));
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
  sendCorsOriginHeader();
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
