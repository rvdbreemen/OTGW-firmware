/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v2.0.0-alpha.118
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

// A: Boot-time flash & filesystem values cached once at startup.
// Avoids repeated SPI-flash queries on every /api/v2/device/info call.
// Uses platform abstraction functions so this compiles on both ESP8266 and ESP32.
struct BootFlashCache {
  uint32_t    sketchSize;
  uint32_t    freeSketchSpace;
  char        flashChipId[9];     // formatted as "%08X"
  float       flashChipSizeMB;
  float       flashChipRealSizeMB;
  float       flashChipSpeedMHz;
  uint8_t     flashChipModeIdx;   // index into flashMode[] array
  float       littleFSSizeMB;
};
static BootFlashCache sBootFlash;

void cacheBootFlashInfo() {
  sBootFlash.sketchSize          = platformSketchSize();
  sBootFlash.freeSketchSpace     = platformFreeSketchSpace();
  snprintf_P(sBootFlash.flashChipId, sizeof(sBootFlash.flashChipId),
             PSTR("%08X"), (unsigned int)platformFlashChipId());
  sBootFlash.flashChipSizeMB     = platformFlashChipSize()     / 1024.0f / 1024.0f;
  sBootFlash.flashChipRealSizeMB = platformFlashChipRealSize() / 1024.0f / 1024.0f;
  sBootFlash.flashChipSpeedMHz   = floorf(platformFlashChipSpeed() / 1000.0f / 1000.0f);
  sBootFlash.flashChipModeIdx    = platformFlashChipMode();
  FSInfo fsinfo;
  platformFSInfo(fsinfo);
  sBootFlash.littleFSSizeMB      = fsinfo.totalBytes / (1024.0f * 1024.0f);
}

//=======================================================================
// CSRF same-origin helper for admin operations (ADR-056 §2)
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

// HTTP Basic Auth helper (ADR-056)
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
static void handleDiscovery(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleDebugDump(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
// TASK-585: WiFi network scan
static void handleNetwork(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);

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
    // Start "devices" sub-object — use chunked JSON.
    // restSendContent(P) keeps these bytes in the ESP32 coalescing buffer
    // (jsonStuff.ino sTxBuf); a raw httpServer.sendContent would flush ahead
    // of the buffered sendJsonMapEntry wrapper and scramble the JSON.
    restSendContentP(PSTR(",\r\n  \"devices\": {"));
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char *addr = getDallasAddress(DallasrealDevice[i].addr);
      if (!addr) continue;
      char entry[100];
      snprintf_P(entry, sizeof(entry),
                 PSTR("%s\r\n    \"%s\": {\"temp\": %4.1f, \"epoch\": %u}"),
                 (i > 0) ? "," : "",
                 addr, DallasrealDevice[i].tempC, (uint32_t)DallasrealDevice[i].lasttime);
      restSendContent(entry);
    }
    restSendContentP(PSTR("\r\n  }"));
  }

  // S0 pulse counter
  restSendContentP(PSTR(",\r\n  \"s0\": {"));
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
    restSendContent(s0buf);
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
    // TASK-438: GW=0 is now monitor (PIC parity); bypass moved to GW=P alias.
    if (modeStr == F("gateway"))       addCommandToQueue("GW=1", 4, true);
    else if (modeStr == F("monitor"))  addCommandToQueue("GW=0", 4, true);
    else if (modeStr == F("bypass"))   addCommandToQueue("GW=P", 4, true);
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
      // TASK-582: CH hysteresis deadband
      sendJsonMapEntry(F("hysteresis_enable"), settings.otd.bHysteresisEnable);
      sendJsonMapEntry(F("hysteresis"),        settings.otd.fHysteresis);
      // TASK-584: ventilation override persistence
      sendJsonMapEntry(F("vent_enable"),      settings.otd.bVentEnable);
      sendJsonMapEntry(F("open_bypass"),      settings.otd.bOpenBypass);
      sendJsonMapEntry(F("auto_bypass"),      settings.otd.bAutoBypass);
      sendJsonMapEntry(F("free_vent_enable"), settings.otd.bFreeVentEnable);
      sendJsonMapEntry(F("vent_setpoint"),    (int)settings.otd.iVentSetpoint);
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
      // TASK-582: CH hysteresis deadband settings
      if (httpServer.hasArg("hysteresisenable")) updateSetting("OTDhysteresisenable", httpServer.arg("hysteresisenable").c_str());
      if (httpServer.hasArg("hysteresis"))       updateSetting("OTDhysteresis", httpServer.arg("hysteresis").c_str());
      // TASK-584: ventilation override persistence settings
      if (httpServer.hasArg("ventenable"))    updateSetting("OTDventenable", httpServer.arg("ventenable").c_str());
      if (httpServer.hasArg("openbypass"))    updateSetting("OTDopenbypass", httpServer.arg("openbypass").c_str());
      if (httpServer.hasArg("autobypass"))    updateSetting("OTDautobypass", httpServer.arg("autobypass").c_str());
      if (httpServer.hasArg("freeventenable")) updateSetting("OTDfreeventenable", httpServer.arg("freeventenable").c_str());
      if (httpServer.hasArg("ventsetpoint"))  updateSetting("OTDventsetpoint", httpServer.arg("ventsetpoint").c_str());
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
  } else if (strcmp_P(words[4], PSTR("boiler-support")) == 0) {
    // TASK-692 port (dev TASK-686): GET /api/v2/otgw/boiler-support ->
    // unsupported_read / unsupported_write arrays sourced from the in-RAM
    // bitmaps populated by processOT. Streamed in chunks so even pathological
    // boilers (hundreds of unsupported ids) do not need a large stack buffer.
    // TASK-696: leading comma emitted via sendContent(F(",")) so we ship one
    // PSTR format string per row instead of two — saves flash on ESP32.
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    httpServer.send_P(200, PSTR("application/json"), PSTR("{\"unsupported_read\":["));
    bool first = true;
    char ent[160];
    for (int i = 0; i <= 255; i++) {
      if (!isBoilerMsgIdUnsupportedRead((uint8_t)i)) continue;
      OTlookup_t item;
      const char* label = "Unknown";
      const char* friendly = "";
      if (i <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[i], item); label = item.label; friendly = item.friendlyname; }
      if (!first) httpServer.sendContent(F(","));
      snprintf_P(ent, sizeof(ent), PSTR("{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}"), i, label, friendly);
      httpServer.sendContent(ent);
      first = false;
    }
    httpServer.sendContent_P(PSTR("],\"unsupported_write\":["));
    first = true;
    for (int i = 0; i <= 255; i++) {
      if (!isBoilerMsgIdUnsupportedWrite((uint8_t)i)) continue;
      OTlookup_t item;
      const char* label = "Unknown";
      const char* friendly = "";
      if (i <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[i], item); label = item.label; friendly = item.friendlyname; }
      if (!first) httpServer.sendContent(F(","));
      snprintf_P(ent, sizeof(ent), PSTR("{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}"), i, label, friendly);
      httpServer.sendContent(ent);
      first = false;
    }
    httpServer.sendContent_P(PSTR("]}"));
    httpServer.sendContent(F(""));
  } else if (strcmp_P(words[4], PSTR("ot-support")) == 0) {
    // TASK-694 port (dev TASK-689): GET /api/v2/otgw/ot-support -> bilateral
    // OT support map. Compact mode — only msgIDs where at least one of the
    // six bitmaps has the bit set. One streamed JSON object per row, no
    // full-payload allocation.
    // TASK-696: single PSTR format per row (leading comma via sendContent).
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    httpServer.send_P(200, PSTR("application/json"), PSTR("{\"msgids\":["));
    bool first = true;
    char row[160];
    for (int i = 0; i <= 255; i++) {
      const uint8_t id = (uint8_t)i;
      const bool tsR  = isThermostatMsgIdSentRead(id);
      const bool tsW  = isThermostatMsgIdSentWrite(id);
      const bool blAR = isBoilerMsgIdAckedRead(id);
      const bool blAW = isBoilerMsgIdAckedWrite(id);
      const bool blUR = isBoilerMsgIdUnsupportedRead(id);
      const bool blUW = isBoilerMsgIdUnsupportedWrite(id);
      if (!(tsR || tsW || blAR || blAW || blUR || blUW)) continue;
      OTlookup_t item;
      const char* label = "Unknown";
      if (id <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[id], item); label = item.label; }
      if (!first) httpServer.sendContent(F(","));
      snprintf_P(row, sizeof(row),
                 PSTR("{\"id\":%u,\"label\":\"%s\",\"tsR\":%s,\"tsW\":%s,\"blAR\":%s,\"blAW\":%s,\"blUR\":%s,\"blUW\":%s}"),
                 id, label,
                 tsR  ? "true" : "false", tsW  ? "true" : "false",
                 blAR ? "true" : "false", blAW ? "true" : "false",
                 blUR ? "true" : "false", blUW ? "true" : "false");
      httpServer.sendContent(row);
      first = false;
    }
    httpServer.sendContent_P(PSTR("]}"));
    httpServer.sendContent(F(""));
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

#if defined(ESP32)
// TASK-508: pull two named fields from a JSON body in one call. Trivial
// wrapper over extractJsonField() (line 2167) so handleSAT label/forget
// branches stay short.
static bool satExtractTwoFields(const char* body,
                                const __FlashStringHelper* k1, char* v1, size_t sz1,
                                const __FlashStringHelper* k2, char* v2, size_t sz2)
{
  if (!body) return false;
  if (!extractJsonField(body, k1, v1, sz1)) return false;
  if (!extractJsonField(body, k2, v2, sz2)) return false;
  return true;
}
#endif

//=== SAT API handler ===
// GET  /api/v2/sat/status               — returns full SAT runtime state
// GET  /api/v2/sat/status?detail=full   — returns extended health diagnostics
// POST /api/v2/sat/target               — set target temperature (body: "21.0" or {"value":"21.0"})
// POST /api/v2/sat/externaltemp         — push indoor temp
// POST /api/v2/sat/externaloutdoor      — push outdoor temp
// POST /api/v2/sat/pvsurplus            — push PV-surplus power in W (TASK-640)
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
  // POST /api/v2/sat/pvsurplus — push PV-surplus power (TASK-640).
  // Body: "1500" or {"value":"1500"} or {"value":1500}. Range 0-50000 W.
  else if (strcasecmp_P(sub, PSTR("pvsurplus")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (httpServer.hasArg(F("plain"))) {
      val = satExtractPostValue(httpServer.arg(F("plain")).c_str(), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandlePvSurplus(val)) {
      sendApiError(400, F("Invalid or missing numeric value (0-50000 W)"));
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
#if HAS_SAT_BLE
  // TASK-508: BLE roster — discovery + select + label + forget.
  // TASK-742: gated on HAS_SAT_BLE (not raw ESP32); ESP8266 has no BLE roster.
  // Sub-routes:
  //   GET  /api/v2/sat/ble/discovery           — stream the roster as JSON
  //   POST /api/v2/sat/ble/select   {mac}      — promote roster MAC to active sensor
  //   POST /api/v2/sat/ble/label    {mac,label}— set persistent label for a roster slot
  //   POST /api/v2/sat/ble/forget   {mac}      — drop slot + clean up HA discovery
  else if (strcasecmp_P(sub, PSTR("ble")) == 0) {
    if (wc < 6) { sendApiError(400, F("Missing BLE sub-action (discovery/select/label/forget)")); return; }
    const char* act = words[5];

    if (strcasecmp_P(act, PSTR("discovery")) == 0) {
      if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
      satBLERosterSendJSON();
    }
    else if (strcasecmp_P(act, PSTR("select")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
      if (!httpServer.hasArg(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      char macBuf[18];
      if (!extractJsonField(httpServer.arg(F("plain")).c_str(),
                            F("mac"), macBuf, sizeof(macBuf))) {
        sendApiError(400, F("Missing 'mac' field"));
        return;
      }
      if (!satBLERosterSelect(macBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else if (strcasecmp_P(act, PSTR("label")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
      if (!httpServer.hasArg(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      char macBuf[18];
      char labelBuf[32];
      if (!satExtractTwoFields(httpServer.arg(F("plain")).c_str(),
                                F("mac"),   macBuf,   sizeof(macBuf),
                                F("label"), labelBuf, sizeof(labelBuf))) {
        sendApiError(400, F("Missing 'mac' or 'label' field"));
        return;
      }
      if (!satBLERosterSetLabel(macBuf, labelBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else if (strcasecmp_P(act, PSTR("forget")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT && method != HTTP_DELETE) {
        sendApiMethodNotAllowed(F("POST, PUT, DELETE"));
        return;
      }
      char macBuf[18];
      if (!httpServer.hasArg(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      if (!extractJsonField(httpServer.arg(F("plain")).c_str(),
                            F("mac"), macBuf, sizeof(macBuf))) {
        sendApiError(400, F("Missing 'mac' field"));
        return;
      }
      if (!satBLERosterForget(macBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else {
      sendApiError(404, F("Unknown BLE sub-action (discovery/select/label/forget)"));
    }
  }
#endif
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
    if (wc == 5 && strcasecmp_P(words[5], PSTR("needs-setup")) == 0) {
      if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
      // "Needs setup" means: after startup grace, outside temp still missing
      // AND weather not yet valid. Browser uses this to trigger the wizard.
      // TASK-511: canonical struct is OTcurrentSystemState (PR #559 used a
      // non-existent state.sat.Toutside). See OTGW-Core.ino:3490 where OT
      // message ID 27 populates this field.
      const bool needs = (millis() >= 300000UL)
                      && (OTcurrentSystemState.Toutside == 0.0f)
                      && (!state.sat.weather.bValid);
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
      sendStartJsonMap(F(""));
      sendJsonMapEntry(F("needs_setup"), needs);
      sendJsonMapEntry(F("has_key"), (settings.sat.sWeatherApiKey[0] != '\0'));
      sendEndJsonMap(F(""));
      return;
    }
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
    } else if (strcasecmp_P(settingName, PSTR("dhw_enable")) == 0) {
      // TASK-516: master DHW enable (HW= command). updateSetting() routes to
      // the SATdhwenable case in settingStuff.ino, which both persists the
      // value and queues HW=0/HW=1 when MsgID 3 HB3=1 (storage tank).
      updateSetting("SATdhwenable", val); handled = true;
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
    } else if (strcasecmp_P(settingName, PSTR("ble_failover")) == 0) {
      updateSetting("SATblefailover", val); handled = true;
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
  // --- TASK-586: Heating curve calibration markers ---
  // GET  /api/v2/sat/markers           — return all markers as JSON array
  // POST /api/v2/sat/markers           — add marker; body: {"outside_temp":5.0,"flow_temp":55.0,"label":"optional"}
  // DELETE /api/v2/sat/markers/<id>    — delete marker by integer id
  else if (strcasecmp_P(sub, PSTR("markers")) == 0) {
    static const char kSatMarkersFile[] PROGMEM = "/sat_markers.json";
    static const int  kSatMarkersMax    = 20;
    char fname[24];
    strlcpy_P(fname, kSatMarkersFile, sizeof(fname));

    // Stream one JSON object {…} from f into buf (nul-terminated).
    // Advances past any leading whitespace/commas/'['. Returns false at ']' or EOF.
    auto readObj = [](File& f, char* buf, size_t sz) -> bool {
      while (f.available()) {
        char c = (char)f.read();
        if (c == '{') {
          buf[0] = '{'; size_t n = 1; int depth = 1;
          while (f.available() && depth > 0 && n < sz - 1) {
            char c2 = (char)f.read();
            buf[n++] = c2;
            if (c2 == '{') depth++;
            else if (c2 == '}') depth--;
          }
          buf[n] = '\0';
          return depth == 0;
        }
        if (c == ']') return false;
      }
      return false;
    };

    if (method == HTTP_GET) {
      // Stream file directly; return empty array if not present
      File f = LittleFS.open(fname, "r");
      if (!f) {
        httpServer.send(200, F("application/json"), F("[]"));
      } else {
        httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
        httpServer.streamFile(f, F("application/json"));
        f.close();
      }
    }
    else if (method == HTTP_POST) {
      // Parse outside_temp, flow_temp, optional label from JSON body
      if (!httpServer.hasArg(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      const String& bodyStr = httpServer.arg(F("plain"));
      const char* body = bodyStr.c_str();
      char otBuf[10], ftBuf[10], lblBuf[32];
      otBuf[0] = '\0'; ftBuf[0] = '\0'; lblBuf[0] = '\0';
      extractJsonField(body, F("outside_temp"), otBuf, sizeof(otBuf));
      extractJsonField(body, F("flow_temp"),    ftBuf, sizeof(ftBuf));
      extractJsonField(body, F("label"),        lblBuf, sizeof(lblBuf));
      float ot = atof(otBuf);
      float ft = atof(ftBuf);
      if (otBuf[0] == '\0' || ftBuf[0] == '\0' ||
          ot < -15.0f || ot > 25.0f || ft < 10.0f || ft > 90.0f) {
        sendApiError(400, F("outside_temp (-15..25) and flow_temp (10..90) required"));
        return;
      }
      // Pass 1: stream existing file to count markers and find max id (~200-byte stack)
      int mcount = 0, maxId = 0;
      {
        char objBuf[200];
        File rf = LittleFS.open(fname, "r");
        if (rf) {
          while (readObj(rf, objBuf, sizeof(objBuf))) {
            mcount++;
            const char* idp = strstr(objBuf, "\"id\":");
            if (idp) {
              idp += 5; while (*idp == ' ') idp++;
              int v = atoi(idp); if (v > maxId) maxId = v;
            }
          }
          rf.close();
        }
      }
      if (mcount >= kSatMarkersMax) {
        sendApiError(409, F("Maximum markers (20) reached; delete one first"));
        return;
      }
      int newId = maxId + 1;
      time_t now = time(nullptr);
      char entry[192];
      for (char* lp = lblBuf; *lp; lp++) { if (*lp == '"' || *lp == '\\') *lp = '_'; }
      snprintf_P(entry, sizeof(entry),
        PSTR("{\"id\":%d,\"outside_temp\":%.1f,\"flow_temp\":%.1f,\"added\":%lu,\"label\":\"%s\"}"),
        newId, ot, ft, (unsigned long)now, lblBuf);
      // Pass 2: write existing markers + new entry to temp file, then rename
      char tmpname[28];
      strlcpy_P(tmpname, PSTR("/sat_markers.tmp"), sizeof(tmpname));
      {
        char objBuf[200];
        File wf = LittleFS.open(tmpname, "w");
        if (!wf) { sendApiError(500, F("Cannot write markers file")); return; }
        wf.print('[');
        bool first = true;
        File rf = LittleFS.open(fname, "r");
        if (rf) {
          while (readObj(rf, objBuf, sizeof(objBuf))) {
            if (!first) wf.print(',');
            wf.print(objBuf);
            first = false;
          }
          rf.close();
        }
        if (!first) wf.print(',');
        wf.print(entry);
        wf.print(']');
        wf.close();
      }
      if (!LittleFS.rename(tmpname, fname)) {
        sendApiError(500, F("Cannot finalize markers file")); return;
      }
      char resp[64];
      snprintf_P(resp, sizeof(resp), PSTR("{\"status\":\"ok\",\"id\":%d}"), newId);
      httpServer.send(201, F("application/json"), resp);
    }
    else if (method == HTTP_DELETE) {
      // DELETE /api/v2/sat/markers/<id>
      if (wc < 6) { sendApiError(400, F("Missing marker id in path")); return; }
      int delId = atoi(words[5]);
      if (delId <= 0) { sendApiError(400, F("Invalid marker id")); return; }
      File rf = LittleFS.open(fname, "r");
      if (!rf) { sendApiError(404, F("No markers found")); return; }
      char tmpname[28];
      strlcpy_P(tmpname, PSTR("/sat_markers.tmp"), sizeof(tmpname));
      File wf = LittleFS.open(tmpname, "w");
      if (!wf) { rf.close(); sendApiError(500, F("Cannot write temp file")); return; }
      wf.print('[');
      bool first = true, found = false;
      char objBuf[200];
      while (readObj(rf, objBuf, sizeof(objBuf))) {
        const char* idp = strstr(objBuf, "\"id\":");
        int thisId = 0;
        if (idp) { idp += 5; while (*idp == ' ') idp++; thisId = atoi(idp); }
        if (thisId == delId) { found = true; continue; }
        if (!first) wf.print(',');
        wf.print(objBuf);
        first = false;
      }
      wf.print(']');
      rf.close();
      wf.close();
      if (!found) {
        LittleFS.remove(tmpname);
        sendApiError(404, F("Marker id not found"));
        return;
      }
      if (!LittleFS.rename(tmpname, fname)) {
        sendApiError(500, F("Cannot finalize markers file")); return;
      }
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else {
      sendApiMethodNotAllowed(F("GET, POST, DELETE"));
    }
  }
  // --- TASK-587: DS18B20 sensor-to-SAT-area mapping ---
  // GET   /api/v2/sat/sensor-areas         — return all 4 area mappings as JSON
  // PATCH /api/v2/sat/sensor-areas         — body: {"area":0,"sensor":"AABBCCDD11223344"} (empty sensor clears)
  else if (strcasecmp_P(sub, PSTR("sensor-areas")) == 0) {
    if (method == HTTP_GET) {
      httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
      char resp[256];
      snprintf_P(resp, sizeof(resp),
        PSTR("{\"areas\":["
             "{\"index\":0,\"sensor\":\"%s\"},"
             "{\"index\":1,\"sensor\":\"%s\"},"
             "{\"index\":2,\"sensor\":\"%s\"},"
             "{\"index\":3,\"sensor\":\"%s\"}"
             "]}"),
        settings.sat.sSensorArea[0],
        settings.sat.sSensorArea[1],
        settings.sat.sSensorArea[2],
        settings.sat.sSensorArea[3]);
      httpServer.send(200, F("application/json"), resp);
    }
    else if (method == HTTP_PATCH || method == HTTP_POST || method == HTTP_PUT) {
      if (!httpServer.hasArg(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      const String& bodyStr = httpServer.arg(F("plain"));
      const char* body = bodyStr.c_str();
      char areaBuf[4], sensorBuf[18];
      areaBuf[0] = '\0'; sensorBuf[0] = '\0';
      extractJsonField(body, F("area"),   areaBuf,   sizeof(areaBuf));
      extractJsonField(body, F("sensor"), sensorBuf, sizeof(sensorBuf));
      if (areaBuf[0] == '\0') { sendApiError(400, F("Missing 'area' field (0-3)")); return; }
      int areaIdx = atoi(areaBuf);
      if (areaIdx < 0 || areaIdx >= 4) { sendApiError(400, F("'area' must be 0-3")); return; }
      // Validate sensor: empty (clear) or exactly 16 hex chars
      size_t slen = strlen(sensorBuf);
      if (slen != 0 && slen != 16) { sendApiError(400, F("'sensor' must be 16 hex chars or empty string")); return; }
      if (slen == 16) {
        for (size_t ci = 0; ci < 16; ci++) {
          if (!isxdigit((unsigned char)sensorBuf[ci])) {
            sendApiError(400, F("'sensor' must be 16 hex chars"));
            return;
          }
        }
        // Normalize to uppercase
        for (size_t ci = 0; ci < 16; ci++) sensorBuf[ci] = toupper((unsigned char)sensorBuf[ci]);
      }
      char settingKey[16];
      snprintf_P(settingKey, sizeof(settingKey), PSTR("SATsensorarea%d"), areaIdx);
      updateSetting(settingKey, sensorBuf);
      httpServer.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else {
      sendApiMethodNotAllowed(F("GET, PATCH, POST, PUT"));
    }
  }
  else {
    sendApiNotFound(originalURI);
  }
}

// TASK-361: stringify VerifyOutcome for telemetry (REST + devinfo).
// Returns a PROGMEM short label matching the enum values in OTGW-firmware.h.
static const __FlashStringHelper* verifyOutcomeLabel(VerifyOutcome o) {
  switch (o) {
    case VerifyOutcome::CLEAN:              return F("clean");
    case VerifyOutcome::MISSING:            return F("missing");
    case VerifyOutcome::ABORTED_HEAP:       return F("aborted_heap");
    case VerifyOutcome::ABORTED_DISCONNECT: return F("aborted_disconnect");
    case VerifyOutcome::UNKNOWN:
    default:                                return F("unknown");
  }
}

//===[ /api/v2/discovery — MQTT auto-discovery verification/republish (ADR-062 / TASK-349) ]===
static void handleDiscovery(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  // GET /api/v2/discovery — status dump
  if (wc == 4 || (wc == 5 && words[4][0] == '\0')) {
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    char msg[384];
    snprintf_P(msg, sizeof(msg),
      PSTR("{\"verification\":{\"active\":%s,\"last_epoch\":%lu,\"last_missing\":%u,\"last_orphan\":%u,\"last_outcome\":\"%S\"},"
           "\"counters\":{\"published_topics\":%lu,\"pending_ids\":%u,\"verify_runs\":%lu,\"republish_triggered\":%lu},"
           "\"settings\":{\"auto_verify\":%s}}"),
      isDiscoveryVerificationActive() ? "true" : "false",
      (unsigned long)state.discovery.iLastVerifyEpoch,
      (unsigned)state.discovery.iLastMissingCount,
      (unsigned)state.discovery.iLastOrphanCount,
      (PGM_P)verifyOutcomeLabel(state.discovery.eLastOutcome),
      (unsigned long)state.discovery.iPublishedTopicCount,
      (unsigned)countPendingDiscoveryIds(),
      (unsigned long)state.discovery.iVerifyRunCount,
      (unsigned long)state.discovery.iRepublishTriggeredCount,
      settings.mqtt.bDiscoveryAutoVerify ? "true" : "false");
    sendCorsOriginHeader();
    httpServer.send(200, F("application/json"), msg);
    return;
  }

  // POST /api/v2/discovery/verify — start verification window
  if (wc > 4 && strcmp_P(words[4], PSTR("verify")) == 0) {
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    if (!state.mqtt.bConnected) { sendApiError(503, F("MQTT not connected")); return; }
    if (platformFreeHeap() < VERIFICATION_MIN_HEAP_START) { sendApiError(503, F("Heap too low for verify")); return; }
    if (isDiscoveryVerificationActive()) { sendApiError(409, F("Verification already active")); return; }
    if (countPendingDiscoveryIds() > 0) { sendApiError(409, F("Discovery drip in progress")); return; }
    if (!startDiscoveryVerification()) { sendApiError(503, F("Verification start refused (see telnet log)")); return; }
    char msg[128];
    snprintf_P(msg, sizeof(msg),
      PSTR("{\"status\":\"verification_started\",\"expected\":%lu,\"window_ms\":15000}"),
      (unsigned long)state.discovery.iPublishedTopicCount);
    sendCorsOriginHeader();
    httpServer.send(202, F("application/json"), msg);
    return;
  }

  // POST /api/v2/discovery/republish — force full re-announce
  if (wc > 4 && strcmp_P(words[4], PSTR("republish")) == 0) {
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    if (!state.mqtt.bConnected) { sendApiError(503, F("MQTT not connected")); return; }

    // TASK-356 (CWE-770): 60 s cooldown between successful republish invocations.
    // Rationale: rapid-fire POSTs keep countPendingDiscoveryIds > 0 permanently,
    // which blocks startDiscoveryVerification() (precondition at MQTTstuff.ino).
    // A post-auth LAN actor could otherwise DoS the verify endpoint. 60 s gives
    // the drip publisher time to drain pending IDs so verify becomes reachable
    // again. Timer is a function-local static so no new globals leak out.
    static unsigned long lastRepublishMs = 0;
    constexpr unsigned long REPUBLISH_COOLDOWN_MS = 60000UL;
    if (lastRepublishMs != 0) {
      const unsigned long elapsed = millis() - lastRepublishMs;
      if (elapsed < REPUBLISH_COOLDOWN_MS) {
        const unsigned long remaining = (REPUBLISH_COOLDOWN_MS - elapsed + 999UL) / 1000UL;
        restResponseStatus = 429;
        char jsonBuff[160];
        snprintf_P(jsonBuff, sizeof(jsonBuff),
          PSTR("{\"error\":{\"status\":429,\"message\":\"Republish cooldown active, retry in %lus\"}}"),
          remaining);
        sendCorsOriginHeader();
        httpServer.send(429, F("application/json"), jsonBuff);
        return;
      }
    }

    markAllMQTTConfigPending();
    char msg[96];
    snprintf_P(msg, sizeof(msg),
      PSTR("{\"status\":\"marked_pending\",\"count\":%u}"),
      (unsigned)countPendingDiscoveryIds());
    lastRepublishMs = millis();  // stamp only after work commits
    sendCorsOriginHeader();
    httpServer.send(200, F("application/json"), msg);
    return;
  }

  sendApiNotFound(originalURI);
}

static void debugFormatLocalIp(char* buf, size_t bufSize)
{
  IPAddress ip = WiFi.localIP();
  snprintf_P(buf, bufSize, PSTR("%u.%u.%u.%u"), ip[0], ip[1], ip[2], ip[3]);
}

static void handleDebugDump(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)
{
  (void)words;
  (void)wc;
  (void)originalURI;
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (!checkHttpAuth()) return;

  char ipBuf[16];
  char ssidBuf[33];
  char hwModeBuf[16];
  char netModeBuf[16];
  debugFormatLocalIp(ipBuf, sizeof(ipBuf));
  strlcpy(ssidBuf, WiFi.SSID().c_str(), sizeof(ssidBuf));
  snprintf_P(hwModeBuf, sizeof(hwModeBuf), PSTR("%S"), (PGM_P)hardwareModeName());
  snprintf_P(netModeBuf, sizeof(netModeBuf), PSTR("%S"), (PGM_P)networkModeName());

  sendStartJsonMap(F("debug"));

  sendJsonMapEntry(F("build.version"), _VERSION);
  sendJsonMapEntry(F("build.number"), (int32_t)_VERSION_BUILD);
  sendJsonMapEntry(F("build.githash"), _VERSION_GITHASH);
  sendJsonMapEntry(F("build.date"), _VERSION_DATE);

  sendJsonMapEntry(F("runtime.heap_free"), (uint32_t)platformFreeHeap());
  sendJsonMapEntry(F("runtime.heap_frag_pct"), (uint32_t)platformHeapFragmentation());
  sendJsonMapEntry(F("runtime.heap_min_free"), (uint32_t)platformMinFreeHeap());
  sendJsonMapEntry(F("runtime.heap_max_alloc"), (uint32_t)platformMaxFreeBlock());
  sendJsonMapEntry(F("runtime.uptime_sec"), (uint32_t)state.uptime.iSeconds);
  sendJsonMapEntry(F("runtime.reboots"), (uint32_t)state.uptime.iRebootCount);
  sendJsonMapEntry(F("runtime.wifi_connected"), (WiFi.status() == WL_CONNECTED));
  sendJsonMapEntry(F("runtime.wifi_rssi"), (int32_t)WiFi.RSSI());
  sendJsonMapEntry(F("runtime.wifi_ip"), ipBuf);
  sendJsonMapEntry(F("runtime.wifi_ssid"), ssidBuf);
  sendJsonMapEntry(F("runtime.hw_mode"), hwModeBuf);
  sendJsonMapEntry(F("runtime.net_mode"), netModeBuf);

  sendJsonMapEntry(F("settings.hostname"), settings.sHostname);
  sendJsonMapEntry(F("settings.http_passwd"), settings.sHTTPpasswd[0] ? "***" : "(not set)");
  sendJsonMapEntry(F("settings.led_blink"), settings.bLEDblink);
  sendJsonMapEntry(F("settings.dark_theme"), settings.bDarkTheme);
  sendJsonMapEntry(F("settings.mydebug"), settings.bMyDEBUG);
  sendJsonMapEntry(F("settings.nightly_restart"), settings.bNightlyRestart);
  sendJsonMapEntry(F("settings.restart_hour"), (uint32_t)settings.iRestartHour);

  sendJsonMapEntry(F("settings.mqtt.enabled"), settings.mqtt.bEnable);
  sendJsonMapEntry(F("settings.mqtt.broker"), settings.mqtt.sBroker);
  sendJsonMapEntry(F("settings.mqtt.port"), (uint32_t)settings.mqtt.iBrokerPort);
  sendJsonMapEntry(F("settings.mqtt.user"), settings.mqtt.sUser);
  sendJsonMapEntry(F("settings.mqtt.passwd"), settings.mqtt.sPasswd[0] ? "***" : "(not set)");
  sendJsonMapEntry(F("settings.mqtt.ha_prefix"), settings.mqtt.sHaprefix);
  sendJsonMapEntry(F("settings.mqtt.top_topic"), settings.mqtt.sTopTopic);
  sendJsonMapEntry(F("settings.mqtt.unique_id"), settings.mqtt.sUniqueid);
  sendJsonMapEntry(F("settings.mqtt.ot_message"), settings.mqtt.bOTmessage);
  sendJsonMapEntry(F("settings.mqtt.on_change"), settings.mqtt.bOnChangePublishing);
  sendJsonMapEntry(F("settings.mqtt.interval"), (uint32_t)settings.mqtt.iInterval);
  sendJsonMapEntry(F("settings.mqtt.sep_sources"), settings.mqtt.bSeparateSources);
  sendJsonMapEntry(F("settings.mqtt.disc_verify"), settings.mqtt.bDiscoveryAutoVerify);
  sendJsonMapEntry(F("settings.mqtt.use_legacy_topics"), settings.mqtt.bUseLegacyOtTopics);
  sendJsonMapEntry(F("settings.mqtt.ha_reboot"), settings.mqtt.bHaRebootDetect);
  sendJsonMapEntry(F("settings.legacy.port25238"), settings.mqtt.bLegacyPort25238Enabled);

  sendJsonMapEntry(F("settings.ntp.enabled"), settings.ntp.bEnable);
  sendJsonMapEntry(F("settings.ntp.timezone"), settings.ntp.sTimezone);
  sendJsonMapEntry(F("settings.ntp.hostname"), settings.ntp.sHostname);
  sendJsonMapEntry(F("settings.ntp.sendtime"), settings.ntp.bSendtime);

  sendJsonMapEntry(F("settings.sensors.enabled"), settings.sensors.bEnabled);
  sendJsonMapEntry(F("settings.sensors.pin"), (int32_t)settings.sensors.iPin);
  sendJsonMapEntry(F("settings.sensors.interval"), (int32_t)settings.sensors.iInterval);

  sendJsonMapEntry(F("settings.s0.enabled"), settings.s0.bEnabled);
  sendJsonMapEntry(F("settings.s0.pin"), (uint32_t)settings.s0.iPin);
  sendJsonMapEntry(F("settings.s0.debounce_ms"), (uint32_t)settings.s0.iDebounceTime);
  sendJsonMapEntry(F("settings.s0.pulse_kw"), (uint32_t)settings.s0.iPulsekw);
  sendJsonMapEntry(F("settings.s0.interval"), (uint32_t)settings.s0.iInterval);

  sendJsonMapEntry(F("settings.outputs.enabled"), settings.outputs.bEnabled);
  sendJsonMapEntry(F("settings.outputs.pin"), (int32_t)settings.outputs.iPin);
  sendJsonMapEntry(F("settings.outputs.trigger_bit"), (int32_t)settings.outputs.iTriggerBit);

  sendJsonMapEntry(F("settings.sat.enabled"), settings.sat.bEnabled);
  sendJsonMapEntry(F("settings.sat.heating_system"), (uint32_t)settings.sat.iHeatingSystem);
  sendJsonMapEntry(F("settings.sat.target_temp"), settings.sat.fTargetTemp);
  sendJsonMapEntry(F("settings.sat.curve_coeff"), settings.sat.fHeatingCurveCoeff);
  sendJsonMapEntry(F("settings.sat.deadband"), settings.sat.fDeadband);
  sendJsonMapEntry(F("settings.sat.control_interval"), (uint32_t)settings.sat.iControlInterval);
  sendJsonMapEntry(F("settings.sat.use_external_temp"), settings.sat.bUseExternalTemp);
  sendJsonMapEntry(F("settings.sat.pwm_auto_switch"), settings.sat.bPwmAutoSwitch);
  sendJsonMapEntry(F("settings.sat.max_rel_mod"), (uint32_t)settings.sat.iMaxRelModulation);
  sendJsonMapEntry(F("settings.sat.ovp_value"), settings.sat.fOvpValue);
  sendJsonMapEntry(F("settings.sat.ovp_enabled"), settings.sat.bOvpEnabled);
  sendJsonMapEntry(F("settings.sat.overshoot_margin"), settings.sat.fOvershootMargin);
  sendJsonMapEntry(F("settings.sat.dhw_setpoint"), settings.sat.fDhwSetpoint);
  sendJsonMapEntry(F("settings.sat.dhw_enabled"), settings.sat.bDhwEnabled);
  sendJsonMapEntry(F("settings.sat.dhw_enable"), settings.sat.bDhwEnable);
  sendJsonMapEntry(F("settings.sat.window_detection"), settings.sat.bWindowDetection);
  sendJsonMapEntry(F("settings.sat.weather_enable"), settings.sat.bWeatherEnable);
  sendJsonMapEntry(F("settings.sat.weather_key"), settings.sat.sWeatherApiKey[0] ? "***" : "(not set)");
  sendJsonMapEntry(F("settings.sat.boiler_capacity"), settings.sat.fBoilerCapacity);
  sendJsonMapEntry(F("settings.sat.simulation"), settings.sat.bSimulation);
  sendJsonMapEntry(F("settings.sat.solar_gain_enable"), settings.sat.bSolarGainEnable);
  sendJsonMapEntry(F("settings.sat.summer_simmer"), settings.sat.bSummerSimmer);
  sendJsonMapEntry(F("settings.sat.comfort_adjust"), settings.sat.bComfortAdjust);
  sendJsonMapEntry(F("settings.sat.multi_area"), settings.sat.bMultiArea);
  sendJsonMapEntry(F("settings.sat.auto_tune"), settings.sat.bAutoTune);
  sendJsonMapEntry(F("settings.sat.max_setpoint"), settings.sat.fMaxSetpoint);
  sendJsonMapEntry(F("settings.sat.sensor_max_age"), (uint32_t)settings.sat.iSensorMaxAgeS);
  sendJsonMapEntry(F("settings.sat.error_monitoring"), settings.sat.bErrorMonitoring);
  sendJsonMapEntry(F("settings.sat.auto_gains"), settings.sat.bAutoGains);
  sendJsonMapEntry(F("settings.sat.thermal_comfort"), settings.sat.bThermalComfort);
  sendJsonMapEntry(F("settings.sat.humidity_timeout"), (uint32_t)settings.sat.iHumidityTimeoutS);
  sendJsonMapEntry(F("settings.sat.heating_mode"), (uint32_t)settings.sat.iHeatingMode);
  sendJsonMapEntry(F("settings.sat.cycles_per_hour"), (uint32_t)settings.sat.iCyclesPerHour);
  sendJsonMapEntry(F("settings.sat.valve_offset"), settings.sat.fValveOffset);
  sendJsonMapEntry(F("settings.sat.solar_freeze_i"), settings.sat.bSolarFreezeIntegral);
  sendJsonMapEntry(F("settings.sat.flush_threshold_h"), (uint32_t)settings.sat.iSatFlushThresholdH);
  sendJsonMapEntry(F("settings.sat.zone_count"), (uint32_t)settings.sat.iZoneCount);
  sendJsonMapEntry(F("settings.sat.zone_timeout_s"), (uint32_t)settings.sat.iZoneTimeoutS);

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  sendJsonMapEntry(F("settings.otd.mode"), (uint32_t)settings.otd.iMode);
  sendJsonMapEntry(F("settings.otd.auto_detect"), settings.otd.bAutoDetect);
  sendJsonMapEntry(F("settings.otd.setback_temp"), settings.otd.fSetbackTemp);
  sendJsonMapEntry(F("settings.otd.setback_timeout"), (uint32_t)settings.otd.iSetbackTimeout);
  sendJsonMapEntry(F("settings.otd.enable_slave"), settings.otd.bEnableSlave);
  sendJsonMapEntry(F("settings.otd.summer_mode"), settings.otd.bSummerMode);
  sendJsonMapEntry(F("settings.otd.fail_safe"), settings.otd.bFailSafe);
  sendJsonMapEntry(F("settings.otd.msg_interval"), (uint32_t)settings.otd.iMsgInterval);
  sendJsonMapEntry(F("settings.otd.has_bypass_relay"), settings.otd.bHasBypassRelay);
  sendJsonMapEntry(F("settings.otd.ch_mode"), (uint32_t)settings.otd.iCHMode);
  sendJsonMapEntry(F("settings.otd.flow_temp"), settings.otd.fFlowTemp);
  sendJsonMapEntry(F("settings.otd.flow_max"), settings.otd.fFlowMax);
  sendJsonMapEntry(F("settings.otd.room_setpoint"), settings.otd.fRoomSetpoint);
  sendJsonMapEntry(F("settings.otd.gradient"), settings.otd.fGradient);
  sendJsonMapEntry(F("settings.otd.exponent"), settings.otd.fExponent);
  sendJsonMapEntry(F("settings.otd.offset"), settings.otd.fOffset);
  sendJsonMapEntry(F("settings.otd.room_comp"), settings.otd.bRoomCompEnabled);
  sendJsonMapEntry(F("settings.otd.kp"), settings.otd.fKp);
  sendJsonMapEntry(F("settings.otd.ki"), settings.otd.fKi);
  sendJsonMapEntry(F("settings.otd.kboost"), settings.otd.fKboost);
#endif

  sendJsonMapEntry(F("state.mqtt.connected"), state.mqtt.bConnected);
  sendJsonMapEntry(F("state.pic.available"), state.pic.bAvailable);
  sendJsonMapEntry(F("state.pic.device_id"), state.pic.sDeviceid);
  sendJsonMapEntry(F("state.pic.type"), state.pic.sType);
  sendJsonMapEntry(F("state.pic.fw_version"), state.pic.sFwversion);

  sendJsonMapEntry(F("state.otbus.online"), state.otBus.bOnline);
  sendJsonMapEntry(F("state.otbus.gateway_mode"), state.otBus.bGatewayMode);
  sendJsonMapEntry(F("state.otbus.gateway_known"), state.otBus.bGatewayModeKnown);
  sendJsonMapEntry(F("state.otbus.boiler_state"), state.otBus.bBoilerState);
  sendJsonMapEntry(F("state.otbus.thermostat_state"), state.otBus.bThermostatState);
  sendJsonMapEntry(F("state.otbus.ps_mode"), state.otBus.bPSmode);

  sendJsonMapEntry(F("state.debug.ot_msg"), state.debug.bOTmsg);
  sendJsonMapEntry(F("state.debug.rest_api"), state.debug.bRestAPI);
  sendJsonMapEntry(F("state.debug.mqtt"), state.debug.bMQTT);
  sendJsonMapEntry(F("state.debug.mqtt_gate"), state.debug.bMQTTGate);
  sendJsonMapEntry(F("state.debug.sensors"), state.debug.bSensors);
  sendJsonMapEntry(F("state.debug.ntp"), state.debug.bNTP);
  sendJsonMapEntry(F("state.debug.sensor_sim"), state.debug.bSensorSim);
  sendJsonMapEntry(F("state.debug.otgw_sim"), state.debug.bOTGWSimulation);
  sendJsonMapEntry(F("state.debug.sat"), state.debug.bSAT);
  sendJsonMapEntry(F("state.debug.otdirect"), state.debug.bOTDirect);
#if HAS_SAT_BLE
  sendJsonMapEntry(F("state.debug.sat_ble"), state.debug.bSATBLE);
#endif

  sendJsonMapEntry(F("state.heap.ws_drops"), (uint32_t)state.heapdiag.iWsDropsTotal);
  sendJsonMapEntry(F("state.heap.mqtt_drops"), (uint32_t)state.heapdiag.iMqttDropsTotal);
  sendJsonMapEntry(F("state.heap.entered_low"), (uint32_t)state.heapdiag.iEnteredLowCount);
  sendJsonMapEntry(F("state.heap.entered_warn"), (uint32_t)state.heapdiag.iEnteredWarningCount);
  sendJsonMapEntry(F("state.heap.entered_crit"), (uint32_t)state.heapdiag.iEnteredCriticalCount);
  sendJsonMapEntry(F("state.heap.drip_slow"), (uint32_t)state.heapdiag.iDripSlowModeCount);

  sendJsonMapEntry(F("state.disco.published"), (uint32_t)state.discovery.iPublishedTopicCount);
  sendJsonMapEntry(F("state.disco.verify_runs"), (uint32_t)state.discovery.iVerifyRunCount);
  sendJsonMapEntry(F("state.disco.republishes"), (uint32_t)state.discovery.iRepublishTriggeredCount);
  sendJsonMapEntry(F("state.disco.last_missing"), (uint32_t)state.discovery.iLastMissingCount);
  sendJsonMapEntry(F("state.disco.last_orphan"), (uint32_t)state.discovery.iLastOrphanCount);
  sendJsonMapEntry(F("state.disco.last_epoch"), (uint32_t)state.discovery.iLastVerifyEpoch);

  { char boilerStatus[20]; satGetBoilerStatusName(boilerStatus, sizeof(boilerStatus));
    sendJsonMapEntry(F("state.sat.boiler_status"), boilerStatus); }
  { char manufacturer[12]; satGetManufacturerName(manufacturer, sizeof(manufacturer));
    sendJsonMapEntry(F("state.sat.manufacturer"), manufacturer); }
  sendJsonMapEntry(F("state.sat.active"), state.sat.bActive);
  sendJsonMapEntry(F("state.sat.control_mode"), (int32_t)state.sat.eControlMode);
  sendJsonMapEntry(F("state.sat.room_temp"), satGetRoomTemp());
  sendJsonMapEntry(F("state.sat.outside_temp"), satGetOutsideTemp());
  sendJsonMapEntry(F("state.sat.heating_curve"), state.sat.fHeatingCurveValue);
  sendJsonMapEntry(F("state.sat.pid_output"), state.sat.fPidOutput);
  sendJsonMapEntry(F("state.sat.final_setpoint"), state.sat.fFinalSetpoint);
  sendJsonMapEntry(F("state.sat.error"), state.sat.fError);
  sendJsonMapEntry(F("state.sat.pid_p"), state.sat.fPidP);
  sendJsonMapEntry(F("state.sat.pid_i"), state.sat.fPidI);
  sendJsonMapEntry(F("state.sat.pid_d"), state.sat.fPidD);
  sendJsonMapEntry(F("state.sat.cycle_count"), (uint32_t)state.sat.iCycleCount);
  sendJsonMapEntry(F("state.sat.last_cycle_class"), (int32_t)state.sat.eLastCycleClass);
  sendJsonMapEntry(F("state.sat.duty_ratio"), state.sat.fDutyRatio);
  sendJsonMapEntry(F("state.sat.pwm_duty"), state.sat.fPwmDutyCycle);
  sendJsonMapEntry(F("state.sat.active_preset"), (int32_t)state.sat.eActivePreset);
  sendJsonMapEntry(F("state.sat.mod_suppressed"), state.sat.bModSuppressed);
  sendJsonMapEntry(F("state.sat.dhw_active"), state.sat.bDhwActive);
  sendJsonMapEntry(F("state.sat.fallback_active"), state.sat.bFallbackActive);
  sendJsonMapEntry(F("state.sat.fallback_reason"), (int32_t)state.sat.eFallbackReason);
  sendJsonMapEntry(F("state.sat.current_mod"), (int32_t)state.sat.iCurrentModulation);
  sendJsonMapEntry(F("state.sat.hsys_detected"), (int32_t)state.sat.iDetectedHeatingSystem);
  sendJsonMapEntry(F("state.sat.weather_valid"), state.sat.weather.bValid);
  sendJsonMapEntry(F("state.sat.weather_temp"), state.sat.weather.fTemperature);
  sendJsonMapEntry(F("state.sat.weather_humidity"), state.sat.weather.fHumidity);
  sendJsonMapEntry(F("state.sat.weather_wind"), state.sat.weather.fWindSpeed);
  sendJsonMapEntry(F("state.sat.weather_cloud"), state.sat.weather.fCloudCover);
#if HAS_WEATHER_FORECAST
  sendJsonMapEntry(F("state.sat.weather_pressure"), state.sat.weather.fPressureMsl);
  sendJsonMapEntry(F("state.sat.weather_is_day"), state.sat.weather.bIsDay);
#endif
  sendJsonMapEntry(F("state.sat.current_power"), state.sat.fCurrentPower);
  sendJsonMapEntry(F("state.sat.energy_total"), state.sat.fEnergyTotal);
  sendJsonMapEntry(F("state.sat.energy_est"), state.sat.fEnergyEstimatedKWh);
  sendJsonMapEntry(F("state.sat.thermal_valid"), state.sat.bThermalModelValid);
  sendJsonMapEntry(F("state.sat.thermal_drop"), state.sat.fThermalDropRate);
  sendJsonMapEntry(F("state.sat.solar_gain"), state.sat.bSolarGainActive);
  sendJsonMapEntry(F("state.sat.summer_active"), state.sat.bSummerActive);
  sendJsonMapEntry(F("state.sat.humidity"), state.sat.fHumidity);
  sendJsonMapEntry(F("state.sat.humidity_valid"), state.sat.bHumidityValid);
  sendJsonMapEntry(F("state.sat.comfort_offset"), state.sat.fComfortOffset);
  sendJsonMapEntry(F("state.sat.area0_temp"), state.sat.fAreaTemp[0]);
  sendJsonMapEntry(F("state.sat.area1_temp"), state.sat.fAreaTemp[1]);
  sendJsonMapEntry(F("state.sat.area2_temp"), state.sat.fAreaTemp[2]);
  sendJsonMapEntry(F("state.sat.area3_temp"), state.sat.fAreaTemp[3]);
  sendJsonMapEntry(F("state.sat.auto_tune_active"), state.sat.bAutoTuneActive);
  sendJsonMapEntry(F("state.sat.auto_tune_cycles"), (uint32_t)state.sat.iAutoTuneCycles);
  sendJsonMapEntry(F("state.sat.auto_tune_score"), state.sat.fAutoTuneScore);
#if HAS_SAT_BLE
  sendJsonMapEntry(F("state.sat.ble_temp"), state.sat.fBleTemp);
  sendJsonMapEntry(F("state.sat.ble_humidity"), state.sat.fBleHumidity);
  sendJsonMapEntry(F("state.sat.ble_valid"), state.sat.bBleTempValid);
  sendJsonMapEntry(F("state.sat.ble_count"), (uint32_t)state.sat.iBleSensorCount);
  sendJsonMapEntry(F("state.sat.ble_battery"), (uint32_t)state.sat.iBleBattery);
  sendJsonMapEntry(F("state.sat.ble_rssi"), (int32_t)state.sat.iBleRssi);
#endif

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  sendJsonMapEntry(F("state.otd.schedule_total"), (uint32_t)state.otd.iScheduleTotal);
  sendJsonMapEntry(F("state.otd.schedule_active"), (uint32_t)state.otd.iScheduleActive);
  sendJsonMapEntry(F("state.otd.schedule_disabled"), (uint32_t)state.otd.iScheduleDisabled);
  sendJsonMapEntry(F("state.otd.override_count"), (uint32_t)state.otd.iOverrideCount);
  sendJsonMapEntry(F("state.otd.bypass_active"), state.otd.bBypassActive);
  sendJsonMapEntry(F("state.otd.stepup_enabled"), state.otd.bStepUpEnabled);
  sendJsonMapEntry(F("state.otd.monitor_mode"), state.otd.bMonitorMode);
  sendJsonMapEntry(F("state.otd.mode"), (int32_t)state.otd.eMode);
  sendJsonMapEntry(F("state.otd.master_mode"), state.otd.bMasterMode);
  sendJsonMapEntry(F("state.otd.thermostat_connected"), state.otd.bThermostatConnected);
  sendJsonMapEntry(F("state.otd.setback_active"), state.otd.bSetbackActive);
  sendJsonMapEntry(F("state.otd.override_mode"), (int32_t)state.otd.eOverrideMode);
  sendJsonMapEntry(F("state.otd.override_f88"), (uint32_t)state.otd.iOverrideF88);
#endif

  sendEndJsonMap(F("debug"));
}

// TASK-585: WiFi network scan
// GET /api/v2/network/scan — async WiFi scan
//   First call: starts scan, returns {"status":"scanning"}
//   Subsequent calls: returns {"status":"ready","networks":[...]} once done
//   Each network: {"ssid":"...","rssi":-60,"channel":6,"secured":true,"connected":true}
// Guard: refuses scan during OTA or PIC flash operations.
static bool    _wifiScanStarted = false;

static void handleNetwork(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)
{
  if (wc < 5 || strcasecmp_P(words[4], PSTR("scan")) != 0) {
    sendApiError(404, F("Unknown network sub-resource (try /scan)"));
    return;
  }
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }

  // Guard: no scan during PIC flash (WiFi scan uses significant CPU)
  if (state.flash.bPICactive) {
    sendApiError(503, F("Scan unavailable during PIC flash"));
    return;
  }

  // Read current scan state
  int16_t scanResult = WiFi.scanComplete();

  if (scanResult == WIFI_SCAN_RUNNING) {
    // Scan in progress — report scanning
    httpServer.send(200, F("application/json"), F("{\"status\":\"scanning\"}"));
    return;
  }

  if (scanResult == WIFI_SCAN_FAILED || !_wifiScanStarted) {
    // Start async scan (non-blocking)
    WiFi.scanNetworks(true /*async*/);
    _wifiScanStarted = true;
    httpServer.send(200, F("application/json"), F("{\"status\":\"scanning\"}"));
    return;
  }

  // scanResult >= 0: scan complete, scanResult = number of networks
  _wifiScanStarted = false;
  // Build JSON response using chunked streaming (avoid large static buffer)
  char connectedSsid[33];
  strlcpy(connectedSsid, WiFi.SSID().c_str(), sizeof(connectedSsid));

  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("application/json"), F(""));

  char chunk[256];
  snprintf_P(chunk, sizeof(chunk), PSTR("{\"status\":\"ready\",\"count\":%d,\"networks\":["), scanResult);
  httpServer.sendContent(chunk);

  for (int16_t i = 0; i < scanResult; i++) {
    char ssidBuf[33];
    strlcpy(ssidBuf, WiFi.SSID(i).c_str(), sizeof(ssidBuf));
    // Escape quotes in SSID
    for (char* p = ssidBuf; *p; p++) { if (*p == '"' || *p == '\\') *p = '_'; }
    bool isConn = (strcmp(ssidBuf, connectedSsid) == 0);
    bool secured = platformWiFiIsEncrypted(i);
    snprintf_P(chunk, sizeof(chunk),
      PSTR("%s{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"secured\":%s,\"connected\":%s}"),
      (i > 0 ? "," : ""),
      ssidBuf, WiFi.RSSI(i), WiFi.channel(i),
      secured ? "true" : "false",
      isConn ? "true" : "false");
    httpServer.sendContent(chunk);
  }
  httpServer.sendContent(F("]}"));
  httpServer.sendContent(F(""));  // terminate chunked transfer

  // Release scan memory
  WiFi.scanDelete();
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
static const char kRouteDiscovery[]  PROGMEM = "discovery";
static const char kRouteDebugDump[]  PROGMEM = "debug";
static const char kRouteNetwork[]    PROGMEM = "network";  // TASK-585

// Dispatch table placed in PROGMEM so the ~136 B of {segment, handler}
// rows live in flash, not DRAM. Same pattern as kSatMqttCmds in
// MQTTstuff.ino. The segment string pointers reference named PROGMEM
// constants (kRouteHealth[] PROGMEM = "health", etc.) so they remain
// usable with strcmp_P after the row is memcpy_P'd into a stack-local.
static const ApiRoute kV2Routes[] PROGMEM = {
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
  { kRouteDiscovery,  handleDiscovery },
  { kRouteDebugDump,  handleDebugDump },
  { kRouteNetwork,    handleNetwork },  // TASK-585: WiFi scan
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

      // Dispatch via route table (table is PROGMEM; copy row into stack-local before reading)
      if (wc > 3) {
        for (size_t i = 0; ; i++) {
          ApiRoute r;
          memcpy_P(&r, &kV2Routes[i], sizeof(ApiRoute));
          if (r.segment == nullptr) break;  // sentinel
          if (strcmp_P(words[3], r.segment) == 0) {
            restResponseStatus = 200; // default; overwritten by sendApiError if handler fails
            r.handler(words, wc, method, originalURI);
            RESTDebugTf(PSTR("REST %s %s => %d v2/%S %lums\r\n"), httpMethodToStr(method), originalURI, restResponseStatus, r.segment, millis() - startMs);
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
//
// Field ordering contract:
// The Debug Info page (data/index.js refreshDeviceInfo) renders rows in
// JSON insertion order via `for (key in device)`. The emit order below
// therefore IS the on-screen order. Fields are grouped semantically
// (firmware, platform, network, time, connections, chip, RAM, flash,
// drops, discovery). When adding a new field, place it inside the
// matching group rather than appending at the end, so related metrics
// stay adjacent on the page. JSON object order is not an API guarantee
// for REST consumers - they should parse by key.
// C: Minimum contiguous heap block required to safely stream this response.
// If maxFreeBlock is below this, return 503 instead of compounding heap pressure.
#define DEVICE_INFO_MIN_HEAP_BLOCK  8192

void sendDeviceInfoV2()
{
  if (platformMaxFreeBlock() < DEVICE_INFO_MIN_HEAP_BLOCK) {
    sendApiError(503, F("low heap"));
    return;
  }
  const uint32_t startMs = millis();
  restPerfBegin(REST_PERF_DEVICE_INFO);
  sendStartJsonMap(F("device"));

  // --- Firmware & build identity ---
  sendJsonMapEntry(F("author"), F("Robert van den Breemen"));
  sendJsonMapEntry(F("fwversion"), _SEMVER_FULL);
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  sendJsonMapEntry(F("compiled"), cMsg);
  // ADR-113 stage 2 (TASK-754): picavailable removed; UI selects on hardware_type.
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

  // --- Platform & hardware identity ---
  sendJsonMapEntry(F("platform"), F(PLATFORM_NAME));
  sendJsonMapEntry(F("board"), boardName());
  sendJsonMapEntry(F("hardware_type"), hardwareTypeName());  // ADR-113: static board class for codepath selection
  sendJsonMapEntry(F("hardwaremode"), hardwareModeName());
  sendJsonMapEntry(F("networkmode"), networkModeName());
  sendJsonMapEntry(F("oledpresent"), state.hw.bOLEDPresent);
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  sendJsonMapEntry(F("ethernetpresent"), state.hw.bEthernetPresent);
  sendJsonMapEntry(F("ethernetlink"), state.net.bEthernetLink);
#endif

  // --- Network identity ---
  sendJsonMapEntry(F("hostname"), CSTR(settings.sHostname));
  sendJsonMapEntry(F("ipaddress"), CSTR(getActiveIP()));
  sendJsonMapEntry(F("macaddress"), CSTR(getActiveMAC()));
  // Current WiFi network parameters (for DHCP-prefill in the UI). Available
  // on both ESP8266 and ESP32 via the standard WiFi API.
  sendJsonMapEntry(F("wifi_current_subnet"),  CSTR(WiFi.subnetMask().toString()));
  sendJsonMapEntry(F("wifi_current_gateway"), CSTR(WiFi.gatewayIP().toString()));
  sendJsonMapEntry(F("wifi_current_dns1"),    CSTR(WiFi.dnsIP(0).toString()));
  sendJsonMapEntry(F("wifi_current_dns2"),    CSTR(WiFi.dnsIP(1).toString()));
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // Current Ethernet network parameters (for DHCP-prefill in the UI). Reported
  // via the EthernetESP32 library (Ethernet.* API).
  sendJsonMapEntry(F("eth_current_subnet"),  CSTR(Ethernet.subnetMask().toString()));
  sendJsonMapEntry(F("eth_current_gateway"), CSTR(Ethernet.gatewayIP().toString()));
  sendJsonMapEntry(F("eth_current_dns"),     CSTR(Ethernet.dnsServerIP().toString()));
#endif
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

  // --- Time, NTP & uptime ---
  sendJsonMapEntry(F("ntpenable"), settings.ntp.bEnable);
  sendJsonMapEntry(F("ntptimezone"), CSTR(settings.ntp.sTimezone));
  sendJsonMapEntry(F("uptime"), upTime());
  sendJsonMapEntry(F("lastreset"), lastReset);
  sendJsonMapEntry(F("bootcount"), state.uptime.iRebootCount);

  // --- Connection status (MQTT, OTGW, thermostat, boiler) ---
  sendJsonMapEntry(F("mqttconnected"), state.mqtt.bConnected);
  // "otcommandinterface" names which OT interface is active, always one or the other, never both.
  if (isPICEnabled())           sendJsonMapEntry(F("otcommandinterface"), F("PIC"));
  else if (isOTDirectEnabled()) sendJsonMapEntry(F("otcommandinterface"), F("OT-Direct"));
  else                          sendJsonMapEntry(F("otcommandinterface"), F("None"));
  if (hasOTCommandInterface()) {
    sendJsonMapEntry(F("thermostatconnected"), state.otBus.bThermostatState);
    sendJsonMapEntry(F("boilerconnected"), state.otBus.bBoilerState);
    sendJsonMapEntry(F("otgwconnected"), state.otBus.bOnline);
  }
  if (isPICEnabled()) {
    sendJsonMapEntry(F("otgwmode"), !isGatewayFirmware() ? "N/A" : state.otBus.bGatewayModeKnown ? CCONOFF(state.otBus.bGatewayMode) : "detecting");
  }
  sendJsonMapEntry(F("otgwsimulation"), state.debug.bOTGWSimulation);

  // --- Chip & CPU ---
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%06X"), (unsigned int)platformChipId());
  sendJsonMapEntry(F("chipid"), cMsg);
  sendJsonMapEntry(F("coreversion"), platformCoreVersion());
  sendJsonMapEntry(F("sdkversion"),  platformSdkVersion());
  sendJsonMapEntry(F("cpufreq"), platformCpuFreqMHz());

  // --- RAM / heap (free heap, largest block, fragmentation, tier transitions) ---
  sendJsonMapEntry(F("freeheap"), platformFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), platformMaxFreeBlock());
  sendJsonMapEntry(F("hd_fragmentation_pct"), getHeapFragmentation());
  sendJsonMapEntry(F("hd_enter_low"),        state.heapdiag.iEnteredLowCount);
  sendJsonMapEntry(F("hd_enter_warning"),    state.heapdiag.iEnteredWarningCount);
  sendJsonMapEntry(F("hd_enter_critical"),   state.heapdiag.iEnteredCriticalCount);

  // --- Flash, sketch & filesystem storage (values cached at boot by cacheBootFlashInfo) ---
  sendJsonMapEntry(F("sketchsize"),       sBootFlash.sketchSize);
  sendJsonMapEntry(F("freesketchspace"),  sBootFlash.freeSketchSpace);
  sendJsonMapEntry(F("flashchipid"),      sBootFlash.flashChipId);
  sendJsonMapEntry(F("flashchipsize"),    sBootFlash.flashChipSizeMB);
  sendJsonMapEntry(F("flashchiprealsize"),sBootFlash.flashChipRealSizeMB);
  sendJsonMapEntry(F("flashchipspeed"),   sBootFlash.flashChipSpeedMHz);
  sendJsonMapEntry(F("flashchipmode"),    flashMode[sBootFlash.flashChipModeIdx < 4 ? sBootFlash.flashChipModeIdx : 4]);
  sendJsonMapEntry(F("LittleFSsize"),     sBootFlash.littleFSSizeMB);

  // --- Reliability drops (heap-pressure side effects) ---
  sendJsonMapEntry(F("hd_ws_drops"),         state.heapdiag.iWsDropsTotal);
  sendJsonMapEntry(F("hd_mqtt_drops"),       state.heapdiag.iMqttDropsTotal);

  // --- MQTT Discovery telemetry (ADR-062 / TASK-349 / TASK-361) ---
  sendJsonMapEntry(F("disc_published_topics"),     state.discovery.iPublishedTopicCount);
  sendJsonMapEntry(F("disc_pending_ids"),          (uint32_t)countPendingDiscoveryIds());
  sendJsonMapEntry(F("disc_verify_runs"),          state.discovery.iVerifyRunCount);
  sendJsonMapEntry(F("disc_republish_triggered"),  state.discovery.iRepublishTriggeredCount);
  sendJsonMapEntry(F("disc_last_missing"),         (uint32_t)state.discovery.iLastMissingCount);
  sendJsonMapEntry(F("disc_last_orphan"),          (uint32_t)state.discovery.iLastOrphanCount);
  sendJsonMapEntry(F("disc_last_outcome"),         verifyOutcomeLabel(state.discovery.eLastOutcome));
  sendJsonMapEntry(F("hd_drip_burst_skip"),        state.heapdiag.iDripActiveBurstSkipCount);
  sendJsonMapEntry(F("hd_drip_cooldown_skip"),     state.heapdiag.iDripCooldownSkipCount);
  sendJsonMapEntry(F("hd_drip_slowmode"),          state.heapdiag.iDripSlowModeCount);
  sendJsonMapEntry(F("perf_sat_status_total_ms"),     state.restperf.satStatus.iLastTotalMs);
  sendJsonMapEntry(F("perf_sat_status_send_ms"),      state.restperf.satStatus.iLastSendMs);
  sendJsonMapEntry(F("perf_sat_status_render_ms"),    state.restperf.satStatus.iLastRenderMs);
  sendJsonMapEntry(F("perf_sat_status_chunks"),       state.restperf.satStatus.iLastChunkCount);
  sendJsonMapEntry(F("perf_device_info_total_ms"),    state.restperf.deviceInfo.iLastTotalMs);
  sendJsonMapEntry(F("perf_device_info_send_ms"),     state.restperf.deviceInfo.iLastSendMs);
  sendJsonMapEntry(F("perf_device_info_render_ms"),   state.restperf.deviceInfo.iLastRenderMs);
  sendJsonMapEntry(F("perf_device_info_chunks"),      state.restperf.deviceInfo.iLastChunkCount);
  sendJsonMapEntry(F("perf_device_info_max_ms"),      state.restperf.deviceInfo.iMaxTotalMs);
  sendJsonMapEntry(F("perf_device_info_samples"),     state.restperf.deviceInfo.iSampleCount);
  sendJsonMapEntry(F("perf_settings_total_ms"),       state.restperf.settings.iLastTotalMs);
  sendJsonMapEntry(F("perf_settings_send_ms"),        state.restperf.settings.iLastSendMs);
  sendJsonMapEntry(F("perf_settings_render_ms"),      state.restperf.settings.iLastRenderMs);
  sendJsonMapEntry(F("perf_settings_chunks"),         state.restperf.settings.iLastChunkCount);

  sendEndJsonMap(F("device"));
  const uint32_t totalMs = millis() - startMs;
  restPerfCommit(REST_PERF_DEVICE_INFO, totalMs);
  RESTDebugTf(PSTR("REST PERF device/info total=%lums send=%lums render=%lums chunks=%lu\r\n"),
              (unsigned long)state.restperf.deviceInfo.iLastTotalMs,
              (unsigned long)state.restperf.deviceInfo.iLastSendMs,
              (unsigned long)state.restperf.deviceInfo.iLastRenderMs,
              (unsigned long)state.restperf.deviceInfo.iLastChunkCount);

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
  // TASK-582: CH hysteresis suspension state
  sendJsonMapEntry(F("ch_suspended"),          state.otd.bCHSuspended);
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
  sendJsonMapEntry(F("ipaddress"), CSTR(getActiveIP()));  // TASK-759: live active-transport IP for the header indicator
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
  const uint32_t startMs = millis();
  restPerfBegin(REST_PERF_SETTINGS);

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
  sendJsonSettingObj(F("mqttonchangepublishing"), settings.mqtt.bOnChangePublishing, "b");
  sendJsonSettingObj(F("mqttinterval"), settings.mqtt.iInterval, "i", 0, 3600);
  sendJsonSettingObj(F("mqttseparatesources"), settings.mqtt.bSeparateSources, "b");
  sendJsonSettingObj(F("mqttuselegacyottopics"), settings.mqtt.bUseLegacyOtTopics, "b");
  sendJsonSettingObj(F("legacyport25238enabled"), settings.mqtt.bLegacyPort25238Enabled, "b");
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
  // PV-surplus setpoint boost (TASK-640)
  sendJsonSettingObj(F("satpvboostenabled"),        settings.sat.bPvBoostEnabled, "b");
  sendJsonSettingObj(F("satpvboostthresholdw"),     (int32_t)settings.sat.iPvBoostThresholdW, "i", 100, 10000);
  sendJsonSettingObj(F("satpvboostholds"),          (int32_t)settings.sat.iPvBoostHoldS, "i", 30, 600);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPvBoostDeltaC, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satpvboostdeltac"), tmpBuf, "f", 0, 5);
  }
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPvBoostMaxIndoorC, 1, 2, tmpBuf);
    sendJsonSettingObj(F("satpvboostmaxindoorc"), tmpBuf, "f", 18, 28);
  }
  sendJsonSettingObj(F("satpvboostmaxdurationmin"), (int32_t)settings.sat.iPvBoostMaxDurationMin, "i", 30, 1440);
#if HAS_SAT_BLE
  // --- SAT BLE Sensor settings (Task #20). TASK-742: gated on HAS_SAT_BLE. ---
  sendJsonSettingObj(F("satbleenable"), settings.sat.bBleEnable, "b");
  sendJsonSettingObj(F("satblefailover"), settings.sat.bBleFailover, "b");
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
  // --- WiFi static IP settings (empty = DHCP) ---
  sendJsonSettingObj(F("wifistaticip"), CSTR(settings.wifi.sStaticIp), "s", 15);
  sendJsonSettingObj(F("wifisubnet"),   CSTR(settings.wifi.sSubnet),   "s", 15);
  sendJsonSettingObj(F("wifigateway"),  CSTR(settings.wifi.sGateway),  "s", 15);
  sendJsonSettingObj(F("wifidns1"),     CSTR(settings.wifi.sDns1),     "s", 15);
  sendJsonSettingObj(F("wifidns2"),     CSTR(settings.wifi.sDns2),     "s", 15);
  char httpPasswordPlaceholder[sizeof("password=40")];
  snprintf_P(httpPasswordPlaceholder,
             sizeof(httpPasswordPlaceholder),
             PSTR("password=%u"),
             static_cast<unsigned>(strnlen(settings.sHTTPpasswd, sizeof(settings.sHTTPpasswd))));
  sendJsonSettingObj(F("httppasswd"), httpPasswordPlaceholder, "p", 40);

  sendEndJsonMap(F("settings"));
  const uint32_t totalMs = millis() - startMs;
  restPerfCommit(REST_PERF_SETTINGS, totalMs);
  RESTDebugTf(PSTR("REST PERF settings total=%lums send=%lums render=%lums chunks=%lu\r\n"),
              (unsigned long)state.restperf.settings.iLastTotalMs,
              (unsigned long)state.restperf.settings.iLastSendMs,
              (unsigned long)state.restperf.settings.iLastRenderMs,
              (unsigned long)state.restperf.settings.iLastChunkCount);

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
  "mqttinterval", "mqttonchangepublishing", "mqttotmessage", "mqttpasswd", "mqttseparatesources", "legacyport25238enabled",
  "mqtttoptopic", "mqttuniqueid", "mqttuser",
  "ntpenable", "ntphostname", "ntpsendtime", "ntptimezone",
  "otdautodetect", "otdenableslave", "otdfailsafe", "otdmode", "otdmsginterval",
  "otdsetbacktemp", "otdsetbacktimeout", "otdsummermode",
  "otgwcommandenable", "otgwcommands",
  "s0counterdebouncetime", "s0counterenabled", "s0counterinterval", "s0counterpin", "s0counterpulsekw",
  "satareaweight0", "satareaweight1", "satareaweight2", "satareaweight3",
  "satautotune", "satautotunerate",
  "satbleenable", "satblefailover", "satbleinterval", "satblemac",
  "satboilercapacity", "satcoefficient", "satcomfortadjust", "satcomforthumidity", "satcomfortmaxoffset",
  "satdeadband", "satenabled", "satexternaltemp",
  "satinterval", "satmanufacturer", "satmultiarea", "satmultiareacount",
  "satovershootmargin", "satpresetaway", "satpresetcomfort", "satpreseteco",
  "satpresetsync", "satpresetsynctopic",
  // TASK-640: PV-surplus setpoint boost settings
  "satpvboostdeltac", "satpvboostenabled", "satpvboostholds",
  "satpvboostmaxdurationmin", "satpvboostmaxindoorc", "satpvboostthresholdw",
  "satpwmautoswitch", "satsimcoolrate", "satsimheatrate", "satsimulation",
  "satsolargain", "satsolarminrise", "satsolaroffset",
  "satsummerminhours", "satsummersimmer", "satsummerthreshold",
  "satsystem", "sattargettemp", "sattempstep", "satthermalcoeff",
  "satweatherenable", "satweatherinterval", "satweatherlat", "satweatherlon",
  "ui_autodownloadlog", "ui_autoexport", "ui_autoscreenshot", "ui_autoscroll",
  "ui_capture", "ui_graphtimewindow", "ui_timestamps",
  "webhookcontenttype", "webhookenable", "webhookenabled",
  "webhookpayload", "webhooktriggerbit", "webhookurloff", "webhookurlon",
  "wifidns1", "wifidns2", "wifigateway", "wifistaticip", "wifisubnet",
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
  // TASK-564: do NOT call flushSettings() synchronously here. The earlier
  // implementation flushed on every POST so the 200 OK was "truthful", but
  // SergeantD's alpha.3 telnet log showed that pattern caused 6 full
  // /settings.ini rewrites in 14 s during a single SAT/BLE form edit (each
  // 30-80 ms of LittleFS work, blocking lwIP and contributing to WS code 1006
  // reconnects). The 200 OK is still truthful: the new value is live in the
  // in-RAM `settings` struct immediately, so any subsequent GET reflects it.
  // updateSetting() schedules the 2-second debounce timer (RESTART_TIMER on
  // timerFlushSettings); rapid sequential field POSTs now coalesce into one
  // flash write. The sister SAT BLE path already relies on the same debounce
  // (TASK-508). Pre-reboot durability is preserved by doRestart() in
  // helperStuff.ino, which still calls flushSettings() synchronously before
  // esp.restart().
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
