/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v1.0.0-rc3
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
    DebugT(">>");
    for (uint_fast8_t  w=0; w<wc; w++)
    {
      Debugf("word[%d] => [%s], ", w, words[w]);
    }
    Debugln(" ");
  }

  if (wc > 1 && strcmp(words[1], "api") == 0) {

    if (wc > 2 && strcmp(words[2], "v1") == 0)
    { //v1 API calls
      if (wc > 3 && strcmp(words[3], "otgw") == 0) {
        if (wc > 4 && strcmp(words[4], "telegraf") == 0) {
          // GET /api/v1/otgw/telegraf
          // Response: see json response
          if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
          sendTelegraf();
        } else if (wc > 4 && strcmp(words[4], "otmonitor") == 0) {
          // GET /api/v1/otgw/otmonitor
          // Response: see json response
          if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
          sendOTmonitor();
        } else if (wc > 4 && strcmp(words[4], "autoconfigure") == 0) {
          // POST /api/v1/otgw/autoconfigure
          // Response: sends all autodiscovery topics to MQTT for HA integration
          if (!isPostOrPut) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
          httpServer.send(200, "text/plain", "OK");
          doAutoConfigure();
        } else if (wc > 5 && strcmp(words[4], "id") == 0) {
          if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
          uint8_t msgId = 0;
          if (parseMsgId(words[5], msgId)) {
            sendOTGWvalue(msgId);
          } else {
            httpServer.send(400, "text/plain", "400: invalid msgid\r\n");
          }
        } else if (wc > 5 && strcmp(words[4], "label") == 0) {
          // GET /api/v1/otgw/label/{msglabel}
          if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
          if (words[5][0] == '\0') { httpServer.send(400, "text/plain", "400: missing label\r\n"); return; }
          sendOTGWlabel(words[5]);
        } else if (wc > 5 && strcmp(words[4], "command") == 0) {
          if (!isPostOrPut) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }

          if (words[5][0] == '\0') {
            httpServer.send(400, "text/plain", "400: missing command\r\n");
            return;
          }

          constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1; // matches OT_cmd_t::cmd buffer
          const size_t cmdLen = strlen(words[5]);
          if ((cmdLen < 3) || (words[5][2] != '=')) {
            httpServer.send(400, "text/plain", "400: invalid command format\r\n");
            return;
          }
          if (cmdLen > kMaxCmdLen) {
            httpServer.send(413, "text/plain", "413: command too long\r\n");
            return;
          }

          addOTWGcmdtoqueue(words[5], static_cast<int>(cmdLen));
          httpServer.send(200, "text/plain", "OK");
        } else {
          sendApiNotFound(originalURI);
        }
      } else {
        sendApiNotFound(originalURI);
      }
    }
    else if (wc > 2 && strcmp(words[2], "v0") == 0)
    { //v0 API calls
      if (wc > 3 && strcmp(words[3], "otgw") == 0) {
        // GET /api/v0/otgw/{msgid}
        if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
        uint8_t msgId = 0;
        if (wc > 4 && parseMsgId(words[4], msgId)) {
          sendOTGWvalue(msgId);
        } else {
          httpServer.send(400, "text/plain", "400: invalid msgid\r\n");
        }
      }
      else if (wc > 3 && strcmp(words[3], "devinfo") == 0) {
        if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
        sendDeviceInfo();
      }
      else if (wc > 3 && strcmp(words[3], "devtime") == 0) {
        if (!isGet) { httpServer.send(405, "text/plain", "405: method not allowed\r\n"); return; }
        sendDeviceTime();
      }
      else if (wc > 3 && strcmp(words[3], "settings") == 0) {
        if (isPostOrPut) {
          postSettings();
        } else if (isGet) {
          sendDeviceSettings();
        } else {
          httpServer.send(405, "text/plain", "405: method not allowed\r\n");
        }
      } else {
        sendApiNotFound(originalURI);
      }
    } else {
      sendApiNotFound(originalURI);
    }
  } else {
    sendApiNotFound(originalURI);
  }
} // processAPI()


//====[ implementing REST API ]====
void sendOTGWvalue(int msgid){
  StaticJsonDocument<256> doc;
  JsonObject root  = doc.to<JsonObject>();
  PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
  if (OTlookupitem.type==ot_undef) {  //message is undefined, return error
    root["error"] = "message undefined: reserved for future use";
  } else if (msgid>= 0 && msgid<= OT_MSGID_MAX) 
  { //message id's need to be between 0 and 127
    //Debug print the values first
    RESTDebugTf(PSTR("%s = %s %s\r\n"), OTlookupitem.label, getOTGWValue(msgid).c_str(), OTlookupitem.unit);
    //build the json
    root["label"] = OTlookupitem.label;
    if (OTlookupitem.type == ot_f88) {
      root["value"] = getOTGWValue(msgid).toFloat(); 
    } else {// all other message types convert to integer
      root["value"] = getOTGWValue(msgid).toInt();
    }
    root["unit"] = OTlookupitem.unit;    
  } else {
    root["error"] = "message id: reserved for future use";
  }
  String sBuff;
  serializeJsonPretty(root, sBuff);
  //RESTDebugTf(PSTR("Json = %s\r\n"), sBuff.c_str());
  //reply with json
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send(200, "application/json", sBuff);
}

void sendOTGWlabel(const char *msglabel){
  StaticJsonDocument<256> doc;
  JsonObject root  = doc.to<JsonObject>();
  uint_fast8_t msgid;
  for (msgid = 0; msgid<= OT_MSGID_MAX; msgid++){
    PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
    if (strcasecmp(OTlookupitem.label, msglabel)==0) break;
  }
  if (msgid > OT_MSGID_MAX){
    root["error"] = "message id: reserved for future use";
  } else if (OTlookupitem.type==ot_undef) {  //message is undefined, return error
    root["error"] = "message undefined: reserved for future use";
  } else 
  { //message id's need to be between 0 and OT_MSGID_MAX
    //RESTDebug print the values first
    RESTDebugTf(PSTR("%s = %s %s\r\n"), OTlookupitem.label, getOTGWValue(msgid).c_str(), OTlookupitem.unit);
    //build the json
    root["label"] = OTlookupitem.label;
    if (OTlookupitem.type == ot_f88) {
      root["value"] = getOTGWValue(msgid).toFloat(); 
    } else {// all other message types convert to integer
      root["value"] = getOTGWValue(msgid).toInt();
    }
    root["unit"] = OTlookupitem.unit;    
  } 
  String sBuff;
  serializeJsonPretty(root, sBuff);
  //RESTDebugTf(PSTR("Json = %s\r\n"), sBuff.c_str());
  //reply with json
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send(200, "application/json", sBuff);
}

void sendTelegraf() 
{
  RESTDebugTln(F("sending OT monitor values to Telegraf...\r"));

  sendStartJsonArray();
  
  sendJsonOTmonObj("flamestatus", isFlameStatus(), "", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("chmodus", isCentralHeatingActive(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("chenable", isCentralHeatingEnabled(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("ch2modus", isCentralHeating2Active(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("ch2enable", isCentralHeating2enabled(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("dhwmode", isDomesticHotWaterActive(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("dhwenable", isDomesticHotWaterEnabled(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("diagnosticindicator", isDiagnosticIndicator(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("faultindicator", isFaultIndicator(),"", msglastupdated[OT_Statusflags]);
  
  sendJsonOTmonObj("coolingmodus", isCoolingEnabled(),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("coolingactive", isCoolingActive(),"", msglastupdated[OT_Statusflags]);  
  sendJsonOTmonObj("otcactive", isOutsideTemperatureCompensationActive(),"", msglastupdated[OT_Statusflags]);

  sendJsonOTmonObj("servicerequest", isServiceRequest(),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("lockoutreset", isLockoutReset(),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("lowwaterpressure", isLowWaterPressure(),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("gasflamefault", isGasFlameFault(),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("airtemp", isAirTemperature(),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("waterovertemperature", isWaterOverTemperature(),"", msglastupdated[OT_ASFflags]);
  

  sendJsonOTmonObj("outsidetemperature", OTcurrentSystemState.Toutside, "°C", msglastupdated[OT_Toutside]);
  sendJsonOTmonObj("roomtemperature", OTcurrentSystemState.Tr, "°C", msglastupdated[OT_Tr]);
  sendJsonOTmonObj("roomsetpoint", OTcurrentSystemState.TrSet, "°C", msglastupdated[OT_TrSet]);
  sendJsonOTmonObj("remoteroomsetpoint", OTcurrentSystemState.TrOverride, "°C", msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj("controlsetpoint", OTcurrentSystemState.TSet,"°C", msglastupdated[OT_TSet]);
  sendJsonOTmonObj("relmodlvl", OTcurrentSystemState.RelModLevel,"%", msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj("maxrelmodlvl", OTcurrentSystemState.MaxRelModLevelSetting, "%", msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj("boilertemperature", OTcurrentSystemState.Tboiler, "°C", msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj("returnwatertemperature", OTcurrentSystemState.Tret,"°C", msglastupdated[OT_Tret]);
  sendJsonOTmonObj("dhwtemperature", OTcurrentSystemState.Tdhw,"°C", msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj("dhwsetpoint", OTcurrentSystemState.TdhwSet,"°C", msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj("maxchwatersetpoint", OTcurrentSystemState.MaxTSet,"°C", msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj("chwaterpressure", OTcurrentSystemState.CHPressure, "bar", msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj("oemfaultcode", OTcurrentSystemState.OEMDiagnosticCode, "", msglastupdated[OT_OEMDiagnosticCode]);

  sendEndJsonArray();

} // sendTelegraf()
//=======================================================================

void sendOTmonitor() 
{
  time_t now = time(nullptr); // needed for Dallas sensor display
  RESTDebugTln(F("sending OT monitor values ...\r"));

  sendStartJsonObj("otmonitor");

  // sendJsonOTmonObj("status hb", byte_to_binary((OTcurrentSystemState.Statusflags>>8) & 0xFF),"", msglastupdated[OT_Statusflags]);
  // sendJsonOTmonObj("status lb", byte_to_binary(OTcurrentSystemState.Statusflags & 0xFF),"", msglastupdated[OT_Statusflags]);

  sendJsonOTmonObj("flamestatus", CONOFF(isFlameStatus()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("chmodus", CONOFF(isCentralHeatingActive()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("chenable", CONOFF(isCentralHeatingEnabled()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("ch2modus", CONOFF(isCentralHeating2Active()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("ch2enable", CONOFF(isCentralHeating2enabled()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("dhwmode", CONOFF(isDomesticHotWaterActive()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("dhwenable", CONOFF(isDomesticHotWaterEnabled()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("diagnosticindicator", CONOFF(isDiagnosticIndicator()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("faultindicator", CONOFF(isFaultIndicator()),"", msglastupdated[OT_Statusflags]);
  
  sendJsonOTmonObj("coolingmodus", CONOFF(isCoolingEnabled()),"", msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj("coolingactive", CONOFF(isCoolingActive()),"", msglastupdated[OT_Statusflags]);  
  sendJsonOTmonObj("otcactive", CONOFF(isOutsideTemperatureCompensationActive()),"", msglastupdated[OT_Statusflags]);

  sendJsonOTmonObj("servicerequest", CONOFF(isServiceRequest()),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("lockoutreset", CONOFF(isLockoutReset()),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("lowwaterpressure", CONOFF(isLowWaterPressure()),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("gasflamefault", CONOFF(isGasFlameFault()),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("airtemp", CONOFF(isAirTemperature()),"", msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj("waterovertemperature", CONOFF(isWaterOverTemperature()),"", msglastupdated[OT_ASFflags]);
  

  sendJsonOTmonObj("outsidetemperature", OTcurrentSystemState.Toutside, "°C", msglastupdated[OT_Toutside]);
  sendJsonOTmonObj("roomtemperature", OTcurrentSystemState.Tr, "°C", msglastupdated[OT_Tr]);
  sendJsonOTmonObj("roomsetpoint", OTcurrentSystemState.TrSet, "°C", msglastupdated[OT_TrSet]);
  sendJsonOTmonObj("remoteroomsetpoint", OTcurrentSystemState.TrOverride, "°C", msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj("controlsetpoint", OTcurrentSystemState.TSet,"°C", msglastupdated[OT_TSet]);
  sendJsonOTmonObj("relmodlvl", OTcurrentSystemState.RelModLevel,"%", msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj("maxrelmodlvl", OTcurrentSystemState.MaxRelModLevelSetting, "%", msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj("boilertemperature", OTcurrentSystemState.Tboiler, "°C", msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj("returnwatertemperature", OTcurrentSystemState.Tret,"°C", msglastupdated[OT_Tret]);
  sendJsonOTmonObj("dhwtemperature", OTcurrentSystemState.Tdhw,"°C", msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj("dhwsetpoint", OTcurrentSystemState.TdhwSet,"°C", msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj("maxchwatersetpoint", OTcurrentSystemState.MaxTSet,"°C", msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj("chwaterpressure", OTcurrentSystemState.CHPressure, "bar", msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj("oemdiagnosticcode", OTcurrentSystemState.OEMDiagnosticCode, "", msglastupdated[OT_OEMDiagnosticCode]);
  sendJsonOTmonObj("oemfaultcode", OTcurrentSystemState.ASFflags & 0xFF, "", msglastupdated[OT_ASFflags]);

  if (settingS0COUNTERenabled) 
  {
    sendJsonOTmonObj("s0powerkw", OTGWs0powerkw , "kW", OTGWs0lasttime);
    sendJsonOTmonObj("s0intervalcount", OTGWs0pulseCount , "", OTGWs0lasttime);
    sendJsonOTmonObj("s0totalcount", OTGWs0pulseCountTot , "", OTGWs0lasttime);
  }
  if (settingGPIOSENSORSenabled) 
  {
    sendJsonOTmonObj("numberofsensors", DallasrealDeviceCount , "", now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      char buf[16];
      snprintf(buf, sizeof(buf), "%.1f", DallasrealDevice[i].tempC);
      sendJsonOTmonObj(strDeviceAddress, buf, "°C", DallasrealDevice[i].lasttime);
    }
  }

  sendEndJsonObj("otmonitor");

} // sendOTmonitor()

//=======================================================================
void sendDeviceInfo() 
{
  sendStartJsonObj("devinfo");

  sendNestedJsonObj("author", "Robert van den Breemen");
  sendNestedJsonObj("fwversion", _SEMVER_FULL);
  sendNestedJsonObj("picavailable", CBOOLEAN(bPICavailable));
  sendNestedJsonObj("picfwversion", sPICfwversion);
  sendNestedJsonObj("picdeviceid", sPICdeviceid);
  sendNestedJsonObj("picfwtype", sPICtype);
  snprintf(cMsg, sizeof(cMsg), "%s %s", __DATE__, __TIME__);
  sendNestedJsonObj("compiled", cMsg);
  sendNestedJsonObj("hostname", CSTR(settingHostname));
  sendNestedJsonObj("ipaddress", CSTR(WiFi.localIP().toString()));
  sendNestedJsonObj("macaddress", CSTR(WiFi.macAddress()));
  sendNestedJsonObj("freeheap", ESP.getFreeHeap());
  sendNestedJsonObj("maxfreeblock", ESP.getMaxFreeBlockSize());
  sendNestedJsonObj("chipid", CSTR(String( ESP.getChipId(), HEX )));
  sendNestedJsonObj("coreversion", CSTR(ESP.getCoreVersion()) );
  sendNestedJsonObj("sdkversion",  ESP.getSdkVersion());
  sendNestedJsonObj("cpufreq", ESP.getCpuFreqMHz());
  sendNestedJsonObj("sketchsize", ESP.getSketchSize() );
  sendNestedJsonObj("freesketchspace",  ESP.getFreeSketchSpace() );

  snprintf(cMsg, sizeof(cMsg), "%08X", ESP.getFlashChipId());
  sendNestedJsonObj("flashchipid", cMsg);  // flashChipId
  sendNestedJsonObj("flashchipsize", (ESP.getFlashChipSize() / 1024.0f / 1024.0f));
  sendNestedJsonObj("flashchiprealsize", (ESP.getFlashChipRealSize() / 1024.0f / 1024.0f));

  LittleFS.info(LittleFSinfo);
  sendNestedJsonObj("LittleFSsize", floorf((LittleFSinfo.totalBytes / (1024.0f * 1024.0f))));

  sendNestedJsonObj("flashchipspeed", floorf((ESP.getFlashChipSpeed() / 1000.0f / 1000.0f)));

  FlashMode_t ideMode = ESP.getFlashChipMode();
  sendNestedJsonObj("flashchipmode", flashMode[ideMode]);
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
  sendNestedJsonObj("ssid", CSTR(WiFi.SSID()));
  sendNestedJsonObj("wifirssi", WiFi.RSSI());
  sendNestedJsonObj("wifiquality", signal_quality_perc_quad(WiFi.RSSI()));
  sendNestedJsonObj("wifiqualitytldr", dBmtoQuality(WiFi.RSSI()));
  sendNestedJsonObj("ntpenable", String(CBOOLEAN(settingNTPenable)));
  sendNestedJsonObj("ntptimezone", CSTR(settingNTPtimezone));
  sendNestedJsonObj("uptime", upTime());
  sendNestedJsonObj("lastreset", lastReset);
  sendNestedJsonObj("bootcount", rebootCount);
  sendNestedJsonObj("mqttconnected", String(CBOOLEAN(statusMQTTconnection)));
  sendNestedJsonObj("thermostatconnected", CBOOLEAN(bOTGWthermostatstate));
  sendNestedJsonObj("boilerconnected", CBOOLEAN(bOTGWboilerstate));      
  sendNestedJsonObj("gatewaymode", CBOOLEAN(bOTGWgatewaystate));      
  sendNestedJsonObj("otgwconnected", CBOOLEAN(bOTGWonline));
  
  sendEndJsonObj("devinfo");

} // sendDeviceInfo()


//=======================================================================
void sendDeviceTime() 
{
  char buf[50];
  
  sendStartJsonObj("devtime");
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf(buf, 49, PSTR("%04d-%02d-%02d %02d:%02d:%02d"), myTime.year(), myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second());
  sendNestedJsonObj("dateTime", buf); 
  sendNestedJsonObj("epoch", (int)now);
  sendNestedJsonObj("message", sMessage);

  sendEndJsonObj("devtime");

} // sendDeviceTime()

//=======================================================================
void sendDeviceSettings() 
{
  RESTDebugTln(F("sending device settings ...\r"));

  sendStartJsonObj("settings");

  //sendJsonSettingObj("string",   settingString,   "p", sizeof(settingString)-1);  
  //sendJsonSettingObj("string",   settingString,   "s", sizeof(settingString)-1);
  //sendJsonSettingObj("float",    settingFloat,    "f", 0, 10,  5);
  //sendJsonSettingObj("intager",  settingInteger , "i", 2, 60);

  sendJsonSettingObj("hostname", CSTR(settingHostname), "s", 32);
  sendJsonSettingObj("mqttenable", settingMQTTenable, "b");
  sendJsonSettingObj("mqttbroker", CSTR(settingMQTTbroker), "s", 32);
  sendJsonSettingObj("mqttbrokerport", settingMQTTbrokerPort, "i", 0, 65535);
  sendJsonSettingObj("mqttuser", CSTR(settingMQTTuser), "s", 32);
  sendJsonSettingObj("mqttpasswd", "notthepassword", "p", 100);
  sendJsonSettingObj("mqtttoptopic", CSTR(settingMQTTtopTopic), "s", 15);
  sendJsonSettingObj("mqtthaprefix", CSTR(settingMQTThaprefix), "s", 20);
  sendJsonSettingObj("mqttharebootdetection", settingMQTTharebootdetection, "b");
  sendJsonSettingObj("mqttuniqueid", CSTR(settingMQTTuniqueid), "s", 20);
  sendJsonSettingObj("mqttotmessage", settingMQTTOTmessage, "b");
  sendJsonSettingObj("ntpenable", settingNTPenable, "b");
  sendJsonSettingObj("ntptimezone", CSTR(settingNTPtimezone), "s", 50);
  sendJsonSettingObj("ntphostname", CSTR(settingNTPhostname), "s", 50);
  sendJsonSettingObj("ntpsendtime", settingNTPsendtime, "b");
  sendJsonSettingObj("ledblink", settingLEDblink, "b");
  sendJsonSettingObj("darktheme", settingDarkTheme, "b");
  sendJsonSettingObj("ui_autoscroll", settingUIAutoScroll, "b");
  sendJsonSettingObj("ui_timestamps", settingUIShowTimestamp, "b");
  sendJsonSettingObj("ui_capture", settingUICaptureMode, "b");
  sendJsonSettingObj("ui_autoscreenshot", settingUIAutoScreenshot, "b");
  sendJsonSettingObj("ui_autodownloadlog", settingUIAutoDownloadLog, "b");
  sendJsonSettingObj("ui_graphtimewindow", settingUIGraphTimeWindow, "i", 0, 1440);
  sendJsonSettingObj("gpiosensorsenabled", settingGPIOSENSORSenabled, "b");
  sendJsonSettingObj("gpiosensorspin", settingGPIOSENSORSpin, "i", 0, 16);
  sendJsonSettingObj("gpiosensorsinterval", settingGPIOSENSORSinterval, "i", 5, 65535);
  sendJsonSettingObj("s0counterenabled", settingS0COUNTERenabled, "b");
  sendJsonSettingObj("s0counterpin", settingS0COUNTERpin, "i", 1, 16);
  sendJsonSettingObj("s0counterdebouncetime", settingS0COUNTERdebouncetime, "i", 0, 1000);
  sendJsonSettingObj("s0counterpulsekw", settingS0COUNTERpulsekw, "i", 1, 5000);
  sendJsonSettingObj("s0counterinterval", settingS0COUNTERinterval, "i", 5, 65535);
  sendJsonSettingObj("gpiooutputsenabled", settingGPIOOUTPUTSenabled, "b");
  sendJsonSettingObj("gpiooutputspin", settingGPIOOUTPUTSpin, "i", 0, 16);
  sendJsonSettingObj("gpiooutputstriggerbit", settingGPIOOUTPUTStriggerBit, "i", 0,16);
  sendJsonSettingObj("otgwcommandenable", settingOTGWcommandenable, "b");
  sendJsonSettingObj("otgwcommands", CSTR(settingOTGWcommands), "s", 128);

  sendEndJsonObj("settings");

} // sendDeviceSettings()


//=======================================================================
void postSettings()
{
  //------------------------------------------------------------ 
  // json string: {"name":"settingInterval","value":9}  
  // json string: {"name":"settingHostname","value":"abc"}  
  //------------------------------------------------------------ 
  // so, why not use ArduinoJSON library?
  // I say: try it yourself ;-) It won't be easy
      char* wPair[5];
      String jsonInStr  = CSTR(httpServer.arg(0));
      char field[25] = {0,};
      char newValue[101]={0,};
      jsonInStr.replace("{", "");
      jsonInStr.replace("}", "");
      jsonInStr.replace("\"", "");
      
      char jsonIn[jsonInStr.length() + 1];
      strcpy(jsonIn, jsonInStr.c_str());

      uint_fast8_t wp = splitString(jsonIn, ',',  wPair, 5) ;
	      for (uint_fast8_t i=0; i<wp; i++)
	      {
	        char* wOut[5];
	        //RESTDebugTf(PSTR("[%d] -> pair[%s]\r\n"), i, wPair[i]);
	        uint8_t wc = splitString(wPair[i], ':',  wOut, 5) ;
	        //RESTDebugTf(PSTR("==> [%s] -> field[%s]->val[%s]\r\n"), wPair[i], wOut[0], wOut[1]);
	        if (wc>1) {
	            if (wOut[0] && strcasecmp(wOut[0], "name") == 0) {
	              if (wOut[1] && (strlen(wOut[1]) < (sizeof(field) - 1))) {
	                strlcpy(field, wOut[1], sizeof(field));
	              }
	            }
	            else if (wOut[0] && strcasecmp(wOut[0], "value") == 0) {
	              if (wOut[1] && (strlen(wOut[1]) < (sizeof(newValue) - 1))) {
	                strlcpy(newValue, wOut[1], sizeof(newValue));
	              }
	            }
	        }
	      }
      if ( field[0] != 0 && newValue[0] != 0 ) {
        RESTDebugTf(PSTR("--> field[%s] => newValue[%s]\r\n"), field, newValue);
        updateSetting(field, newValue);
        httpServer.send(200, "application/json", httpServer.arg(0));
      } else {
        // Internal client error? It could not proess the client request.
        httpServer.send(400, "application/json", httpServer.arg(0));
      }

} // postSettings()


//====================================================
void sendApiNotFound(const char *URI)
{
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send ( 404, "text/html", "<!DOCTYPE HTML><html><head>");

  strlcpy(cMsg, "<style>body { background-color: lightgray; font-size: 15pt;}", sizeof(cMsg));
  strlcat(cMsg,  "</style></head><body>", sizeof(cMsg));
  httpServer.sendContent(cMsg);

  strlcpy(cMsg, "<h1>OTGW firmware</h1><b1>", sizeof(cMsg));
  httpServer.sendContent(cMsg);

  strlcpy(cMsg, "<br>[<b>", sizeof(cMsg));
  strlcat(cMsg, URI, sizeof(cMsg));
  strlcat(cMsg, "</b>] is not a valid ", sizeof(cMsg));
  httpServer.sendContent(cMsg);
  
  strlcpy(cMsg, "</body></html>\r\n", sizeof(cMsg));
  httpServer.sendContent(cMsg);

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
