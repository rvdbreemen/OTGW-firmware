/* 
***************************************************************************  
**  Program  : restAPI
**  Version  : v0.9.0
**
**  Copyright (c) 2021 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#define RESTDebugTln(...) ({ if (bDebugRestAPI) DebugTln(__VA_ARGS__);    })
#define RESTDebugln(...)  ({ if (bDebugRestAPI) Debugln(__VA_ARGS__);    })
#define RESTDebugTf(...)  ({ if (bDebugRestAPI) DebugTf(__VA_ARGS__);    })
#define RESTDebugf(...)   ({ if (bDebugRestAPI) Debugf(__VA_ARGS__);    })
#define RESTDebugT(...)   ({ if (bDebugRestAPI) DebugT(__VA_ARGS__);    })
#define RESTDebug(...)    ({ if (bDebugRestAPI) Debug(__VA_ARGS__);    })



//=======================================================================
void processAPI() 
{
  static char response[80] = "";
  char fName[40] = "";
  char URI[50]   = "";
  String words[10];

  strlcpy( URI, httpServer.uri().c_str(), sizeof(URI) );

  if (httpServer.method() == HTTP_GET)
        RESTDebugTf("from[%s] URI[%s] method[GET] \r\n"
                                  , httpServer.client().remoteIP().toString().c_str()
                                        , URI); 
  else  RESTDebugTf("from[%s] URI[%s] method[PUT] \r\n" 
                                  , httpServer.client().remoteIP().toString().c_str()
                                        , URI); 

  if (ESP.getFreeHeap() < 8500) // to prevent firmware from crashing!
  {
    RESTDebugTf("==> Bailout due to low heap (%d bytes))\r\n", ESP.getFreeHeap() );
    httpServer.send(500, "text/plain", "500: internal server error (low heap)\r\n"); 
    return;
  }

  int8_t wc = splitString(URI, '/', words, 10);
  
  if (bDebugRestAPI)
  {
    DebugT(">>");
    for (int w=0; w<wc; w++)
    {
      Debugf("word[%d] => [%s], ", w, words[w].c_str());
    }
    Debugln(" ");
  }

  if (words[1] == "api"){

    if (words[2] == "v1") 
    { //v1 API calls
      if (words[3] == "otgw"){
         if (words[4] == "telegraf") {
          // GET /api/v1/otgw/telegraf
          // Response: see json response
          sendTelegraf();
         } else if (words[4] == "otmonitor") {
          // GET /api/v1/otgw/otmonitor
          // Response: see json response
          sendOTmonitor();
        } else if (words[4] == "autoconfigure") {
          // POST /api/v1/otgw/autoconfigure
          // Response: sends all autodiscovery topics to MQTT for HA integration
          httpServer.send(200, "text/plain", "OK");
          doAutoConfigure();
        } else if (words[4] == "id"){
          //what the heck should I do?
          // /api/v1/otgw/id/{msgid}   msgid = OpenTherm Message Id (0-127)
          // Response: label, value, unit
          // {
          //   "label": "Tr",
          //   "value": "0.00",
          //   "unit": "°C"
          // }
          sendOTGWvalue(words[5].toInt());  
        } else if (words[4] == "label"){
          //what the heck should I do?
          // /api/v1/otgw/label/{msglabel} = OpenTherm Label (matching string)
          // Response: label, value, unit
          // {
          //   "label": "Tr",
          //   "value": "0.00",
          //   "unit": "°C"
          // }   
          sendOTGWlabel(CSTR(words[5]));
        } else if (words[4] == "command"){
          if (httpServer.method() == HTTP_PUT || httpServer.method() == HTTP_POST)
          {
            /* how to post a command to OTGW
            ** POST or PUT = /api/v1/otgw/command/{command} = Any command you want
            ** Response: 200 OK
            */
            //Add a command to OTGW queue 
            addOTWGcmdtoqueue(CSTR(words[5]), words[5].length());
            httpServer.send(200, "text/plain", "OK");
          } else sendApiNotFound(URI);
        } else sendApiNotFound(URI);
      }
      else sendApiNotFound(URI);
    } 
    else if (words[2] == "v0")
    { //v0 API calls
      if (words[3] == "otgw"){
        //what the heck should I do?
        // /api/v0/otgw/{msgid}   msgid = OpenTherm Message Id
        // Response: label, value, unit
        // {
        //   "label": "Tr",
        //   "value": "0.00",
        //   "unit": "°C"
        // }
        sendOTGWvalue(words[4].toInt()); 
      } 
      else if (words[3] == "devinfo")
      {
        sendDeviceInfo();
      }
      else if (words[3] == "devtime")
      {
        sendDeviceTime();
      }
      else if (words[3] == "settings")
      {
        if (httpServer.method() == HTTP_PUT || httpServer.method() == HTTP_POST)
        {
          postSettings();
        }
        else
        {
          sendDeviceSettings();
        }
      } else sendApiNotFound(URI);
    } else sendApiNotFound(URI);
  } else sendApiNotFound(URI);
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
    RESTDebugTf("%s = %s %s\r\n", OTlookupitem.label, getOTGWValue(msgid).c_str(), OTlookupitem.unit);
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
  //RESTDebugTf("Json = %s\r\n", sBuff.c_str());
  //reply with json
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, "application/json", sBuff);
}

void sendOTGWlabel(const char *msglabel){
  StaticJsonDocument<256> doc;
  JsonObject root  = doc.to<JsonObject>();
  int msgid;
  PROGMEM_readAnything (&OTmap[msgid], OTlookupitem);
  for (msgid = 0; msgid<= OT_MSGID_MAX; msgid++){
    if (stricmp(OTlookupitem.label, msglabel)==0) break;
  }
  if (msgid > OT_MSGID_MAX){
    root["error"] = "message id: reserved for future use";
  } else if (OTlookupitem.type==ot_undef) {  //message is undefined, return error
    root["error"] = "message undefined: reserved for future use";
  } else 
  { //message id's need to be between 0 and OT_MSGID_MAX
    //RESTDebug print the values first
    RESTDebugTf("%s = %s %s\r\n", OTlookupitem.label, getOTGWValue(msgid).c_str(), OTlookupitem.unit);
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
  //RESTDebugTf("Json = %s\r\n", sBuff.c_str());
  //reply with json
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
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
  

  sendJsonOTmonObj("outsidetemperature", OTdataObject.Toutside, "°C", msglastupdated[OT_Toutside]);
  sendJsonOTmonObj("roomtemperature", OTdataObject.Tr, "°C", msglastupdated[OT_Tr]);
  sendJsonOTmonObj("roomsetpoint", OTdataObject.TrSet, "°C", msglastupdated[OT_TrSet]);
  sendJsonOTmonObj("remoteroomsetpoint", OTdataObject.TrOverride, "°C", msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj("controlsetpoint", OTdataObject.TSet,"°C", msglastupdated[OT_TSet]);
  sendJsonOTmonObj("relmodlvl", OTdataObject.RelModLevel,"%", msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj("maxrelmodlvl", OTdataObject.MaxRelModLevelSetting, "%", msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj("boilertemperature", OTdataObject.Tboiler, "°C", msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj("returnwatertemperature", OTdataObject.Tret,"°C", msglastupdated[OT_Tret]);
  sendJsonOTmonObj("dhwtemperature", OTdataObject.Tdhw,"°C", msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj("dhwsetpoint", OTdataObject.TdhwSet,"°C", msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj("maxchwatersetpoint", OTdataObject.MaxTSet,"°C", msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj("chwaterpressure", OTdataObject.CHPressure, "bar", msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj("oemfaultcode", OTdataObject.OEMDiagnosticCode, "", msglastupdated[OT_OEMDiagnosticCode]);

  sendEndJsonArray();

} // sendTelegraf()
//=======================================================================

void sendOTmonitor() 
{
  RESTDebugTln(F("sending OT monitor values ...\r"));

  sendStartJsonObj("otmonitor");

  // sendJsonOTmonObj("status hb", byte_to_binary((OTdataObject.Statusflags>>8) & 0xFF),"", msglastupdated[OT_Statusflags]);
  // sendJsonOTmonObj("status lb", byte_to_binary(OTdataObject.Statusflags & 0xFF),"", msglastupdated[OT_Statusflags]);

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
  

  sendJsonOTmonObj("outsidetemperature", OTdataObject.Toutside, "°C", msglastupdated[OT_Toutside]);
  sendJsonOTmonObj("roomtemperature", OTdataObject.Tr, "°C", msglastupdated[OT_Tr]);
  sendJsonOTmonObj("roomsetpoint", OTdataObject.TrSet, "°C", msglastupdated[OT_TrSet]);
  sendJsonOTmonObj("remoteroomsetpoint", OTdataObject.TrOverride, "°C", msglastupdated[OT_TrOverride]);
  sendJsonOTmonObj("controlsetpoint", OTdataObject.TSet,"°C", msglastupdated[OT_TSet]);
  sendJsonOTmonObj("relmodlvl", OTdataObject.RelModLevel,"%", msglastupdated[OT_RelModLevel]);
  sendJsonOTmonObj("maxrelmodlvl", OTdataObject.MaxRelModLevelSetting, "%", msglastupdated[OT_MaxRelModLevelSetting]);
 
  sendJsonOTmonObj("boilertemperature", OTdataObject.Tboiler, "°C", msglastupdated[OT_Tboiler]);
  sendJsonOTmonObj("returnwatertemperature", OTdataObject.Tret,"°C", msglastupdated[OT_Tret]);
  sendJsonOTmonObj("dhwtemperature", OTdataObject.Tdhw,"°C", msglastupdated[OT_Tdhw]);
  sendJsonOTmonObj("dhwsetpoint", OTdataObject.TdhwSet,"°C", msglastupdated[OT_TdhwSet]);
  sendJsonOTmonObj("maxchwatersetpoint", OTdataObject.MaxTSet,"°C", msglastupdated[OT_MaxTSet]);
  sendJsonOTmonObj("chwaterpressure", OTdataObject.CHPressure, "bar", msglastupdated[OT_CHPressure]);
  sendJsonOTmonObj("oemdiagnosticcode", OTdataObject.OEMDiagnosticCode, "", msglastupdated[OT_OEMDiagnosticCode]);
  sendJsonOTmonObj("oemfaultcode", OTdataObject.ASFflags && 0xFF, "", msglastupdated[OT_ASFflags]);
  
  sendEndJsonObj("otmonitor");

} // sendOTmonitor()

//=======================================================================
void sendDeviceInfo() 
{
  sendStartJsonObj("devinfo");

  sendNestedJsonObj("author", "Robert van den Breemen");
  sendNestedJsonObj("fwversion", _FW_VERSION);
  sendNestedJsonObj("picfwversion", CSTR(sPICfwversion));
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
  sendNestedJsonObj("sketchsize", formatFloat( (ESP.getSketchSize() / 1024.0), 3));
  sendNestedJsonObj("freesketchspace", formatFloat( (ESP.getFreeSketchSpace() / 1024.0), 3));

  snprintf(cMsg, sizeof(cMsg), "%08X", ESP.getFlashChipId());
  sendNestedJsonObj("flashchipid", cMsg);  // flashChipId
  sendNestedJsonObj("flashchipsize", formatFloat((ESP.getFlashChipSize() / 1024.0 / 1024.0), 3));
  sendNestedJsonObj("flashchiprealsize", formatFloat((ESP.getFlashChipRealSize() / 1024.0 / 1024.0), 3));

  LittleFS.info(LittleFSinfo);
  sendNestedJsonObj("LittleFSsize", formatFloat( (LittleFSinfo.totalBytes / (1024.0 * 1024.0)), 0));

  sendNestedJsonObj("flashchipspeed", formatFloat((ESP.getFlashChipSpeed() / 1000.0 / 1000.0), 0));

  FlashMode_t ideMode = ESP.getFlashChipMode();
  sendNestedJsonObj("flashchipmode", flashMode[ideMode]);
  sendNestedJsonObj("boardtype",
#if defined(ARDUINO_ESP8266_NODEMCU)
     "ESP8266_NODEMCU"
#elif defined(ARDUINO_ESP8266_GENERIC)
     "ESP8266_GENERIC"
#elif defined(ESP8266_ESP01)
     "ESP8266_ESP01"
#elif defined(ESP8266_ESP12)
     "ESP8266_ESP12"
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
     "WEMOS_D1MINI"
#else 
     "Unknown board"
#endif

  );
  sendNestedJsonObj("ssid", CSTR(WiFi.SSID()));
  sendNestedJsonObj("wifirssi", WiFi.RSSI());
  sendNestedJsonObj("ntpenable", String(CBOOLEAN(settingNTPenable)));
  sendNestedJsonObj("ntptimezone", CSTR(settingNTPtimezone));
  sendNestedJsonObj("uptime", upTime());
  sendNestedJsonObj("lastreset", lastReset);
  sendNestedJsonObj("bootcount", rebootCount);
  sendNestedJsonObj("mqttconnected", String(CBOOLEAN(statusMQTTconnection)));
  sendNestedJsonObj("thermostatconnected", CBOOLEAN(bOTGWthermostatstate));
  sendNestedJsonObj("boilerconnected", CBOOLEAN(bOTGWboilerstate));      
  sendNestedJsonObj("picconnected", CBOOLEAN(bOTGWonline));
  
  sendEndJsonObj("devinfo");

} // sendDeviceInfo()


//=======================================================================
void sendDeviceTime() 
{
  char actTime[50];
  
  sendStartJsonObj("devtime");
  snprintf(actTime, 49, "%04d-%02d-%02d %02d:%02d:%02d", year(), month(), day()
                                                       , hour(), minute(), second());
  sendNestedJsonObj("dateTime", actTime); 
  sendNestedJsonObj("epoch", (int)now());
  sendNestedJsonObj("message", sMessage);

  sendEndJsonObj("devtime");

} // sendDeviceTime()

//=======================================================================
void sendDeviceSettings() 
{
  RESTDebugTln(F("sending device settings ...\r"));

  sendStartJsonObj("settings");
  
  //sendJsonSettingObj("string",   settingString,   "s", sizeof(settingString)-1);
  //sendJsonSettingObj("float",    settingFloat,    "f", 0, 10,  5);
  //sendJsonSettingObj("intager",  settingInteger , "i", 2, 60);

  sendJsonSettingObj("hostname", CSTR(settingHostname), "s", 32);
  sendJsonSettingObj("mqttenable", settingMQTTenable, "b");
  sendJsonSettingObj("mqttbroker", CSTR(settingMQTTbroker), "s", 32);
  sendJsonSettingObj("mqttbrokerport", settingMQTTbrokerPort, "i", 0, 65535);
  sendJsonSettingObj("mqttuser", CSTR(settingMQTTuser), "s", 32);
  sendJsonSettingObj("mqttpasswd", CSTR(settingMQTTpasswd), "s", 100);
  sendJsonSettingObj("mqtttoptopic", CSTR(settingMQTTtopTopic), "s", 15);
  sendJsonSettingObj("mqtthaprefix", CSTR(settingMQTThaprefix), "s", 20);
  sendJsonSettingObj("mqttuniqueid", CSTR(settingMQTTuniqueid), "s", 20);
  sendJsonSettingObj("mqttotmessage", settingMQTTOTmessage, "b");
  sendJsonSettingObj("ntpenable", settingNTPenable, "b");
  sendJsonSettingObj("ntptimezone", CSTR(settingNTPtimezone), "s", 50);
  sendJsonSettingObj("ntphostname", CSTR(settingNTPhostname), "s", 50);
  sendJsonSettingObj("ledblink", settingLEDblink, "b");
  sendJsonSettingObj("gpiosensorsenabled", settingGPIOSENSORSenabled, "b");
  sendJsonSettingObj("gpiosensorspin", settingGPIOSENSORSpin, "i", 0, 16);
  sendJsonSettingObj("gpiosensorsinterval", settingGPIOSENSORSinterval, "i", 5, 65535);
  sendJsonSettingObj("gpiooutputsenabled", settingGPIOOUTPUTSenabled, "b");
  sendJsonSettingObj("gpiooutputspin", settingGPIOOUTPUTSpin, "i", 0, 16);
  sendJsonSettingObj("gpiooutputstriggerbit", settingGPIOOUTPUTStriggerBit, "i", 0,16);
  sendJsonSettingObj("otgwcommandenable", settingOTGWcommandenable, "b");
  sendJsonSettingObj("otgwcommands", CSTR(settingOTGWcommands), "s", 32);

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
      String wOut[5];
      String wPair[5];
      String jsonIn  = CSTR(httpServer.arg(0));
      char field[25] = "";
      char newValue[101]="";
      jsonIn.replace("{", "");
      jsonIn.replace("}", "");
      jsonIn.replace("\"", "");
      int8_t wp = splitString(jsonIn.c_str(), ',',  wPair, 5) ;
      for (int i=0; i<wp; i++)
      {
        //RESTDebugTf("[%d] -> pair[%s]\r\n", i, wPair[i].c_str());
        int8_t wc = splitString(wPair[i].c_str(), ':',  wOut, 5) ;
        //RESTDebugTf("==> [%s] -> field[%s]->val[%s]\r\n", wPair[i].c_str(), wOut[0].c_str(), wOut[1].c_str());
        if (wOut[0].equalsIgnoreCase("name"))  strCopy(field, sizeof(field), wOut[1].c_str());
        if (wOut[0].equalsIgnoreCase("value")) strCopy(newValue, sizeof(newValue), wOut[1].c_str());
      }
      RESTDebugTf("--> field[%s] => newValue[%s]\r\n", field, newValue);
      updateSetting(field, newValue);
      httpServer.send(200, "application/json", httpServer.arg(0));

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
