/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v0.9.5
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//=======================================================================
void writeSettings(bool show) 
{

  //let's use JSON to write the setting file
  DebugTf("Writing to [%s] ..\r\n", SETTINGS_FILE);
  File file = LittleFS.open(SETTINGS_FILE, "w"); // open for reading and writing
  if (!file) 
  {
    DebugTf("open(%s, 'w') FAILED!!! --> Bailout\r\n", SETTINGS_FILE);
    return;
  }
  yield();

  DebugT(F("Start writing setting data "));

  //const size_t capacity = JSON_OBJECT_SIZE(6);  // save more setting, grow # of objects accordingly
  DynamicJsonDocument doc(1024);
  JsonObject root  = doc.to<JsonObject>();
  root["hostname"] = settingHostname;
  root["MQTTenable"] = settingMQTTenable;
  root["MQTTbroker"] = settingMQTTbroker;
  root["MQTTbrokerPort"] = settingMQTTbrokerPort;
  root["MQTTuser"] = settingMQTTuser;
  root["MQTTpasswd"] = settingMQTTpasswd;
  root["MQTTtoptopic"] = settingMQTTtopTopic;
  root["MQTThaprefix"] = settingMQTThaprefix;
  root["MQTTharebootdetection"]= settingMQTTharebootdetection;	  
  root["MQTTuniqueid"] = settingMQTTuniqueid;
  root["MQTTOTmessage"] = settingMQTTOTmessage;
  root["MQTTharebootdetection"]= settingMQTTharebootdetection;  
  root["NTPenable"] = settingNTPenable;
  root["NTPtimezone"] = settingNTPtimezone;
  root["NTPhostname"] = settingNTPhostname;
  root["LEDblink"] = settingLEDblink;
  root["GPIOSENSORSenabled"] = settingGPIOSENSORSenabled;
  root["GPIOSENSORSpin"] = settingGPIOSENSORSpin;
  root["GPIOSENSORSinterval"] = settingGPIOSENSORSinterval;
  root["S0COUNTERenabled"] = settingS0COUNTERenabled;
  root["S0COUNTERpin"] = settingS0COUNTERpin;
  root["S0COUNTERdebouncetime"] = settingS0COUNTERdebouncetime;
  root["S0COUNTERpulsekw"] = settingS0COUNTERpulsekw;
  root["S0COUNTERinterval"] = settingS0COUNTERinterval;
  root["OTGWcommandenable"] = settingOTGWcommandenable;
  root["OTGWcommands"] = settingOTGWcommands;
  root["GPIOOUTPUTSenabled"] = settingGPIOOUTPUTSenabled;
  root["GPIOOUTPUTSpin"] = settingGPIOOUTPUTSpin;
  root["GPIOOUTPUTStriggerBit"] = settingGPIOOUTPUTStriggerBit;

  serializeJsonPretty(root, file);
  Debugln(F("... done!"));
  if (show)  serializeJsonPretty(root, TelnetStream); //Debug stream ;-)
  file.close();  

} // writeSettings()


//=======================================================================
void readSettings(bool show) 
{

  // Open file for reading
  File file =  LittleFS.open(SETTINGS_FILE, "r");

  DebugTf(" %s ..\r\n", SETTINGS_FILE);
  if (!LittleFS.exists(SETTINGS_FILE)) 
  {  //create settings file if it does not exist yet.
    DebugTln(F(" .. file not found! --> created file!"));
    writeSettings(show);
    readSettings(false); //now it should work...
    return;
  }

  // Deserialize the JSON document
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    DebugTln(F("Failed to read file, use existing defaults."));
    DebugTf("Settings Deserialisation error:  %s \r\n", error.c_str());
    return;
  }

  // Copy values from the JsonDocument to the Config 
  settingHostname         = doc["hostname"].as<String>();
  if (settingHostname.length()==0) settingHostname = _HOSTNAME;
  settingMQTTenable       = doc["MQTTenable"]|settingMQTTenable; 
  settingMQTTbroker       = doc["MQTTbroker"].as<String>();
  settingMQTTbrokerPort   = doc["MQTTbrokerPort"]; //default port
  settingMQTTuser         = doc["MQTTuser"].as<String>();
  settingMQTTpasswd       = doc["MQTTpasswd"].as<String>();
  settingMQTTtopTopic     = doc["MQTTtoptopic"].as<String>();
  if (settingMQTTtopTopic=="null") {
    settingMQTTtopTopic = _HOSTNAME;
    settingMQTTtopTopic.toLowerCase();
  }
  settingMQTThaprefix     = doc["MQTThaprefix"].as<String>();
  if (settingMQTThaprefix=="null") settingMQTThaprefix = HOME_ASSISTANT_DISCOVERY_PREFIX;
  settingMQTTharebootdetection = doc["MQTTharebootdetection"]|settingMQTTharebootdetection;	  
  settingMQTTuniqueid     = doc["MQTTuniqueid"].as<String>();
  if (settingMQTTuniqueid=="null") settingMQTTuniqueid = getUniqueId();

  settingMQTTOTmessage    = doc["MQTTOTmessage"]|settingMQTTOTmessage;
  settingNTPenable        = doc["NTPenable"]; 
  settingNTPtimezone      = doc["NTPtimezone"].as<String>();
  if (settingNTPtimezone=="null")  settingNTPtimezone = "Europe/Amsterdam"; //default to amsterdam timezone
  settingNTPhostname      = doc["NTPhostname"].as<String>();
  if (settingNTPhostname=="null")  settingNTPhostname = NTP_HOST_DEFAULT; 
  settingLEDblink         = doc["LEDblink"]|settingLEDblink;
  settingGPIOSENSORSenabled = doc["GPIOSENSORSenabled"] | settingGPIOSENSORSenabled;
  settingGPIOSENSORSpin = doc["GPIOSENSORSpin"] | settingGPIOSENSORSpin;
  settingGPIOSENSORSinterval = doc["GPIOSENSORSinterval"] | settingGPIOSENSORSinterval;
  CHANGE_INTERVAL_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS); 
  settingS0COUNTERenabled = doc["S0COUNTERenabled"] | settingS0COUNTERenabled;
  settingS0COUNTERpin = doc["S0COUNTERpin"] | settingS0COUNTERpin;
  settingS0COUNTERdebouncetime = doc["S0COUNTERdebouncetime"] | settingS0COUNTERdebouncetime;
  settingS0COUNTERpulsekw = doc["S0COUNTERpulsekw"] | settingS0COUNTERpulsekw;
  settingS0COUNTERinterval = doc["S0COUNTERinterval"] | settingS0COUNTERinterval;
  CHANGE_INTERVAL_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS); 
  settingOTGWcommandenable = doc["OTGWcommandenable"] | settingOTGWcommandenable;
  settingOTGWcommands     = doc["OTGWcommands"].as<String>();
  if (settingOTGWcommands=="null") settingOTGWcommands = "";
  settingGPIOOUTPUTSenabled = doc["GPIOOUTPUTSenabled"] | settingGPIOOUTPUTSenabled;
  settingGPIOOUTPUTSpin = doc["GPIOOUTPUTSpin"] | settingGPIOOUTPUTSpin;
  settingGPIOOUTPUTStriggerBit = doc["GPIOOUTPUTStriggerBit"] | settingGPIOOUTPUTStriggerBit;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf("Hostname              : %s\r\n", CSTR(settingHostname));
    Debugf("MQTT enabled          : %s\r\n", CBOOLEAN(settingMQTTenable));
    Debugf("MQTT broker           : %s\r\n", CSTR(settingMQTTbroker));
    Debugf("MQTT port             : %d\r\n", settingMQTTbrokerPort);
    Debugf("MQTT username         : %s\r\n", CSTR(settingMQTTuser));
    Debugf("MQTT password         : %s\r\n", CSTR(settingMQTTpasswd));
    Debugf("MQTT toptopic         : %s\r\n", CSTR(settingMQTTtopTopic));
    Debugf("MQTT uniqueid         : %s\r\n", CSTR(settingMQTTuniqueid));
    Debugf("HA prefix             : %s\r\n", CSTR(settingMQTThaprefix));
    Debugf("HA reboot detection   : %s\r\n", CBOOLEAN(settingMQTTharebootdetection));
    Debugf("NTP enabled           : %s\r\n", CBOOLEAN(settingNTPenable));
    Debugf("NPT timezone          : %s\r\n", CSTR(settingNTPtimezone));
    Debugf("NPT hostname          : %s\r\n", CSTR(settingNTPhostname));
    Debugf("Led Blink             : %s\r\n", CBOOLEAN(settingLEDblink));
    Debugf("GPIO Sensors          : %s\r\n", CBOOLEAN(settingGPIOSENSORSenabled));
    Debugf("GPIO Sen. Pin         : %d\r\n", settingGPIOSENSORSpin);
    Debugf("GPIO Interval         : %d\r\n", settingGPIOSENSORSinterval);
    Debugf("S0 Counter            : %s\r\n", CBOOLEAN(settingS0COUNTERenabled));
    Debugf("S0 Counter Pin        : %d\r\n", settingS0COUNTERpin);
    Debugf("S0 Counter Debouncetime:%d\r\n", settingS0COUNTERdebouncetime);
    Debugf("S0 Counter Pulses/kw  : %d\r\n", settingS0COUNTERpulsekw);
    Debugf("S0 Counter Interval   : %d\r\n", settingS0COUNTERinterval);
    Debugf("OTGW boot cmd enabled : %s\r\n", CBOOLEAN(settingOTGWcommandenable));
    Debugf("OTGW boot cmd         : %s\r\n", CSTR(settingOTGWcommands));
    Debugf("GPIO Outputs          : %s\r\n", CBOOLEAN(settingGPIOOUTPUTSenabled));
    Debugf("GPIO Out. Pin         : %d\r\n", settingGPIOOUTPUTSpin);
    Debugf("GPIO Out. Trg. Bit    : %d\r\n", settingGPIOOUTPUTStriggerBit);
    }
  
  Debugln(F("-\r\n"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{ //do not just trust the caller to do the right thing, server side validation is here!
  DebugTf("-> field[%s], newValue[%s]\r\n", field, newValue);

  if (strcasecmp(field, "hostname")==0) 
  { //make sure we have a valid hostname here...
    settingHostname = String(newValue);
    if (settingHostname.length()==0) settingHostname=_HOSTNAME; 
    int pos = settingMQTTtopTopic.indexOf("."); //strip away anything beyond the dot
    if (pos){
      settingMQTTtopTopic = settingMQTTtopTopic.substring(0, pos-1);
    }
    
    //Update some settings right now 
    startMDNS(CSTR(settingHostname));
    startLLMNR(CSTR(settingHostname));
  
    //Restart MQTT connection every "save settings"
    startMQTT();

    Debugln();
    DebugTf("Need reboot before new %s.local will be available!\r\n\n", CSTR(settingHostname));
  }
  if (strcasecmp(field, "MQTTenable")==0)      settingMQTTenable = EVALBOOLEAN(newValue);
  if (strcasecmp(field, "MQTTbroker")==0)      settingMQTTbroker = String(newValue);
  if (strcasecmp(field, "MQTTbrokerPort")==0)  settingMQTTbrokerPort = atoi(newValue);
  if (strcasecmp(field, "MQTTuser")==0)        settingMQTTuser = String(newValue);
  if (strcasecmp(field, "MQTTpasswd")==0)      settingMQTTpasswd = String(newValue);
  if (strcasecmp(field, "MQTTtoptopic")==0)    {
    settingMQTTtopTopic = String(newValue);
    if (settingMQTTtopTopic.length()==0)    {
      settingMQTTtopTopic = _HOSTNAME;
      settingMQTTtopTopic.toLowerCase();
    }
  }
  if (strcasecmp(field, "MQTThaprefix")==0)    {
    settingMQTThaprefix = String(newValue);
    if (settingMQTThaprefix.length()==0)    settingMQTThaprefix = HOME_ASSISTANT_DISCOVERY_PREFIX;
  }
  if (strcasecmp(field, "MQTTharebootdetection")==0)      settingMQTTharebootdetection = EVALBOOLEAN(newValue);
  if (strcasecmp(field, "MQTTuniqueid") == 0)  {
    settingMQTTuniqueid = String(newValue);     
    if (settingMQTTuniqueid.length() == 0)   settingMQTTuniqueid = getUniqueId();
  }
  if (strcasecmp(field, "MQTTOTmessage")==0)   settingMQTTOTmessage = EVALBOOLEAN(newValue);
  if (strstr(field, "mqtt") != NULL)        startMQTT();//restart MQTT on change of any setting
  
  if (strcasecmp(field, "NTPenable")==0)      settingNTPenable = EVALBOOLEAN(newValue);
  if (strcasecmp(field, "NTPhostname")==0)    {
    settingNTPhostname = String(newValue); 
    startNTP();
  }
  if (strcasecmp(field, "NTPtimezone")==0)    {
    settingNTPtimezone = String(newValue);
    startNTP();  // update timezone if changed
  }
  if (strcasecmp(field, "LEDblink")==0)      settingLEDblink = EVALBOOLEAN(newValue);
  if (strcasecmp(field, "GPIOSENSORSenabled") == 0)
  {
    settingGPIOSENSORSenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO SENSORS will search for sensors on pin GPIO%d!\r\n\n", settingGPIOSENSORSpin);
  }
  if (strcasecmp(field, "GPIOSENSORSpin") == 0)    
  {
    settingGPIOSENSORSpin = atoi(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO SENSORS will use new pin GPIO%d!\r\n\n", settingGPIOSENSORSpin);
  }
  if (strcasecmp(field, "GPIOSENSORSinterval") == 0) {
    settingGPIOSENSORSinterval = atoi(newValue);
    CHANGE_INTERVAL_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS); 
  }
  if (strcasecmp(field, "S0COUNTERenabled") == 0)
  {
    settingS0COUNTERenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf("Need reboot before S0 Counter starts counting on pin GPIO%d!\r\n\n", settingS0COUNTERpin);
  }
  if (strcasecmp(field, "S0COUNTERpin") == 0)    
  {
    settingS0COUNTERpin = atoi(newValue);
    Debugln();
    DebugTf("Need reboot before S0 Counter will use new pin GPIO%d!\r\n\n", settingS0COUNTERpin);
  }
  if (strcasecmp(field, "S0COUNTERdebouncetime") == 0) settingS0COUNTERdebouncetime = atoi(newValue);
  if (strcasecmp(field, "S0COUNTERpulsekw") == 0)      settingS0COUNTERpulsekw = atoi(newValue);

  if (strcasecmp(field, "S0COUNTERinterval") == 0) {
    settingS0COUNTERinterval = atoi(newValue);
    CHANGE_INTERVAL_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS); 
  }
  if (strcasecmp(field, "OTGWcommandenable")==0)    settingOTGWcommandenable = EVALBOOLEAN(newValue);
  if (strcasecmp(field, "OTGWcommands")==0)         settingOTGWcommands = String(newValue);
  if (strcasecmp(field, "GPIOOUTPUTSenabled") == 0)
  {
    settingGPIOOUTPUTSenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO OUTPUTS will be enabled on pin GPIO%d!\r\n\n", settingGPIOOUTPUTSenabled);
  }
  if (strcasecmp(field, "GPIOOUTPUTSpin") == 0)
  {
    settingGPIOOUTPUTSpin = atoi(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO OUTPUTS will use new pin GPIO%d!\r\n\n", settingGPIOOUTPUTSpin);
  }
  if (strcasecmp(field, "GPIOOUTPUTStriggerBit") == 0)
  {
    settingGPIOOUTPUTStriggerBit = atoi(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO OUTPUTS will use new trigger bit %d!\r\n\n", settingGPIOOUTPUTStriggerBit);
  }

  //finally update write settings
  writeSettings(false);

  //Restart MQTT connection every "save settings" (this seems to be save to do, but is called for each changed field now )
  if (settingMQTTenable)   startMQTT();

  // if (strstr(field, "hostname")!= NULL) {
  //   //restart wifi
  //   startWIFI( CSTR(settingHostname), 240)
  // } 

} // updateSetting()


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
