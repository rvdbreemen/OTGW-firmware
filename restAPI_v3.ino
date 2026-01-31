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
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  JsonObject network = links.createNestedObject(F("network"));
  network[F("href")] = F("/api/v3/config/network");
  network[F("description")] = F("Network configuration");
  
  JsonObject ntp = links.createNestedObject(F("ntp"));
  ntp[F("href")] = F("/api/v3/config/ntp");
  ntp[F("description")] = F("NTP time configuration");
  
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
  
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
}

//=======================================================================
// Main v3 API router - called from processAPI() in restAPI.ino
//=======================================================================
void processAPIv3(char words[][32], uint8_t wc, const char *originalURI) {
  const HTTPMethod method = httpServer.method();
  const bool isGet = (method == HTTP_GET);
  const bool isPost = (method == HTTP_POST);
  
  // GET /api/v3 - API root
  if (wc == 3) {
    if (!isGet) { 
      httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
      return; 
    }
    sendV3Root();
    return;
  }
  
  // /api/v3/system/*
  if (wc > 3 && strcmp_P(words[3], PSTR("system")) == 0) {
    if (!isGet) { 
      httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
      return; 
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("health")) == 0) {
      sendV3SystemHealth();
    } else if (wc > 4 && strcmp_P(words[4], PSTR("info")) == 0) {
      sendV3SystemInfo();
    } else {
      sendV3Error(404, "Not found", originalURI);
    }
    return;
  }
  
  // /api/v3/otgw/*
  if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
    if (wc == 4) {
      if (!isGet) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      sendV3OTGWIndex();
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("status")) == 0) {
      if (!isGet) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      sendV3OTGWStatus();
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("messages")) == 0) {
      if (!isGet) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      
      if (wc > 5) {
        // GET /api/v3/otgw/messages/{id}
        uint8_t msgId = 0;
        if (parseMsgId(words[5], msgId)) {
          sendV3OTGWMessage(msgId);
        } else {
          sendV3Error(400, "Invalid message ID");
        }
      } else {
        // GET /api/v3/otgw/messages - list all messages (future implementation)
        sendV3Error(501, "Not implemented", "Use /api/v3/otgw/messages/{id}");
      }
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("command")) == 0) {
      if (!isPost) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      sendV3OTGWCommand();
      return;
    }
    
    sendV3Error(404, "Not found", originalURI);
    return;
  }
  
  // /api/v3/config/*
  if (wc > 3 && strcmp_P(words[3], PSTR("config")) == 0) {
    if (wc == 4) {
      if (!isGet) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      sendV3ConfigIndex();
      return;
    }
    
    if (wc > 4 && strcmp_P(words[4], PSTR("mqtt")) == 0) {
      if (!isGet) { 
        httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
        return; 
      }
      sendV3ConfigMQTT();
      return;
    }
    
    sendV3Error(404, "Not found", originalURI);
    return;
  }
  
  // /api/v3/sensors/*
  if (wc > 3 && strcmp_P(words[3], PSTR("sensors")) == 0) {
    if (!isGet) { 
      httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); 
      return; 
    }
    
    if (wc == 4) {
      sendV3SensorsIndex();
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
