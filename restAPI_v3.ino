/* 
***************************************************************************  
**  Program  : restAPI_v3 - REST API v3 with HATEOAS support (ADR-025)
**  Version  : v1.0.0-rc6
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// REST API v3 - Richardson Maturity Model Level 3 (HATEOAS)
// See: docs/adr/ADR-025-rest-api-v3-hateoas.md
// See: docs/wiki/1-API-v3-Overview.md

#define V3_API_VERSION "3.0.0"

//=======================================================================
// Helper: Add HATEOAS links to response
//=======================================================================
static void addHATEOASLinks(JsonObject &links, const char *selfPath) {
  // Self link
  JsonObject self = links.createNestedObject(F("self"));
  self[F("href")] = selfPath;
  
  // API root link
  JsonObject apiRoot = links.createNestedObject(F("api_root"));
  apiRoot[F("href")] = F("/api/v3");
}

// Overload for PROGMEM strings (F() macro)
static void addHATEOASLinks(JsonObject &links, const __FlashStringHelper *selfPath) {
  // Self link
  JsonObject self = links.createNestedObject(F("self"));
  self[F("href")] = selfPath;
  
  // API root link
  JsonObject apiRoot = links.createNestedObject(F("api_root"));
  apiRoot[F("href")] = F("/api/v3");
}

//=======================================================================
// Helper: CORS preflight for OPTIONS
//=======================================================================
static void sendV3Options() {
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD"));
  httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, If-None-Match, If-Match"));
  httpServer.sendHeader(F("Access-Control-Max-Age"), F("86400"));
  httpServer.send(204, F("text/plain"), F(""));
}

//=======================================================================
// Helper: Simple FNV-1a hash for ETag
//=======================================================================
static uint32_t fnv1a32(const char *data) {
  if (!data) return 0;
  uint32_t hash = 2166136261UL;
  const uint8_t *p = reinterpret_cast<const uint8_t*>(data);
  while (*p) {
    hash ^= *p++;
    hash *= 16777619UL;
  }
  return hash;
}

//=======================================================================
// Helper: Send JSON with optional ETag support
//=======================================================================
static void sendV3Json(int code, const char *json, bool useEtag = false) {
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  addV3RateLimitHeaders();

  if (useEtag && json) {
    char etag[16];
    uint32_t hash = fnv1a32(json);
    snprintf_P(etag, sizeof(etag), PSTR("\"%08lX\""), (unsigned long)hash);

    String ifNoneMatch = httpServer.header(F("If-None-Match"));
    if (ifNoneMatch.length() > 0 && ifNoneMatch == etag) {
      httpServer.sendHeader(F("ETag"), etag);
      httpServer.send(304, F("text/plain"), F(""));
      return;
    }

    httpServer.sendHeader(F("ETag"), etag);
  }

  httpServer.send(code, F("application/json"), json ? json : "");
}

//=======================================================================
// Helper: HEAD response (no body)
//=======================================================================
static void sendV3HeadOk() {
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  addV3RateLimitHeaders();
  httpServer.send(200, F("application/json"), F(""));
}

//=======================================================================
// Helper: Parse integer query parameter
//=======================================================================
static bool parseQueryInt(const __FlashStringHelper *name, int &outValue) {
  if (!httpServer.hasArg(name)) return false;
  const String value = httpServer.arg(name);
  if (value.length() == 0) return false;
  outValue = value.toInt();
  return true;
}

static bool parseQueryLong(const __FlashStringHelper *name, long &outValue) {
  if (!httpServer.hasArg(name)) return false;
  const String value = httpServer.arg(name);
  if (value.length() == 0) return false;
  outValue = value.toInt();
  return true;
}

//=======================================================================
// Helper: Rate limiting
//=======================================================================
static const uint8_t V3_RATE_LIMIT_ENTRIES = 8;
static const uint16_t V3_RATE_LIMIT_MAX = 60;   // requests per window
static const uint32_t V3_RATE_LIMIT_WINDOW_MS = 60000UL;

struct V3RateLimitEntry {
  uint32_t ip;
  uint32_t windowStart;
  uint16_t count;
};

static V3RateLimitEntry v3RateLimits[V3_RATE_LIMIT_ENTRIES] = {0};
static uint16_t v3RateRemaining = 0;
static uint32_t v3RateResetSec = 0;

static void addV3RateLimitHeaders() {
  if (v3RateResetSec == 0) return;
  httpServer.sendHeader(F("X-RateLimit-Limit"), String(V3_RATE_LIMIT_MAX));
  httpServer.sendHeader(F("X-RateLimit-Remaining"), String(v3RateRemaining));
  httpServer.sendHeader(F("X-RateLimit-Reset"), String(v3RateResetSec));
}

static bool checkV3RateLimit() {
  IPAddress ip = httpServer.client().remoteIP();
  const uint32_t ipKey = (uint32_t)ip;
  const uint32_t nowMs = millis();

  int slot = -1;
  for (uint8_t i = 0; i < V3_RATE_LIMIT_ENTRIES; i++) {
    if (v3RateLimits[i].ip == ipKey) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    for (uint8_t i = 0; i < V3_RATE_LIMIT_ENTRIES; i++) {
      if (v3RateLimits[i].ip == 0) {
        slot = i;
        v3RateLimits[i].ip = ipKey;
        v3RateLimits[i].windowStart = nowMs;
        v3RateLimits[i].count = 0;
        break;
      }
    }
  }

  if (slot < 0) {
    slot = 0;
    v3RateLimits[slot].ip = ipKey;
    v3RateLimits[slot].windowStart = nowMs;
    v3RateLimits[slot].count = 0;
  }

  V3RateLimitEntry &entry = v3RateLimits[slot];
  if ((nowMs - entry.windowStart) >= V3_RATE_LIMIT_WINDOW_MS) {
    entry.windowStart = nowMs;
    entry.count = 0;
  }

  if (entry.count >= V3_RATE_LIMIT_MAX) {
    v3RateRemaining = 0;
    v3RateResetSec = (V3_RATE_LIMIT_WINDOW_MS - (nowMs - entry.windowStart)) / 1000UL;
    return false;
  }

  entry.count++;
  v3RateRemaining = (entry.count >= V3_RATE_LIMIT_MAX) ? 0 : (V3_RATE_LIMIT_MAX - entry.count);
  v3RateResetSec = (V3_RATE_LIMIT_WINDOW_MS - (nowMs - entry.windowStart)) / 1000UL;
  return true;
}

//=======================================================================
// Helper: Send JSON error response with proper HTTP status
//=======================================================================
static void sendV3Error(int code, const char *message, const char *details = nullptr) {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("error")] = true;
  root[F("code")] = code;
  root[F("message")] = message;
  if (details) {
    root[F("details")] = details;
  }
  
  // Add timestamp
  root[F("timestamp")] = (unsigned long)time(nullptr);
  
  // Add links for recovery
  JsonObject links = root.createNestedObject(F("_links"));
  JsonObject apiRoot = links.createNestedObject(F("api_root"));
  apiRoot[F("href")] = F("/api/v3");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  addV3RateLimitHeaders();
  httpServer.send(code, F("application/json"), sBuff);
}

//=======================================================================
// API Root - GET /api/v3
//=======================================================================
static void sendV3Root() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("version")] = V3_API_VERSION;
  root[F("api_level")] = 3;
  root[F("description")] = F("OTGW Firmware REST API v3 with HATEOAS support");
  
  // Available resources with links
  JsonObject links = root.createNestedObject(F("_links"));
  
  addHATEOASLinks(links, F("/api/v3"));
  
  // System resources
  JsonObject health = links.createNestedObject(F("health"));
  health[F("href")] = F("/api/v3/system/health");
  health[F("description")] = F("System health status");
  
  JsonObject info = links.createNestedObject(F("info"));
  info[F("href")] = F("/api/v3/system/info");
  info[F("description")] = F("Device information");
  
  // Configuration resources
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");
  config[F("description")] = F("Device configuration");
  
  // OTGW resources
  JsonObject otgw = links.createNestedObject(F("otgw"));
  otgw[F("href")] = F("/api/v3/otgw");
  otgw[F("description")] = F("OpenTherm Gateway interface");
  
  // Sensors resources  
  JsonObject sensors = links.createNestedObject(F("sensors"));
  sensors[F("href")] = F("/api/v3/sensors");
  sensors[F("description")] = F("Sensor data");

  // Export resources
  JsonObject exportRes = links.createNestedObject(F("export"));
  exportRes[F("href")] = F("/api/v3/export");
  exportRes[F("description")] = F("Export formats");

  // Legacy API links
  JsonObject legacyV0 = links.createNestedObject(F("legacy_v0"));
  legacyV0[F("href")] = F("/api/v0");
  JsonObject legacyV1 = links.createNestedObject(F("legacy_v1"));
  legacyV1[F("href")] = F("/api/v1");
  JsonObject legacyV2 = links.createNestedObject(F("legacy_v2"));
  legacyV2[F("href")] = F("/api/v2");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// System Health - GET /api/v3/system/health
//=======================================================================
static void sendV3SystemHealth() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("status")] = F("UP");
  root[F("timestamp")] = (unsigned long)time(nullptr);
  
  // Components health
  JsonObject components = root.createNestedObject(F("components"));
  
  components[F("wifi")] = WiFi.status() == WL_CONNECTED ? F("UP") : F("DOWN");
  components[F("mqtt")] = statusMQTTconnection ? F("UP") : F("DOWN");
  components[F("otgw")] = (OTGWSerial.available() >= 0) ? F("UP") : F("DOWN");
  components[F("filesystem")] = LittleFS.begin() ? F("UP") : F("DOWN");
  
  // System metrics
  JsonObject metrics = root.createNestedObject(F("metrics"));
  metrics[F("uptime")] = millis() / 1000;
  metrics[F("free_heap")] = ESP.getFreeHeap();
  metrics[F("heap_fragmentation")] = ESP.getHeapFragmentation();
  metrics[F("wifi_rssi")] = WiFi.RSSI();
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system/health"));
  
  JsonObject sysInfo = links.createNestedObject(F("system_info"));
  sysInfo[F("href")] = F("/api/v3/system/info");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// System Info - GET /api/v3/system/info
//=======================================================================
static void sendV3SystemInfo() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("hostname")] = CSTR(settingHostname);
  root[F("firmware_version")] = _VERSION;
  root[F("compile_date")] = __DATE__;
  root[F("compile_time")] = __TIME__;
  root[F("chip_id")] = String(ESP.getChipId(), HEX);
  root[F("core_version")] = ESP.getCoreVersion();
  root[F("sdk_version")] = ESP.getSdkVersion();
  root[F("cpu_freq")] = ESP.getCpuFreqMHz();
  root[F("flash_size")] = ESP.getFlashChipSize();
  root[F("flash_speed")] = ESP.getFlashChipSpeed();
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system/info"));
  
  JsonObject health = links.createNestedObject(F("health"));
  health[F("href")] = F("/api/v3/system/health");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// System Overview - GET /api/v3/system
//=======================================================================
static void sendV3SystemOverview() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("status")] = F("online");
  root[F("uptime")] = upTime();
  root[F("hostname")] = CSTR(settingHostname);
  root[F("ip_address")] = CSTR(WiFi.localIP().toString());

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system"));

  JsonObject info = links.createNestedObject(F("info"));
  info[F("href")] = F("/api/v3/system/info");
  JsonObject health = links.createNestedObject(F("health"));
  health[F("href")] = F("/api/v3/system/health");
  JsonObject time = links.createNestedObject(F("time"));
  time[F("href")] = F("/api/v3/system/time");
  JsonObject network = links.createNestedObject(F("network"));
  network[F("href")] = F("/api/v3/system/network");
  JsonObject stats = links.createNestedObject(F("stats"));
  stats[F("href")] = F("/api/v3/system/stats");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// System Time - GET /api/v3/system/time
//=======================================================================
static void sendV3SystemTime() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();

  time_t now = time(nullptr);
  TimeZone myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);

  char dateTime[24];
  snprintf_P(dateTime, sizeof(dateTime), PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
             myTime.year(), myTime.month(), myTime.day(),
             myTime.hour(), myTime.minute(), myTime.second());

  root[F("datetime")] = dateTime;
  root[F("epoch")] = (int)now;
  root[F("timezone")] = CSTR(settingNTPtimezone);
  root[F("ntp_enabled")] = settingNTPenable;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system/time"));
  JsonObject system = links.createNestedObject(F("system"));
  system[F("href")] = F("/api/v3/system");

  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// System Network - GET /api/v3/system/network
//=======================================================================
static void sendV3SystemNetwork() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("ssid")] = CSTR(WiFi.SSID());
  root[F("ip_address")] = CSTR(WiFi.localIP().toString());
  root[F("gateway")] = CSTR(WiFi.gatewayIP().toString());
  root[F("subnet")] = CSTR(WiFi.subnetMask().toString());
  root[F("mac_address")] = CSTR(WiFi.macAddress());
  root[F("rssi")] = WiFi.RSSI();
  root[F("quality")] = signal_quality_perc_quad(WiFi.RSSI());

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system/network"));
  JsonObject system = links.createNestedObject(F("system"));
  system[F("href")] = F("/api/v3/system");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// System Stats - GET /api/v3/system/stats
//=======================================================================
static void sendV3SystemStats() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("free_heap")] = ESP.getFreeHeap();
  root[F("max_free_block")] = ESP.getMaxFreeBlockSize();
  root[F("heap_fragmentation")] = ESP.getHeapFragmentation();
  root[F("sketch_size")] = ESP.getSketchSize();
  root[F("free_sketch_space")] = ESP.getFreeSketchSpace();
  root[F("flash_size")] = ESP.getFlashChipSize();

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/system/stats"));
  JsonObject system = links.createNestedObject(F("system"));
  system[F("href")] = F("/api/v3/system");

  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Index - GET /api/v3/otgw
//=======================================================================
static void sendV3OTGWIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("description")] = F("OpenTherm Gateway interface");
  root[F("available")] = (OTGWSerial.available() >= 0);
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw"));
  
  JsonObject status = links.createNestedObject(F("status"));
  status[F("href")] = F("/api/v3/otgw/status");
  status[F("description")] = F("Current OTGW status");
  
  JsonObject messages = links.createNestedObject(F("messages"));
  messages[F("href")] = F("/api/v3/otgw/messages");
  messages[F("description")] = F("OpenTherm messages");
  messages[F("templated")] = true;
  
  JsonObject command = links.createNestedObject(F("command"));
  command[F("href")] = F("/api/v3/otgw/command");
  command[F("description")] = F("Send OTGW command");
  command[F("method")] = F("POST");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Status - GET /api/v3/otgw/status
//=======================================================================
static void sendV3OTGWStatus() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  bool otgwOnline = (OTGWSerial.available() >= 0);
  root[F("available")] = otgwOnline;
  root[F("timestamp")] = (unsigned long)time(nullptr);
  
  if (otgwOnline) {
    // Key temperature values
    root[F("room_setpoint")] = getOTGWValue(16).toFloat();
    root[F("room_temp")] = getOTGWValue(24).toFloat();
    root[F("boiler_temp")] = getOTGWValue(25).toFloat();
    root[F("dhw_temp")] = getOTGWValue(26).toFloat();
    root[F("return_temp")] = getOTGWValue(28).toFloat();
    root[F("ch_pressure")] = getOTGWValue(41).toFloat();
  }
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/status"));
  
  JsonObject otgwRoot = links.createNestedObject(F("otgw"));
  otgwRoot[F("href")] = F("/api/v3/otgw");
  
  JsonObject messages = links.createNestedObject(F("messages"));
  messages[F("href")] = F("/api/v3/otgw/messages");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Helpers: OTGW message filters
//=======================================================================
static bool matchesMessageCategory(const char *category) {
  if (!category) return true;

  if (strcasecmp_P(category, PSTR("temperature")) == 0) {
    return (strcmp(OTlookupitem.unit, "°C") == 0);
  }
  if (strcasecmp_P(category, PSTR("pressure")) == 0) {
    return (strcmp(OTlookupitem.unit, "bar") == 0);
  }
  if (strcasecmp_P(category, PSTR("percentage")) == 0) {
    return (strcmp(OTlookupitem.unit, "%") == 0);
  }
  if (strcasecmp_P(category, PSTR("flow")) == 0) {
    return (strcmp(OTlookupitem.unit, "l/min") == 0);
  }

  return false;
}

static bool isSupportedCategory(const char *category) {
  if (!category) return true;
  return (strcasecmp_P(category, PSTR("temperature")) == 0) ||
         (strcasecmp_P(category, PSTR("pressure")) == 0) ||
         (strcasecmp_P(category, PSTR("percentage")) == 0) ||
         (strcasecmp_P(category, PSTR("flow")) == 0);
}

//=======================================================================
// OTGW Messages - GET /api/v3/otgw/messages
//=======================================================================
static void sendV3OTGWMessagesList() {
  const String labelQuery = httpServer.hasArg(F("label")) ? httpServer.arg(F("label")) : String();
  const String categoryQuery = httpServer.hasArg(F("category")) ? httpServer.arg(F("category")) : String();

  if (labelQuery.length() > 0) {
    for (uint8_t msgId = 0; msgId <= OT_MSGID_MAX; msgId++) {
      PROGMEM_readAnything(&OTmap[msgId], OTlookupitem);
      if (OTlookupitem.type == ot_undef) continue;
      if (strcasecmp(OTlookupitem.label, labelQuery.c_str()) == 0) {
        sendV3OTGWMessage(msgId);
        return;
      }
    }
    sendV3Error(404, "Message not found", labelQuery.c_str());
    return;
  }

  int page = 1;
  int perPage = 10;
  parseQueryInt(F("page"), page);
  parseQueryInt(F("per_page"), perPage);
  if (page < 1) page = 1;
  if (perPage < 1) perPage = 1;
  if (perPage > 20) perPage = 20;

  const char *categoryFilter = categoryQuery.length() ? categoryQuery.c_str() : nullptr;
  long updatedAfter = 0;
  parseQueryLong(F("updated_after"), updatedAfter);

  if (categoryFilter && !isSupportedCategory(categoryFilter)) {
    sendV3Error(400, "Invalid category", categoryFilter);
    return;
  }

  int total = 0;
  for (uint8_t msgId = 0; msgId <= OT_MSGID_MAX; msgId++) {
    PROGMEM_readAnything(&OTmap[msgId], OTlookupitem);
    if (OTlookupitem.type == ot_undef) continue;
    if (!matchesMessageCategory(categoryFilter)) continue;
    if (updatedAfter > 0 && (long)msglastupdated[msgId] <= updatedAfter) continue;
    total++;
  }

  const int totalPages = (total + perPage - 1) / perPage;
  if (totalPages > 0 && page > totalPages) page = totalPages;

  const int startIndex = (page - 1) * perPage;
  const int endIndex = startIndex + perPage;

  StaticJsonDocument<2048> doc;
  JsonObject root = doc.to<JsonObject>();
  JsonArray messages = root.createNestedArray(F("messages"));

  int index = 0;
  for (uint8_t msgId = 0; msgId <= OT_MSGID_MAX; msgId++) {
    PROGMEM_readAnything(&OTmap[msgId], OTlookupitem);
    if (OTlookupitem.type == ot_undef) continue;
    if (!matchesMessageCategory(categoryFilter)) continue;
    if (updatedAfter > 0 && (long)msglastupdated[msgId] <= updatedAfter) continue;

    if (index >= startIndex && index < endIndex) {
      JsonObject msg = messages.createNestedObject();
      msg[F("id")] = msgId;
      msg[F("label")] = OTlookupitem.label;
      msg[F("unit")] = OTlookupitem.unit;
      msg[F("updated")] = (unsigned long)msglastupdated[msgId];
      if (OTlookupitem.type == ot_f88) {
        msg[F("value")] = getOTGWValue(msgId).toFloat();
      } else {
        msg[F("value")] = getOTGWValue(msgId).toInt();
      }
    }

    index++;
    if (index >= endIndex) break;
  }

  JsonObject pagination = root.createNestedObject(F("pagination"));
  pagination[F("page")] = page;
  pagination[F("per_page")] = perPage;
  pagination[F("total")] = total;
  pagination[F("total_pages")] = totalPages;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/messages"));

  if (page > 1) {
    char prev[64];
    snprintf_P(prev, sizeof(prev), PSTR("/api/v3/otgw/messages?page=%d&per_page=%d"), page - 1, perPage);
    JsonObject prevLink = links.createNestedObject(F("prev"));
    prevLink[F("href")] = prev;
  }
  if (page < totalPages) {
    char next[64];
    snprintf_P(next, sizeof(next), PSTR("/api/v3/otgw/messages?page=%d&per_page=%d"), page + 1, perPage);
    JsonObject nextLink = links.createNestedObject(F("next"));
    nextLink[F("href")] = next;
  }

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Thermostat - GET /api/v3/otgw/thermostat
//=======================================================================
static void sendV3OTGWThermostat() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("connected")] = bOTGWthermostatstate;
  root[F("room_temperature")] = OTcurrentSystemState.Tr;
  root[F("room_setpoint")] = OTcurrentSystemState.TrSet;
  root[F("remote_room_setpoint")] = OTcurrentSystemState.TrOverride;
  root[F("control_setpoint")] = OTcurrentSystemState.TSet;
  root[F("relative_modulation")] = OTcurrentSystemState.RelModLevel;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/thermostat"));
  JsonObject otgw = links.createNestedObject(F("otgw"));
  otgw[F("href")] = F("/api/v3/otgw");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Boiler - GET /api/v3/otgw/boiler
//=======================================================================
static void sendV3OTGWBoiler() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("connected")] = bOTGWboilerstate;
  root[F("boiler_temperature")] = OTcurrentSystemState.Tboiler;
  root[F("return_temperature")] = OTcurrentSystemState.Tret;
  root[F("dhw_temperature")] = OTcurrentSystemState.Tdhw;
  root[F("dhw_setpoint")] = OTcurrentSystemState.TdhwSet;
  root[F("max_ch_setpoint")] = OTcurrentSystemState.MaxTSet;
  root[F("ch_pressure")] = OTcurrentSystemState.CHPressure;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/boiler"));
  JsonObject otgw = links.createNestedObject(F("otgw"));
  otgw[F("href")] = F("/api/v3/otgw");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Command Queue - GET /api/v3/otgw/commands
//=======================================================================
static void sendV3OTGWCommands() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("count")] = cmdptr;
  root[F("capacity")] = CMDQUEUE_MAX;

  JsonArray queue = root.createNestedArray(F("queue"));
  for (int i = 0; i < cmdptr; i++) {
    JsonObject entry = queue.createNestedObject();
    entry[F("index")] = i;
    entry[F("command")] = cmdqueue[i].cmd;
    entry[F("retry_count")] = cmdqueue[i].retrycnt;
    entry[F("due_ms")] = cmdqueue[i].due;
  }

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/commands"));
  JsonObject otgw = links.createNestedObject(F("otgw"));
  otgw[F("href")] = F("/api/v3/otgw");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Command - POST /api/v3/otgw/commands
//=======================================================================
static void sendV3OTGWCommandsPost() {
  // Parse JSON body
  StaticJsonDocument<256> reqDoc;
  DeserializationError error = deserializeJson(reqDoc, httpServer.arg(F("plain")));

  if (error) {
    sendV3Error(400, "Invalid JSON", error.c_str());
    return;
  }

  const char* cmd = reqDoc[F("command")];
  if (!cmd || strlen(cmd) < 3 || cmd[2] != '=') {
    sendV3Error(400, "Invalid command format", "Expected format: XX=value");
    return;
  }

  addOTWGcmdtoqueue(cmd, strlen(cmd));

  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  root[F("success")] = true;
  root[F("command")] = cmd;
  root[F("status")] = F("queued");
  root[F("timestamp")] = (unsigned long)time(nullptr);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/commands"));
  JsonObject queue = links.createNestedObject(F("queue"));
  queue[F("href")] = F("/api/v3/otgw/commands");

  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));

  sendV3Json(201, sBuff);
}

//=======================================================================
// OTGW Autoconfigure - POST /api/v3/otgw/autoconfigure
//=======================================================================
static void sendV3OTGWAutoconfigure() {
  doAutoConfigure();

  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  root[F("success")] = true;
  root[F("timestamp")] = (unsigned long)time(nullptr);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/autoconfigure"));
  JsonObject otgw = links.createNestedObject(F("otgw"));
  otgw[F("href")] = F("/api/v3/otgw");

  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// OTGW Message by ID - GET /api/v3/otgw/messages/{id}
//=======================================================================
static void sendV3OTGWMessage(uint8_t msgId) {
  if (msgId > OT_MSGID_MAX) {
    sendV3Error(400, "Invalid message ID", "ID must be between 0 and 127");
    return;
  }
  
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  PROGMEM_readAnything(&OTmap[msgId], OTlookupitem);
  
  if (OTlookupitem.type == ot_undef) {
    sendV3Error(404, "Message undefined", "Reserved for future use");
    return;
  }
  
  root[F("id")] = msgId;
  root[F("label")] = OTlookupitem.label;
  root[F("unit")] = OTlookupitem.unit;
  
  if (OTlookupitem.type == ot_f88) {
    root[F("value")] = getOTGWValue(msgId).toFloat();
  } else {
    root[F("value")] = getOTGWValue(msgId).toInt();
  }
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  char selfPath[48];
  snprintf_P(selfPath, sizeof(selfPath), PSTR("/api/v3/otgw/messages/%d"), msgId);
  addHATEOASLinks(links, selfPath);
  
  JsonObject messages = links.createNestedObject(F("messages"));
  messages[F("href")] = F("/api/v3/otgw/messages");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// OTGW Command - POST /api/v3/otgw/command
//=======================================================================
static void sendV3OTGWCommand() {
  // Parse JSON body
  StaticJsonDocument<256> reqDoc;
  DeserializationError error = deserializeJson(reqDoc, httpServer.arg(F("plain")));
  
  if (error) {
    sendV3Error(400, "Invalid JSON", error.c_str());
    return;
  }
  
  const char* cmd = reqDoc[F("command")];
  if (!cmd || strlen(cmd) < 3 || cmd[2] != '=') {
    sendV3Error(400, "Invalid command format", "Expected format: XX=value");
    return;
  }
  
  // Queue the command
  addOTWGcmdtoqueue(cmd, strlen(cmd));
  
  // Response
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("success")] = true;
  root[F("command")] = cmd;
  root[F("queued")] = true;
  root[F("timestamp")] = (unsigned long)time(nullptr);
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/otgw/command"));
  
  JsonObject status = links.createNestedObject(F("status"));
  status[F("href")] = F("/api/v3/otgw/status");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJson(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Config Index - GET /api/v3/config
//=======================================================================
static void sendV3ConfigIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("description")] = F("Device configuration management");
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config"));
  
  JsonObject mqtt = links.createNestedObject(F("mqtt"));
  mqtt[F("href")] = F("/api/v3/config/mqtt");
  mqtt[F("description")] = F("MQTT configuration");

  JsonObject settings = links.createNestedObject(F("settings"));
  settings[F("href")] = F("/api/v3/config/settings");
  settings[F("description")] = F("Device settings");
  
  JsonObject network = links.createNestedObject(F("network"));
  network[F("href")] = F("/api/v3/config/network");
  network[F("description")] = F("Network configuration");
  
  JsonObject ntp = links.createNestedObject(F("ntp"));
  ntp[F("href")] = F("/api/v3/config/ntp");
  ntp[F("description")] = F("NTP time configuration");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// Config Settings - GET /api/v3/config/settings
//=======================================================================
static void sendV3ConfigSettings() {
  StaticJsonDocument<1536> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("hostname")] = CSTR(settingHostname);
  root[F("mqttenable")] = settingMQTTenable;
  root[F("mqttbroker")] = CSTR(settingMQTTbroker);
  root[F("mqttbrokerport")] = settingMQTTbrokerPort;
  root[F("mqttuser")] = CSTR(settingMQTTuser);
  root[F("mqtthaprefix")] = CSTR(settingMQTThaprefix);
  root[F("mqttuniqueid")] = CSTR(settingMQTTuniqueid);
  root[F("mqttotmessage")] = settingMQTTOTmessage;
  root[F("mqttharebootdetection")] = settingMQTTharebootdetection;
  root[F("ntpenable")] = settingNTPenable;
  root[F("ntptimezone")] = CSTR(settingNTPtimezone);
  root[F("ntphostname")] = CSTR(settingNTPhostname);
  root[F("ntpsendtime")] = settingNTPsendtime;
  root[F("ledblink")] = settingLEDblink;
  root[F("darktheme")] = settingDarkTheme;
  root[F("ui_autoscroll")] = settingUIAutoScroll;
  root[F("ui_timestamps")] = settingUIShowTimestamp;
  root[F("ui_capture")] = settingUICaptureMode;
  root[F("ui_autoscreenshot")] = settingUIAutoScreenshot;
  root[F("ui_autodownloadlog")] = settingUIAutoDownloadLog;
  root[F("ui_autoexport")] = settingUIAutoExport;
  root[F("ui_graphtimewindow")] = settingUIGraphTimeWindow;
  root[F("gpiosensorsenabled")] = settingGPIOSENSORSenabled;
  root[F("gpiosensorslegacyformat")] = settingGPIOSENSORSlegacyformat;
  root[F("gpiosensorspin")] = settingGPIOSENSORSpin;
  root[F("gpiosensorsinterval")] = settingGPIOSENSORSinterval;
  root[F("s0counterenabled")] = settingS0COUNTERenabled;
  root[F("s0counterpin")] = settingS0COUNTERpin;
  root[F("s0counterdebouncetime")] = settingS0COUNTERdebouncetime;
  root[F("s0counterpulsekw")] = settingS0COUNTERpulsekw;
  root[F("s0counterinterval")] = settingS0COUNTERinterval;
  root[F("gpiooutputsenabled")] = settingGPIOOUTPUTSenabled;
  root[F("gpiooutputspin")] = settingGPIOOUTPUTSpin;
  root[F("gpiooutputstriggerbit")] = settingGPIOOUTPUTStriggerBit;
  root[F("otgwcommandenable")] = settingOTGWcommandenable;
  root[F("otgwcommands")] = CSTR(settingOTGWcommands);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/settings"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");
  JsonObject mqtt = links.createNestedObject(F("mqtt"));
  mqtt[F("href")] = F("/api/v3/config/mqtt");
  JsonObject ntp = links.createNestedObject(F("ntp"));
  ntp[F("href")] = F("/api/v3/config/ntp");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// Helpers: Apply JSON settings updates
//=======================================================================
static bool applySettingUpdate(const char *key, const JsonVariant &value) {
  if (!key || key[0] == '\0') return false;

  char valueBuf[64];
  valueBuf[0] = '\0';

  if (value.is<const char*>()) {
    strlcpy(valueBuf, value.as<const char*>(), sizeof(valueBuf));
  } else if (value.is<bool>()) {
    if (value.as<bool>()) {
      strlcpy_P(valueBuf, PSTR("true"), sizeof(valueBuf));
    } else {
      strlcpy_P(valueBuf, PSTR("false"), sizeof(valueBuf));
    }
  } else if (value.is<int>() || value.is<long>()) {
    snprintf_P(valueBuf, sizeof(valueBuf), PSTR("%ld"), (long)value.as<long>());
  } else if (value.is<float>() || value.is<double>()) {
    dtostrf(value.as<double>(), 0, 3, valueBuf);
  } else {
    return false;
  }

  updateSetting(key, valueBuf);
  return true;
}

static bool applySettingsFromJson(JsonObject obj, JsonArray updated) {
  bool changed = false;
  for (JsonPair kv : obj) {
    const char *key = kv.key().c_str();
    if (applySettingUpdate(key, kv.value())) {
      updated.add(key);
      changed = true;
    }
  }
  return changed;
}

//=======================================================================
// Config Settings - PATCH/PUT /api/v3/config/settings
//=======================================================================
static void updateV3ConfigSettings() {
  StaticJsonDocument<512> reqDoc;
  DeserializationError error = deserializeJson(reqDoc, httpServer.arg(F("plain")));
  if (error) {
    sendV3Error(400, "Invalid JSON", error.c_str());
    return;
  }

  if (!reqDoc.is<JsonObject>()) {
    sendV3Error(400, "Invalid payload", "Expected JSON object");
    return;
  }

  StaticJsonDocument<512> respDoc;
  JsonObject root = respDoc.to<JsonObject>();
  JsonArray updated = root.createNestedArray(F("updated"));

  const bool changed = applySettingsFromJson(reqDoc.as<JsonObject>(), updated);
  if (changed) {
    writeSettings(false);
  }

  root[F("success")] = changed;
  root[F("timestamp")] = (unsigned long)time(nullptr);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/settings"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Config MQTT - GET /api/v3/config/mqtt
//=======================================================================
static void sendV3ConfigMQTT() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("enabled")] = settingMQTTenable;
  root[F("broker")] = CSTR(settingMQTTbroker);
  root[F("port")] = settingMQTTbrokerPort;
  root[F("user")] = CSTR(settingMQTTuser);
  root[F("topic_prefix")] = CSTR(settingMQTTtopTopic);
  root[F("ha_prefix")] = CSTR(settingMQTThaprefix);
  root[F("ha_reboot_detection")] = settingMQTTharebootdetection;
  root[F("unique_id")] = CSTR(settingMQTTuniqueid);
  root[F("ot_message")] = settingMQTTOTmessage;
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/mqtt"));
  
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// Config MQTT - PATCH /api/v3/config/mqtt
//=======================================================================
static void updateV3ConfigMQTT() {
  StaticJsonDocument<384> reqDoc;
  DeserializationError error = deserializeJson(reqDoc, httpServer.arg(F("plain")));
  if (error) {
    sendV3Error(400, "Invalid JSON", error.c_str());
    return;
  }

  if (!reqDoc.is<JsonObject>()) {
    sendV3Error(400, "Invalid payload", "Expected JSON object");
    return;
  }

  StaticJsonDocument<384> respDoc;
  JsonObject root = respDoc.to<JsonObject>();
  JsonArray updated = root.createNestedArray(F("updated"));

  const bool changed = applySettingsFromJson(reqDoc.as<JsonObject>(), updated);
  if (changed) {
    writeSettings(false);
  }

  root[F("success")] = changed;
  root[F("timestamp")] = (unsigned long)time(nullptr);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/mqtt"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Config NTP - GET /api/v3/config/ntp
//=======================================================================
static void sendV3ConfigNTP() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("enabled")] = settingNTPenable;
  root[F("timezone")] = CSTR(settingNTPtimezone);
  root[F("hostname")] = CSTR(settingNTPhostname);
  root[F("sendtime")] = settingNTPsendtime;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/ntp"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// Config NTP - PATCH /api/v3/config/ntp
//=======================================================================
static void updateV3ConfigNTP() {
  StaticJsonDocument<256> reqDoc;
  DeserializationError error = deserializeJson(reqDoc, httpServer.arg(F("plain")));
  if (error) {
    sendV3Error(400, "Invalid JSON", error.c_str());
    return;
  }

  if (!reqDoc.is<JsonObject>()) {
    sendV3Error(400, "Invalid payload", "Expected JSON object");
    return;
  }

  StaticJsonDocument<384> respDoc;
  JsonObject root = respDoc.to<JsonObject>();
  JsonArray updated = root.createNestedArray(F("updated"));

  const bool changed = applySettingsFromJson(reqDoc.as<JsonObject>(), updated);
  if (changed) {
    writeSettings(false);
  }

  root[F("success")] = changed;
  root[F("timestamp")] = (unsigned long)time(nullptr);

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/ntp"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Config Network - GET /api/v3/config/network
//=======================================================================
static void sendV3ConfigNetwork() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("ssid")] = CSTR(WiFi.SSID());
  root[F("ip_address")] = CSTR(WiFi.localIP().toString());
  root[F("gateway")] = CSTR(WiFi.gatewayIP().toString());
  root[F("subnet")] = CSTR(WiFi.subnetMask().toString());
  root[F("mac_address")] = CSTR(WiFi.macAddress());

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/config/network"));
  JsonObject config = links.createNestedObject(F("config"));
  config[F("href")] = F("/api/v3/config");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Sensors Index - GET /api/v3/sensors
//=======================================================================
static void sendV3SensorsIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("description")] = F("External sensor data");
  
  // HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/sensors"));
  
  if (settingGPIOSENSORSenabled) {
    JsonObject dallas = links.createNestedObject(F("dallas"));
    dallas[F("href")] = F("/api/v3/sensors/dallas");
    dallas[F("description")] = F("Dallas temperature sensors");
  }
  
  if (settingS0COUNTERenabled) {
    JsonObject s0 = links.createNestedObject(F("s0"));
    s0[F("href")] = F("/api/v3/sensors/s0");
    s0[F("description")] = F("S0 pulse counter");
  }
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Sensors Dallas - GET /api/v3/sensors/dallas
//=======================================================================
static void sendV3SensorsDallas() {
  if (!settingGPIOSENSORSenabled) {
    sendV3Error(404, "Dallas sensors disabled");
    return;
  }

  const String connectedQuery = httpServer.hasArg(F("connected")) ? httpServer.arg(F("connected")) : String();
  const bool filterConnected = (connectedQuery.length() > 0);
  const bool wantConnected = filterConnected && (connectedQuery == F("true") || connectedQuery == F("1"));

  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();

  int returnedCount = 0;
  JsonArray sensors = root.createNestedArray(F("sensors"));

  for (int i = 0; i < DallasrealDeviceCount; i++) {
    const bool isConnected = (DallasrealDevice[i].tempC != DEVICE_DISCONNECTED_C);
    if (filterConnected && (isConnected != wantConnected)) continue;

    JsonObject sensor = sensors.createNestedObject();
    const char *address = getDallasAddress(DallasrealDevice[i].addr);
    sensor[F("address")] = address;
    sensor[F("temperature_c")] = DallasrealDevice[i].tempC;
    sensor[F("updated")] = (unsigned long)DallasrealDevice[i].lasttime;
    sensor[F("connected")] = isConnected;
    returnedCount++;
  }

  root[F("count")] = returnedCount;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/sensors/dallas"));
  JsonObject sensorsRoot = links.createNestedObject(F("sensors"));
  sensorsRoot[F("href")] = F("/api/v3/sensors");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Sensors Dallas by Address - GET /api/v3/sensors/dallas/{address}
//=======================================================================
static void sendV3SensorsDallasAddress(const char *address) {
  if (!settingGPIOSENSORSenabled) {
    sendV3Error(404, "Dallas sensors disabled");
    return;
  }
  if (!address || address[0] == '\0') {
    sendV3Error(400, "Missing address");
    return;
  }

  for (int i = 0; i < DallasrealDeviceCount; i++) {
    const char *sensorAddress = getDallasAddress(DallasrealDevice[i].addr);
    if (strcasecmp(sensorAddress, address) == 0) {
      StaticJsonDocument<512> doc;
      JsonObject root = doc.to<JsonObject>();

      root[F("address")] = sensorAddress;
      root[F("temperature_c")] = DallasrealDevice[i].tempC;
      root[F("updated")] = (unsigned long)DallasrealDevice[i].lasttime;
      root[F("connected")] = (DallasrealDevice[i].tempC != DEVICE_DISCONNECTED_C);

      JsonObject links = root.createNestedObject(F("_links"));
      char selfPath[64];
      snprintf_P(selfPath, sizeof(selfPath), PSTR("/api/v3/sensors/dallas/%s"), sensorAddress);
      addHATEOASLinks(links, selfPath);
      JsonObject dallas = links.createNestedObject(F("dallas"));
      dallas[F("href")] = F("/api/v3/sensors/dallas");

      char sBuff[JSON_BUFF_MAX];
      serializeJsonPretty(root, sBuff, sizeof(sBuff));

      sendV3Json(200, sBuff);
      return;
    }
  }

  sendV3Error(404, "Sensor not found", address);
}

//=======================================================================
// Sensors S0 - GET /api/v3/sensors/s0
//=======================================================================
static void sendV3SensorsS0() {
  if (!settingS0COUNTERenabled) {
    sendV3Error(404, "S0 counter disabled");
    return;
  }

  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("pulse_count_interval")] = OTGWs0pulseCount;
  root[F("pulse_count_total")] = OTGWs0pulseCountTot;
  root[F("power_kw")] = OTGWs0powerkw;
  root[F("updated")] = (unsigned long)OTGWs0lasttime;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/sensors/s0"));
  JsonObject sensorsRoot = links.createNestedObject(F("sensors"));
  sensorsRoot[F("href")] = F("/api/v3/sensors");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Export Index - GET /api/v3/export
//=======================================================================
static void sendV3ExportIndex() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("description")] = F("Export formats");

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/export"));
  JsonObject telegraf = links.createNestedObject(F("telegraf"));
  telegraf[F("href")] = F("/api/v3/export/telegraf");
  JsonObject otmonitor = links.createNestedObject(F("otmonitor"));
  otmonitor[F("href")] = F("/api/v3/export/otmonitor");
  JsonObject prometheus = links.createNestedObject(F("prometheus"));
  prometheus[F("href")] = F("/api/v3/export/prometheus");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Export Telegraf - GET /api/v3/export/telegraf
//=======================================================================
static void sendV3ExportTelegraf() {
  sendTelegraf();
}

//=======================================================================
// Export OTmonitor - GET /api/v3/export/otmonitor
//=======================================================================
static void sendV3ExportOTmonitor() {
  sendOTmonitorV2();
}

//=======================================================================
// Export Prometheus - GET /api/v3/export/prometheus
//=======================================================================
static void sendV3ExportPrometheus() {
  sendV3Error(501, "Not implemented", "Prometheus export is planned");
}

//=======================================================================
// PIC Status - GET /api/v3/pic
//=======================================================================
static void sendV3PICStatus() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("available")] = bPICavailable;
  root[F("firmware_version")] = sPICfwversion;
  root[F("device_id")] = sPICdeviceid;
  root[F("firmware_type")] = sPICtype;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/pic"));
  JsonObject firmware = links.createNestedObject(F("firmware"));
  firmware[F("href")] = F("/api/v3/pic/firmware");
  JsonObject flashStatus = links.createNestedObject(F("flash_status"));
  flashStatus[F("href")] = F("/api/v3/pic/flash/status");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// PIC Firmware - GET /api/v3/pic/firmware
//=======================================================================
static void sendV3PICFirmware() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("available")] = bPICavailable;
  root[F("version")] = sPICfwversion;
  root[F("type")] = sPICtype;
  root[F("device_id")] = sPICdeviceid;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/pic/firmware"));
  JsonObject pic = links.createNestedObject(F("pic"));
  pic[F("href")] = F("/api/v3/pic");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff, true);
}

//=======================================================================
// PIC Flash Status - GET /api/v3/pic/flash/status
//=======================================================================
static void sendV3PICFlashStatus() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();

  root[F("flashing")] = isPICFlashing;
  root[F("progress")] = currentPICFlashProgress;
  root[F("filename")] = currentPICFlashFile;
  root[F("error")] = errorupgrade;

  JsonObject links = root.createNestedObject(F("_links"));
  addHATEOASLinks(links, F("/api/v3/pic/flash/status"));
  JsonObject pic = links.createNestedObject(F("pic"));
  pic[F("href")] = F("/api/v3/pic");

  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));

  sendV3Json(200, sBuff);
}

//=======================================================================
// Main v3 API router - called from processAPI() in restAPI.ino
//=======================================================================
void processAPIv3(char words[][32], uint8_t wc, const char *originalURI) {
  const HTTPMethod method = httpServer.method();
  const bool isGet = (method == HTTP_GET);
  const bool isPost = (method == HTTP_POST);
  const bool isPut = (method == HTTP_PUT);
  const bool isPatch = (method == HTTP_PATCH);
  const bool isHead = (method == HTTP_HEAD);
  const bool isOptions = (method == HTTP_OPTIONS);

  if (isOptions) {
    sendV3Options();
    return;
  }

  if (!checkV3RateLimit()) {
    sendV3Error(429, "Too many requests");
    return;
  }
  
  // GET /api/v3 - API root
  if (wc == 3) {
    if (isHead) { sendV3HeadOk(); return; }
    if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
    sendV3Root();
    return;
  }
  
  // /api/v3/system/*
  if (wc > 3 && strcmp_P(words[3], PSTR("system")) == 0) {
    if (isHead) { sendV3HeadOk(); return; }
    if (!isGet) { sendV3Error(405, "Method not allowed"); return; }

    if (wc == 4) {
      sendV3SystemOverview();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("health")) == 0) {
      sendV3SystemHealth();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("info")) == 0) {
      sendV3SystemInfo();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("time")) == 0) {
      sendV3SystemTime();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("network")) == 0) {
      sendV3SystemNetwork();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("stats")) == 0) {
      sendV3SystemStats();
    } else {
      sendV3Error(404, "Not found", originalURI);
    }
    return;
  }
  
  // /api/v3/otgw/*
  if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
    if (wc == 4) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWIndex();
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWStatus();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("thermostat")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWThermostat();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("boiler")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWBoiler();
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("messages")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }

      if (wc > 5) {
        // GET /api/v3/otgw/messages/{id}
        uint8_t msgId = 0;
        if (parseMsgId(words[5], msgId)) {
          sendV3OTGWMessage(msgId);
        } else {
          sendV3Error(400, "Invalid message ID");
        }
      } else {
        // GET /api/v3/otgw/messages - list messages with pagination/filtering
        sendV3OTGWMessagesList();
      }
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("command")) == 0) {
      if (!isPost) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWCommand();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("commands")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (isGet) {
        sendV3OTGWCommands();
        return;
      }
      if (isPost) {
        sendV3OTGWCommandsPost();
        return;
      }
      sendV3Error(405, "Method not allowed");
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("autoconfigure")) == 0) {
      if (!isPost) { sendV3Error(405, "Method not allowed"); return; }
      sendV3OTGWAutoconfigure();
      return;
    }
    
    sendV3Error(404, "Not found", originalURI);
    return;
  }
  
  // /api/v3/config/*
  if (wc > 3 && strcmp_P(words[3], PSTR("config")) == 0) {
    if (wc == 4) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3ConfigIndex();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("settings")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (isGet) {
        sendV3ConfigSettings();
        return;
      }
      if (isPatch || isPut) {
        updateV3ConfigSettings();
        return;
      }
      sendV3Error(405, "Method not allowed");
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("mqtt")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (isGet) {
        sendV3ConfigMQTT();
        return;
      }
      if (isPatch) {
        updateV3ConfigMQTT();
        return;
      }
      sendV3Error(405, "Method not allowed");
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("ntp")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (isGet) {
        sendV3ConfigNTP();
        return;
      }
      if (isPatch) {
        updateV3ConfigNTP();
        return;
      }
      sendV3Error(405, "Method not allowed");
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("network")) == 0) {
      if (isHead) { sendV3HeadOk(); return; }
      if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
      sendV3ConfigNetwork();
      return;
    }
    
    sendV3Error(404, "Not found", originalURI);
    return;
  }
  
  // /api/v3/sensors/*
  if (wc > 3 && strcmp_P(words[3], PSTR("sensors")) == 0) {
    if (isHead) { sendV3HeadOk(); return; }
    if (!isGet) { sendV3Error(405, "Method not allowed"); return; }
    
    if (wc == 4) {
      sendV3SensorsIndex();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("dallas")) == 0) {
      if (wc > 5) {
        sendV3SensorsDallasAddress(words[5]);
      } else {
        sendV3SensorsDallas();
      }
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("s0")) == 0) {
      sendV3SensorsS0();
      return;
    }
    
    sendV3Error(404, "Not found", originalURI);
    return;
  }

  // /api/v3/export/*
  if (wc > 3 && strcmp_P(words[3], PSTR("export")) == 0) {
    if (isHead) { sendV3HeadOk(); return; }
    if (!isGet) { sendV3Error(405, "Method not allowed"); return; }

    if (wc == 4) {
      sendV3ExportIndex();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("telegraf")) == 0) {
      sendV3ExportTelegraf();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("otmonitor")) == 0) {
      sendV3ExportOTmonitor();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("prometheus")) == 0) {
      sendV3ExportPrometheus();
      return;
    }

    sendV3Error(404, "Not found", originalURI);
    return;
  }

  // /api/v3/pic/*
  if (wc > 3 && strcmp_P(words[3], PSTR("pic")) == 0) {
    if (isHead) { sendV3HeadOk(); return; }
    if (!isGet) { sendV3Error(405, "Method not allowed"); return; }

    if (wc == 4) {
      sendV3PICStatus();
      return;
    }

    if (wc > 4 && strcmp_P(words[4], PSTR("firmware")) == 0) {
      sendV3PICFirmware();
      return;
    }

    if (wc > 5 && strcmp_P(words[4], PSTR("flash")) == 0 && strcmp_P(words[5], PSTR("status")) == 0) {
      sendV3PICFlashStatus();
      return;
    }

    sendV3Error(404, "Not found", originalURI);
    return;
  }
  
  // Not found
  sendV3Error(404, "Not found", originalURI);
}

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
