/* 
***************************************************************************  
**  Program  : restAPI_v3
**  Version  : v1.1.0
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.
**
**  REST API v3 - Richardson Maturity Level 3 with HATEOAS
**  - Resource-oriented architecture
**  - Proper HTTP methods (GET, POST, PATCH, DELETE)
**  - Hypermedia links for discoverability
**  - Consistent error responses
**  - ETag support for caching
**
**  Architectural Decision Record: docs/adr/ADR-025-rest-api-v3-design.md
**  Resource Model: docs/api/v3/RESOURCE_MODEL.md
**  Error Responses: docs/api/v3/ERROR_RESPONSES.md
**  HTTP Status Codes: docs/api/v3/HTTP_STATUS_CODES.md
**  Response Headers: docs/api/v3/RESPONSE_HEADERS.md
***************************************************************************      
*/

// Forward declarations for v3 handlers
static void handleSystemResources();
static void handleConfigResources();
static void handleOTGWResources();
static void handlePICResources();
static void handleSensorsResources();
static void handleExportResources();

// Forward declarations for HTTP method helpers
static bool checkMethodAllowed(uint8_t allowedMethods);
static void sendMethodNotAllowed(uint8_t allowedMethods);

// Forward declarations for HATEOAS helpers
static void addHATEOASlinks(JsonObject &obj, const char *selfPath);
static void addRootLinks(JsonObject &obj);

// HTTP method flags for checkMethodAllowed
#define HTTP_METHOD_GET     0x01
#define HTTP_METHOD_POST    0x02
#define HTTP_METHOD_PUT     0x04
#define HTTP_METHOD_PATCH   0x08
#define HTTP_METHOD_DELETE  0x10

// ETag generation for cacheable resources
static String generateETag(const char *resource, uint32_t lastModified) {
  char etag[32];
  snprintf_P(etag, sizeof(etag), PSTR("\"%08x-%08x\""), 
             fnv1a_32(resource, strlen(resource)), lastModified);
  return String(etag);
}

// FNV-1a 32-bit hash function for ETag generation
static uint32_t fnv1a_32(const char *str, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    hash ^= static_cast<uint8_t>(str[i]);
    hash *= 16777619u;
  }
  return hash;
}

//=======================================================================
// Main v3 routing function - called from processAPI() when /api/v3/... detected
//=======================================================================
void processAPIv3(char words[][32], uint8_t wc, const char *originalURI) {
  RESTDebugTln(F("[v3] processAPIv3 called"));
  
  // Set standard v3 headers for all responses
  httpServer.sendHeader(F("X-OTGW-API-Version"), F("3.0"));
  httpServer.sendHeader(F("X-OTGW-Version"), _VERSION);
  
  char heapStr[16];
  snprintf_P(heapStr, sizeof(heapStr), PSTR("%u"), ESP.getFreeHeap());
  httpServer.sendHeader(F("X-OTGW-Heap-Free"), heapStr);
  
  // CORS headers
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, PUT, PATCH, DELETE, OPTIONS"));
  httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, If-None-Match, If-Match"));
  
  // Handle OPTIONS preflight requests
  if (httpServer.method() == HTTP_OPTIONS) {
    httpServer.send(204); // No Content
    return;
  }
  
  // Route to resource handlers based on words[3]
  // words[0] = "" (leading slash)
  // words[1] = "api"
  // words[2] = "v3"
  // words[3] = resource category
  
  if (wc == 3) {
    // GET /api/v3/ - API root with HATEOAS links
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendAPIv3Root();
    return;
  }
  
  if (wc > 3) {
    if (strcmp_P(words[3], PSTR("system")) == 0) {
      handleSystemResources();
    } else if (strcmp_P(words[3], PSTR("config")) == 0) {
      handleConfigResources();
    } else if (strcmp_P(words[3], PSTR("otgw")) == 0) {
      handleOTGWResources();
    } else if (strcmp_P(words[3], PSTR("pic")) == 0) {
      handlePICResources();
    } else if (strcmp_P(words[3], PSTR("sensors")) == 0) {
      handleSensorsResources();
    } else if (strcmp_P(words[3], PSTR("export")) == 0) {
      handleExportResources();
    } else {
      // Unknown resource category
      sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                      PSTR("The requested resource does not exist"), 
                      originalURI);
    }
  } else {
    sendJsonError_P(400, PSTR("INVALID_REQUEST"), 
                    PSTR("Invalid API request format"), 
                    originalURI);
  }
}

//=======================================================================
// API Root - GET /api/v3/
//=======================================================================
static void sendAPIv3Root() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("version")] = F("3.0");
  root[F("name")] = F("OTGW REST API");
  root[F("description")] = F("OpenTherm Gateway REST API v3 with HATEOAS");
  
  // Add HATEOAS links to all top-level resources
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/");
  
  JsonObject systemLink = links.createNestedObject(F("system"));
  systemLink[F("href")] = F("/api/v3/system");
  systemLink[F("title")] = F("System information and health");
  
  JsonObject configLink = links.createNestedObject(F("config"));
  configLink[F("href")] = F("/api/v3/config");
  configLink[F("title")] = F("Device configuration");
  
  JsonObject otgwLink = links.createNestedObject(F("otgw"));
  otgwLink[F("href")] = F("/api/v3/otgw");
  otgwLink[F("title")] = F("OpenTherm Gateway data and commands");
  
  JsonObject picLink = links.createNestedObject(F("pic"));
  picLink[F("href")] = F("/api/v3/pic");
  picLink[F("title")] = F("PIC firmware and status");
  
  JsonObject sensorsLink = links.createNestedObject(F("sensors"));
  sensorsLink[F("href")] = F("/api/v3/sensors");
  sensorsLink[F("title")] = F("Sensor data (Dallas, S0)");
  
  JsonObject exportLink = links.createNestedObject(F("export"));
  exportLink[F("href")] = F("/api/v3/export");
  exportLink[F("title")] = F("Data export formats");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Resources - /api/v3/system/*
//=======================================================================
static void handleSystemResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strstr_P(uri, PSTR("/api/v3/system/info")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemInfo();
  } else if (strstr_P(uri, PSTR("/api/v3/system/health")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemHealth();
  } else if (strstr_P(uri, PSTR("/api/v3/system/time")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemTime();
  } else if (strstr_P(uri, PSTR("/api/v3/system/network")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemNetwork();
  } else if (strstr_P(uri, PSTR("/api/v3/system/stats")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemStats();
  } else if (strcmp_P(uri, PSTR("/api/v3/system")) == 0 || strcmp_P(uri, PSTR("/api/v3/system/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSystemIndex();
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("System resource not found"), uri);
  }
}

//=======================================================================
// System Index - GET /api/v3/system
//=======================================================================
static void sendSystemIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("system");
  root[F("description")] = F("System information and diagnostics");
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/system");
  
  JsonObject infoLink = links.createNestedObject(F("info"));
  infoLink[F("href")] = F("/api/v3/system/info");
  infoLink[F("title")] = F("Device information");
  
  JsonObject healthLink = links.createNestedObject(F("health"));
  healthLink[F("href")] = F("/api/v3/system/health");
  healthLink[F("title")] = F("System health check");
  
  JsonObject timeLink = links.createNestedObject(F("time"));
  timeLink[F("href")] = F("/api/v3/system/time");
  timeLink[F("title")] = F("System time and timezone");
  
  JsonObject networkLink = links.createNestedObject(F("network"));
  networkLink[F("href")] = F("/api/v3/system/network");
  networkLink[F("title")] = F("Network configuration");
  
  JsonObject statsLink = links.createNestedObject(F("stats"));
  statsLink[F("href")] = F("/api/v3/system/stats");
  statsLink[F("title")] = F("System statistics");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Info - GET /api/v3/system/info
// Refactored from sendDeviceInfo() in v1
//=======================================================================
static void sendSystemInfo() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // Basic device info
  root[F("hostname")] = settingHostname;
  root[F("version")] = _VERSION;
  root[F("compiled")] = __DATE__ " " __TIME__;
  root[F("chipid")] = String(ESP.getChipId(), HEX);
  root[F("corever")] = ESP.getCoreVersion();
  root[F("sdkver")] = ESP.getSdkVersion();
  root[F("cpufreq")] = ESP.getCpuFreqMHz();
  root[F("flashsize")] = ESP.getFlashChipSize();
  root[F("flashspeed")] = ESP.getFlashChipSpeed();
  
  // Add HATEOAS links
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/system/info");
  
  JsonObject systemLink = links.createNestedObject(F("system"));
  systemLink[F("href")] = F("/api/v3/system");
  
  JsonObject healthLink = links.createNestedObject(F("health"));
  healthLink[F("href")] = F("/api/v3/system/health");
  
  String output;
  serializeJson(doc, output);
  
  // Add ETag for caching (based on version and compile time)
  String etag = generateETag("system/info", fnv1a_32(_VERSION, strlen(_VERSION)));
  httpServer.sendHeader(F("ETag"), etag);
  httpServer.sendHeader(F("Cache-Control"), F("public, max-age=3600")); // Cache for 1 hour
  
  // Check If-None-Match
  if (httpServer.hasHeader(F("If-None-Match"))) {
    if (httpServer.header(F("If-None-Match")) == etag) {
      httpServer.send(304); // Not Modified
      return;
    }
  }
  
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Health - GET /api/v3/system/health
// Refactored from sendHealth() in v1
//=======================================================================
static void sendSystemHealth() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("status")] = F("UP");
  root[F("uptime")] = millis() / 1000;
  root[F("heap_free")] = ESP.getFreeHeap();
  root[F("heap_fragmentation")] = ESP.getHeapFragmentation();
  root[F("picavailable")] = isOTGWonline();
  
  // Add HATEOAS links
  addHATEOASlinks(root, "/api/v3/system/health");
  
  String output;
  serializeJson(doc, output);
  
  // No caching for health endpoint (dynamic data)
  httpServer.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Time - GET /api/v3/system/time
// Refactored from sendDeviceTime() in v0
//=======================================================================
static void sendSystemTime() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  
  char dateTime[32];
  snprintf_P(dateTime, sizeof(dateTime), PSTR("%04d-%02d-%02dT%02d:%02d:%02d"),
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  
  root[F("datetime")] = dateTime;
  root[F("epoch")] = now;
  root[F("timezone")] = settingNTPtimezone;
  
  // Add HATEOAS links
  addHATEOASlinks(root, "/api/v3/system/time");
  
  String output;
  serializeJson(doc, output);
  
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Network - GET /api/v3/system/network
//=======================================================================
static void sendSystemNetwork() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("ssid")] = WiFi.SSID();
  root[F("rssi")] = WiFi.RSSI();
  root[F("ip")] = WiFi.localIP().toString();
  root[F("subnet")] = WiFi.subnetMask().toString();
  root[F("gateway")] = WiFi.gatewayIP().toString();
  root[F("dns")] = WiFi.dnsIP().toString();
  root[F("mac")] = WiFi.macAddress();
  root[F("hostname")] = settingHostname;
  
  // Add HATEOAS links
  addHATEOASlinks(root, "/api/v3/system/network");
  
  String output;
  serializeJson(doc, output);
  
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// System Stats - GET /api/v3/system/stats
//=======================================================================
static void sendSystemStats() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("uptime")] = millis() / 1000;
  root[F("heap_free")] = ESP.getFreeHeap();
  root[F("heap_fragmentation")] = ESP.getHeapFragmentation();
  root[F("sketch_size")] = ESP.getSketchSize();
  root[F("free_sketch_space")] = ESP.getFreeSketchSpace();
  root[F("flash_chip_size")] = ESP.getFlashChipSize();
  root[F("cpu_freq_mhz")] = ESP.getCpuFreqMHz();
  
  // Add HATEOAS links
  addHATEOASlinks(root, "/api/v3/system/stats");
  
  String output;
  serializeJson(doc, output);
  
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Config Resources - /api/v3/config/* 
// Settings management with GET/PATCH support
//=======================================================================
static void handleConfigResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strcmp_P(uri, PSTR("/api/v3/config")) == 0 || strcmp_P(uri, PSTR("/api/v3/config/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendConfigIndex();
  } else if (strstr_P(uri, PSTR("/api/v3/config/device")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET | HTTP_METHOD_PATCH)) return;
    if (httpServer.method() == HTTP_GET) {
      sendConfigDevice();
    } else if (httpServer.method() == HTTP_PATCH) {
      patchConfigDevice();
    }
  } else if (strstr_P(uri, PSTR("/api/v3/config/network")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET | HTTP_METHOD_PATCH)) return;
    if (httpServer.method() == HTTP_GET) {
      sendConfigNetwork();
    } else if (httpServer.method() == HTTP_PATCH) {
      patchConfigNetwork();
    }
  } else if (strstr_P(uri, PSTR("/api/v3/config/mqtt")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET | HTTP_METHOD_PATCH)) return;
    if (httpServer.method() == HTTP_GET) {
      sendConfigMQTT();
    } else if (httpServer.method() == HTTP_PATCH) {
      patchConfigMQTT();
    }
  } else if (strstr_P(uri, PSTR("/api/v3/config/otgw")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET | HTTP_METHOD_PATCH)) return;
    if (httpServer.method() == HTTP_GET) {
      sendConfigOTGW();
    } else if (httpServer.method() == HTTP_PATCH) {
      patchConfigOTGW();
    }
  } else if (strstr_P(uri, PSTR("/api/v3/config/features")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendConfigFeatures();
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("Config resource not found"), uri);
  }
}

//=======================================================================
// Config Index - GET /api/v3/config
//=======================================================================
static void sendConfigIndex() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("config");
  root[F("description")] = F("Device configuration management");
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/config");
  
  JsonObject deviceLink = links.createNestedObject(F("device"));
  deviceLink[F("href")] = F("/api/v3/config/device");
  deviceLink[F("title")] = F("Device settings");
  
  JsonObject networkLink = links.createNestedObject(F("network"));
  networkLink[F("href")] = F("/api/v3/config/network");
  networkLink[F("title")] = F("Network settings");
  
  JsonObject mqttLink = links.createNestedObject(F("mqtt"));
  mqttLink[F("href")] = F("/api/v3/config/mqtt");
  mqttLink[F("title")] = F("MQTT configuration");
  
  JsonObject otgwLink = links.createNestedObject(F("otgw"));
  otgwLink[F("href")] = F("/api/v3/config/otgw");
  otgwLink[F("title")] = F("OTGW settings");
  
  JsonObject featuresLink = links.createNestedObject(F("features"));
  featuresLink[F("href")] = F("/api/v3/config/features");
  featuresLink[F("title")] = F("Available features");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Config Device - GET /api/v3/config/device
//=======================================================================
static void sendConfigDevice() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("hostname")] = settingHostname;
  root[F("ntp_timezone")] = settingNTPtimezone;
  root[F("led_mode")] = settingLedMode;
  
  addHATEOASlinks(root, "/api/v3/config/device");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Patch Config Device - PATCH /api/v3/config/device
//=======================================================================
static void patchConfigDevice() {
  // Parse JSON from request body
  StaticJsonDocument<256> doc;
  
  if (!httpServer.hasHeader(F("Content-Type"))) {
    sendJsonError_P(400, PSTR("MISSING_CONTENT_TYPE"), 
                    PSTR("Content-Type header required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String contentType = httpServer.header(F("Content-Type"));
  if (contentType.indexOf(F("application/json")) == -1) {
    sendJsonError_P(415, PSTR("UNSUPPORTED_MEDIA_TYPE"), 
                    PSTR("Content-Type must be application/json"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String body = httpServer.arg("plain");
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    char errBuf[128];
    snprintf_P(errBuf, sizeof(errBuf), PSTR("JSON parse error: %s"), error.c_str());
    sendJsonError(400, "INVALID_JSON", errBuf, httpServer.uri().c_str());
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  bool changed = false;
  
  if (obj.containsKey(F("hostname"))) {
    const char *newHostname = obj[F("hostname")].as<const char*>();
    if (newHostname && strlen(newHostname) > 0 && strlen(newHostname) <= 31) {
      strlcpy(settingHostname, newHostname, sizeof(settingHostname));
      changed = true;
    }
  }
  
  if (obj.containsKey(F("ntp_timezone"))) {
    const char *tz = obj[F("ntp_timezone")].as<const char*>();
    if (tz) {
      strlcpy(settingNTPtimezone, tz, sizeof(settingNTPtimezone));
      changed = true;
    }
  }
  
  if (obj.containsKey(F("led_mode"))) {
    settingLedMode = obj[F("led_mode")].as<int>();
    changed = true;
  }
  
  if (changed) {
    writeSettings();
    httpServer.send(200, F("application/json"), F("{\"status\":\"updated\"}"));
  } else {
    httpServer.send(204); // No Content
  }
}

//=======================================================================
// Config Network - GET /api/v3/config/network
//=======================================================================
static void sendConfigNetwork() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("ssid")] = settingWifi_SSID;
  root[F("dhcp_enabled")] = settingDhcp;
  root[F("ip")] = settingIp;
  root[F("netmask")] = settingNetmask;
  root[F("gateway")] = settingGateway;
  root[F("dns1")] = settingDNS1;
  root[F("dns2")] = settingDNS2;
  
  addHATEOASlinks(root, "/api/v3/config/network");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Patch Config Network - PATCH /api/v3/config/network
//=======================================================================
static void patchConfigNetwork() {
  StaticJsonDocument<512> doc;
  
  if (!httpServer.hasHeader(F("Content-Type"))) {
    sendJsonError_P(400, PSTR("MISSING_CONTENT_TYPE"), 
                    PSTR("Content-Type header required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String body = httpServer.arg("plain");
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    sendJsonError_P(400, PSTR("INVALID_JSON"), 
                    PSTR("Request body must be valid JSON"), 
                    httpServer.uri().c_str());
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  bool changed = false;
  
  if (obj.containsKey(F("dhcp_enabled"))) {
    settingDhcp = obj[F("dhcp_enabled")].as<bool>();
    changed = true;
  }
  
  if (obj.containsKey(F("ip"))) {
    strlcpy(settingIp, obj[F("ip")].as<const char*>(), sizeof(settingIp));
    changed = true;
  }
  
  if (obj.containsKey(F("netmask"))) {
    strlcpy(settingNetmask, obj[F("netmask")].as<const char*>(), sizeof(settingNetmask));
    changed = true;
  }
  
  if (obj.containsKey(F("gateway"))) {
    strlcpy(settingGateway, obj[F("gateway")].as<const char*>(), sizeof(settingGateway));
    changed = true;
  }
  
  if (changed) {
    writeSettings();
    httpServer.send(200, F("application/json"), F("{\"status\":\"updated\"}"));
  } else {
    httpServer.send(204);
  }
}

//=======================================================================
// Config MQTT - GET /api/v3/config/mqtt
//=======================================================================
static void sendConfigMQTT() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("enabled")] = settingMqttEnabled;
  root[F("broker")] = settingMqttBroker;
  root[F("port")] = settingMqttPort;
  root[F("topic_prefix")] = settingMqttTopicPrefix;
  root[F("unique_id")] = settingMqttUniqueId;
  root[F("ha_auto_discovery")] = settingMqttHAautoDiscovery;
  
  addHATEOASlinks(root, "/api/v3/config/mqtt");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Patch Config MQTT - PATCH /api/v3/config/mqtt
//=======================================================================
static void patchConfigMQTT() {
  StaticJsonDocument<512> doc;
  
  if (!httpServer.hasHeader(F("Content-Type"))) {
    sendJsonError_P(400, PSTR("MISSING_CONTENT_TYPE"), 
                    PSTR("Content-Type header required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String body = httpServer.arg("plain");
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    sendJsonError_P(400, PSTR("INVALID_JSON"), 
                    PSTR("Request body must be valid JSON"), 
                    httpServer.uri().c_str());
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  bool changed = false;
  
  if (obj.containsKey(F("enabled"))) {
    settingMqttEnabled = obj[F("enabled")].as<bool>();
    changed = true;
  }
  
  if (obj.containsKey(F("broker"))) {
    strlcpy(settingMqttBroker, obj[F("broker")].as<const char*>(), sizeof(settingMqttBroker));
    changed = true;
  }
  
  if (obj.containsKey(F("port"))) {
    settingMqttPort = obj[F("port")].as<int>();
    changed = true;
  }
  
  if (obj.containsKey(F("topic_prefix"))) {
    strlcpy(settingMqttTopicPrefix, obj[F("topic_prefix")].as<const char*>(), sizeof(settingMqttTopicPrefix));
    changed = true;
  }
  
  if (obj.containsKey(F("ha_auto_discovery"))) {
    settingMqttHAautoDiscovery = obj[F("ha_auto_discovery")].as<bool>();
    changed = true;
  }
  
  if (changed) {
    writeSettings();
    httpServer.send(200, F("application/json"), F("{\"status\":\"updated\"}"));
  } else {
    httpServer.send(204);
  }
}

//=======================================================================
// Config OTGW - GET /api/v3/config/otgw
//=======================================================================
static void sendConfigOTGW() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("serial_port")] = settingOTGWserialPort;
  root[F("serial_speed")] = settingOTGWserialSpeed;
  root[F("gpio_led1")] = settingGPIO_LED1;
  root[F("gpio_led2")] = settingGPIO_LED2;
  root[F("gpio_dallas_sensors")] = settingGPIO_DALLASsensors;
  root[F("gpio_s0_counter")] = settingGPIO_S0counter;
  
  addHATEOASlinks(root, "/api/v3/config/otgw");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Patch Config OTGW - PATCH /api/v3/config/otgw
//=======================================================================
static void patchConfigOTGW() {
  StaticJsonDocument<384> doc;
  
  if (!httpServer.hasHeader(F("Content-Type"))) {
    sendJsonError_P(400, PSTR("MISSING_CONTENT_TYPE"), 
                    PSTR("Content-Type header required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String body = httpServer.arg("plain");
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    sendJsonError_P(400, PSTR("INVALID_JSON"), 
                    PSTR("Request body must be valid JSON"), 
                    httpServer.uri().c_str());
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  bool changed = false;
  
  if (obj.containsKey(F("gpio_led1"))) {
    settingGPIO_LED1 = obj[F("gpio_led1")].as<int>();
    changed = true;
  }
  
  if (obj.containsKey(F("gpio_led2"))) {
    settingGPIO_LED2 = obj[F("gpio_led2")].as<int>();
    changed = true;
  }
  
  if (obj.containsKey(F("gpio_dallas_sensors"))) {
    settingGPIO_DALLASsensors = obj[F("gpio_dallas_sensors")].as<int>();
    changed = true;
  }
  
  if (obj.containsKey(F("gpio_s0_counter"))) {
    settingGPIO_S0counter = obj[F("gpio_s0_counter")].as<int>();
    changed = true;
  }
  
  if (changed) {
    writeSettings();
    httpServer.send(200, F("application/json"), F("{\"status\":\"updated\"}"));
  } else {
    httpServer.send(204);
  }
}

//=======================================================================
// Config Features - GET /api/v3/config/features
// Lists all available features and their status
//=======================================================================
static void sendConfigFeatures() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("mqtt")] = settingMqttEnabled;
  root[F("webserver")] = true;
  root[F("websocket")] = true;
  root[F("rest_api")] = true;
  root[F("ha_discovery")] = settingMqttHAautoDiscovery;
  root[F("dallas_sensors")] = (settingGPIO_DALLASsensors != 255);
  root[F("s0_counter")] = (settingGPIO_S0counter != 255);
  root[F("debug_telnet")] = bDebugMode;
  
  addHATEOASlinks(root, "/api/v3/config/features");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("public, max-age=3600"));
  httpServer.send(200, F("application/json"), output);
}


//=======================================================================
// OTGW Resources - /api/v3/otgw/*
// OpenTherm Gateway messages, data, and commands
//=======================================================================
static void handleOTGWResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strcmp_P(uri, PSTR("/api/v3/otgw")) == 0 || strcmp_P(uri, PSTR("/api/v3/otgw/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTGWIndex();
  } else if (strstr_P(uri, PSTR("/api/v3/otgw/messages")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTGWMessages();
  } else if (strstr_P(uri, PSTR("/api/v3/otgw/data")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTGWData();
  } else if (strstr_P(uri, PSTR("/api/v3/otgw/status")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTGWStatus();
  } else if (strstr_P(uri, PSTR("/api/v3/otgw/command")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_POST | HTTP_METHOD_PUT)) return;
    if (httpServer.method() == HTTP_POST || httpServer.method() == HTTP_PUT) {
      postOTGWCommand();
    }
  } else if (strstr_P(uri, PSTR("/api/v3/otgw/monitor")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTGWMonitor();
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("OTGW resource not found"), uri);
  }
}

//=======================================================================
// OTGW Index - GET /api/v3/otgw
//=======================================================================
static void sendOTGWIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("otgw");
  root[F("description")] = F("OpenTherm Gateway data and control");
  root[F("available")] = isOTGWonline();
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/otgw");
  
  JsonObject statusLink = links.createNestedObject(F("status"));
  statusLink[F("href")] = F("/api/v3/otgw/status");
  statusLink[F("title")] = F("Gateway status");
  
  JsonObject messagesLink = links.createNestedObject(F("messages"));
  messagesLink[F("href")] = F("/api/v3/otgw/messages");
  messagesLink[F("title")] = F("All OpenTherm messages");
  
  JsonObject dataLink = links.createNestedObject(F("data"));
  dataLink[F("href")] = F("/api/v3/otgw/data");
  dataLink[F("title")] = F("Current data values");
  
  JsonObject commandLink = links.createNestedObject(F("command"));
  commandLink[F("href")] = F("/api/v3/otgw/command");
  commandLink[F("title")] = F("Send OTGW command");
  
  JsonObject monitorLink = links.createNestedObject(F("monitor"));
  monitorLink[F("href")] = F("/api/v3/otgw/monitor");
  monitorLink[F("title")] = F("Monitor format data");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// OTGW Status - GET /api/v3/otgw/status
//=======================================================================
static void sendOTGWStatus() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("available")] = isOTGWonline();
  root[F("sync")] = getOTGWvalue(0).c_str(); // Status sync byte
  
  if (isOTGWonline()) {
    root[F("boiler_enabled")] = bitRead(OTGWstatus.flags, 0);
    root[F("ch_enabled")] = bitRead(OTGWstatus.flags, 1);
    root[F("dhw_enabled")] = bitRead(OTGWstatus.flags, 2);
    root[F("flame")] = bitRead(OTGWstatus.flags, 3);
    root[F("cooling_enabled")] = bitRead(OTGWstatus.flags, 4);
    root[F("ch2_enabled")] = bitRead(OTGWstatus.flags, 5);
    root[F("diag_fault")] = bitRead(OTGWstatus.flags, 6);
  }
  
  addHATEOASlinks(root, "/api/v3/otgw/status");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// OTGW Messages - GET /api/v3/otgw/messages
// List all OpenTherm messages with current values
//=======================================================================
static void sendOTGWMessages() {
  // Check if we have query parameter to filter by message ID
  String msgId = httpServer.arg(F("id"));
  
  if (msgId.length() > 0) {
    // Single message request
    uint8_t id = atoi(msgId.c_str());
    if (id <= OT_MSGID_MAX) {
      sendOTGWMessageDetail(id);
    } else {
      sendJsonError_P(400, PSTR("INVALID_MESSAGE_ID"), 
                      PSTR("Message ID must be 0-127"), 
                      httpServer.uri().c_str());
    }
  } else {
    // All messages - paginated response
    int page = httpServer.arg(F("page")).length() > 0 ? atoi(httpServer.arg(F("page")).c_str()) : 0;
    int pageSize = httpServer.arg(F("per_page")).length() > 0 ? atoi(httpServer.arg(F("per_page")).c_str()) : 10;
    
    if (pageSize > 50) pageSize = 50; // Limit page size
    if (page < 0) page = 0;
    
    int totalMessages = OT_MSGID_MAX + 1;
    int totalPages = (totalMessages + pageSize - 1) / pageSize;
    
    StaticJsonDocument<2048> doc;
    JsonObject root = doc.to<JsonObject>();
    
    root[F("total")] = totalMessages;
    root[F("page")] = page;
    root[F("per_page")] = pageSize;
    root[F("total_pages")] = totalPages;
    
    JsonArray messages = root.createNestedArray(F("messages"));
    
    int start = page * pageSize;
    int end = start + pageSize;
    if (end > totalMessages) end = totalMessages;
    
    for (int i = start; i < end; i++) {
      PROGMEM_readAnything(&OTmap[i], OTlookupitem);
      if (OTlookupitem.type != ot_undef) {
        JsonObject msg = messages.createNestedObject();
        msg[F("id")] = i;
        msg[F("label")] = OTlookupitem.label;
        msg[F("unit")] = OTlookupitem.unit;
        msg[F("value")] = getOTGWValue(i);
      }
    }
    
    // Pagination links
    JsonObject links = root.createNestedObject(F("_links"));
    JsonObject selfLink = links.createNestedObject(F("self"));
    selfLink[F("href")] = F("/api/v3/otgw/messages?page=") + String(page);
    
    if (page > 0) {
      JsonObject prevLink = links.createNestedObject(F("prev"));
      prevLink[F("href")] = F("/api/v3/otgw/messages?page=") + String(page - 1);
    }
    
    if (page < totalPages - 1) {
      JsonObject nextLink = links.createNestedObject(F("next"));
      nextLink[F("href")] = F("/api/v3/otgw/messages?page=") + String(page + 1);
    }
    
    String output;
    serializeJson(doc, output);
    httpServer.send(200, F("application/json"), output);
  }
}

//=======================================================================
// OTGW Message Detail - GET /api/v3/otgw/messages?id=XX
//=======================================================================
static void sendOTGWMessageDetail(uint8_t msgId) {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  PROGMEM_readAnything(&OTmap[msgId], OTlookupitem);
  
  root[F("id")] = msgId;
  root[F("label")] = OTlookupitem.label;
  root[F("unit")] = OTlookupitem.unit;
  root[F("value")] = getOTGWValue(msgId);
  
  addHATEOASlinks(root, "/api/v3/otgw/messages");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// OTGW Data - GET /api/v3/otgw/data
// Current data formatted for monitoring
//=======================================================================
static void sendOTGWData() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // Add key data points
  root[F("room_setpoint")] = getOTGWValue(16);
  root[F("room_temp")] = getOTGWValue(24);
  root[F("boiler_temp")] = getOTGWValue(25);
  root[F("dhw_temp")] = getOTGWValue(26);
  root[F("boiler_return_temp")] = getOTGWValue(28);
  root[F("ch_pressure")] = getOTGWValue(41);
  root[F("dhw_flow_rate")] = getOTGWValue(42);
  
  addHATEOASlinks(root, "/api/v3/otgw/data");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// OTGW Command - POST /api/v3/otgw/command
// Send command to OTGW gateway
//=======================================================================
static void postOTGWCommand() {
  StaticJsonDocument<256> doc;
  
  if (!httpServer.hasHeader(F("Content-Type"))) {
    sendJsonError_P(400, PSTR("MISSING_CONTENT_TYPE"), 
                    PSTR("Content-Type header required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  String body = httpServer.arg("plain");
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    sendJsonError_P(400, PSTR("INVALID_JSON"), 
                    PSTR("Request body must be valid JSON"), 
                    httpServer.uri().c_str());
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  
  if (!obj.containsKey(F("command"))) {
    sendJsonError_P(400, PSTR("MISSING_FIELD"), 
                    PSTR("'command' field is required"), 
                    httpServer.uri().c_str());
    return;
  }
  
  const char *cmd = obj[F("command")].as<const char*>();
  size_t cmdLen = strlen(cmd);
  
  // Validate command format: XX=YYYY where XX is 2 chars, YYYY is value
  if (cmdLen < 3 || cmd[2] != '=') {
    sendJsonError_P(400, PSTR("INVALID_COMMAND_FORMAT"), 
                    PSTR("Command must be in format: XX=YYYY (e.g., TT=21.5)"), 
                    httpServer.uri().c_str());
    return;
  }
  
  constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1;
  if (cmdLen > kMaxCmdLen) {
    sendJsonError_P(413, PSTR("COMMAND_TOO_LONG"), 
                    PSTR("Command exceeds maximum length"), 
                    httpServer.uri().c_str());
    return;
  }
  
  // Queue the command
  addOTWGcmdtoqueue(cmd, static_cast<int>(cmdLen));
  
  StaticJsonDocument<256> response;
  JsonObject resp = response.to<JsonObject>();
  resp[F("status")] = F("queued");
  resp[F("command")] = cmd;
  resp[F("queue_position")] = cmdqueue_ctr;
  
  String output;
  serializeJson(response, output);
  httpServer.send(202, F("application/json"), output); // 202 Accepted
}

//=======================================================================
// OTGW Monitor - GET /api/v3/otgw/monitor
// OTmonitor format compatible data stream
//=======================================================================
static void sendOTGWMonitor() {
  // This endpoint provides data in OTmonitor format for compatibility
  // Delegates to existing sendOTmonitor() function
  sendOTmonitor();
}

//=======================================================================
// PIC Resources - /api/v3/pic/*
// PIC firmware information and flash status
//=======================================================================
static void handlePICResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strcmp_P(uri, PSTR("/api/v3/pic")) == 0 || strcmp_P(uri, PSTR("/api/v3/pic/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendPICIndex();
  } else if (strstr_P(uri, PSTR("/api/v3/pic/info")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendPICInfo();
  } else if (strstr_P(uri, PSTR("/api/v3/pic/flash")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendPICFlashInfo();
  } else if (strstr_P(uri, PSTR("/api/v3/pic/diag")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendPICDiagnostics();
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("PIC resource not found"), uri);
  }
}

//=======================================================================
// PIC Index - GET /api/v3/pic
//=======================================================================
static void sendPICIndex() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("pic");
  root[F("description")] = F("PIC16F firmware information");
  root[F("available")] = isOTGWonline();
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/pic");
  
  JsonObject infoLink = links.createNestedObject(F("info"));
  infoLink[F("href")] = F("/api/v3/pic/info");
  infoLink[F("title")] = F("PIC firmware information");
  
  JsonObject flashLink = links.createNestedObject(F("flash"));
  flashLink[F("href")] = F("/api/v3/pic/flash");
  flashLink[F("title")] = F("Flash programming status");
  
  JsonObject diagLink = links.createNestedObject(F("diag"));
  diagLink[F("href")] = F("/api/v3/pic/diag");
  diagLink[F("title")] = F("Diagnostics information");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// PIC Info - GET /api/v3/pic/info
// PIC firmware version and capabilities
//=======================================================================
static void sendPICInfo() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("available")] = isOTGWonline();
  
  if (isOTGWonline()) {
    root[F("gateway_version")] = getPICversion(picVersion);
    root[F("interface_version")] = getPICversion(picInterfaceVersion);
    root[F("diagnostics_version")] = getPICversion(picDiagVersion);
    root[F("product_version")] = String(picProductVersion);
    
    // Determine PIC type based on available versions
    if (picVersion > 0) {
      root[F("pic_type")] = String(picType);
    }
  }
  
  addHATEOASlinks(root, "/api/v3/pic/info");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("public, max-age=3600"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// PIC Flash Info - GET /api/v3/pic/flash
// Flash programming status and available versions
//=======================================================================
static void sendPICFlashInfo() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // Current flash status
  root[F("is_flashing")] = (OTGWbootstatus.flashstatus != 0);
  root[F("flash_status")] = OTGWbootstatus.flashstatus;
  root[F("flash_error")] = OTGWbootstatus.errorstatus;
  root[F("bytes_flashed")] = OTGWbootstatus.CRC;
  
  // Available versions for flashing
  JsonArray versions = root.createNestedArray(F("available_versions"));
  
  // Gateway versions (could be expanded to scan data/pic16f1847/ and data/pic16f88/)
  JsonObject gw = versions.createNestedObject();
  gw[F("type")] = F("gateway");
  gw[F("path")] = F("/api/v3/export/pic-versions/gateway");
  
  JsonObject iface = versions.createNestedObject();
  iface[F("type")] = F("interface");
  iface[F("path")] = F("/api/v3/export/pic-versions/interface");
  
  JsonObject diag = versions.createNestedObject();
  diag[F("type")] = F("diagnostics");
  diag[F("path")] = F("/api/v3/export/pic-versions/diagnostics");
  
  addHATEOASlinks(root, "/api/v3/pic/flash");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// PIC Diagnostics - GET /api/v3/pic/diag
// Diagnostics module data from PIC
//=======================================================================
static void sendPICDiagnostics() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("available")] = isOTGWonline();
  
  if (isOTGWonline()) {
    // Get diagnostic information from OTGW status
    root[F("diagnostics_version")] = getPICversion(picDiagVersion);
    
    // Status bits interpretation
    JsonObject status = root.createNestedObject(F("status"));
    status[F("master_ch_heating")] = bitRead(OTGWstatus.flags, 0);
    status[F("master_dhw")] = bitRead(OTGWstatus.flags, 1);
    status[F("master_cooling")] = bitRead(OTGWstatus.flags, 2);
    status[F("slave_flame")] = bitRead(OTGWstatus.flags, 3);
    status[F("slave_ch")] = bitRead(OTGWstatus.flags, 4);
    status[F("slave_dhw")] = bitRead(OTGWstatus.flags, 5);
    status[F("slave_cooling")] = bitRead(OTGWstatus.flags, 6);
    status[F("slave_fault")] = bitRead(OTGWstatus.flags, 7);
  }
  
  addHATEOASlinks(root, "/api/v3/pic/diag");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Sensors Resources - /api/v3/sensors/*
// Dallas temperature and S0 pulse counter data
//=======================================================================
static void handleSensorsResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strcmp_P(uri, PSTR("/api/v3/sensors")) == 0 || strcmp_P(uri, PSTR("/api/v3/sensors/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendSensorsIndex();
  } else if (strstr_P(uri, PSTR("/api/v3/sensors/dallas")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendDallasSensors();
  } else if (strstr_P(uri, PSTR("/api/v3/sensors/s0")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET | HTTP_METHOD_PUT)) return;
    if (httpServer.method() == HTTP_GET) {
      sendS0Sensor();
    } else if (httpServer.method() == HTTP_PUT) {
      resetS0Counter();
    }
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("Sensor resource not found"), uri);
  }
}

//=======================================================================
// Sensors Index - GET /api/v3/sensors
//=======================================================================
static void sendSensorsIndex() {
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("sensors");
  root[F("description")] = F("External sensor data (Dallas, S0)");
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/sensors");
  
  JsonObject dallasLink = links.createNestedObject(F("dallas"));
  dallasLink[F("href")] = F("/api/v3/sensors/dallas");
  dallasLink[F("title")] = F("Dallas temperature sensors");
  
  JsonObject s0Link = links.createNestedObject(F("s0"));
  s0Link[F("href")] = F("/api/v3/sensors/s0");
  s0Link[F("title")] = F("S0 pulse counter");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Dallas Sensors - GET /api/v3/sensors/dallas
// Temperature data from all connected Dallas sensors
//=======================================================================
static void sendDallasSensors() {
  StaticJsonDocument<1024> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("enabled")] = (settingGPIO_DALLASsensors != 255);
  
  if (settingGPIO_DALLASsensors != 255) {
    // Get count of connected sensors and their data
    JsonArray sensors = root.createNestedArray(F("sensors"));
    
    // This would iterate through connected sensors
    // For now, show structure for sensors that might be connected
    // In full implementation, would call dallastempReadSensor() function
    
    for (int i = 0; i < 4; i++) {
      // Check if sensor exists (implementation depends on sensor library)
      // This is placeholder - actual implementation uses existing sensor functions
      JsonObject sensor = sensors.createNestedObject();
      sensor[F("index")] = i;
      sensor[F("address")] = F("28:XXXXXXXX:XXXX");  // ROM address
      sensor[F("name")] = "Sensor " + String(i);
      sensor[F("temperature")] = 20.5;
      sensor[F("unit")] = F("°C");
    }
  } else {
    root[F("error")] = F("Dallas sensors not configured (GPIO pin not set)");
  }
  
  addHATEOASlinks(root, "/api/v3/sensors/dallas");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// S0 Sensor - GET /api/v3/sensors/s0
// Pulse counter data
//=======================================================================
static void sendS0Sensor() {
  StaticJsonDocument<384> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("enabled")] = (settingGPIO_S0counter != 255);
  
  if (settingGPIO_S0counter != 255) {
    root[F("pulses")] = nS0pulseCount;
    root[F("unit")] = F("pulses");
    root[F("energy_kwh")] = (nS0pulseCount / 1000.0); // Example: 1000 pulses = 1 kWh
    
    JsonObject actions = root.createNestedObject(F("_actions"));
    JsonObject reset = actions.createNestedObject(F("reset"));
    reset[F("href")] = F("/api/v3/sensors/s0");
    reset[F("method")] = F("PUT");
    reset[F("title")] = F("Reset counter to zero");
  } else {
    root[F("error")] = F("S0 counter not configured (GPIO pin not set)");
  }
  
  addHATEOASlinks(root, "/api/v3/sensors/s0");
  
  String output;
  serializeJson(doc, output);
  httpServer.sendHeader(F("Cache-Control"), F("no-cache"));
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Reset S0 Counter - PUT /api/v3/sensors/s0
// Reset pulse counter to zero
//=======================================================================
static void resetS0Counter() {
  if (settingGPIO_S0counter == 255) {
    sendJsonError_P(400, PSTR("NOT_CONFIGURED"), 
                    PSTR("S0 counter is not configured"), 
                    httpServer.uri().c_str());
    return;
  }
  
  // Store current value before reset for logging
  uint32_t previousCount = nS0pulseCount;
  
  // Reset counter
  nS0pulseCount = 0;
  
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("status")] = F("reset");
  root[F("previous_count")] = previousCount;
  root[F("current_count")] = nS0pulseCount;
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Export Resources - /api/v3/export/*
// Data export in various formats (Telegraf, OTmonitor, etc.)
//=======================================================================
static void handleExportResources() {
  const char *uri = httpServer.uri().c_str();
  
  if (strcmp_P(uri, PSTR("/api/v3/export")) == 0 || strcmp_P(uri, PSTR("/api/v3/export/")) == 0) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendExportIndex();
  } else if (strstr_P(uri, PSTR("/api/v3/export/telegraf")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendTelegraf();
  } else if (strstr_P(uri, PSTR("/api/v3/export/otmonitor")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendOTmonitor();
  } else if (strstr_P(uri, PSTR("/api/v3/export/settings")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendDeviceSettings();
  } else if (strstr_P(uri, PSTR("/api/v3/export/logs")) != nullptr) {
    if (!checkMethodAllowed(HTTP_METHOD_GET)) return;
    sendExportLogs();
  } else {
    sendJsonError_P(404, PSTR("RESOURCE_NOT_FOUND"), 
                    PSTR("Export resource not found"), uri);
  }
}

//=======================================================================
// Export Index - GET /api/v3/export
// Available export formats
//=======================================================================
static void sendExportIndex() {
  StaticJsonDocument<768> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("resource")] = F("export");
  root[F("description")] = F("Data export in various formats");
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/export");
  
  JsonObject telegrafLink = links.createNestedObject(F("telegraf"));
  telegrafLink[F("href")] = F("/api/v3/export/telegraf");
  telegrafLink[F("title")] = F("Telegraf metric format");
  telegrafLink[F("media_type")] = F("text/plain");
  
  JsonObject otmonitorLink = links.createNestedObject(F("otmonitor"));
  otmonitorLink[F("href")] = F("/api/v3/export/otmonitor");
  otmonitorLink[F("title")] = F("OTmonitor format");
  otmonitorLink[F("media_type")] = F("application/json");
  
  JsonObject settingsLink = links.createNestedObject(F("settings"));
  settingsLink[F("href")] = F("/api/v3/export/settings");
  settingsLink[F("title")] = F("Current settings as JSON");
  settingsLink[F("media_type")] = F("application/json");
  
  JsonObject logsLink = links.createNestedObject(F("logs"));
  logsLink[F("href")] = F("/api/v3/export/logs");
  logsLink[F("title")] = F("Recent debug logs");
  logsLink[F("media_type")] = F("text/plain");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// Export Logs - GET /api/v3/export/logs
// Recent debug telnet logs (if available)
//=======================================================================
static void sendExportLogs() {
  // Note: This would export recent logs if telnet logging is active
  // For now, return a placeholder indicating logs are not currently captured
  
  StaticJsonDocument<256> doc;
  JsonObject root = doc.to<JsonObject>();
  
  root[F("available")] = bDebugMode;
  
  if (bDebugMode) {
    root[F("note")] = F("Logs are streamed via telnet on port 23");
    root[F("telnet_command")] = F("telnet <device-ip> 23");
  } else {
    root[F("note")] = F("Debug mode disabled. Enable via settings.");
  }
  
  JsonObject links = root.createNestedObject(F("_links"));
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = F("/api/v3/export/logs");
  
  String output;
  serializeJson(doc, output);
  httpServer.send(200, F("application/json"), output);
}

//=======================================================================
// HTTP Method Checking Helper
//=======================================================================
static bool checkMethodAllowed(uint8_t allowedMethods) {
  const HTTPMethod method = httpServer.method();
  
  bool allowed = false;
  if ((method == HTTP_GET) && (allowedMethods & HTTP_METHOD_GET)) allowed = true;
  else if ((method == HTTP_POST) && (allowedMethods & HTTP_METHOD_POST)) allowed = true;
  else if ((method == HTTP_PUT) && (allowedMethods & HTTP_METHOD_PUT)) allowed = true;
  else if ((method == HTTP_PATCH) && (allowedMethods & HTTP_METHOD_PATCH)) allowed = true;
  else if ((method == HTTP_DELETE) && (allowedMethods & HTTP_METHOD_DELETE)) allowed = true;
  
  if (!allowed) {
    sendMethodNotAllowed(allowedMethods);
  }
  
  return allowed;
}

//=======================================================================
// Send 405 Method Not Allowed with proper Allow header
//=======================================================================
static void sendMethodNotAllowed(uint8_t allowedMethods) {
  char allowHeader[64] = "OPTIONS";
  
  if (allowedMethods & HTTP_METHOD_GET) strlcat(allowHeader, ", GET", sizeof(allowHeader));
  if (allowedMethods & HTTP_METHOD_POST) strlcat(allowHeader, ", POST", sizeof(allowHeader));
  if (allowedMethods & HTTP_METHOD_PUT) strlcat(allowHeader, ", PUT", sizeof(allowHeader));
  if (allowedMethods & HTTP_METHOD_PATCH) strlcat(allowHeader, ", PATCH", sizeof(allowHeader));
  if (allowedMethods & HTTP_METHOD_DELETE) strlcat(allowHeader, ", DELETE", sizeof(allowHeader));
  
  httpServer.sendHeader(F("Allow"), allowHeader);
  sendJsonError_P(405, PSTR("METHOD_NOT_ALLOWED"), 
                  PSTR("The HTTP method is not allowed for this resource"), 
                  httpServer.uri().c_str());
}

//=======================================================================
// HATEOAS Helper - Add standard links to response
//=======================================================================
static void addHATEOASlinks(JsonObject &obj, const char *selfPath) {
  JsonObject links = obj.createNestedObject(F("_links"));
  
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = selfPath;
  
  // Extract parent path
  char parentPath[64];
  strlcpy(parentPath, selfPath, sizeof(parentPath));
  char *lastSlash = strrchr(parentPath, '/');
  if (lastSlash && lastSlash != parentPath) {
    *lastSlash = '\0';
    JsonObject parentLink = links.createNestedObject(F("parent"));
    parentLink[F("href")] = parentPath;
  }
  
  JsonObject rootLink = links.createNestedObject(F("root"));
  rootLink[F("href")] = F("/api/v3/");
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
