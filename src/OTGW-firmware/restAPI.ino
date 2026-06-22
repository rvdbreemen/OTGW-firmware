/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v1.7.0-beta.15
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
  // Reference-bind (no copy): header() returns const String&, and sendHeader()
  // takes const String&. Avoids a per-response heap String allocation across
  // ~17 call sites — pure heap-churn reduction, identical behavior.
  const String& origin = httpServer.header("Origin");
  if (origin.length() > 0) {
    httpServer.sendHeader(F("Access-Control-Allow-Origin"), origin);
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
// Avoids 8 SPI-flash queries on every /api/v2/device/info call.
struct BootFlashCache {
  uint32_t    sketchSize;
  uint32_t    freeSketchSpace;
  char        flashChipId[9];     // formatted as "%08X"
  float       flashChipSizeMB;
  float       flashChipRealSizeMB;
  float       flashChipSpeedMHz;
  char        flashChipMode[8];   // copied from PROGMEM flashMode[] ("Unknown"=7+nul; was const char* into RODATA)
  float       littleFSSizeMB;
};
static BootFlashCache sBootFlash;

void cacheBootFlashInfo() {
  sBootFlash.sketchSize          = ESP.getSketchSize();
  sBootFlash.freeSketchSpace     = ESP.getFreeSketchSpace();
  snprintf_P(sBootFlash.flashChipId, sizeof(sBootFlash.flashChipId),
             PSTR("%08X"), ESP.getFlashChipId());
  sBootFlash.flashChipSizeMB     = ESP.getFlashChipSize()     / 1024.0f / 1024.0f;
  sBootFlash.flashChipRealSizeMB = ESP.getFlashChipRealSize() / 1024.0f / 1024.0f;
  sBootFlash.flashChipSpeedMHz   = floorf(ESP.getFlashChipSpeed() / 1000.0f / 1000.0f);
  { uint8_t fm = ESP.getFlashChipMode(); if (fm > 4) fm = 4;  // bound to flashMode[] (idx 4 = "Unknown")
    strlcpy_P(sBootFlash.flashChipMode, flashMode[fm], sizeof(sBootFlash.flashChipMode)); }
  FSInfo fsinfo;
  LittleFS.info(fsinfo);
  sBootFlash.littleFSSizeMB      = fsinfo.totalBytes / (1024.0f * 1024.0f);
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

constexpr uint8_t API_MAX_WORDS = 6;   // deepest route = 6 segments (words[0..5]); no handler reads words[6+] (was 8)
constexpr size_t  API_WORD_LEN  = 32;

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
static void handleDiscovery(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleDebugDump(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);
static void handleMqtt(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI);

void sendOTGWvalue(int msgid);
void sendOTGWlabel(const char *msglabel);
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
  if (!isPICEnabled()) {
    sendApiError(503, F("No PIC detected - commands disabled"));
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
  addOTWGcmdtoqueue(cmdStr, static_cast<int>(cmdLen));
  sendCorsOriginHeader();
  httpServer.send(202, F("application/json"), F("{\"status\":\"queued\"}"));
}

static void sendOTGWSimulationStatus() {
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
    sendOTGWSimulationStatus();
    return;
  }

  if (wc > 4 && strcmp_P(words[4], PSTR("start")) == 0) {
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    setOTGWSimulationEnabled(true);
    sendOTGWSimulationStatus();
    return;
  }

  if (wc > 4 && strcmp_P(words[4], PSTR("stop")) == 0) {
    if (!isPostOrPut) { sendApiMethodNotAllowed(F("POST, PUT")); return; }
    setOTGWSimulationEnabled(false);
    sendOTGWSimulationStatus();
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
      sendOTGWvalue(msgId);
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
    sendOTGWlabel(words[5]);
  } else if (strcmp_P(words[4], PSTR("boiler-support")) == 0) {
    // TASK-686: GET /api/v2/otgw/boiler-support → unsupported_read / unsupported_write
    // arrays sourced from the in-RAM bitmaps populated by processOT (TASK-684/685).
    // Streamed in chunks so even pathological boilers (hundreds of unsupported ids)
    // do not need a large stack buffer.
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
      snprintf_P(ent, sizeof(ent),
                 first ? PSTR("{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}")
                       : PSTR(",{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}"),
                 i, label, friendly);
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
      snprintf_P(ent, sizeof(ent),
                 first ? PSTR("{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}")
                       : PSTR(",{\"id\":%d,\"label\":\"%s\",\"friendly\":\"%s\"}"),
                 i, label, friendly);
      httpServer.sendContent(ent);
      first = false;
    }
    httpServer.sendContent_P(PSTR("]}"));
    httpServer.sendContent(F(""));
  } else if (strcmp_P(words[4], PSTR("overrides")) == 0) {
    // ADR-082 / TASK-805: GET /api/v2/otgw/overrides → active gateway overrides
    // (answer-override A frames + thermostat-substituted T frames) that the
    // boiler-side-worldview gate drops from canonical. Additive; canonical
    // endpoints are unchanged. Streamed one row per active entry (≤11).
    if (!isGet) { sendApiMethodNotAllowed(F("GET")); return; }
    sendCorsOriginHeader();
    httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    httpServer.send_P(200, PSTR("application/json"), PSTR("{\"overrides\":["));
    bool first = true;
    char row[160];
    char valbuf[16];
    for (uint8_t i = 0; i < OVERRIDE_STORE_MAX; i++) {
      const OTOverrideEntry_t &e = otOverrideStore[i];
      if (!isOTOverrideActive(e)) continue;
      const char* label = messageIDToString(static_cast<OpenThermMessageID>(e.id));
      dtostrf(e.value, 3, 2, valbuf);
      const uint32_t ageSec = (millis() - e.lastSeen) / 1000UL;
      snprintf_P(row, sizeof(row),
                 first ? PSTR("{\"id\":%u,\"label\":\"%s\",\"value\":%s,\"kind\":\"%s\",\"age\":%lu}")
                       : PSTR(",{\"id\":%u,\"label\":\"%s\",\"value\":%s,\"kind\":\"%s\",\"age\":%lu}"),
                 e.id, label, valbuf,
                 (e.kind == OVERRIDE_KIND_ANSWER) ? "answer" : "substituted",
                 (unsigned long)ageSec);
      httpServer.sendContent(row);
      first = false;
    }
    httpServer.sendContent_P(PSTR("]}"));
    httpServer.sendContent(F(""));
  } else if (strcmp_P(words[4], PSTR("ot-support")) == 0) {
    // TASK-689: GET /api/v2/otgw/ot-support → bilateral OT support map.
    // Compact mode — only msgIDs where at least one of the six bitmaps has the
    // bit set. One streamed JSON object per row, no full-payload allocation.
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
      snprintf_P(row, sizeof(row),
                 first ? PSTR("{\"id\":%u,\"label\":\"%s\",\"tsR\":%s,\"tsW\":%s,\"blAR\":%s,\"blAW\":%s,\"blUR\":%s,\"blUW\":%s}")
                       : PSTR(",{\"id\":%u,\"label\":\"%s\",\"tsR\":%s,\"tsW\":%s,\"blAR\":%s,\"blAW\":%s,\"blUR\":%s,\"blUW\":%s}"),
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
    if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_START) { sendApiError(503, F("Heap too low for verify")); return; }
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

//===[ /api/v2/mqtt — MQTT runtime actions ]===
static void handleMqtt(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  // POST /api/v2/mqtt/republish — force full OT value republish (e.g. after broker wipe)
  if (wc > 4 && strcmp_P(words[4], PSTR("republish")) == 0) {
    if (method != HTTP_POST) { sendApiMethodNotAllowed(F("POST")); return; }
    if (!state.mqtt.bConnected) { sendApiError(503, F("MQTT not connected")); return; }
    requestMQTTRepublishAll();
    sendCorsOriginHeader();
    httpServer.send(200, F("application/json"), F("{\"status\":\"ok\",\"message\":\"OT value republish requested\"}"));
    return;
  }
  sendApiNotFound(originalURI);
}

// GET /api/v2/debug — machine-readable dump of all settings and runtime state.
// Auth-protected: contains SSID, broker address, and other config details.
static void handleDebugDump(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (!checkHttpAuth()) return;

  sendStartJsonMap(F("debug"));

  // [build]
  sendJsonMapEntry(F("build.version"),  _VERSION);
  sendJsonMapEntry(F("build.number"),   (int32_t)_VERSION_BUILD);
  sendJsonMapEntry(F("build.githash"),  _VERSION_GITHASH);
  sendJsonMapEntry(F("build.date"),     _VERSION_DATE);

  // [runtime]
  sendJsonMapEntry(F("runtime.heap_free"),     (int32_t)ESP.getFreeHeap());
  sendJsonMapEntry(F("runtime.heap_frag_pct"), (int32_t)ESP.getHeapFragmentation());
  sendJsonMapEntry(F("runtime.heap_min_free"), (int32_t)getMinFreeHeap());
  sendJsonMapEntry(F("runtime.heap_max_block"),(int32_t)ESP.getMaxFreeBlockSize());
  sendJsonMapEntry(F("runtime.uptime_sec"),    (int32_t)state.uptime.iSeconds);
  sendJsonMapEntry(F("runtime.reboots"),       (int32_t)state.uptime.iRebootCount);
  sendJsonMapEntry(F("runtime.wifi_rssi"),     (int32_t)WiFi.RSSI());
  {
    char ipbuf[16];
    strlcpy(ipbuf, WiFi.localIP().toString().c_str(), sizeof(ipbuf));
    sendJsonMapEntry(F("runtime.wifi_ip"),     ipbuf);
    strlcpy(ipbuf, WiFi.SSID().c_str(), sizeof(ipbuf));
    sendJsonMapEntry(F("runtime.wifi_ssid"),   ipbuf);
  }
  sendJsonMapEntry(F("runtime.wifi_connected"), (WiFi.status() == WL_CONNECTED));

  // [settings]
  sendJsonMapEntry(F("settings.hostname"),   settings.sHostname);
  sendJsonMapEntry(F("settings.led_blink"),  settings.bLEDblink);
  sendJsonMapEntry(F("settings.http_auth"),  (settings.sHTTPpasswd[0] != '\0'));

  // [settings.mqtt]
  sendJsonMapEntry(F("settings.mqtt.broker"),    settings.mqtt.sBroker);
  sendJsonMapEntry(F("settings.mqtt.port"),      (int32_t)settings.mqtt.iBrokerPort);
  sendJsonMapEntry(F("settings.mqtt.user"),      settings.mqtt.sUser);
  sendJsonMapEntry(F("settings.mqtt.passwd"),    "***");
  sendJsonMapEntry(F("settings.mqtt.toptopic"),  settings.mqtt.sTopTopic);
  sendJsonMapEntry(F("settings.mqtt.ha_prefix"), settings.mqtt.sHaprefix);
  sendJsonMapEntry(F("settings.mqtt.unique_id"), settings.mqtt.sUniqueid);
  sendJsonMapEntry(F("settings.mqtt.on_change"), settings.mqtt.bOnChangePublishing);
  sendJsonMapEntry(F("settings.mqtt.interval"),  (int32_t)settings.mqtt.iInterval);
  sendJsonMapEntry(F("settings.mqtt.enabled"),   settings.mqtt.bEnable);
  sendJsonMapEntry(F("settings.mqtt.disc_verify"), settings.mqtt.bDiscoveryAutoVerify);
  sendJsonMapEntry(F("settings.mqtt.sep_src"),   settings.mqtt.bSeparateSources);
  sendJsonMapEntry(F("settings.legacy.port_25238"), settings.mqtt.bLegacyPort25238Enabled);

  // [settings.ntp]
  sendJsonMapEntry(F("settings.ntp.server"),  settings.ntp.sHostname);
  sendJsonMapEntry(F("settings.ntp.tz"),      settings.ntp.sTimezone);
  sendJsonMapEntry(F("settings.ntp.enabled"), settings.ntp.bEnable);

  // [settings.sensors]
  sendJsonMapEntry(F("settings.sensors.enabled"),  settings.sensors.bEnabled);
  sendJsonMapEntry(F("settings.sensors.gpio"),     (int32_t)settings.sensors.iPin);
  sendJsonMapEntry(F("settings.sensors.interval"), (int32_t)settings.sensors.iInterval);

  // [settings.s0]
  sendJsonMapEntry(F("settings.s0.enabled"),  settings.s0.bEnabled);
  sendJsonMapEntry(F("settings.s0.gpio"),     (int32_t)settings.s0.iPin);
  sendJsonMapEntry(F("settings.s0.interval"), (int32_t)settings.s0.iInterval);

  // [state.mqtt]
  sendJsonMapEntry(F("state.mqtt.connected"), state.mqtt.bConnected);

  // [state.otgw]
  sendJsonMapEntry(F("state.otgw.online"),    state.otgw.bOnline);
  sendJsonMapEntry(F("state.otgw.ps_mode"),   state.otgw.bPSmode);
  sendJsonMapEntry(F("state.pic.available"),  state.pic.bAvailable);
  sendJsonMapEntry(F("state.pic.fwversion"),  state.pic.sFwversion);

  // [state.debug]
  sendJsonMapEntry(F("state.debug.otgw_sim"),   state.debug.bOTGWSimulation);
  sendJsonMapEntry(F("state.debug.sensor_sim"), state.debug.bSensorSim);
  sendJsonMapEntry(F("state.debug.restapi"),    state.debug.bRestAPI);
  sendJsonMapEntry(F("state.debug.mqtt"),       state.debug.bMQTT);

  sendEndJsonMap(F("debug"));
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
static const char kRouteDiscovery[]  PROGMEM = "discovery";
static const char kRouteDebugDump[]  PROGMEM = "debug";
static const char kRouteMqtt[]       PROGMEM = "mqtt";

static const ApiRoute kV2Routes[] = {
  { kRouteHealth,     handleHealth },
  { kRouteSettings,   handleSettings },
  { kRouteSensors,    handleSensors },
  { kRouteDevice,     handleDevice },
  { kRouteFlash,      handleFlash },
  { kRoutePic,        handlePic },
  { kRouteFirmware,   handleFirmware },
  { kRouteFilesystem, handleFilesystem },
  { kRouteSimulate,   handleSimulate },
  { kRouteOtgw,       handleOtgw },
  { kRouteWebhook,    handleWebhook },
  { kRouteDiscovery,  handleDiscovery },
  { kRouteDebugDump,  handleDebugDump },
  { kRouteMqtt,       handleMqtt },
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

  if (ESP.getFreeHeap() < 4096) {
    RESTDebugTf(PSTR("REST %s %s => 500 low heap (%u)\r\n"), httpMethodToStr(method), originalURI, ESP.getFreeHeap());
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
void sendOTGWvalue(int msgid){
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

void sendOTGWlabel(const char *msglabel){
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
// (firmware, network, time, connections, chip, RAM, flash, drops,
// discovery). When adding a new field, place it inside the matching
// group rather than appending at the end, so related metrics stay
// adjacent on the page. JSON object order is not an API guarantee for
// REST consumers - they should parse by key.
// Keep one pbuf-sized contiguous block available while streaming. processAPI()
// already rejects requests below 4096 bytes free heap; requiring 8192 bytes
// here rejected healthy-but-fragmented beta.24 requests even though JSON uses
// a static 512-byte flush buffer.
#define DEVICE_INFO_MIN_HEAP_BLOCK  1536

void sendDeviceInfoV2()
{
  if (ESP.getMaxFreeBlockSize() < DEVICE_INFO_MIN_HEAP_BLOCK) {
    sendApiError(503, F("low heap"));
    return;
  }
  sendStartJsonMap(F("device"));

  // --- Firmware & build identity ---
  sendJsonMapEntry(F("author"), F("Robert van den Breemen"));
  sendJsonMapEntry(F("fwversion"), _SEMVER_FULL);
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%s %s"), __DATE__, __TIME__);
  sendJsonMapEntry(F("compiled"), cMsg);
  sendJsonMapEntry(F("picavailable"), state.pic.bAvailable);
  if (isPICEnabled()) {
    sendJsonMapEntry(F("picfwversion"), state.pic.sFwversion);
    sendJsonMapEntry(F("picdeviceid"), state.pic.sDeviceid);
    sendJsonMapEntry(F("picfwtype"), state.pic.sType);
  }

  // --- Network identity ---
  // This endpoint is intentionally unauthenticated (ADR-032, trusted-LAN model).
  // Network identity, including LAN topology (subnet/gateway/DNS), is exposed by
  // design alongside the already-public IP/MAC/SSID/chipid fields. Do not add an
  // auth gate here without revisiting ADR-032.
  sendJsonMapEntry(F("hostname"), CSTR(settings.sHostname));
  sendJsonMapEntry(F("ipaddress"),            CSTR(WiFi.localIP().toString()));
  sendJsonMapEntry(F("wifi_current_subnet"),  CSTR(WiFi.subnetMask().toString()));
  sendJsonMapEntry(F("wifi_current_gateway"), CSTR(WiFi.gatewayIP().toString()));
  sendJsonMapEntry(F("wifi_current_dns1"),    CSTR(WiFi.dnsIP(0).toString()));
  sendJsonMapEntry(F("wifi_current_dns2"),    CSTR(WiFi.dnsIP(1).toString()));
  sendJsonMapEntry(F("macaddress"), CSTR(WiFi.macAddress()));
  sendJsonMapEntry(F("ssid"), CSTR(WiFi.SSID()));
  sendJsonMapEntry(F("wifirssi"), WiFi.RSSI());
  sendJsonMapEntry(F("wifiquality"), signal_quality_perc_quad(WiFi.RSSI()));
  sendJsonMapEntry(F("wifiquality_text"), dBmtoQuality(WiFi.RSSI()));

  // --- Time, NTP & uptime ---
  sendJsonMapEntry(F("ntpenable"), settings.ntp.bEnable);
  sendJsonMapEntry(F("ntptimezone"), CSTR(settings.ntp.sTimezone));
  sendJsonMapEntry(F("uptime"), upTime());
  sendJsonMapEntry(F("lastreset"), lastReset);
  sendJsonMapEntry(F("bootcount"), state.uptime.iRebootCount);

  // --- Connection status (MQTT, OTGW, thermostat, boiler) ---
  sendJsonMapEntry(F("mqttconnected"), state.mqtt.bConnected);
  if (isPICEnabled()) {
    sendJsonMapEntry(F("thermostatconnected"), state.otgw.bThermostatState);
    sendJsonMapEntry(F("boilerconnected"), state.otgw.bBoilerState);
    sendJsonMapEntry(F("otgwmode"), !isGatewayFirmware() ? "N/A" : state.otgw.bGatewayModeKnown ? CCONOFF(state.otgw.bGatewayMode) : "detecting");
    sendJsonMapEntry(F("otgwconnected"), state.otgw.bOnline);
  }
  sendJsonMapEntry(F("otgwsimulation"), state.debug.bOTGWSimulation);

  // --- Chip & CPU ---
  sendJsonMapEntry(F("chipid"), CSTR(String( ESP.getChipId(), HEX )));
  sendJsonMapEntry(F("coreversion"), CSTR(ESP.getCoreVersion()) );
  sendJsonMapEntry(F("sdkversion"),  ESP.getSdkVersion());
  sendJsonMapEntry(F("cpufreq"), ESP.getCpuFreqMHz());

  // --- RAM / heap (free heap, largest block, fragmentation, tier transitions) ---
  sendJsonMapEntry(F("freeheap"), ESP.getFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), ESP.getMaxFreeBlockSize());
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
  sendJsonMapEntry(F("flashchipmode"),    sBootFlash.flashChipMode);
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
  sendJsonMapEntry(F("mqttconnected"), CBOOLEAN(state.mqtt.bConnected));
  sendJsonMapEntry(F("otgwconnected"), CBOOLEAN(state.otgw.bOnline));
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
  sendJsonMapEntry(F("psmode"), state.otgw.bPSmode);
  sendJsonMapEntry(F("otgwsimulation"), state.debug.bOTGWSimulation);
  sendJsonMapEntry(F("freeheap"), ESP.getFreeHeap());
  sendJsonMapEntry(F("maxfreeblock"), ESP.getMaxFreeBlockSize());

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
  sendJsonSettingObj(F("wifistaticip"), CSTR(settings.wifi.sStaticIp), "s", 15);
  sendJsonSettingObj(F("wifisubnet"), CSTR(settings.wifi.sSubnet), "s", 15);
  sendJsonSettingObj(F("wifigateway"), CSTR(settings.wifi.sGateway), "s", 15);
  sendJsonSettingObj(F("wifidns1"), CSTR(settings.wifi.sDns1), "s", 15);
  sendJsonSettingObj(F("wifidns2"), CSTR(settings.wifi.sDns2), "s", 15);
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
  sendJsonSettingObj(F("otgwcommandenable"), settings.otgw.bEnable, "b");
  sendJsonSettingObj(F("otgwcommands"), CSTR(settings.otgw.sCommands), "s", 128);
  sendJsonSettingObj(F("webhookenable"), settings.webhook.bEnabled, "b");
  sendJsonSettingObj(F("webhookurlon"), CSTR(settings.webhook.sURLon), "s", 100);
  sendJsonSettingObj(F("webhookurloff"), CSTR(settings.webhook.sURLoff), "s", 100);
  sendJsonSettingObj(F("webhooktriggerbit"), settings.webhook.iTriggerBit, "i", 0, 15);
  sendJsonSettingObj(F("webhookpayload"), CSTR(settings.webhook.sPayload), "s", 200);
  sendJsonSettingObj(F("webhookcontenttype"), CSTR(settings.webhook.sContentType), "s", 31);
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
  "darktheme", "gpiooutputsenabled", "gpiooutputspin", "gpiooutputstriggerbit",
  "gpiosensorsenabled", "gpiosensorsinterval", "gpiosensorslegacyformat", "gpiosensorspin",
  "hostname", "httppasswd", "ledblink", "legacyport25238enabled", "nightlyrestart", "nightlyrestarthour",
  "mqttbroker", "mqttbrokerport", "mqttenable", "mqtthaprefix", "mqttharebootdetection",
  "mqttinterval", "mqttonchangepublishing", "mqttotmessage", "mqttpasswd", "mqttseparatesources",
  "mqtttoptopic", "mqttuniqueid", "mqttuser",
  "ntpenable", "ntphostname", "ntpsendtime", "ntptimezone",
  "otgwcommandenable", "otgwcommands",
  "s0counterdebouncetime", "s0counterenabled", "s0counterinterval", "s0counterpin", "s0counterpulsekw",
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
  
  // Stream the file content directly to response (heap-guarded like every
  // other file-serving caller: streamFileGuarded sends 503 when the largest
  // contiguous block is below HTTP_SERVE_MIN_MAXBLOCK instead of serving).
  streamFileGuarded(labelsFile, F("application/json"));
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
