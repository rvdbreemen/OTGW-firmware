/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v2.0.0-alpha.212
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
  // headerCompat() returns a pointer into a static buffer (no allocation);
  // strlcpy into our own static keeps this allocation-free on the hot path.
  static char originBuf[128];
  strlcpy(originBuf, headerCompat(F("Origin")), sizeof(originBuf));
  if (originBuf[0] != '\0') {
    webPushHeader(F("Access-Control-Allow-Origin"), originBuf);
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
  webSend(httpCode, F("application/json"), jsonBuff);
}

// T44: 405 responses with RFC 7231 §6.5.5 Allow header
static void sendApiMethodNotAllowed(const __FlashStringHelper* allowedMethods) {
  webPushHeader(F("Allow"), allowedMethods);
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

  strlcpy(originBuf, headerCompat(F("Origin")), sizeof(originBuf));
  if (originBuf[0] == '\0') {
    strlcpy(originBuf, headerCompat(F("Referer")), sizeof(originBuf));
  }
  if (originBuf[0] == '\0') return true;  // no origin header — allow (non-browser client)

  strlcpy(hostBuf, hostHeaderCompat(), sizeof(hostBuf));
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

  // SECURITY (ADR-056): do NOT short-circuit OPTIONS here. The /api/v2 CORS
  // preflight is already answered upstream in processAPI (OPTIONS -> sendApiOptions
  // -> 204, before any handler or this auth check). A blanket "OPTIONS returns
  // true" would let a cross-origin CORS preflight bypass Basic Auth AND the
  // same-origin CSRF check on the HTTP_ANY admin routes (/ReBoot, /ResetWireless,
  // /pic) whose handlers reach their side effect directly: a malicious LAN page's
  // preflight could wipe WiFi credentials or reboot the device. OPTIONS therefore
  // falls through to the same auth + same-origin gate as every other method.

  if (!authenticateCompat("admin", settings.sHTTPpasswd)) {
    webRequestAuth();
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
  webPushHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, OPTIONS"));
  webPushHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
  webPushHeader(F("Access-Control-Max-Age"), F("86400"));
  webSendStatus(204);
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
  webSend(202, F("application/json"), F("{\"status\":\"queued\"}"));
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
  webSend(200, F("application/json"), jsonBuf);
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
  // ADR-141: ArduinoJson v7. Native types replace the hand-rolled streaming map
  // plus the raw restSendContent "devices"/"s0" sub-objects.
  JsonDocument doc;
  JsonObject sensors = doc[F("sensors")].to<JsonObject>();

  // Dallas temperature sensors
  sensors[F("dallas_enabled")]  = settings.sensors.bEnabled;
  sensors[F("dallas_detected")] = bSensorsDetected;
  sensors[F("dallas_count")]    = (int32_t)DallasrealDeviceCount;
  sensors[F("dallas_gpio")]     = (int32_t)settings.sensors.iPin;
  sensors[F("dallas_poll_sec")] = (int32_t)settings.sensors.iInterval;
  sensors[F("simulated")]       = state.debug.bSensorSim;

  // Individual sensor readings
  if (bSensorsDetected || state.debug.bSensorSim) {
    JsonObject devices = sensors[F("devices")].to<JsonObject>();
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char *addr = getDallasAddress(DallasrealDevice[i].addr);
      if (!addr) continue;
      // getDallasAddress() returns a shared static buffer overwritten each call;
      // String() copies the key so multiple devices do not alias to the last addr.
      JsonObject dev = devices[String(addr)].to<JsonObject>();
      dev[F("temp")]  = DallasrealDevice[i].tempC;
      dev[F("epoch")] = (uint32_t)DallasrealDevice[i].lasttime;
    }
  }

  // S0 pulse counter
  JsonObject s0 = sensors[F("s0")].to<JsonObject>();
  s0[F("enabled")]  = settings.s0.bEnabled;
  s0[F("gpio")]     = settings.s0.iPin;
  s0[F("poll_sec")] = settings.s0.iInterval;
  s0[F("pulses")]   = (uint32_t)OTGWs0pulseCount;
  s0[F("total")]    = (uint32_t)OTGWs0pulseCountTot;
  s0[F("power_kw")] = OTGWs0powerkw;
  s0[F("epoch")]    = (uint32_t)OTGWs0lasttime;

  restSendJson(doc);
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
    if (!hasArgCompat("mode")) { sendApiError(400, F("Missing 'mode' parameter")); return; }
    String modeStr = argCompat("mode");
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
      JsonDocument doc;
      JsonObject o = doc[F("otdirect_settings")].to<JsonObject>();
      o[F("mode")]            = (int32_t)settings.otd.iMode;
      o[F("setback_temp")]    = settings.otd.fSetbackTemp;
      o[F("setback_timeout")] = (int32_t)settings.otd.iSetbackTimeout;
      // TASK-183: PI room compensation + heating curve
      o[F("ch_mode")]         = (int32_t)settings.otd.iCHMode;
      o[F("flow_temp")]       = settings.otd.fFlowTemp;
      o[F("flow_max")]        = settings.otd.fFlowMax;
      o[F("room_setpoint")]   = settings.otd.fRoomSetpoint;
      o[F("gradient")]        = settings.otd.fGradient;
      o[F("exponent")]        = settings.otd.fExponent;
      o[F("offset")]          = settings.otd.fOffset;
      o[F("room_comp")]       = settings.otd.bRoomCompEnabled;
      o[F("kp")]              = settings.otd.fKp;
      o[F("ki")]              = settings.otd.fKi;
      o[F("kboost")]          = settings.otd.fKboost;
      // TASK-582: CH hysteresis deadband
      o[F("hysteresis_enable")] = settings.otd.bHysteresisEnable;
      o[F("hysteresis")]        = settings.otd.fHysteresis;
      // TASK-584: ventilation override persistence
      o[F("vent_enable")]      = settings.otd.bVentEnable;
      o[F("open_bypass")]      = settings.otd.bOpenBypass;
      o[F("auto_bypass")]      = settings.otd.bAutoBypass;
      o[F("free_vent_enable")] = settings.otd.bFreeVentEnable;
      o[F("vent_setpoint")]    = (int32_t)settings.otd.iVentSetpoint;
      restSendJson(doc);
    } else if (method == HTTP_POST || method == HTTP_PUT) {
      if (hasArgCompat("setbacktemp"))    updateSetting("OTDsetbacktemp", argCompat("setbacktemp"));
      if (hasArgCompat("setbacktimeout")) updateSetting("OTDsetbacktimeout", argCompat("setbacktimeout"));
      // TASK-183: PI room compensation + heating curve settings
      if (hasArgCompat("chmode"))       updateSetting("OTDchmode", argCompat("chmode"));
      if (hasArgCompat("flowtemp"))     updateSetting("OTDflowtemp", argCompat("flowtemp"));
      if (hasArgCompat("flowmax"))      updateSetting("OTDflowmax", argCompat("flowmax"));
      if (hasArgCompat("roomsetpoint")) updateSetting("OTDroomsetpoint", argCompat("roomsetpoint"));
      if (hasArgCompat("gradient"))     updateSetting("OTDgradient", argCompat("gradient"));
      if (hasArgCompat("exponent"))     updateSetting("OTDexponent", argCompat("exponent"));
      if (hasArgCompat("offset"))       updateSetting("OTDoffset", argCompat("offset"));
      if (hasArgCompat("roomcomp"))     updateSetting("OTDroomcomp", argCompat("roomcomp"));
      if (hasArgCompat("kp"))           updateSetting("OTDkp", argCompat("kp"));
      if (hasArgCompat("ki"))           updateSetting("OTDki", argCompat("ki"));
      if (hasArgCompat("kboost"))       updateSetting("OTDkboost", argCompat("kboost"));
      // TASK-582: CH hysteresis deadband settings
      if (hasArgCompat("hysteresisenable")) updateSetting("OTDhysteresisenable", argCompat("hysteresisenable"));
      if (hasArgCompat("hysteresis"))       updateSetting("OTDhysteresis", argCompat("hysteresis"));
      // TASK-584: ventilation override persistence settings
      if (hasArgCompat("ventenable"))    updateSetting("OTDventenable", argCompat("ventenable"));
      if (hasArgCompat("openbypass"))    updateSetting("OTDopenbypass", argCompat("openbypass"));
      if (hasArgCompat("autobypass"))    updateSetting("OTDautobypass", argCompat("autobypass"));
      if (hasArgCompat("freeventenable")) updateSetting("OTDfreeventenable", argCompat("freeventenable"));
      if (hasArgCompat("ventsetpoint"))  updateSetting("OTDventsetpoint", argCompat("ventsetpoint"));
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
      if (!hasArgCompat("action") || !hasArgCompat("msgid")) {
        sendApiError(400, F("Missing 'action' and/or 'msgid' parameter")); return;
      }
      // Validate msgid is numeric 0-127
      const char* msgidRaw = argCompat("msgid");
      long msgidVal = strtol(msgidRaw, nullptr, 10);
      if (msgidVal < 0 || msgidVal > 127) {
        sendApiError(400, F("Invalid msgid (0-127)")); return;
      }
      char msgidBuf[4];
      strlcpy(msgidBuf, msgidRaw, sizeof(msgidBuf));

      const char* actionRaw = argCompat("action");
      char cmdBuf[16];
      if (strcmp_P(actionRaw, PSTR("sr")) == 0 || strcmp_P(actionRaw, PSTR("rm")) == 0) {
        if (!hasArgCompat("value")) { sendApiError(400, F("Missing 'value' parameter")); return; }
        const char* valRaw = argCompat("value");
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
    const char* body = bodyCompat();
    char cmdBuf[64] = "";
    if (!extractJsonField(String(body), F("command"), cmdBuf, sizeof(cmdBuf))) {
      strlcpy(cmdBuf, body, sizeof(cmdBuf));
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
    webSend(202, F("application/json"), F("{\"status\":\"accepted\"}"));
    doAutoConfigure();
  } else if (strcmp_P(words[4], PSTR("label")) == 0) {
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    if (wc <= 5 || words[5][0] == '\0') { sendApiError(400, F("Missing label")); return; }
    sendOTLabel(words[5]);
  } else if (strcmp_P(words[4], PSTR("boiler-support")) == 0) {
    // TASK-692 port (dev TASK-686): GET /api/v2/otgw/boiler-support ->
    // unsupported_read / unsupported_write arrays sourced from the in-RAM
    // bitmaps populated by processOT. ADR-141: built as one ArduinoJson v7
    // document so the OTmap label/friendly strings are escape-safe (the old
    // snprintf path did not escape them). label/friendly are PROGMEM const
    // char* literals — store-by-pointer is safe, they outlive the document.
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray ur = root[F("unsupported_read")].to<JsonArray>();
    for (int i = 0; i <= 255; i++) {
      if (!isBoilerMsgIdUnsupportedRead((uint8_t)i)) continue;
      OTlookup_t item;
      const char* label = "Unknown";
      const char* friendly = "";
      if (i <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[i], item); label = item.label; friendly = item.friendlyname; }
      JsonObject e = ur.add<JsonObject>();
      e[F("id")]       = i;
      e[F("label")]    = label;
      e[F("friendly")] = friendly;
    }
    JsonArray uw = root[F("unsupported_write")].to<JsonArray>();
    for (int i = 0; i <= 255; i++) {
      if (!isBoilerMsgIdUnsupportedWrite((uint8_t)i)) continue;
      OTlookup_t item;
      const char* label = "Unknown";
      const char* friendly = "";
      if (i <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[i], item); label = item.label; friendly = item.friendlyname; }
      JsonObject e = uw.add<JsonObject>();
      e[F("id")]       = i;
      e[F("label")]    = label;
      e[F("friendly")] = friendly;
    }
    restSendJson(doc);
  } else if (strcmp_P(words[4], PSTR("ot-support")) == 0) {
    // TASK-694 port (dev TASK-689): GET /api/v2/otgw/ot-support -> bilateral
    // OT support map. Compact mode — only msgIDs where at least one of the
    // six bitmaps has the bit set. ADR-141: ArduinoJson v7 document; the six
    // ts*/bl* fields stay real JSON bools (assigned from the native bool vars).
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray msgids = root[F("msgids")].to<JsonArray>();
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
      JsonObject e = msgids.add<JsonObject>();
      e[F("id")]    = id;
      e[F("label")] = label;
      e[F("tsR")]   = tsR;
      e[F("tsW")]   = tsW;
      e[F("blAR")]  = blAR;
      e[F("blAW")]  = blAW;
      e[F("blUR")]  = blUR;
      e[F("blUW")]  = blUW;
    }
    restSendJson(doc);
  } else if (strcmp_P(words[4], PSTR("overrides")) == 0) {
    // ADR-118: GET /api/v2/otgw/overrides -> active gateway-override values that the
    // boiler-side-worldview gate (ADR-096/103) drops from canonical. Additive surface;
    // distinct from the OT-Direct overrides under /api/v2/otdirect/overrides.
    // ADR-141: ArduinoJson v7 document; value is the native float (serialised
    // directly, no dtostrf) and age_s an unsigned long.
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray overrides = root[F("overrides")].to<JsonArray>();
    const uint32_t now = millis();
    for (int i = 0; i < OVERRIDE_STORE_MAX; i++) {
      if (otOverrideStore[i].lastSeen == 0) continue;
      if ((now - otOverrideStore[i].lastSeen) >= OVERRIDE_ACTIVE_TIMEOUT) continue;
      const uint8_t id = otOverrideStore[i].id;
      OTlookup_t item;
      const char* label = "Unknown";
      const char* friendly = "";
      if (id <= OT_MSGID_MAX) { PROGMEM_readAnything(&OTmap[id], item); label = item.label; friendly = item.friendlyname; }
      JsonObject e = overrides.add<JsonObject>();
      e[F("id")]       = id;
      e[F("label")]    = label;
      e[F("friendly")] = friendly;
      e[F("kind")]     = (otOverrideStore[i].kind == OT_OVERRIDE_ANSWER) ? "answer" : "substituted";
      e[F("value")]    = otOverrideStore[i].value;
      e[F("age_s")]    = (unsigned long)((now - otOverrideStore[i].lastSeen) / 1000UL);
    }
    restSendJson(doc);
  } else {
    sendApiNotFound(originalURI);
  }
}

static void handleWebhook(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (wc > 4 && strcmp_P(words[4], PSTR("test")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST")); return; }
    String stateParam = argCompat(F("state"));
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
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
// and auto-tune diagnostics. Built as one ArduinoJson v7 document (ADR-141).
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

  // --- Build JSON response (ADR-141: ArduinoJson v7) ---
  // Empty-key wrapper = a flat ROOT object: doc.to<JsonObject>().
  // ArduinoJson serialises NaN/Inf as null, matching the old satSendJsonFloat
  // null handling, so the per-field decimal precision is intentionally dropped.
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();

  // Synchronization problem indicators (AC#3)
  o[F("sync_setpoint")]       = syncSetpoint;
  o[F("sync_modulation")]     = syncModulation;
  o[F("sync_ch")]             = syncCH;

  // Pressure diagnostics (AC#4)
  o[F("pressure_smoothed")]   = state.sat.fSmoothedPressure;
  o[F("pressure_drop_rate")]  = state.sat.fPressureDropRate;
  o[F("pressure_alarm")]      = state.sat.bPressureAlarm;

  // Cycle diagnostics (AC#5)
  o[F("cycle_kind")]          = ckNames[ckIdx];
  o[F("cycle_duration")]      = state.sat.fLastCycleDuration;
  o[F("cycle_count")]         = state.sat.iCycleCount;
  o[F("cycle_fraction_ch")]   = state.sat.fLastCycleFractionCH;
  o[F("cycle_fraction_dhw")]  = state.sat.fLastCycleFractionDHW;

  // Error / curve statistics (AC#6)
  o[F("error_mean")]          = state.sat.fMeanError;
  o[F("error_stddev")]        = state.sat.fErrorStdDev;
  o[F("error_samples")]       = (int32_t)state.sat.iErrorSampleCount;
  o[F("heating_curve_recommendation")] = state.sat.sHeatCurveRec;

  // Health booleans (AC#7)
  o[F("flame_health")]        = flameHealth;
  o[F("device_health")]       = deviceHealth;
  o[F("cycle_health")]        = cycleHealth;
  o[F("cycle_class")]         = ccNames[ccIdx];

  // Auto-tune and OVP calibration (AC#8)
  o[F("auto_tune_score")]     = state.sat.fAutoTuneScore;
  o[F("auto_tune_cycles")]    = (int32_t)state.sat.iAutoTuneCycles;
  o[F("ovp_phase")]           = (int32_t)state.sat.eCalibPhase;

  restSendJson(doc);
}

// Check whether the current request carries ?detail=full
static bool satRequestHasDetailFull()
{
  return hasArgCompat(F("detail"))
      && strcmp_P(argCompat(F("detail")), PSTR("full")) == 0;
}

#if HAS_SAT_BLE
// TASK-508: pull two named fields from a JSON body in one call. Trivial
// wrapper over extractJsonField() (line 2167) so handleSAT label/forget
// branches stay short. TASK-743: gated on the HAS_SAT_BLE capability flag
// (not raw ESP32) to match its only caller (the BLE label/forget branch).
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
// POST /api/v2/sat/sim/event            — inject sim scenario (window_open/solar_gain/sensor_noise/sensor_dropout/dhw_demand); 409 if sim off
// POST /api/v2/sat/flush                — flush short-lived data (PID integral + cycle window)
// POST /api/v2/sat/settings/<name>      — update any SAT setting (mirrors all MQTT sat/* commands)
static void handleSAT(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)
{
  if (!checkHttpAuth()) return;

  if (wc <= 4) {
    // GET /api/v2/sat — default to status
    if (method == HTTP_GET) {
      webPushHeader(F("Cache-Control"), F("no-cache"));
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
    webPushHeader(F("Cache-Control"), F("no-cache"));
    if (satRequestHasDetailFull()) { satSendHealthJSON(); }
    else                           { satSendStatusJSON(); }
  }
  else if (strcasecmp_P(sub, PSTR("target")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleTargetTemp(val)) {
      sendApiError(400, F("Invalid or missing value (5.0-30.0)"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("externaltemp")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleExternalTemp(val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("externaloutdoor")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleExternalOutdoor(val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  // POST /api/v2/sat/pvsurplus — push PV-surplus power (TASK-640).
  // Body: "1500" or {"value":"1500"} or {"value":1500}. Range 0-50000 W.
  else if (strcasecmp_P(sub, PSTR("pvsurplus")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandlePvSurplus(val)) {
      sendApiError(400, F("Invalid or missing numeric value (0-50000 W)"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  // POST /api/v2/sat/sim/event — inject a bench scenario (TASK-797 / plan §12 F2).
  // Body: {"event":"window_open|solar_gain","value":<num>,"duration_s":<int>}.
  // Honoured only while simulation is active (HTTP 409 otherwise) so it never
  // perturbs a real-boiler rig. Topic/value parsing via extractJsonField (no
  // ArduinoJson, per ADR).
  else if (strcasecmp_P(sub, PSTR("sim")) == 0) {
    if (wc < 6 || strcasecmp_P(words[5], PSTR("event")) != 0) {
      sendApiError(404, F("Unknown sim sub-resource (expected sim/event)"));
      return;
    }
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    if (!settings.sat.bSimulation) {
      sendApiError(409, F("simulation inactive: enable bSimulation before injecting events"));
      return;
    }
    if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
    const char* body = argCompat(F("plain"));
    char evtBuf[20], valBuf[16], durBuf[12];
    if (!extractJsonField(body, F("event"), evtBuf, sizeof(evtBuf))) {
      sendApiError(400, F("Missing 'event' field"));
      return;
    }
    float   value = extractJsonField(body, F("value"), valBuf, sizeof(valBuf)) ? atof(valBuf) : 0.0f;
    int32_t durS  = extractJsonField(body, F("duration_s"), durBuf, sizeof(durBuf)) ? atoi(durBuf) : 0;
    if (!satSimInjectEvent(evtBuf, value, durS)) {
      sendApiError(400, F("Unknown or unsupported event (window_open, solar_gain, sensor_noise, sensor_dropout, dhw_demand)"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("reset_integral")) == 0) {
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    satResetIntegral();
    webSend(200, F("application/json"), F("{\"status\":\"ok\",\"integral\":0}"));
  }
  else if (strcasecmp_P(sub, PSTR("flush")) == 0) {
    // POST /api/v2/sat/flush — clear short-lived SAT data (PID integral + cycle window) (Task #237)
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    satFlushShortLivedData();
    webSend(200, F("application/json"), F("{\"result\":\"ok\",\"flushed\":[\"pid\",\"cycles\"]}"));
  }
  else if (strcasecmp_P(sub, PSTR("window")) == 0) {
    if (method == HTTP_POST || method == HTTP_PUT) {
      char valBuf[16];
      const char* val = nullptr;
      if (hasArgCompat(F("plain"))) {
        val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
      } else if (wc > 5) {
        val = words[5];
      }
      if (!val) { sendApiError(400, F("Missing value (open/closed)")); return; }
      bool isOpen = (strcasecmp_P(val, PSTR("open")) == 0 ||
                    strcasecmp_P(val, PSTR("1")) == 0 ||
                    strcasecmp_P(val, PSTR("ON")) == 0);
      satHandleWindow(isOpen);
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
    } else {
      sendApiMethodNotAllowed(F("POST, PUT"));
    }
  }
  else if (strcasecmp_P(sub, PSTR("preset")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing preset name (away/eco/comfort/sleep/activity/home)")); return; }
    satHandlePreset(val);
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("enable")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing value (0/1)")); return; }
    satHandleEnabled(val);
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
      webPushHeader(F("Cache-Control"), F("no-cache"));
      satBLERosterSendJSON();
    }
    else if (strcasecmp_P(act, PSTR("select")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
      if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      char macBuf[18];
      if (!extractJsonField(argCompat(F("plain")),
                            F("mac"), macBuf, sizeof(macBuf))) {
        sendApiError(400, F("Missing 'mac' field"));
        return;
      }
      if (!satBLERosterSelect(macBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else if (strcasecmp_P(act, PSTR("label")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
      if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      char macBuf[18];
      char labelBuf[32];
      if (!satExtractTwoFields(argCompat(F("plain")),
                                F("mac"),   macBuf,   sizeof(macBuf),
                                F("label"), labelBuf, sizeof(labelBuf))) {
        sendApiError(400, F("Missing 'mac' or 'label' field"));
        return;
      }
      if (!satBLERosterSetLabel(macBuf, labelBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
    }
    else if (strcasecmp_P(act, PSTR("forget")) == 0) {
      if (method != HTTP_POST && method != HTTP_PUT && method != HTTP_DELETE) {
        sendApiMethodNotAllowed(F("POST, PUT, DELETE"));
        return;
      }
      char macBuf[18];
      if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      if (!extractJsonField(argCompat(F("plain")),
                            F("mac"), macBuf, sizeof(macBuf))) {
        sendApiError(400, F("Missing 'mac' field"));
        return;
      }
      if (!satBLERosterForget(macBuf)) {
        sendApiError(404, F("MAC not in roster"));
        return;
      }
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val) { sendApiError(400, F("Missing mode (continuous/pwm)")); return; }
    satHandleControlMode(val);
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("humidity")) == 0) {
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    char valBuf[16];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 5) {
      val = words[5];
    }
    if (!val || !satHandleHumidity(val)) {
      sendApiError(400, F("Invalid or missing value (0-100)"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
      webPushHeader(F("Cache-Control"), F("no-cache"));
      JsonDocument doc;
      JsonObject o = doc.to<JsonObject>();
      o[F("needs_setup")] = needs;
      o[F("has_key")]     = (settings.sat.sWeatherApiKey[0] != '\0');
      restSendJson(doc);
      return;
    }
    if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
    webPushHeader(F("Cache-Control"), F("no-cache"));
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
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
    } else if (wc > 6) {
      val = words[6];
    }
    if (!val || !satHandleAreaTemp((uint8_t)area, val)) {
      sendApiError(400, F("Invalid or missing numeric value"));
      return;
    }
    webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
  }
  else if (strcasecmp_P(sub, PSTR("settings")) == 0) {
    // POST/PUT /api/v2/sat/settings/<setting-name> — mirrors all MQTT sat/<sub-command> handlers
    if (method != HTTP_POST && method != HTTP_PUT) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    if (wc <= 5) { sendApiError(400, F("Missing setting name")); return; }
    const char* settingName = words[5];
    char valBuf[64];
    const char* val = nullptr;
    if (hasArgCompat(F("plain"))) {
      val = satExtractPostValue(argCompat(F("plain")), valBuf, sizeof(valBuf));
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
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
      // TASK-795 §4.2: refuse to enable simulation while a real boiler is on the
      // bus — sim is a boiler-absent bench mode, not a co-existence mode.
      if (EVALBOOLEAN(val) && satBoilerHardwarePresent()) {
        sendApiError(409, F("simulation unavailable: boiler hardware detected"));
        return;
      }
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
    webSend(200, F("application/json"), respBuf);
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
      if (!LittleFS.exists(fname)) {
        webSend(200, F("application/json"), F("[]"));
      } else {
        webPushHeader(F("Cache-Control"), F("no-cache"));
        webSendFile(fname, F("application/json"), /*gzip=*/false);
      }
    }
    else if (method == HTTP_POST) {
      // Parse outside_temp, flow_temp, optional label from JSON body
      if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      const char* body = bodyCompat();
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
      webSend(201, F("application/json"), resp);
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
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
      webPushHeader(F("Cache-Control"), F("no-cache"));
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
      webSend(200, F("application/json"), resp);
    }
    else if (method == HTTP_PATCH || method == HTTP_POST || method == HTTP_PUT) {
      if (!hasArgCompat(F("plain"))) { sendApiError(400, F("Missing JSON body")); return; }
      const char* body = bodyCompat();
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
      webSend(200, F("application/json"), F("{\"status\":\"ok\"}"));
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
    webSend(200, F("application/json"), msg);
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
    webSend(202, F("application/json"), msg);
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
        webSend(429, F("application/json"), jsonBuff);
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
    webSend(200, F("application/json"), msg);
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

  // ADR-141: ArduinoJson v7. Native types replace the hand-rolled streaming map.
  JsonDocument doc;
  JsonObject d = doc[F("debug")].to<JsonObject>();

  d[F("build.version")] = _VERSION;
  d[F("build.number")] = (int32_t)_VERSION_BUILD;
  d[F("build.githash")] = _VERSION_GITHASH;
  d[F("build.date")] = _VERSION_DATE;

  d[F("runtime.heap_free")] = (uint32_t)platformFreeHeap();
  d[F("runtime.heap_frag_pct")] = (uint32_t)platformHeapFragmentation();
  d[F("runtime.heap_min_free")] = (uint32_t)platformMinFreeHeap();
  d[F("runtime.heap_max_alloc")] = (uint32_t)platformMaxFreeBlock();
  d[F("runtime.uptime_sec")] = (uint32_t)state.uptime.iSeconds;
  d[F("runtime.reboots")] = (uint32_t)state.uptime.iRebootCount;
  d[F("runtime.wifi_connected")] = (WiFi.status() == WL_CONNECTED);
  d[F("runtime.wifi_rssi")] = (int32_t)WiFi.RSSI();
  d[F("runtime.wifi_ip")] = ipBuf;
  d[F("runtime.wifi_ssid")] = ssidBuf;
  d[F("runtime.hw_mode")] = hwModeBuf;
  d[F("runtime.net_mode")] = netModeBuf;

  d[F("settings.hostname")] = settings.sHostname;
  d[F("settings.http_passwd")] = settings.sHTTPpasswd[0] ? "***" : "(not set)";
  d[F("settings.led_blink")] = settings.bLEDblink;
  d[F("settings.dark_theme")] = settings.bDarkTheme;
  d[F("settings.mydebug")] = settings.bMyDEBUG;
  d[F("settings.nightly_restart")] = settings.bNightlyRestart;
  d[F("settings.restart_hour")] = (uint32_t)settings.iRestartHour;

  d[F("settings.mqtt.enabled")] = settings.mqtt.bEnable;
  d[F("settings.mqtt.broker")] = settings.mqtt.sBroker;
  d[F("settings.mqtt.port")] = (uint32_t)settings.mqtt.iBrokerPort;
  d[F("settings.mqtt.user")] = settings.mqtt.sUser;
  d[F("settings.mqtt.passwd")] = settings.mqtt.sPasswd[0] ? "***" : "(not set)";
  d[F("settings.mqtt.ha_prefix")] = settings.mqtt.sHaprefix;
  d[F("settings.mqtt.top_topic")] = settings.mqtt.sTopTopic;
  d[F("settings.mqtt.unique_id")] = settings.mqtt.sUniqueid;
  d[F("settings.mqtt.ot_message")] = settings.mqtt.bOTmessage;
  d[F("settings.mqtt.on_change")] = settings.mqtt.bOnChangePublishing;
  d[F("settings.mqtt.interval")] = (uint32_t)settings.mqtt.iInterval;
  d[F("settings.mqtt.sep_sources")] = settings.mqtt.bSeparateSources;
  d[F("settings.mqtt.disc_verify")] = settings.mqtt.bDiscoveryAutoVerify;
  d[F("settings.mqtt.use_legacy_topics")] = settings.mqtt.bUseLegacyOtTopics;
  d[F("settings.mqtt.ha_reboot")] = settings.mqtt.bHaRebootDetect;
  d[F("settings.legacy.port25238")] = settings.mqtt.bLegacyPort25238Enabled;

  d[F("settings.ntp.enabled")] = settings.ntp.bEnable;
  d[F("settings.ntp.timezone")] = settings.ntp.sTimezone;
  d[F("settings.ntp.hostname")] = settings.ntp.sHostname;
  d[F("settings.ntp.sendtime")] = settings.ntp.bSendtime;

  d[F("settings.sensors.enabled")] = settings.sensors.bEnabled;
  d[F("settings.sensors.pin")] = (int32_t)settings.sensors.iPin;
  d[F("settings.sensors.interval")] = (int32_t)settings.sensors.iInterval;

  d[F("settings.s0.enabled")] = settings.s0.bEnabled;
  d[F("settings.s0.pin")] = (uint32_t)settings.s0.iPin;
  d[F("settings.s0.debounce_ms")] = (uint32_t)settings.s0.iDebounceTime;
  d[F("settings.s0.pulse_kw")] = (uint32_t)settings.s0.iPulsekw;
  d[F("settings.s0.interval")] = (uint32_t)settings.s0.iInterval;

  d[F("settings.outputs.enabled")] = settings.outputs.bEnabled;
  d[F("settings.outputs.pin")] = (int32_t)settings.outputs.iPin;
  d[F("settings.outputs.trigger_bit")] = (int32_t)settings.outputs.iTriggerBit;

  d[F("settings.sat.enabled")] = settings.sat.bEnabled;
  d[F("settings.sat.heating_system")] = (uint32_t)settings.sat.iHeatingSystem;
  d[F("settings.sat.target_temp")] = settings.sat.fTargetTemp;
  d[F("settings.sat.curve_coeff")] = settings.sat.fHeatingCurveCoeff;
  d[F("settings.sat.deadband")] = settings.sat.fDeadband;
  d[F("settings.sat.control_interval")] = (uint32_t)settings.sat.iControlInterval;
  d[F("settings.sat.use_external_temp")] = settings.sat.bUseExternalTemp;
  d[F("settings.sat.pwm_auto_switch")] = settings.sat.bPwmAutoSwitch;
  d[F("settings.sat.max_rel_mod")] = (uint32_t)settings.sat.iMaxRelModulation;
  d[F("settings.sat.ovp_value")] = settings.sat.fOvpValue;
  d[F("settings.sat.ovp_enabled")] = settings.sat.bOvpEnabled;
  d[F("settings.sat.overshoot_margin")] = settings.sat.fOvershootMargin;
  d[F("settings.sat.dhw_setpoint")] = settings.sat.fDhwSetpoint;
  d[F("settings.sat.dhw_enabled")] = settings.sat.bDhwEnabled;
  d[F("settings.sat.dhw_enable")] = settings.sat.bDhwEnable;
  d[F("settings.sat.window_detection")] = settings.sat.bWindowDetection;
  d[F("settings.sat.weather_enable")] = settings.sat.bWeatherEnable;
  d[F("settings.sat.weather_key")] = settings.sat.sWeatherApiKey[0] ? "***" : "(not set)";
  d[F("settings.sat.boiler_capacity")] = settings.sat.fBoilerCapacity;
  d[F("settings.sat.simulation")] = settings.sat.bSimulation;
  d[F("settings.sat.solar_gain_enable")] = settings.sat.bSolarGainEnable;
  d[F("settings.sat.summer_simmer")] = settings.sat.bSummerSimmer;
  d[F("settings.sat.comfort_adjust")] = settings.sat.bComfortAdjust;
  d[F("settings.sat.multi_area")] = settings.sat.bMultiArea;
  d[F("settings.sat.auto_tune")] = settings.sat.bAutoTune;
  d[F("settings.sat.max_setpoint")] = settings.sat.fMaxSetpoint;
  d[F("settings.sat.sensor_max_age")] = (uint32_t)settings.sat.iSensorMaxAgeS;
  d[F("settings.sat.error_monitoring")] = settings.sat.bErrorMonitoring;
  d[F("settings.sat.auto_gains")] = settings.sat.bAutoGains;
  d[F("settings.sat.thermal_comfort")] = settings.sat.bThermalComfort;
  d[F("settings.sat.humidity_timeout")] = (uint32_t)settings.sat.iHumidityTimeoutS;
  d[F("settings.sat.heating_mode")] = (uint32_t)settings.sat.iHeatingMode;
  d[F("settings.sat.cycles_per_hour")] = (uint32_t)settings.sat.iCyclesPerHour;
  d[F("settings.sat.valve_offset")] = settings.sat.fValveOffset;
  d[F("settings.sat.solar_freeze_i")] = settings.sat.bSolarFreezeIntegral;
  d[F("settings.sat.flush_threshold_h")] = (uint32_t)settings.sat.iSatFlushThresholdH;
  d[F("settings.sat.zone_count")] = (uint32_t)settings.sat.iZoneCount;
  d[F("settings.sat.zone_timeout_s")] = (uint32_t)settings.sat.iZoneTimeoutS;

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  d[F("settings.otd.mode")] = (uint32_t)settings.otd.iMode;
  d[F("settings.otd.auto_detect")] = settings.otd.bAutoDetect;
  d[F("settings.otd.setback_temp")] = settings.otd.fSetbackTemp;
  d[F("settings.otd.setback_timeout")] = (uint32_t)settings.otd.iSetbackTimeout;
  d[F("settings.otd.enable_slave")] = settings.otd.bEnableSlave;
  d[F("settings.otd.summer_mode")] = settings.otd.bSummerMode;
  d[F("settings.otd.fail_safe")] = settings.otd.bFailSafe;
  d[F("settings.otd.msg_interval")] = (uint32_t)settings.otd.iMsgInterval;
  d[F("settings.otd.has_bypass_relay")] = settings.otd.bHasBypassRelay;
  d[F("settings.otd.ch_mode")] = (uint32_t)settings.otd.iCHMode;
  d[F("settings.otd.flow_temp")] = settings.otd.fFlowTemp;
  d[F("settings.otd.flow_max")] = settings.otd.fFlowMax;
  d[F("settings.otd.room_setpoint")] = settings.otd.fRoomSetpoint;
  d[F("settings.otd.gradient")] = settings.otd.fGradient;
  d[F("settings.otd.exponent")] = settings.otd.fExponent;
  d[F("settings.otd.offset")] = settings.otd.fOffset;
  d[F("settings.otd.room_comp")] = settings.otd.bRoomCompEnabled;
  d[F("settings.otd.kp")] = settings.otd.fKp;
  d[F("settings.otd.ki")] = settings.otd.fKi;
  d[F("settings.otd.kboost")] = settings.otd.fKboost;
#endif

  d[F("state.mqtt.connected")] = state.mqtt.bConnected;
  d[F("state.pic.available")] = state.pic.bAvailable;
  d[F("state.pic.device_id")] = state.pic.sDeviceid;
  d[F("state.pic.type")] = state.pic.sType;
  d[F("state.pic.fw_version")] = state.pic.sFwversion;

  d[F("state.otbus.online")] = state.otBus.bOnline;
  d[F("state.otbus.gateway_mode")] = state.otBus.bGatewayMode;
  d[F("state.otbus.gateway_known")] = state.otBus.bGatewayModeKnown;
  d[F("state.otbus.boiler_state")] = state.otBus.bBoilerState;
  d[F("state.otbus.thermostat_state")] = state.otBus.bThermostatState;
  d[F("state.otbus.ps_mode")] = state.otBus.bPSmode;

  d[F("state.debug.ot_msg")] = state.debug.bOTmsg;
  d[F("state.debug.rest_api")] = state.debug.bRestAPI;
  d[F("state.debug.mqtt")] = state.debug.bMQTT;
  d[F("state.debug.mqtt_gate")] = state.debug.bMQTTGate;
  d[F("state.debug.sensors")] = state.debug.bSensors;
  d[F("state.debug.ntp")] = state.debug.bNTP;
  d[F("state.debug.sensor_sim")] = state.debug.bSensorSim;
  d[F("state.debug.otgw_sim")] = state.debug.bOTGWSimulation;
  d[F("state.debug.sat")] = state.debug.bSAT;
  d[F("state.debug.otdirect")] = state.debug.bOTDirect;
#if HAS_SAT_BLE
  d[F("state.debug.sat_ble")] = state.debug.bSATBLE;
#endif

  d[F("state.heap.ws_drops")] = (uint32_t)state.heapdiag.iWsDropsTotal;
  d[F("state.heap.mqtt_drops")] = (uint32_t)state.heapdiag.iMqttDropsTotal;
  d[F("state.heap.entered_low")] = (uint32_t)state.heapdiag.iEnteredLowCount;
  d[F("state.heap.entered_warn")] = (uint32_t)state.heapdiag.iEnteredWarningCount;
  d[F("state.heap.entered_crit")] = (uint32_t)state.heapdiag.iEnteredCriticalCount;
  d[F("state.heap.drip_slow")] = (uint32_t)state.heapdiag.iDripSlowModeCount;

  d[F("state.disco.published")] = (uint32_t)state.discovery.iPublishedTopicCount;
  d[F("state.disco.verify_runs")] = (uint32_t)state.discovery.iVerifyRunCount;
  d[F("state.disco.republishes")] = (uint32_t)state.discovery.iRepublishTriggeredCount;
  d[F("state.disco.last_missing")] = (uint32_t)state.discovery.iLastMissingCount;
  d[F("state.disco.last_orphan")] = (uint32_t)state.discovery.iLastOrphanCount;
  d[F("state.disco.last_epoch")] = (uint32_t)state.discovery.iLastVerifyEpoch;

  { char boilerStatus[20]; satGetBoilerStatusName(boilerStatus, sizeof(boilerStatus));
    d[F("state.sat.boiler_status")] = boilerStatus; }
  { char manufacturer[12]; satGetManufacturerName(manufacturer, sizeof(manufacturer));
    d[F("state.sat.manufacturer")] = manufacturer; }
  d[F("state.sat.active")] = state.sat.bActive;
  d[F("state.sat.control_mode")] = (int32_t)state.sat.eControlMode;
  d[F("state.sat.room_temp")] = satGetRoomTemp();
  d[F("state.sat.outside_temp")] = satGetOutsideTemp();
  d[F("state.sat.heating_curve")] = state.sat.fHeatingCurveValue;
  d[F("state.sat.pid_output")] = state.sat.fPidOutput;
  d[F("state.sat.final_setpoint")] = state.sat.fFinalSetpoint;
  d[F("state.sat.error")] = state.sat.fError;
  d[F("state.sat.pid_p")] = state.sat.fPidP;
  d[F("state.sat.pid_i")] = state.sat.fPidI;
  d[F("state.sat.pid_d")] = state.sat.fPidD;
  d[F("state.sat.cycle_count")] = (uint32_t)state.sat.iCycleCount;
  d[F("state.sat.last_cycle_class")] = (int32_t)state.sat.eLastCycleClass;
  d[F("state.sat.duty_ratio")] = state.sat.fDutyRatio;
  d[F("state.sat.pwm_duty")] = state.sat.fPwmDutyCycle;
  d[F("state.sat.active_preset")] = (int32_t)state.sat.eActivePreset;
  d[F("state.sat.mod_suppressed")] = state.sat.bModSuppressed;
  d[F("state.sat.dhw_active")] = state.sat.bDhwActive;
  d[F("state.sat.fallback_active")] = state.sat.bFallbackActive;
  d[F("state.sat.fallback_reason")] = (int32_t)state.sat.eFallbackReason;
  d[F("state.sat.current_mod")] = (int32_t)state.sat.iCurrentModulation;
  d[F("state.sat.hsys_detected")] = (int32_t)state.sat.iDetectedHeatingSystem;
  d[F("state.sat.weather_valid")] = state.sat.weather.bValid;
  d[F("state.sat.weather_temp")] = state.sat.weather.fTemperature;
  d[F("state.sat.weather_humidity")] = state.sat.weather.fHumidity;
  d[F("state.sat.weather_wind")] = state.sat.weather.fWindSpeed;
  d[F("state.sat.weather_cloud")] = state.sat.weather.fCloudCover;
#if HAS_WEATHER_FORECAST
  d[F("state.sat.weather_pressure")] = state.sat.weather.fPressureMsl;
  d[F("state.sat.weather_is_day")] = state.sat.weather.bIsDay;
#endif
  d[F("state.sat.current_power")] = state.sat.fCurrentPower;
  d[F("state.sat.energy_total")] = state.sat.fEnergyTotal;
  d[F("state.sat.energy_est")] = state.sat.fEnergyEstimatedKWh;
  d[F("state.sat.thermal_valid")] = state.sat.bThermalModelValid;
  d[F("state.sat.thermal_drop")] = state.sat.fThermalDropRate;
  d[F("state.sat.solar_gain")] = state.sat.bSolarGainActive;
  d[F("state.sat.summer_active")] = state.sat.bSummerActive;
  d[F("state.sat.humidity")] = state.sat.fHumidity;
  d[F("state.sat.humidity_valid")] = state.sat.bHumidityValid;
  d[F("state.sat.comfort_offset")] = state.sat.fComfortOffset;
  d[F("state.sat.area0_temp")] = state.sat.fAreaTemp[0];
  d[F("state.sat.area1_temp")] = state.sat.fAreaTemp[1];
  d[F("state.sat.area2_temp")] = state.sat.fAreaTemp[2];
  d[F("state.sat.area3_temp")] = state.sat.fAreaTemp[3];
  d[F("state.sat.auto_tune_active")] = state.sat.bAutoTuneActive;
  d[F("state.sat.auto_tune_cycles")] = (uint32_t)state.sat.iAutoTuneCycles;
  d[F("state.sat.auto_tune_score")] = state.sat.fAutoTuneScore;
#if HAS_SAT_BLE
  d[F("state.sat.ble_temp")] = state.sat.fBleTemp;
  d[F("state.sat.ble_humidity")] = state.sat.fBleHumidity;
  d[F("state.sat.ble_valid")] = state.sat.bBleTempValid;
  d[F("state.sat.ble_count")] = (uint32_t)state.sat.iBleSensorCount;
  d[F("state.sat.ble_battery")] = (uint32_t)state.sat.iBleBattery;
  d[F("state.sat.ble_rssi")] = (int32_t)state.sat.iBleRssi;
#endif

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  d[F("state.otd.schedule_total")] = (uint32_t)state.otd.iScheduleTotal;
  d[F("state.otd.schedule_active")] = (uint32_t)state.otd.iScheduleActive;
  d[F("state.otd.schedule_disabled")] = (uint32_t)state.otd.iScheduleDisabled;
  d[F("state.otd.override_count")] = (uint32_t)state.otd.iOverrideCount;
  d[F("state.otd.bypass_active")] = state.otd.bBypassActive;
  d[F("state.otd.stepup_enabled")] = state.otd.bStepUpEnabled;
  d[F("state.otd.monitor_mode")] = state.otd.bMonitorMode;
  d[F("state.otd.mode")] = (int32_t)state.otd.eMode;
  d[F("state.otd.master_mode")] = state.otd.bMasterMode;
  d[F("state.otd.thermostat_connected")] = state.otd.bThermostatConnected;
  d[F("state.otd.setback_active")] = state.otd.bSetbackActive;
  d[F("state.otd.override_mode")] = (int32_t)state.otd.eOverrideMode;
  d[F("state.otd.override_f88")] = (uint32_t)state.otd.iOverrideF88;
#endif

  restSendJson(doc);
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
    webSend(200, F("application/json"), F("{\"status\":\"scanning\"}"));
    return;
  }

  if (scanResult == WIFI_SCAN_FAILED || !_wifiScanStarted) {
    // Start async scan (non-blocking)
    WiFi.scanNetworks(true /*async*/);
    _wifiScanStarted = true;
    webSend(200, F("application/json"), F("{\"status\":\"scanning\"}"));
    return;
  }

  // scanResult >= 0: scan complete, scanResult = number of networks
  _wifiScanStarted = false;
  // ADR-141: ArduinoJson v7. Bounded array (visible AP count). The SSID is still
  // sanitised in place ('"'/'\\' -> '_') before assignment to preserve the prior
  // value semantics; String() copies each into the doc.
  char connectedSsid[33];
  strlcpy(connectedSsid, WiFi.SSID().c_str(), sizeof(connectedSsid));

  webPushHeader(F("Cache-Control"), F("no-cache"));

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root[F("status")] = F("ready");
  root[F("count")]  = (int32_t)scanResult;
  JsonArray nets = root[F("networks")].to<JsonArray>();
  for (int16_t i = 0; i < scanResult; i++) {
    char ssidBuf[33];
    strlcpy(ssidBuf, WiFi.SSID(i).c_str(), sizeof(ssidBuf));
    // Sanitise quotes/backslash in SSID (kept from the hand-rolled version)
    for (char* p = ssidBuf; *p; p++) { if (*p == '"' || *p == '\\') *p = '_'; }
    bool isConn = (strcmp(ssidBuf, connectedSsid) == 0);
    bool secured = platformWiFiIsEncrypted(i);
    JsonObject net = nets.add<JsonObject>();
    net[F("ssid")]      = String(ssidBuf);
    net[F("rssi")]      = WiFi.RSSI(i);
    net[F("channel")]   = WiFi.channel(i);
    net[F("secured")]   = secured;
    net[F("connected")] = isConn;
  }
  restSendJson(doc);

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
void processAPI(AsyncWebServerRequest *request)
{
  // Bind the per-request context. processAPI is reached from the /api route AND
  // the onNotFound catch-all (which already bound it); rebinding is idempotent.
  webBeginRequest(request);

  // Static buffers save ~356 bytes of stack. Safe under ESPAsyncWebServer's
  // single-task (async_tcp) handler serialization — no concurrent re-entry.
  static char URI[50];
  static char words[API_MAX_WORDS][API_WORD_LEN];
  static char originalURI[sizeof(URI)];
  URI[0] = '\0';
  memset(words, 0, sizeof(words));

  const HTTPMethod method = methodCompat();
  const unsigned long startMs = millis();
  const size_t uriLen = strlcpy(URI, uriCompat(), sizeof(URI));
  strlcpy(originalURI, URI, sizeof(originalURI));

  if (uriLen >= sizeof(URI)) {
    RESTDebugTf(PSTR("REST %s %s => 414 URI too long\r\n"), httpMethodToStr(method), originalURI);
    webSendP(414, PSTR("text/plain"), PSTR("414: URI too long\r\n"));
    return;
  }

  if (platformFreeHeap() < 4096) {
    RESTDebugTf(PSTR("REST %s %s => 500 low heap (%u)\r\n"), httpMethodToStr(method), originalURI, platformFreeHeap());
    webSendP(500, PSTR("text/plain"), PSTR("500: internal server error (low heap)\r\n"));
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
    JsonDocument doc;
    doc.to<JsonObject>()[F("error")] = F("message id: out of range");
    restSendJson(doc);
    return;
  }
  PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
  if (OTlookupitem.type == ot_undef) {
    JsonDocument doc;
    doc.to<JsonObject>()[F("error")] = F("message undefined: reserved for future use");
    restSendJson(doc);
    return;
  }
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  o[F("label")] = OTlookupitem.label;
  if (OTlookupitem.type == ot_f88) {
    o[F("value")] = (float)atof(getOTGWValue(msgid));
  } else {
    o[F("value")] = (int32_t)atoi(getOTGWValue(msgid));
  }
  o[F("unit")] = OTlookupitem.unit;
  restSendJson(doc);
}

void sendOTLabel(const char *msglabel){
  uint_fast8_t msgid;
  for (msgid = 0; msgid <= OT_MSGID_MAX; msgid++){
    PROGMEM_readAnything(&OTmap[msgid], OTlookupitem);
    if (strcasecmp(OTlookupitem.label, msglabel) == 0) break;
  }
  if (msgid > OT_MSGID_MAX){
    JsonDocument doc;
    doc.to<JsonObject>()[F("error")] = F("message id: reserved for future use");
    restSendJson(doc);
    return;
  }
  if (OTlookupitem.type == ot_undef) {
    JsonDocument doc;
    doc.to<JsonObject>()[F("error")] = F("message undefined: reserved for future use");
    restSendJson(doc);
    return;
  }
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  o[F("label")] = OTlookupitem.label;
  if (OTlookupitem.type == ot_f88) {
    o[F("value")] = (float)atof(getOTGWValue(msgid));
  } else {
    o[F("value")] = (int32_t)atoi(getOTGWValue(msgid));
  }
  o[F("unit")] = OTlookupitem.unit;
  restSendJson(doc);
}

//=======================================================================
// (ADR-141) The F()-keyed sendJsonOTmonMapEntry / sendStartJsonMap /
// sendEndJsonMap wrapper overloads that used to live here were removed when
// sendOTmonitorV2() was migrated to ArduinoJson v7. Their only caller was
// sendOTmonitorV2(); the base char* overloads in jsonStuff.ino are untouched.
//=======================================================================

void sendOTmonitorV2()
{
  time_t now = time(nullptr); // needed for Dallas sensor display

  // TASK-865.5 (ADR-123 Phase-1): this is the cross-task READER of the decoded
  // OTGWState snapshot (OTcurrentSystemState.*). Acquire the OTStateLock so the
  // writer (processOT, via drainOTFrameQueue) cannot tear a multi-byte field
  // mid-read once the producer is lifted into a FreeRTOS task (seq6). processOT
  // is never called from here, so the non-recursive mutex cannot self-deadlock.
  //
  // TASK-879: BOUNDED acquire (never the default 0 == portMAX_DELAY). This handler
  // runs on the async_tcp task; the writer (processOT) holds otStateMutex across
  // per-frame I/O, so an unbounded wait here lets a single slow producer frame
  // wedge the async_tcp service task (which is WDT-subscribed) and stall every
  // HTTP request. On timeout we proceed WITHOUT the lock: a torn multi-byte read
  // is cosmetic for a status snapshot, and serving slightly-stale JSON is far
  // better than hanging port 80. A request thread must never block forever on a
  // lock the loop task can hold across writes.
  OTStateLock stateLock(OT_STATE_READ_LOCK_MS);

  // ADR-141: ArduinoJson v7. Each entry is the OTmon compact object shape
  // "name": {"value": V, "unit": "U", "epoch": E}. The generic lambda mirrors
  // the old sendJsonOTmonMapEntry overloads: V is serialised by its native type
  // (CONOFF()/CBOOLEAN strings stay strings, numerics stay numbers).
  JsonDocument doc;
  JsonObject mon = doc[F("otmonitor")].to<JsonObject>();
  auto emit = [&](const __FlashStringHelper* name, auto value,
                  const __FlashStringHelper* unit, uint32_t epoch) {
    JsonObject e = mon[name].to<JsonObject>();
    e[F("value")] = value;
    e[F("unit")]  = unit;
    e[F("epoch")] = epoch;
  };

  emit(F("flamestatus"), CONOFF(isFlameStatus()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("chmodus"), CONOFF(isCentralHeatingActive()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("chenable"), CONOFF(isCentralHeatingEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("ch2modus"), CONOFF(isCentralHeating2Active()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("ch2enable"), CONOFF(isCentralHeating2enabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("dhwmode"), CONOFF(isDomesticHotWaterActive()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("dhwenable"), CONOFF(isDomesticHotWaterEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("diagnosticindicator"), CONOFF(isDiagnosticIndicator()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("faultindicator"), CONOFF(isFaultIndicator()),F(""), getMsgLastUpdated(OT_Statusflags));

  emit(F("coolingmodus"), CONOFF(isCoolingEnabled()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("coolingactive"), CONOFF(isCoolingActive()),F(""), getMsgLastUpdated(OT_Statusflags));
  emit(F("otcactive"), CONOFF(isOutsideTemperatureCompensationActive()),F(""), getMsgLastUpdated(OT_Statusflags));

  if (getMsgLastUpdated(OT_ASFflags)) {
    emit(F("servicerequest"), CONOFF(isServiceRequest()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("lockoutreset"), CONOFF(isLockoutReset()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("lowwaterpressure"), CONOFF(isLowWaterPressure()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("gasflamefault"), CONOFF(isGasFlameFault()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("airtemp"), CONOFF(isAirTemperature()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("waterovertemperature"), CONOFF(isWaterOverTemperature()),F(""), getMsgLastUpdated(OT_ASFflags));
    emit(F("oemfaultcode"), (int32_t)(OTcurrentSystemState.ASFflags & 0xFF), F(""), getMsgLastUpdated(OT_ASFflags));
  }

  if (getMsgLastUpdated(OT_Toutside))            emit(F("outsidetemperature"), OTcurrentSystemState.Toutside, F("°C"), getMsgLastUpdated(OT_Toutside));
  if (getMsgLastUpdated(OT_Tr))                   emit(F("roomtemperature"), OTcurrentSystemState.Tr, F("°C"), getMsgLastUpdated(OT_Tr));
  if (getMsgLastUpdated(OT_TrSet))                emit(F("roomsetpoint"), OTcurrentSystemState.TrSet, F("°C"), getMsgLastUpdated(OT_TrSet));
  if (getMsgLastUpdated(OT_TrOverride))           emit(F("remoteroomsetpoint"), OTcurrentSystemState.TrOverride, F("°C"), getMsgLastUpdated(OT_TrOverride));
  if (getMsgLastUpdated(OT_TSet))                 emit(F("controlsetpoint"), OTcurrentSystemState.TSet,F("°C"), getMsgLastUpdated(OT_TSet));
  if (getMsgLastUpdated(OT_RelModLevel))          emit(F("relmodlvl"), OTcurrentSystemState.RelModLevel,F("%"), getMsgLastUpdated(OT_RelModLevel));
  if (getMsgLastUpdated(OT_MaxRelModLevelSetting))emit(F("maxrelmodlvl"), OTcurrentSystemState.MaxRelModLevelSetting, F("%"), getMsgLastUpdated(OT_MaxRelModLevelSetting));

  if (getMsgLastUpdated(OT_Tboiler))              emit(F("boilertemperature"), OTcurrentSystemState.Tboiler, F("°C"), getMsgLastUpdated(OT_Tboiler));
  if (getMsgLastUpdated(OT_Tret))                 emit(F("returnwatertemperature"), OTcurrentSystemState.Tret,F("°C"), getMsgLastUpdated(OT_Tret));
  if (getMsgLastUpdated(OT_Tdhw))                 emit(F("dhwtemperature"), OTcurrentSystemState.Tdhw,F("°C"), getMsgLastUpdated(OT_Tdhw));
  if (getMsgLastUpdated(OT_TdhwSet))              emit(F("dhwsetpoint"), OTcurrentSystemState.TdhwSet,F("°C"), getMsgLastUpdated(OT_TdhwSet));
  if (getMsgLastUpdated(OT_MaxTSet))              emit(F("maxchwatersetpoint"), OTcurrentSystemState.MaxTSet,F("°C"), getMsgLastUpdated(OT_MaxTSet));
  if (getMsgLastUpdated(OT_CHPressure))           emit(F("chwaterpressure"), OTcurrentSystemState.CHPressure, F("bar"), getMsgLastUpdated(OT_CHPressure));
  if (getMsgLastUpdated(OT_OEMDiagnosticCode))    emit(F("oemdiagnosticcode"), OTcurrentSystemState.OEMDiagnosticCode, F(""), getMsgLastUpdated(OT_OEMDiagnosticCode));

  if (settings.s0.bEnabled)
  {
    emit(F("s0powerkw"), OTGWs0powerkw , F("kW"), OTGWs0lasttime);
    emit(F("s0intervalcount"), OTGWs0pulseCount , F(""), OTGWs0lasttime);
    emit(F("s0totalcount"), OTGWs0pulseCountTot , F(""), OTGWs0lasttime);
  }
  emit(F("sensorsimulation"), state.debug.bSensorSim, F(""), now);
  if (settings.sensors.bEnabled || state.debug.bSensorSim)
  {
    emit(F("numberofsensors"), DallasrealDeviceCount , F(""), now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      if (!strDeviceAddress) continue;
      // Dallas variant adds "type":"dallas" between unit and epoch.
      // getDallasAddress() returns a shared static buffer overwritten each call;
      // String() copies the key so multiple devices do not alias to the last addr.
      JsonObject e = mon[String(strDeviceAddress)].to<JsonObject>();
      e[F("value")] = DallasrealDevice[i].tempC;
      e[F("unit")]  = F("°C");
      e[F("type")]  = F("dallas");
      e[F("epoch")] = (uint32_t)DallasrealDevice[i].lasttime;
      // Labels now managed by Web UI via /dallas_labels.ini file (not sent in API)
    }
  }

  restSendJson(doc);
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

  // ADR-141: ArduinoJson v7. Native types replace the hand-rolled streaming map.
  // String-returning calls (getActiveIP/MAC, WiFi.*.toString, dBmtoQuality) are
  // assigned DIRECTLY (not via CSTR): ArduinoJson copies a String by value, whereas
  // CSTR(<temporary String>) would hand it a const char* into a dying temporary that
  // dangles by serialise time. char[] fields are copied too (mutable char* path).
  JsonDocument doc;
  JsonObject o = doc[F("device")].to<JsonObject>();

  // --- Firmware & build identity ---
  o[F("author")] = F("Robert van den Breemen");
  o[F("fwversion")] = _SEMVER_FULL;
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  o[F("compiled")] = cMsg;
  // ADR-113 stage 2 (TASK-754): picavailable removed; UI selects on hardware_type.
  if (isPICEnabled()) {
    o[F("picfwversion")] = state.pic.sFwversion;
    o[F("picdeviceid")] = state.pic.sDeviceid;
    o[F("picfwtype")] = state.pic.sType;
  }
  o[F("otdirectavailable")] = isOTDirectEnabled();
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    {
      const char* modeStr = "gateway";
      if (state.otd.eMode == OTD_MODE_MONITOR) modeStr = "monitor";
      else if (state.otd.eMode == OTD_MODE_BYPASS) modeStr = "bypass";
      else if (state.otd.eMode == OTD_MODE_MASTER) modeStr = "master";
      else if (state.otd.eMode == OTD_MODE_LOOPBACK) modeStr = "loopback";
      o[F("otdmode")] = modeStr;
    }
    o[F("otdbypass")] = state.otd.bBypassActive;
    o[F("otdmonitor")] = state.otd.bMonitorMode;
    o[F("otdmaster")] = state.otd.bMasterMode;
    o[F("otdstepup")] = state.otd.bStepUpEnabled;
    o[F("otdthermostat")] = state.otd.bThermostatConnected;
    o[F("otdsetback")] = state.otd.bSetbackActive;
    o[F("otdschedtotal")] = state.otd.iScheduleTotal;
    o[F("otdschedactive")] = state.otd.iScheduleActive;
    o[F("otdscheddisabled")] = state.otd.iScheduleDisabled;
    o[F("otdoverrides")] = state.otd.iOverrideCount;
  }
#endif

  // --- Platform & hardware identity ---
  o[F("platform")] = F(PLATFORM_NAME);
  o[F("board")] = boardName();
  o[F("hardware_type")] = hardwareTypeName();  // ADR-113: static board class for codepath selection
  o[F("hardwaremode")] = hardwareModeName();
  o[F("networkmode")] = networkModeName();
  o[F("oledpresent")] = state.hw.bOLEDPresent;
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  o[F("ethernetpresent")] = state.hw.bEthernetPresent;
  o[F("ethernetlink")] = state.net.bEthernetLink;
#endif

  // --- Network identity ---
  o[F("hostname")] = settings.sHostname;
  o[F("ipaddress")] = getActiveIP();
  o[F("macaddress")] = getActiveMAC();
  // Current WiFi network parameters (for DHCP-prefill in the UI). Available
  // on both ESP8266 and ESP32 via the standard WiFi API.
  o[F("wifi_current_subnet")]  = WiFi.subnetMask().toString();
  o[F("wifi_current_gateway")] = WiFi.gatewayIP().toString();
  o[F("wifi_current_dns1")]    = WiFi.dnsIP(0).toString();
  o[F("wifi_current_dns2")]    = WiFi.dnsIP(1).toString();
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // Current Ethernet network parameters (for DHCP-prefill in the UI). Reported
  // via the EthernetESP32 library (Ethernet.* API).
  o[F("eth_current_subnet")]  = Ethernet.subnetMask().toString();
  o[F("eth_current_gateway")] = Ethernet.gatewayIP().toString();
  o[F("eth_current_dns")]     = Ethernet.dnsServerIP().toString();
#endif
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    o[F("ssid")] = state.net.sAPSSID;
    o[F("wifirssi")] = 0;
    o[F("wifiquality")] = 0;
    o[F("wifiquality_text")] = F("AP Mode");
    o[F("apfallback")] = true;
  } else
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) {
    o[F("ssid")] = F("Wired");
    o[F("wifirssi")] = 0;
    o[F("wifiquality")] = 100;
    o[F("wifiquality_text")] = F("Wired");
  } else
#endif
  {
    o[F("ssid")] = WiFi.SSID();
    o[F("wifirssi")] = WiFi.RSSI();
    o[F("wifiquality")] = signal_quality_perc_quad(WiFi.RSSI());
    o[F("wifiquality_text")] = dBmtoQuality(WiFi.RSSI());
  }

  // --- Time, NTP & uptime ---
  o[F("ntpenable")] = settings.ntp.bEnable;
  o[F("ntptimezone")] = settings.ntp.sTimezone;
  o[F("uptime")] = upTime();
  o[F("lastreset")] = lastReset;
  o[F("bootcount")] = state.uptime.iRebootCount;

  // --- Connection status (MQTT, OTGW, thermostat, boiler) ---
  o[F("mqttconnected")] = state.mqtt.bConnected;
  // "otcommandinterface" names which OT interface is active, always one or the other, never both.
  if (isPICEnabled())           o[F("otcommandinterface")] = F("PIC");
  else if (isOTDirectEnabled()) o[F("otcommandinterface")] = F("OT-Direct");
  else                          o[F("otcommandinterface")] = F("None");
  if (hasOTCommandInterface()) {
    o[F("thermostatconnected")] = state.otBus.bThermostatState;
    o[F("boilerconnected")] = state.otBus.bBoilerState;
    o[F("otgwconnected")] = state.otBus.bOnline;
  }
  if (isPICEnabled()) {
    o[F("otgwmode")] = !isGatewayFirmware() ? "N/A" : state.otBus.bGatewayModeKnown ? CCONOFF(state.otBus.bGatewayMode) : "detecting";
  }
  o[F("otgwsimulation")] = state.debug.bOTGWSimulation;

  // --- Chip & CPU ---
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%06X"), (unsigned int)platformChipId());
  o[F("chipid")] = cMsg;
  o[F("coreversion")] = platformCoreVersion();
  o[F("sdkversion")]  = platformSdkVersion();
  o[F("cpufreq")] = platformCpuFreqMHz();

  // --- RAM / heap (free heap, largest block, fragmentation, tier transitions) ---
  o[F("freeheap")] = platformFreeHeap();
  o[F("maxfreeblock")] = platformMaxFreeBlock();
  o[F("hd_fragmentation_pct")] = getHeapFragmentation();
  o[F("hd_enter_low")]        = state.heapdiag.iEnteredLowCount;
  o[F("hd_enter_warning")]    = state.heapdiag.iEnteredWarningCount;
  o[F("hd_enter_critical")]   = state.heapdiag.iEnteredCriticalCount;

  // --- Flash, sketch & filesystem storage (values cached at boot by cacheBootFlashInfo) ---
  o[F("sketchsize")]       = sBootFlash.sketchSize;
  o[F("freesketchspace")]  = sBootFlash.freeSketchSpace;
  o[F("flashchipid")]      = sBootFlash.flashChipId;
  o[F("flashchipsize")]    = sBootFlash.flashChipSizeMB;
  o[F("flashchiprealsize")]= sBootFlash.flashChipRealSizeMB;
  o[F("flashchipspeed")]   = sBootFlash.flashChipSpeedMHz;
  o[F("flashchipmode")]    = flashMode[sBootFlash.flashChipModeIdx < 4 ? sBootFlash.flashChipModeIdx : 4];
  o[F("LittleFSsize")]     = sBootFlash.littleFSSizeMB;

  // --- Reliability drops (heap-pressure side effects) ---
  o[F("hd_ws_drops")]         = state.heapdiag.iWsDropsTotal;
  o[F("hd_mqtt_drops")]       = state.heapdiag.iMqttDropsTotal;

  // --- MQTT Discovery telemetry (ADR-062 / TASK-349 / TASK-361) ---
  o[F("disc_published_topics")]     = state.discovery.iPublishedTopicCount;
  o[F("disc_pending_ids")]          = (uint32_t)countPendingDiscoveryIds();
  o[F("disc_verify_runs")]          = state.discovery.iVerifyRunCount;
  o[F("disc_republish_triggered")]  = state.discovery.iRepublishTriggeredCount;
  o[F("disc_last_missing")]         = (uint32_t)state.discovery.iLastMissingCount;
  o[F("disc_last_orphan")]          = (uint32_t)state.discovery.iLastOrphanCount;
  o[F("disc_last_outcome")]         = verifyOutcomeLabel(state.discovery.eLastOutcome);
  o[F("hd_drip_burst_skip")]        = state.heapdiag.iDripActiveBurstSkipCount;
  o[F("hd_drip_cooldown_skip")]     = state.heapdiag.iDripCooldownSkipCount;
  o[F("hd_drip_slowmode")]          = state.heapdiag.iDripSlowModeCount;
  o[F("perf_sat_status_total_ms")]     = state.restperf.satStatus.iLastTotalMs;
  o[F("perf_sat_status_send_ms")]      = state.restperf.satStatus.iLastSendMs;
  o[F("perf_sat_status_render_ms")]    = state.restperf.satStatus.iLastRenderMs;
  o[F("perf_sat_status_chunks")]       = state.restperf.satStatus.iLastChunkCount;
  o[F("perf_device_info_total_ms")]    = state.restperf.deviceInfo.iLastTotalMs;
  o[F("perf_device_info_send_ms")]     = state.restperf.deviceInfo.iLastSendMs;
  o[F("perf_device_info_render_ms")]   = state.restperf.deviceInfo.iLastRenderMs;
  o[F("perf_device_info_chunks")]      = state.restperf.deviceInfo.iLastChunkCount;
  o[F("perf_device_info_max_ms")]      = state.restperf.deviceInfo.iMaxTotalMs;
  o[F("perf_device_info_samples")]     = state.restperf.deviceInfo.iSampleCount;
  o[F("perf_settings_total_ms")]       = state.restperf.settings.iLastTotalMs;
  o[F("perf_settings_send_ms")]        = state.restperf.settings.iLastSendMs;
  o[F("perf_settings_render_ms")]      = state.restperf.settings.iLastRenderMs;
  o[F("perf_settings_chunks")]         = state.restperf.settings.iLastChunkCount;

  restSendJson(doc);
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
  updateLittleFSStatus(F("/.health"));

  // ADR-141: ArduinoJson v7. Native types replace the hand-rolled streaming map,
  // which emitted booleans as quoted strings ("false") via CBOOLEAN — now real
  // JSON booleans. uptime/networkmode are copied into the doc (String / flash).
  JsonDocument doc;
  JsonObject h = doc[F("health")].to<JsonObject>();
  h[F("status")]         = LittleFSmounted ? "UP" : "DEGRADED";
  h[F("uptime")]         = upTime();
  h[F("heap")]           = platformFreeHeap();
  h[F("networkmode")]    = networkModeName();
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  h[F("wifirssi")]       = (state.net.eMode == NET_ETHERNET) ? 0 : WiFi.RSSI();
#else
  h[F("wifirssi")]       = WiFi.RSSI();
#endif
  h[F("mqttconnected")]  = state.mqtt.bConnected;
  h[F("otgwconnected")]  = state.otBus.bOnline;
  h[F("littlefsMounted")] = LittleFSmounted;

  restSendJson(doc);

} // sendHealth()

//=======================================================================
// Sends latest stored abnormal reboot/crash diagnostics if available.
// Returns: {"crashlog":{"available":true,"summary":"...","details":"..."}}
void sendDeviceCrashLog()
{
  char crashSummary[160] = {0};
  char crashDetails[160] = {0};
  bool hasCrashLog = readLatestCrashLog(crashSummary, sizeof(crashSummary), crashDetails, sizeof(crashDetails));

  JsonDocument doc;
  JsonObject o = doc[F("crashlog")].to<JsonObject>();
  o[F("available")] = hasCrashLog;
  o[F("summary")] = hasCrashLog ? crashSummary : "";
  o[F("details")] = hasCrashLog ? crashDetails : "";
  restSendJson(doc);
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
  JsonDocument doc;
  JsonObject o = doc[F("pic_settings")].to<JsonObject>();
  // Active settings
  o[F("setpoint_override")]   = state.picSettings.sSetpointOverride;
  o[F("setback")]             = state.picSettings.sSetback;
  o[F("dhw_override")]        = state.picSettings.sDhwOverride;
  // Hardware configuration
  o[F("gpio")]                = state.picSettings.sGpio;
  o[F("gpio_states")]         = state.picSettings.sGpioStates;
  o[F("led")]                 = state.picSettings.sLed;
  o[F("tweaks")]              = state.picSettings.sTweaks;
  o[F("temp_sensor")]         = state.picSettings.sTempSensor;
  o[F("smart_power")]         = state.picSettings.sSmartPower;
  o[F("thermostat_detect")]   = state.picSettings.sThermostatDetect;
  // Diagnostics
  o[F("builddate")]           = state.picSettings.sBuilddate;
  o[F("clock_mhz")]           = state.picSettings.sClockMHz;
  o[F("reset_cause")]         = state.picSettings.sResetCause;
  o[F("standalone_interval")] = state.picSettings.sStandaloneInterval;
  o[F("voltage_ref")]         = state.picSettings.sVoltageRef;
  restSendJson(doc);
} // sendPICsettings()

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
//=======================================================================
// Sends OT-direct (OTGW32) runtime status as JSON map.
// Parallel to sendPICsettings() — exposes OTGW32-specific diagnostics.
// Returns: {"otdirect_status":{"bypass":false,"stepup":true,...}}
void sendOTDirectStatus()
{
  JsonDocument doc;
  JsonObject o = doc[F("otdirect_status")].to<JsonObject>();
  // Operating mode
  {
    const char* modeStr = "gateway";
    if (state.otd.eMode == OTD_MODE_MONITOR) modeStr = "monitor";
    else if (state.otd.eMode == OTD_MODE_BYPASS) modeStr = "bypass";
    else if (state.otd.eMode == OTD_MODE_MASTER) modeStr = "master";
    else if (state.otd.eMode == OTD_MODE_LOOPBACK) modeStr = "loopback";
    o[F("mode")]             = modeStr;
  }
  // Hardware state
  o[F("bypass")]           = state.otd.bBypassActive;
  o[F("stepup")]           = state.otd.bStepUpEnabled;
  o[F("monitor_mode")]     = state.otd.bMonitorMode;
  o[F("master_mode")]      = state.otd.bMasterMode;
  // Thermostat connectivity
  o[F("thermostat_connected")] = state.otd.bThermostatConnected;
  o[F("setback_active")]   = state.otd.bSetbackActive;
  // Schedule statistics
  o[F("schedule_total")]   = state.otd.iScheduleTotal;
  o[F("schedule_active")]  = state.otd.iScheduleActive;
  o[F("schedule_disabled")] = state.otd.iScheduleDisabled;
  // Override status
  o[F("overrides_active")] = state.otd.iOverrideCount;
  // OT bus state
  o[F("ot_online")]        = state.otBus.bOnline;
  o[F("thermostat")]       = state.otBus.bThermostatState;
  o[F("boiler")]           = state.otBus.bBoilerState;
  // TASK-184: flame ratio metrics
  o[F("flame_duty_pct")]         = (int32_t)getFlameRatioDuty();
  {
    char freqBuf[8];
    dtostrf(getFlameRatioFreq(), 1, 1, freqBuf);
    o[F("flame_cycles_per_hour")] = freqBuf;  // kept as a quoted string (dtostrf output)
  }
  // TASK-582: CH hysteresis suspension state
  o[F("ch_suspended")]          = state.otd.bCHSuspended;
  restSendJson(doc);
} // sendOTDirectStatus()
#endif

//=======================================================================
void sendPICFlashStatus()
{
  // Minimal PIC flash status endpoint for polling during flash
  // Returns: {"flashstatus":{"flashing":true|false,"progress":0-100,"filename":"...","error":"..."}}
  JsonDocument doc;
  JsonObject o = doc[F("flashstatus")].to<JsonObject>();
  o[F("flashing")] = state.flash.bPICactive;
  o[F("progress")] = state.flash.iPICprogress;
  o[F("filename")] = state.flash.sPICfile;
  o[F("error")] = state.flash.sError;
  restSendJson(doc);
} // sendPICFlashStatus()

//=======================================================================
void sendPICUpdateCheck()
{
  // On-demand PIC firmware update check.
  // Only called when the user opens the PIC firmware tab — never on a timer.
  //
  // TASK-865.14: the outbound HTTP HEAD to otgw.tclcode.com is BLOCKING and this
  // handler runs on the AsyncTCP task (ADR-132). Running it here froze all HTTP +
  // the ADR-133 WebSocket for the request's duration. Instead this endpoint is a
  // pure STATUS QUERY: it kicks a fresh check only from the IDLE state and returns
  // the cached result + a status flag. The loop()-context worker
  // (handlePendingPicHttp) performs the outbound HTTP. The frontend re-polls while
  // status=="checking".
  //
  // The result is sticky for the rest of this boot once it lands (READY/ERROR), so
  // a plain poll never re-hits the server. To force a fresh outbound check (the
  // original "re-check on every tab open" behaviour) the caller passes ?recheck=1,
  // which resets the cache to IDLE so the kick below re-queues exactly one check.
  bool havePic = (strcmp_P(state.pic.sDeviceid, PSTR("unknown")) != 0 && state.pic.sDeviceid[0] != '\0');

  if (havePic && hasArgCompat("recheck") && state.pic.iUpdateCheck != PIC_UPDATE_CHECKING) {
    state.pic.iUpdateCheck = PIC_UPDATE_IDLE;   // explicit re-check request
  }

  // Kick off a fresh check only from IDLE. queuePicUpdateCheck() flips the state to
  // CHECKING (and is a no-op returning false if a PIC HTTP job is already pending),
  // so READY and ERROR are both stable and reportable below — no re-queue per poll.
  if (havePic && state.pic.iUpdateCheck == PIC_UPDATE_IDLE) {
    queuePicUpdateCheck();
  }

  const char *latest = (state.pic.iUpdateCheck == PIC_UPDATE_READY) ? state.pic.sLatestFw : "";
  bool updateAvailable = (state.pic.iUpdateCheck == PIC_UPDATE_READY &&
                          latest[0] != '\0' &&
                          strcmp(latest, state.pic.sFwversion) != 0);

  // status: "ready" | "checking" | "error" | "unavailable"
  const char *status;
  if (!havePic)                                         status = "unavailable";
  else if (state.pic.iUpdateCheck == PIC_UPDATE_READY)  status = "ready";
  else if (state.pic.iUpdateCheck == PIC_UPDATE_ERROR)  status = "error";
  else                                                  status = "checking";

  JsonDocument doc;
  JsonObject o = doc[F("pic_update")].to<JsonObject>();
  o[F("current")] = state.pic.sFwversion;
  o[F("latest")] = latest;
  o[F("update_available")] = updateAvailable;
  o[F("status")] = status;
  restSendJson(doc);
} // sendPICUpdateCheck()

//=======================================================================
void sendFilesystemHashCheck()
{
  // Read the hash stored in LittleFS and compare with the compiled-in firmware hash.
  // Uses the cached getFilesystemHash() — safe to call from an HTTP handler.
  const char* fsHash = getFilesystemHash();
  bool match = (fsHash[0] != '\0' &&
                strcasecmp(fsHash, _VERSION_GITHASH) == 0);
  JsonDocument doc;
  JsonObject o = doc[F("filesystem_check")].to<JsonObject>();
  o[F("match")] = match;
  o[F("fw_hash")] = _VERSION_GITHASH;
  o[F("fs_hash")] = fsHash;
  restSendJson(doc);
} // sendFilesystemHashCheck()

//=======================================================================
void sendFlashStatus()
{
  // Unified flash status endpoint - minimal response with only fields used by frontend
  // Returns: {"flashstatus":{"flashing":bool,"pic_flashing":bool,"pic_progress":0-100,"pic_filename":"...","pic_error":"..."}}
  JsonDocument doc;
  JsonObject o = doc[F("flashstatus")].to<JsonObject>();
  o[F("flashing")] = isFlashing();
  if (isPICEnabled()) {
    o[F("pic_flashing")] = state.flash.bPICactive;
    o[F("pic_progress")] = state.flash.iPICprogress;
    o[F("pic_filename")] = state.flash.sPICfile;
    o[F("pic_error")] = state.flash.sError;
  }
  restSendJson(doc);
} // sendFlashStatus()


//=======================================================================
void sendDeviceTimeV2() 
{
  char buf[50];

  JsonDocument doc;
  JsonObject o = doc[F("devtime")].to<JsonObject>();
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf_P(buf, sizeof(buf), PSTR("%04d-%02d-%02d %02d:%02d:%02d"), myTime.year(), myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second());
  o[F("dateTime")] = buf;
  o[F("epoch")] = (int32_t)now;
  o[F("message")] = getStatusMessageText();
  o[F("psmode")] = state.otBus.bPSmode;
  o[F("otgwsimulation")] = state.debug.bOTGWSimulation;
  o[F("freeheap")] = platformFreeHeap();
  o[F("maxfreeblock")] = platformMaxFreeBlock();
  o[F("networkmode")] = networkModeName();
  o[F("ipaddress")] = getActiveIP();  // TASK-759: live active-transport IP for the header indicator
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    o[F("apfallback")] = true;
    o[F("wifiquality")] = 0;
  } else
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) {
    o[F("wifiquality")] = 100;
  } else
#endif
  {
    o[F("wifiquality")] = signal_quality_perc_quad(WiFi.RSSI());
  }

  restSendJson(doc);

} // sendDeviceTimeV2()

//=======================================================================
void sendDeviceSettings()
{
  const uint32_t startMs = millis();
  restPerfBegin(REST_PERF_SETTINGS);

  // ADR-141: ArduinoJson v7. Each setting becomes a typed sub-object, mirroring
  // the four sendJsonSettingObj overloads:
  //   addStr  -> {"value":"<escaped>","type":"s","maxlen":N}
  //   addInt  -> {"value":<int>,"type":"i","min":N,"max":N}
  //   addNum  -> {"value":<number>,"type":"f","min":N,"max":N}  (float/string source)
  //   addBool -> {"value":<bool>,"type":"b"}
  // addNum injects the dtostrf token verbatim via serialized(String(...)) so the
  // exact digits (and the int-truncated min/max, matching the old overload) are
  // preserved. char[]/String value sources are copied by ArduinoJson.
  JsonDocument doc;
  JsonObject s = doc[F("settings")].to<JsonObject>();
  auto addStr = [&](const __FlashStringHelper* name, const char* value, const char* type, int maxlen) {
    JsonObject e = s[name].to<JsonObject>();
    // Copy the value: some sources are block-scoped locals (ssidBuf) that would
    // dangle before restSendJson(); ArduinoJson stores const char* by pointer.
    e[F("value")]  = String(value);
    e[F("type")]   = type;
    e[F("maxlen")] = maxlen;
  };
  auto addInt = [&](const __FlashStringHelper* name, int32_t value, const char* type, int minValue, int maxValue) {
    JsonObject e = s[name].to<JsonObject>();
    e[F("value")] = value;
    e[F("type")]  = type;
    e[F("min")]   = minValue;
    e[F("max")]   = maxValue;
  };
  auto addNum = [&](const __FlashStringHelper* name, const char* numStr, const char* type, int minValue, int maxValue) {
    JsonObject e = s[name].to<JsonObject>();
    e[F("value")] = serialized(String(numStr));  // raw number token, exact digits
    e[F("type")]  = type;
    e[F("min")]   = minValue;
    e[F("max")]   = maxValue;
  };
  auto addBool = [&](const __FlashStringHelper* name, bool value, const char* type) {
    JsonObject e = s[name].to<JsonObject>();
    e[F("value")] = value;
    e[F("type")]  = type;
  };

  addStr(F("hostname"), CSTR(settings.sHostname), "s", 32);
  { char ssidBuf[33]; strlcpy(ssidBuf, WiFi.SSID().c_str(), sizeof(ssidBuf)); addStr(F("ssid"), ssidBuf, "r", 32); }
  addBool(F("mqttenable"), settings.mqtt.bEnable, "b");
  addStr(F("mqttbroker"), CSTR(settings.mqtt.sBroker), "s", 32);
  addInt(F("mqttbrokerport"), settings.mqtt.iBrokerPort, "i", 0, 65535);
  addStr(F("mqttuser"), CSTR(settings.mqtt.sUser), "s", 32);
  char mqttPasswordPlaceholder[sizeof("password=100")];
  snprintf_P(mqttPasswordPlaceholder,
             sizeof(mqttPasswordPlaceholder),
             PSTR("password=%u"),
             static_cast<unsigned>(strnlen(settings.mqtt.sPasswd, sizeof(settings.mqtt.sPasswd))));
  addStr(F("mqttpasswd"), mqttPasswordPlaceholder, "p", 100);
  addStr(F("mqtttoptopic"), CSTR(settings.mqtt.sTopTopic), "s", 15);
  addStr(F("mqtthaprefix"), CSTR(settings.mqtt.sHaprefix), "s", 20);
  addBool(F("mqttharebootdetection"), settings.mqtt.bHaRebootDetect, "b");
  addStr(F("mqttuniqueid"), CSTR(settings.mqtt.sUniqueid), "s", 20);
  addBool(F("mqttotmessage"), settings.mqtt.bOTmessage, "b");
  addBool(F("mqttonchangepublishing"), settings.mqtt.bOnChangePublishing, "b");
  addInt(F("mqttinterval"), settings.mqtt.iInterval, "i", 0, 3600);
  addBool(F("mqttseparatesources"), settings.mqtt.bSeparateSources, "b");
  addBool(F("mqttuselegacyottopics"), settings.mqtt.bUseLegacyOtTopics, "b");
  addBool(F("legacyport25238enabled"), settings.mqtt.bLegacyPort25238Enabled, "b");
  addBool(F("ntpenable"), settings.ntp.bEnable, "b");
  addStr(F("ntptimezone"), CSTR(settings.ntp.sTimezone), "s", 50);
  addStr(F("ntphostname"), CSTR(settings.ntp.sHostname), "s", 50);
  addBool(F("ntpsendtime"), settings.ntp.bSendtime, "b");
  addBool(F("ledblink"), settings.bLEDblink, "b");
  addBool(F("darktheme"), settings.bDarkTheme, "b");
  addBool(F("nightlyrestart"), settings.bNightlyRestart, "b");
  addInt(F("nightlyrestarthour"), (int)settings.iRestartHour, "i", 0, 23);
#if HAS_RUNTIME_HW_DETECT
  // ADR-127 combo: persisted hardware-mode selector (0=auto, 1=pic, 2=otdirect).
  addInt(F("boardmode"), (int)settings.iBoardMode, "i", 0, 2);
#endif
  addBool(F("ui_autoscroll"), settings.ui.bAutoScroll, "b");
  addBool(F("ui_timestamps"), settings.ui.bShowTimestamp, "b");
  addBool(F("ui_capture"), settings.ui.bCaptureMode, "b");
  addBool(F("ui_autoscreenshot"), settings.ui.bAutoScreenshot, "b");
  addBool(F("ui_autodownloadlog"), settings.ui.bAutoDownloadLog, "b");
  addBool(F("ui_autoexport"), settings.ui.bAutoExport, "b");
  addInt(F("ui_graphtimewindow"), settings.ui.iGraphTimeWindow, "i", 0, 1440);
  addBool(F("gpiosensorsenabled"), settings.sensors.bEnabled, "b");
  addBool(F("gpiosensorslegacyformat"), settings.sensors.bLegacyFormat, "b");
  addInt(F("gpiosensorspin"), settings.sensors.iPin, "i", 0, 16);
  addInt(F("gpiosensorsinterval"), settings.sensors.iInterval, "i", 5, 65535);
  addBool(F("s0counterenabled"), settings.s0.bEnabled, "b");
  addInt(F("s0counterpin"), settings.s0.iPin, "i", 1, 16);
  addInt(F("s0counterdebouncetime"), settings.s0.iDebounceTime, "i", 0, 1000);
  addInt(F("s0counterpulsekw"), settings.s0.iPulsekw, "i", 1, 5000);
  addInt(F("s0counterinterval"), settings.s0.iInterval, "i", 5, 65535);
  addBool(F("gpiooutputsenabled"), settings.outputs.bEnabled, "b");
  addInt(F("gpiooutputspin"), settings.outputs.iPin, "i", 0, 16);
  addInt(F("gpiooutputstriggerbit"), settings.outputs.iTriggerBit, "i", 0, 16);
  addBool(F("otgwcommandenable"), settings.picBoot.bEnable, "b");
  addStr(F("otgwcommands"), CSTR(settings.picBoot.sCommands), "s", 128);
  addBool(F("webhookenable"), settings.webhook.bEnabled, "b");
  addStr(F("webhookurlon"), CSTR(settings.webhook.sURLon), "s", 100);
  addStr(F("webhookurloff"), CSTR(settings.webhook.sURLoff), "s", 100);
  addInt(F("webhooktriggerbit"), settings.webhook.iTriggerBit, "i", 0, 15);
  addStr(F("webhookpayload"), CSTR(settings.webhook.sPayload), "s", 200);
  addStr(F("webhookcontenttype"), CSTR(settings.webhook.sContentType), "s", 31);
  // --- SAT settings ---
  addBool(F("satenabled"), settings.sat.bEnabled, "b");
  addInt(F("satsystem"), settings.sat.iHeatingSystem, "i", 0, 3);  // 0=auto,1=radiators,2=heat_pump,3=underfloor
  addInt(F("satmanufacturer"), settings.sat.iManufacturer, "i", 0, SAT_MFR_COUNT - 1);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fTargetTemp, 1, 1, tmpBuf);
    addNum(F("sattargettemp"), tmpBuf, "f", 5, 30);
    dtostrf(settings.sat.fTargetTempStep, 1, 1, tmpBuf);
    addNum(F("sattempstep"), tmpBuf, "f", 0.1, 1.0);
    dtostrf(settings.sat.fHeatingCurveCoeff, 1, 1, tmpBuf);
    addNum(F("satcoefficient"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fDeadband, 1, 2, tmpBuf);
    addNum(F("satdeadband"), tmpBuf, "f", 0, 2);
  }
  addInt(F("satinterval"), settings.sat.iControlInterval, "i", 10, 300);
  addBool(F("satexternaltemp"), settings.sat.bUseExternalTemp, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPresetComfort, 1, 1, tmpBuf);
    addNum(F("satpresetcomfort"), tmpBuf, "f", 15, 28);
    dtostrf(settings.sat.fPresetEco, 1, 1, tmpBuf);
    addNum(F("satpreseteco"), tmpBuf, "f", 10, 22);
    dtostrf(settings.sat.fPresetAway, 1, 1, tmpBuf);
    addNum(F("satpresetaway"), tmpBuf, "f", 5, 18);
  }
  addBool(F("satpwmautoswitch"), settings.sat.bPwmAutoSwitch, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPresetSleep, 1, 1, tmpBuf);
    addNum(F("satpresetsleep"), tmpBuf, "f", 5, 25);
    dtostrf(settings.sat.fPresetActivity, 1, 1, tmpBuf);
    addNum(F("satpresetactivity"), tmpBuf, "f", 5, 20);
    dtostrf(settings.sat.fPresetHome, 1, 1, tmpBuf);
    addNum(F("satpresethome"), tmpBuf, "f", 10, 25);
  }
  addInt(F("satmaxmodulation"), settings.sat.iMaxRelModulation, "i", 0, 100);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fOvpValue, 1, 1, tmpBuf);
    addNum(F("satovpvalue"), tmpBuf, "f", 0, 90);
  }
  addBool(F("satovpenabled"), settings.sat.bOvpEnabled, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fOvershootMargin, 1, 1, tmpBuf);
    addNum(F("satovershootmargin"), tmpBuf, "f", 0, 5);
  }
  // --- SAT Weather settings (Task #50) ---
  addBool(F("satweatherenable"), settings.sat.bWeatherEnable, "b");
  {
    char tmpBuf[12];
    dtostrf(settings.sat.fWeatherLat, 1, 4, tmpBuf);
    addNum(F("satweatherlat"), tmpBuf, "f", -90, 90);
    dtostrf(settings.sat.fWeatherLon, 1, 4, tmpBuf);
    addNum(F("satweatherlon"), tmpBuf, "f", -180, 180);
  }
  addInt(F("satweatherinterval"), settings.sat.iWeatherInterval, "i", 300, 3600);
  // --- SAT Power/Energy settings (Task #45) ---
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fBoilerCapacity, 1, 1, tmpBuf);
    addNum(F("satboilercapacity"), tmpBuf, "f", 1, 100);
  }
  // --- SAT Preset Sync settings (Task #46) ---
  addBool(F("satpresetsync"), settings.sat.bPresetSync, "b");
  addStr(F("satpresetsynctopic"), CSTR(settings.sat.sPresetSyncTopic), "s", 64);
  // --- SAT Simulation settings (Task #37) ---
  addBool(F("satsimulation"), settings.sat.bSimulation, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSimHeatRate, 1, 2, tmpBuf);
    addNum(F("satsimheatrate"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fSimCoolRate, 1, 2, tmpBuf);
    addNum(F("satsimcoolrate"), tmpBuf, "f", 0, 5);
  }
  // --- SAT Thermal Drop Learning (Task #21) ---
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fThermalCoeff, 1, 4, tmpBuf);
    addNum(F("satthermalcoeff"), tmpBuf, "f", 0, 1);
  }
  // --- SAT Solar Gain settings (Task #23) ---
  addBool(F("satsolargain"), settings.sat.bSolarGainEnable, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSolarMinRiseRate, 1, 1, tmpBuf);
    addNum(F("satsolarminrise"), tmpBuf, "f", 0, 5);
    dtostrf(settings.sat.fSolarSetpointOffset, 1, 1, tmpBuf);
    addNum(F("satsolaroffset"), tmpBuf, "f", 0, 10);
  }
  // --- SAT Summer Simmer settings (Task #24) ---
  addBool(F("satsummersimmer"), settings.sat.bSummerSimmer, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fSummerThreshold, 1, 1, tmpBuf);
    addNum(F("satsummerthreshold"), tmpBuf, "f", 5, 35);
  }
  addInt(F("satsummerminhours"), settings.sat.iSummerMinHours, "i", 1, 48);
  // --- SAT Thermal Comfort settings (Task #28/#47) ---
  addBool(F("satcomfortadjust"), settings.sat.bComfortAdjust, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fComfortHumidity, 1, 0, tmpBuf);
    addNum(F("satcomforthumidity"), tmpBuf, "f", 10, 90);
    dtostrf(settings.sat.fComfortMaxOffset, 1, 1, tmpBuf);
    addNum(F("satcomfortmaxoffset"), tmpBuf, "f", 0, 3);
  }
  // --- SAT Multi-area settings (Task #25) ---
  addBool(F("satmultiarea"), settings.sat.bMultiArea, "b");
  addInt(F("satmultiareacount"), settings.sat.iMultiAreaCount, "i", 0, 4);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAreaWeight[0], 1, 2, tmpBuf);
    addNum(F("satareaweight0"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[1], 1, 2, tmpBuf);
    addNum(F("satareaweight1"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[2], 1, 2, tmpBuf);
    addNum(F("satareaweight2"), tmpBuf, "f", 0, 10);
    dtostrf(settings.sat.fAreaWeight[3], 1, 2, tmpBuf);
    addNum(F("satareaweight3"), tmpBuf, "f", 0, 10);
  }
  // --- SAT PID Auto-Tuning settings (Task #27) ---
  addBool(F("satautotune"), settings.sat.bAutoTune, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAutoTuneRate, 1, 3, tmpBuf);
    addNum(F("satautotunerate"), tmpBuf, "f", 0, 1);
  }
  // --- SAT Python parity settings (Task #82) ---
  addInt(F("satsensormaxage"), (int32_t)settings.sat.iSensorMaxAgeS, "i", 60, 86400);
  addBool(F("saterrormon"), settings.sat.bErrorMonitoring, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fAutoGainsValue, 1, 2, tmpBuf);
    addNum(F("satautogains"), tmpBuf, "f", 0, 10);
  }
  addInt(F("satheatingmode"), settings.sat.iHeatingMode, "i", 0, 1);
  addInt(F("satcyclesperhour"), settings.sat.iCyclesPerHour, "i", 2, 6);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fValveOffset, 1, 2, tmpBuf);
    addNum(F("satvalveoffset"), tmpBuf, "f", -1, 1);
  }
  addBool(F("satsolarfreezeint"), settings.sat.bSolarFreezeIntegral, "b");
  // PV-surplus setpoint boost (TASK-640)
  addBool(F("satpvboostenabled"),        settings.sat.bPvBoostEnabled, "b");
  addInt(F("satpvboostthresholdw"),     (int32_t)settings.sat.iPvBoostThresholdW, "i", 100, 10000);
  addInt(F("satpvboostholds"),          (int32_t)settings.sat.iPvBoostHoldS, "i", 30, 600);
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPvBoostDeltaC, 1, 2, tmpBuf);
    addNum(F("satpvboostdeltac"), tmpBuf, "f", 0, 5);
  }
  {
    char tmpBuf[8];
    dtostrf(settings.sat.fPvBoostMaxIndoorC, 1, 2, tmpBuf);
    addNum(F("satpvboostmaxindoorc"), tmpBuf, "f", 18, 28);
  }
  addInt(F("satpvboostmaxdurationmin"), (int32_t)settings.sat.iPvBoostMaxDurationMin, "i", 30, 1440);
#if HAS_SAT_BLE
  // --- SAT BLE Sensor settings (Task #20). TASK-742: gated on HAS_SAT_BLE. ---
  addBool(F("satbleenable"), settings.sat.bBleEnable, "b");
  addBool(F("satblefailover"), settings.sat.bBleFailover, "b");
  addStr(F("satblemac"), CSTR(settings.sat.sBleMAC), "s", 17);
  addInt(F("satbleinterval"), settings.sat.iBleInterval, "i", 10, 300);
#endif
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  // --- OT-Direct settings (OTGW32 only) ---
  addInt(F("otdmode"), settings.otd.iMode, "i", 0, 4);
  addBool(F("otdautodetect"), settings.otd.bAutoDetect, "b");
  {
    char tmpBuf[8];
    dtostrf(settings.otd.fSetbackTemp, 1, 1, tmpBuf);
    addNum(F("otdsetbacktemp"), tmpBuf, "f", 1, 30);
  }
  addInt(F("otdsetbacktimeout"), settings.otd.iSetbackTimeout, "i", 5, 255);
  addBool(F("otdenableslave"), settings.otd.bEnableSlave, "b");
  addBool(F("otdsummermode"), settings.otd.bSummerMode, "b");
  addBool(F("otdfailsafe"), settings.otd.bFailSafe, "b");
  addInt(F("otdmsginterval"), settings.otd.iMsgInterval, "i", 100, 1275);
  addBool(F("otdhasbypassrelay"), settings.otd.bHasBypassRelay, "b");
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // --- Ethernet settings (only when a W5500 was actually probed) ---
  // The driver is always compiled in on Ethernet-capable boards, but on the
  // combo running in PIC/Classic mode there is no W5500 (initEthernet() is
  // skipped, probeW5500() never runs) so bEthernetPresent stays false. Gate the
  // EXPOSURE on that runtime fact (TASK-865.18): a Classic-mode combo then sees
  // no dead Ethernet fields, while a real OTGW32 (probe succeeded) still does.
  // Omitting "ethstaticip" here keeps the whole eth UI section out of the web
  // page — index.js only renders it when that trigger key is present.
  if (state.hw.bEthernetPresent) {
    addBool(F("ethstaticip"), settings.eth.bStaticIP, "b");
    addStr(F("ethipaddress"), CSTR(settings.eth.sIPaddress), "s", 15);
    addStr(F("ethgateway"), CSTR(settings.eth.sGateway), "s", 15);
    addStr(F("ethsubnet"), CSTR(settings.eth.sSubnet), "s", 15);
    addStr(F("ethdns"), CSTR(settings.eth.sDNS), "s", 15);
  }
#endif
  // --- WiFi static IP settings (empty = DHCP) ---
  addStr(F("wifistaticip"), CSTR(settings.wifi.sStaticIp), "s", 15);
  addStr(F("wifisubnet"),   CSTR(settings.wifi.sSubnet),   "s", 15);
  addStr(F("wifigateway"),  CSTR(settings.wifi.sGateway),  "s", 15);
  addStr(F("wifidns1"),     CSTR(settings.wifi.sDns1),     "s", 15);
  addStr(F("wifidns2"),     CSTR(settings.wifi.sDns2),     "s", 15);
  char httpPasswordPlaceholder[sizeof("password=40")];
  snprintf_P(httpPasswordPlaceholder,
             sizeof(httpPasswordPlaceholder),
             PSTR("password=%u"),
             static_cast<unsigned>(strnlen(settings.sHTTPpasswd, sizeof(settings.sHTTPpasswd))));
  addStr(F("httppasswd"), httpPasswordPlaceholder, "p", 40);

  restSendJson(doc);
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
#if HAS_RUNTIME_HW_DETECT
  "boardmode",  // ADR-127 combo hardware-mode override
#endif
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
  const char* body = bodyCompat();
  if (body[0] == '\0' || body[0] != '{') {
    webSend(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  char field[50];
  if (!extractJsonField(body, F("name"), field, sizeof(field))) {
    webSend(400, F("application/json"), F("{\"error\":\"Missing name\"}"));
    return;
  }

  if (!isKnownSetting(field)) {
    DebugTf(PSTR("postSettings: unknown field [%s], rejected\r\n"), field);
    webSend(400, F("application/json"), F("{\"error\":\"Unknown setting name\"}"));
    return;
  }

  // 150 bytes covers the largest setting value (settingOTGWcommands, max 128 chars).
  char newValue[150];
  if (!extractJsonField(body, F("value"), newValue, sizeof(newValue))) {
    webSend(400, F("application/json"), F("{\"error\":\"Missing value\"}"));
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
  webSend(200, F("application/json"), body);

} // postSettings()


//====[ Dallas sensor label file operations ]====

// Get single Dallas sensor label from file by address
// GET /api/v1/sensors/label?address=28FF64D1841703F1
// Get all Dallas sensor labels from file (bulk read)
void getDallasLabels() {
  if (!LittleFS.exists(F("/dallas_labels.ini"))) {
    // No file exists - return empty JSON object
    webSend(200, F("application/json"), F("{}"));
    return;
  }

  // Stream the file content directly to response
  webSendFile("/dallas_labels.ini", F("application/json"), /*gzip=*/false);
}

// Update all Dallas sensor labels in file (bulk operation)
void updateAllDallasLabels() {
  const char* body = bodyCompat();

  // Validate: body must be a non-empty JSON object (starts with '{', ends with '}')
  size_t bodyLen = strlen(body);
  if (bodyLen == 0 || body[0] != '{' || body[bodyLen - 1] != '}') {
    webSend(400, F("application/json"),
      F("{\"success\":false,\"error\":\"Empty or invalid JSON body\"}"));
    return;
  }
  // Limit file size to prevent filling LittleFS (labels are short strings).
  // Note: the captured body buffer is WEB_BODY_MAX_LEN; a payload larger than
  // that is truncated at capture, so this also reflects the capture ceiling.
  if (bodyLen > 2048) {
    webSend(400, F("application/json"),
      F("{\"success\":false,\"error\":\"Body too large (max 2048 bytes)\"}"));
    return;
  }
  // Basic structural check: all keys and values must be quoted strings.
  // Scan for unquoted colons preceded/followed by quoted strings.
  {
    const char* p = body + 1; // skip opening '{'
    while (*p && *p != '}') {
      while (*p == ' ' || *p == '\r' || *p == '\n' || *p == ',') p++;
      if (*p == '}') break;
      if (*p != '"') {
        webSend(400, F("application/json"),
          F("{\"success\":false,\"error\":\"Invalid JSON: expected quoted key\"}"));
        return;
      }
      // skip key string
      p++;
      while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
      if (*p != '"') { webSend(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: unterminated key\"}")); return; }
      p++; // skip closing quote
      while (*p == ' ') p++;
      if (*p != ':') { webSend(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: expected colon\"}")); return; }
      p++; // skip colon
      while (*p == ' ') p++;
      if (*p != '"') { webSend(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: expected quoted value\"}")); return; }
      // skip value string
      p++;
      while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
      if (*p != '"') { webSend(400, F("application/json"), F("{\"success\":false,\"error\":\"Invalid JSON: unterminated value\"}")); return; }
      p++; // skip closing quote
    }
  }

  // Write validated JSON to file
  File labelsFile = LittleFS.open(F("/dallas_labels.ini"), "w");
  if (!labelsFile) {
    webSendP(500, PSTR("application/json"),
      PSTR("{\"success\":false,\"error\":\"Failed to open file for writing\"}"));
    return;
  }

  labelsFile.print(body);
  labelsFile.close();

  webSend(200, F("application/json"), F("{\"success\":true}"));
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
  AsyncResponseStream *s = restBeginStream("text/html; charset=UTF-8");
  if (!s) return;
  // restBeginStream defaults the status to 200; set 404 explicitly is not
  // exposed on AsyncResponseStream, so emit via setCode below.
  s->setCode(404);
  s->print(F("<!DOCTYPE HTML><html><head>"));
  s->print(F("<style>body { background-color: lightgray; font-size: 15pt;}</style></head><body>"));
  s->print(F("<h1>OTGW firmware</h1><b1>"));
  s->print(F("<br>[<b>"));
  // HTML-escape URI to prevent reflected XSS
  String escapedURI = String(URI);
  escapedURI.replace(F("&"), F("&amp;"));
  escapedURI.replace(F("<"), F("&lt;"));
  escapedURI.replace(F(">"), F("&gt;"));
  escapedURI.replace(F("\""), F("&quot;"));
  escapedURI.replace(F("'"), F("&#39;"));
  s->print(escapedURI);
  s->print(F("</b>] is not a valid "));
  s->print(F("</body></html>\r\n"));
  restFinalize();

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
