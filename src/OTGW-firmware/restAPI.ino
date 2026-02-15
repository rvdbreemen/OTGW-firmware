/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v1.0.1
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
  constexpr uint8_t MAX_WORDS = 10;
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

    if (wc > 2 && strcmp_P(words[2], PSTR("v1")) == 0)
    { //v1 API calls
      if (wc > 3 && strcmp_P(words[3], PSTR("health")) == 0) {
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        sendHealth();
      } else if (wc > 3 && strcmp_P(words[3], PSTR("devtime")) == 0) {
        // GET /api/v1/devtime - Map format version
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        sendDeviceTimeV2();
      } else if (wc > 3 && strcmp_P(words[3], PSTR("flashstatus")) == 0) {
        // GET /api/v1/flashstatus - Unified flash status for both ESP and PIC
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        sendFlashStatus();
      } else if (wc > 3 && strcmp_P(words[3], PSTR("settings")) == 0) {
        if (isPostOrPut) {
          postSettings();
        } else if (isGet) {
          sendDeviceSettings();
        } else {
          httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n"));
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("pic")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("flashstatus")) == 0) {
          // GET /api/v1/pic/flashstatus
          // Minimal endpoint for polling PIC flash state during upgrade
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          sendPICFlashStatus();
        } else {
          sendApiNotFound(originalURI);
        }
      } else if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
        if (wc > 4 && strcmp_P(words[4], PSTR("telegraf")) == 0) {
          // GET /api/v1/otgw/telegraf
          // Response: see json response
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          sendTelegraf();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("otmonitor")) == 0) {
          // GET /api/v1/otgw/otmonitor
          // Response: see json response
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          sendOTmonitor();
        } else if (wc > 4 && strcmp_P(words[4], PSTR("autoconfigure")) == 0) {
          // POST /api/v1/otgw/autoconfigure
          // Response: sends all autodiscovery topics to MQTT for HA integration
          if (!isPostOrPut) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          httpServer.send_P(200, PSTR("text/plain"), PSTR("OK"));
          doAutoConfigure();
        } else if (wc > 5 && strcmp_P(words[4], PSTR("id")) == 0) {
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          uint8_t msgId = 0;
          if (parseMsgId(words[5], msgId)) {
            sendOTGWvalue(msgId);
          } else {
            httpServer.send_P(400, PSTR("text/plain"), PSTR("400: invalid msgid\r\n"));
          }
        } else if (wc > 5 && strcmp_P(words[4], PSTR("label")) == 0) {
          // GET /api/v1/otgw/label/{msglabel}
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          if (words[5][0] == '\0') { httpServer.send_P(400, PSTR("text/plain"), PSTR("400: missing label\r\n")); return; }
          sendOTGWlabel(words[5]);
        } else if (wc > 5 && strcmp_P(words[4], PSTR("command")) == 0) {
          if (!isPostOrPut) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }

          if (words[5][0] == '\0') {
            httpServer.send_P(400, PSTR("text/plain"), PSTR("400: missing command\r\n"));
            return;
          }

          constexpr size_t kMaxCmdLen = sizeof(cmdqueue[0].cmd) - 1; // matches OT_cmd_t::cmd buffer
          const size_t cmdLen = strlen(words[5]);
          if ((cmdLen < 3) || (words[5][2] != '=')) {
            httpServer.send_P(400, PSTR("text/plain"), PSTR("400: invalid command format\r\n"));
            return;
          }
          if (cmdLen > kMaxCmdLen) {
            httpServer.send_P(413, PSTR("text/plain"), PSTR("413: command too long\r\n"));
            return;
          }

          addOTWGcmdtoqueue(words[5], static_cast<int>(cmdLen));
          httpServer.send_P(200, PSTR("text/plain"), PSTR("OK"));
        } else {
          sendApiNotFound(originalURI);
        }
      } else {
        sendApiNotFound(originalURI);
      }
    }
    else if (wc > 2 && strcmp_P(words[2], PSTR("v2")) == 0)
    { //v2 API calls
      if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0 && wc > 4 && strcmp_P(words[4], PSTR("otmonitor")) == 0) {
          // GET /api/v2/otgw/otmonitor
          if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
          sendOTmonitorV2();
      } else {
          sendApiNotFound(originalURI);
      }
    }
    else if (wc > 2 && strcmp_P(words[2], PSTR("v0")) == 0)
    { //v0 API calls
      if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
        // GET /api/v0/otgw/{msgid}
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        uint8_t msgId = 0;
        if (wc > 4 && parseMsgId(words[4], msgId)) {
          sendOTGWvalue(msgId);
        } else {
          httpServer.send_P(400, PSTR("text/plain"), PSTR("400: invalid msgid\r\n"));
        }
      }
      else if (wc > 3 && strcmp_P(words[3], PSTR("devinfo")) == 0) {
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        sendDeviceInfo();
      }
      else if (wc > 3 && strcmp_P(words[3], PSTR("devtime")) == 0) {
        if (!isGet) { httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n")); return; }
        sendDeviceTime();
      }
      else if (wc > 3 && strcmp_P(words[3], PSTR("settings")) == 0) {
        if (isPostOrPut) {
          postSettings();
        } else if (isGet) {
          sendDeviceSettings();
        } else {
          httpServer.send_P(405, PSTR("text/plain"), PSTR("405: method not allowed\r\n"));
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
    root[F("error")] = "message undefined: reserved for future use";
  } else if (msgid>= 0 && msgid<= OT_MSGID_MAX) 
  { //message id's need to be between 0 and 127
    //Debug print the values first
    RESTDebugTf(PSTR("%s = %s %s\r\n"), OTlookupitem.label, getOTGWValue(msgid), OTlookupitem.unit);
    //build the json
    root[F("label")] = OTlookupitem.label;
    if (OTlookupitem.type == ot_f88) {
      root[F("value")] = atof(getOTGWValue(msgid)); 
    } else {// all other message types convert to integer
      root[F("value")] = atoi(getOTGWValue(msgid));
    }
    root[F("unit")] = OTlookupitem.unit;    
  } else {
    root[F("error")] = "message id: reserved for future use";
  }
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));
  //RESTDebugTf(PSTR("Json = %s\r\n"), sBuff);
  //reply with json
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(200, F("application/json"), sBuff);
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
    root[F("error")] = "message id: reserved for future use";
  } else if (OTlookupitem.type==ot_undef) {  //message is undefined, return error
    root[F("error")] = "message undefined: reserved for future use";
  } else 
  { //message id's need to be between 0 and OT_MSGID_MAX
    //RESTDebug print the values first
    RESTDebugTf(PSTR("%s = %s %s\r\n"), OTlookupitem.label, getOTGWValue(msgid), OTlookupitem.unit);
    //build the json
    root[F("label")] = OTlookupitem.label;
    if (OTlookupitem.type == ot_f88) {
      root[F("value")] = atof(getOTGWValue(msgid)); 
    } else {// all other message types convert to integer
      root[F("value")] = atoi(getOTGWValue(msgid));
    }
    root[F("unit")] = OTlookupitem.unit;    
  } 
  char sBuff[JSON_BUFF_MAX];
  serializeJsonPretty(root, sBuff, sizeof(sBuff));
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

void sendTelegraf() 
{
  RESTDebugTln(F("sending OT monitor values to Telegraf...\r"));

  sendStartJsonArray();
  
  sendJsonOTmonObj(F("flamestatus"), isFlameStatus(), F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("chmodus"), isCentralHeatingActive(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("chenable"), isCentralHeatingEnabled(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("ch2modus"), isCentralHeating2Active(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("ch2enable"), isCentralHeating2enabled(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("dhwmode"), isDomesticHotWaterActive(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("dhwenable"), isDomesticHotWaterEnabled(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("diagnosticindicator"), isDiagnosticIndicator(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("faultindicator"), isFaultIndicator(),F(""), msglastupdated[OT_Statusflags]);
  
  sendJsonOTmonObj(F("coolingmodus"), isCoolingEnabled(),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("coolingactive"), isCoolingActive(),F(""), msglastupdated[OT_Statusflags]);  
  sendJsonOTmonObj(F("otcactive"), isOutsideTemperatureCompensationActive(),F(""), msglastupdated[OT_Statusflags]);

  sendJsonOTmonObj(F("servicerequest"), isServiceRequest(),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("lockoutreset"), isLockoutReset(),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("lowwaterpressure"), isLowWaterPressure(),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("gasflamefault"), isGasFlameFault(),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("airtemp"), isAirTemperature(),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("waterovertemperature"), isWaterOverTemperature(),F(""), msglastupdated[OT_ASFflags]);
  

  sendJsonOTmonObj(F("outsidetemperature"), OTcurrentSystemState.Toutside, F("°C"), msglastupdated[OT_Toutside]);
  sendJsonOTmonObj(F("roomtemperature"), OTcurrentSystemState.Tr, F("°C"), msglastupdated[OT_Tr]);
  sendJsonOTmonObj(F("roomsetpoint"), OTcurrentSystemState.TrSet, F("°C"), msglastupdated[OT_TrSet]);
  sendJsonOTmonObj(F("remoteroomsetpoint"), OTcurrentSystemState.TrOverride, F("°C"), msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj(F("controlsetpoint"), OTcurrentSystemState.TSet,F("°C"), msglastupdated[OT_TSet]);
  sendJsonOTmonObj(F("relmodlvl"), OTcurrentSystemState.RelModLevel,F("%"), msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj(F("maxrelmodlvl"), OTcurrentSystemState.MaxRelModLevelSetting, F("%"), msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj(F("boilertemperature"), OTcurrentSystemState.Tboiler, F("°C"), msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj(F("returnwatertemperature"), OTcurrentSystemState.Tret,F("°C"), msglastupdated[OT_Tret]);
  sendJsonOTmonObj(F("dhwtemperature"), OTcurrentSystemState.Tdhw,F("°C"), msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj(F("dhwsetpoint"), OTcurrentSystemState.TdhwSet,F("°C"), msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj(F("maxchwatersetpoint"), OTcurrentSystemState.MaxTSet,F("°C"), msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj(F("chwaterpressure"), OTcurrentSystemState.CHPressure, F("bar"), msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj(F("oemfaultcode"), OTcurrentSystemState.OEMDiagnosticCode, F(""), msglastupdated[OT_OEMDiagnosticCode]);

  sendEndJsonArray();

} // sendTelegraf()
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
  if (settingGPIOSENSORSenabled) 
  {
    sendJsonOTmonMapEntry(F("numberofsensors"), DallasrealDeviceCount , F(""), now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      sendJsonOTmonMapEntry(strDeviceAddress, DallasrealDevice[i].tempC, F("°C"), DallasrealDevice[i].lasttime);
    }
  }

  sendEndJsonMap(F("otmonitor"));
}

void sendOTmonitor() 
{
  time_t now = time(nullptr); // needed for Dallas sensor display
  RESTDebugTln(F("sending OT monitor values (V1)...\r"));

  sendStartJsonObj(F("otmonitor"));

  sendJsonOTmonObj(F("flamestatus"), CONOFF(isFlameStatus()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("chmodus"), CONOFF(isCentralHeatingActive()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("chenable"), CONOFF(isCentralHeatingEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("ch2modus"), CONOFF(isCentralHeating2Active()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("ch2enable"), CONOFF(isCentralHeating2enabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("dhwmode"), CONOFF(isDomesticHotWaterActive()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("dhwenable"), CONOFF(isDomesticHotWaterEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("diagnosticindicator"), CONOFF(isDiagnosticIndicator()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("faultindicator"), CONOFF(isFaultIndicator()),F(""), msglastupdated[OT_Statusflags]);
  
  sendJsonOTmonObj(F("coolingmodus"), CONOFF(isCoolingEnabled()),F(""), msglastupdated[OT_Statusflags]);
  sendJsonOTmonObj(F("coolingactive"), CONOFF(isCoolingActive()),F(""), msglastupdated[OT_Statusflags]);  
  sendJsonOTmonObj(F("otcactive"), CONOFF(isOutsideTemperatureCompensationActive()),F(""), msglastupdated[OT_Statusflags]);

  sendJsonOTmonObj(F("servicerequest"), CONOFF(isServiceRequest()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("lockoutreset"), CONOFF(isLockoutReset()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("lowwaterpressure"), CONOFF(isLowWaterPressure()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("gasflamefault"), CONOFF(isGasFlameFault()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("airtemp"), CONOFF(isAirTemperature()),F(""), msglastupdated[OT_ASFflags]);
  sendJsonOTmonObj(F("waterovertemperature"), CONOFF(isWaterOverTemperature()),F(""), msglastupdated[OT_ASFflags]);
  

  sendJsonOTmonObj(F("outsidetemperature"), OTcurrentSystemState.Toutside, F("°C"), msglastupdated[OT_Toutside]);
  sendJsonOTmonObj(F("roomtemperature"), OTcurrentSystemState.Tr, F("°C"), msglastupdated[OT_Tr]);
  sendJsonOTmonObj(F("roomsetpoint"), OTcurrentSystemState.TrSet, F("°C"), msglastupdated[OT_TrSet]);
  sendJsonOTmonObj(F("remoteroomsetpoint"), OTcurrentSystemState.TrOverride, F("°C"), msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj(F("controlsetpoint"), OTcurrentSystemState.TSet,F("°C"), msglastupdated[OT_TSet]);
  sendJsonOTmonObj(F("relmodlvl"), OTcurrentSystemState.RelModLevel,F("%"), msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj(F("maxrelmodlvl"), OTcurrentSystemState.MaxRelModLevelSetting, F("%"), msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj(F("boilertemperature"), OTcurrentSystemState.Tboiler, F("°C"), msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj(F("returnwatertemperature"), OTcurrentSystemState.Tret,F("°C"), msglastupdated[OT_Tret]);
  sendJsonOTmonObj(F("dhwtemperature"), OTcurrentSystemState.Tdhw,F("°C"), msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj(F("dhwsetpoint"), OTcurrentSystemState.TdhwSet,F("°C"), msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj(F("maxchwatersetpoint"), OTcurrentSystemState.MaxTSet,F("°C"), msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj(F("chwaterpressure"), OTcurrentSystemState.CHPressure, F("bar"), msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj(F("oemdiagnosticcode"), OTcurrentSystemState.OEMDiagnosticCode, F(""), msglastupdated[OT_OEMDiagnosticCode]);
  sendJsonOTmonObj(F("oemfaultcode"), OTcurrentSystemState.ASFflags & 0xFF, F(""), msglastupdated[OT_ASFflags]);

  if (settingS0COUNTERenabled) 
  {
    sendJsonOTmonObj(F("s0powerkw"), OTGWs0powerkw , F("kW"), OTGWs0lasttime);
    sendJsonOTmonObj(F("s0intervalcount"), OTGWs0pulseCount , F(""), OTGWs0lasttime);
    sendJsonOTmonObj(F("s0totalcount"), OTGWs0pulseCountTot , F(""), OTGWs0lasttime);
  }
  if (settingGPIOSENSORSenabled) 
  {
    sendJsonOTmonObj(F("numberofsensors"), DallasrealDeviceCount , F(""), now );
    for (int i = 0; i < DallasrealDeviceCount; i++) {
        const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
        sendJsonOTmonObj(strDeviceAddress, DallasrealDevice[i].tempC, F("°C"), DallasrealDevice[i].lasttime);
    }
  }

  sendEndJsonObj(F("otmonitor"));


} // sendOTmonitor()

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
  sendNestedJsonObj(F("wifiqualitytldr"), dBmtoQuality(WiFi.RSSI()));
  sendNestedJsonObj(F("ntpenable"), CBOOLEAN(settingNTPenable));
  sendNestedJsonObj(F("ntptimezone"), CSTR(settingNTPtimezone));
  sendNestedJsonObj(F("uptime"), upTime());
  sendNestedJsonObj(F("lastreset"), lastReset);
  sendNestedJsonObj(F("bootcount"), rebootCount);
  sendNestedJsonObj(F("mqttconnected"), CBOOLEAN(statusMQTTconnection));
  sendNestedJsonObj(F("thermostatconnected"), CBOOLEAN(bOTGWthermostatstate));
  sendNestedJsonObj(F("boilerconnected"), CBOOLEAN(bOTGWboilerstate));      
  sendNestedJsonObj(F("gatewaymode"), CBOOLEAN(bOTGWgatewaystate));      
  sendNestedJsonObj(F("otgwconnected"), CBOOLEAN(bOTGWonline));
  
  sendEndJsonObj(F("devinfo"));

} // sendDeviceInfo()

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

  sendEndJsonMap(F("devtime"));

} // sendDeviceTimeV2()

//=======================================================================
void sendDeviceSettings() 
{
  RESTDebugTln(F("sending device settings ...\r"));

  sendStartJsonObj(F("settings"));

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

  sendEndJsonObj(F("settings"));

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
      // String jsonInStr  = CSTR(httpServer.arg(0));
      char field[25] = {0,};
      char newValue[101]={0,};
      
      // Use buffer to process input to avoid String class
      // Assume max input matches our temp buffer size
      char jsonIn[256];
      strlcpy(jsonIn, httpServer.arg(0).c_str(), sizeof(jsonIn));

      // Remove braces and quotes in place
      replaceAll(jsonIn, sizeof(jsonIn), "{", "");
      replaceAll(jsonIn, sizeof(jsonIn), "}", "");
      replaceAll(jsonIn, sizeof(jsonIn), "\"", "");

      uint_fast8_t wp = splitString(jsonIn, ',',  wPair, 5) ;
	      for (uint_fast8_t i=0; i<wp; i++)
	      {
	        char* wOut[5];
	        //RESTDebugTf(PSTR("[%d] -> pair[%s]\r\n"), i, wPair[i]);
	        uint8_t wc = splitString(wPair[i], ':',  wOut, 5) ;
	        //RESTDebugTf(PSTR("==> [%s] -> field[%s]->val[%s]\r\n"), wPair[i], wOut[0], wOut[1]);
	        if (wc>1) {
	            if (wOut[0] && strcasecmp_P(wOut[0], PSTR("name")) == 0) {
	              if (wOut[1] && (strlen(wOut[1]) < (sizeof(field) - 1))) {
	                strlcpy(field, wOut[1], sizeof(field));
	              }
	            }
	            else if (wOut[0] && strcasecmp_P(wOut[0], PSTR("value")) == 0) {
	              if (wOut[1] && (strlen(wOut[1]) < (sizeof(newValue) - 1))) {
	                strlcpy(newValue, wOut[1], sizeof(newValue));
	              }
	            }
	        }
	      }
      if ( field[0] != 0 && newValue[0] != 0 ) {
        RESTDebugTf(PSTR("--> field[%s] => newValue[%s]\r\n"), field, newValue);
        updateSetting(field, newValue);
        httpServer.send(200, F("application/json"), httpServer.arg(0));
      } else {
        // Internal client error? It could not proess the client request.
        httpServer.send(400, F("application/json"), httpServer.arg(0));
      }

} // postSettings()


//====================================================
void sendApiNotFound(const char *URI)
{
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send_P(404, PSTR("text/html; charset=UTF-8"), PSTR("<!DOCTYPE HTML><html><head>"));

  httpServer.sendContent_P(PSTR("<style>body { background-color: lightgray; font-size: 15pt;}</style></head><body>"));
  httpServer.sendContent_P(PSTR("<h1>OTGW firmware</h1><b1>"));
  httpServer.sendContent_P(PSTR("<br>[<b>"));
  httpServer.sendContent(URI);
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
