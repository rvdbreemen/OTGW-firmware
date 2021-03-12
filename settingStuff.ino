/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v0.8.0
**
**  Copyright (c) 2021 Robert van den Breemen
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
  DynamicJsonDocument doc(512);
  JsonObject root  = doc.to<JsonObject>();
  root["hostname"] = settingHostname;
  root["MQTTenable"] = settingMQTTenable;
  root["MQTTbroker"] = settingMQTTbroker;
  root["MQTTbrokerPort"] = settingMQTTbrokerPort;
  root["MQTTuser"] = settingMQTTuser;
  root["MQTTpasswd"] = settingMQTTpasswd;
  root["MQTTtoptopic"] = settingMQTTtopTopic;
  root["MQTThaprefix"] = settingMQTThaprefix;
  root["MQTTOTmessage"] = settingMQTTOTmessage;
  root["NTPenable"] = settingNTPenable;
  root["NTPtimezone"] = settingNTPtimezone;
  root["LEDblink"] = settingLEDblink;
  root["GPIOSENSORSenabled"] = settingGPIOSENSORSenabled;
  root["GPIOSENSORSpin"] = settingGPIOSENSORSpin;

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
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    DebugTln(F("Failed to read file, use existing defaults."));
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
  settingMQTTOTmessage    = doc["MQTTOTmessage"]|settingMQTTOTmessage;
  settingNTPenable        = doc["NTPenable"]; 
  settingNTPtimezone      = doc["NTPtimezone"].as<String>();
  if (settingNTPtimezone=="null")  settingNTPtimezone = "Europe/Amsterdam"; //default to amsterdam timezone
  settingLEDblink         = doc["LEDblink"]|settingLEDblink;
  settingGPIOSENSORSenabled = doc["GPIOSENSORSenabled"] | settingGPIOSENSORSenabled;
  settingGPIOSENSORSpin = doc["GPIOSENSORSpin"] | settingGPIOSENSORSpin;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  //Update some settings right now 
  MDNS.setHostname(CSTR(settingHostname));    // start advertising with new(?) settingHostname

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf("Hostname      : %s\r\n",  CSTR(settingHostname));
    Debugf("MQTT enabled  : %s\r\n",  CBOOLEAN(settingMQTTenable));
    Debugf("MQTT broker   : %s\r\n",  CSTR(settingMQTTbroker));
    Debugf("MQTT port     : %d\r\n",  settingMQTTbrokerPort);
    Debugf("MQTT username : %s\r\n",  CSTR(settingMQTTuser));
    Debugf("MQTT password : %s\r\n",  CSTR(settingMQTTpasswd));
    Debugf("MQTT toptopic : %s\r\n",  CSTR(settingMQTTtopTopic));
    Debugf("HA prefix     : %s\r\n",  CSTR(settingMQTThaprefix));
    Debugf("NTP enabled   : %s\r\n",  CBOOLEAN(settingNTPenable));
    Debugf("NPT timezone  : %s\r\n",  CSTR(settingNTPtimezone));
    Debugf("Led Blink     : %s\r\n",  CBOOLEAN(settingLEDblink));
    Debugf("GPIO Sensors  : %s\r\n",  CBOOLEAN(settingGPIOSENSORSenabled));
    Debugf("GPIO Sen. Pin : %d\r\n",  settingGPIOSENSORSpin);
    }
  
  Debugln(F("-\r\n"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{ //do not just trust the caller to do the right thing, server side validation is here!
  DebugTf("-> field[%s], newValue[%s]\r\n", field, newValue);

  if (stricmp(field, "hostname")==0) 
  { //make sure we have a valid hostname here...
    settingHostname = String(newValue);
    if (settingHostname.length()==0) settingHostname=_HOSTNAME; 
    int pos = settingMQTTtopTopic.indexOf("."); //strip away anything beyond the dot
    if (pos){
      settingMQTTtopTopic = settingMQTTtopTopic.substring(0, pos-1);
    }
    
    Debugln();
    DebugTf("Need reboot before new %s.local will be available!\r\n\n", CSTR(settingHostname));
  }
  if (stricmp(field, "MQTTenable")==0)      settingMQTTenable = EVALBOOLEAN(newValue);
  if (stricmp(field, "MQTTbroker")==0)      settingMQTTbroker = String(newValue);
  if (stricmp(field, "MQTTbrokerPort")==0)  settingMQTTbrokerPort = atoi(newValue);
  if (stricmp(field, "MQTTuser")==0)        settingMQTTuser = String(newValue);
  if (stricmp(field, "MQTTpasswd")==0)      settingMQTTpasswd = String(newValue);
  if (stricmp(field, "MQTTtoptopic")==0)    {
    settingMQTTtopTopic = String(newValue);
    if (settingMQTTtopTopic.length()==0)    {
      settingMQTTtopTopic = _HOSTNAME;
      settingMQTTtopTopic.toLowerCase();
    }
  }
  if (stricmp(field, "MQTThaprefix")==0)    {
    settingMQTThaprefix = String(newValue);
    if (settingMQTThaprefix.length()==0) settingMQTThaprefix = HOME_ASSISTANT_DISCOVERY_PREFIX;
  }
  if (stricmp(field, "MQTTOTmessage")==0)  settingMQTTOTmessage = EVALBOOLEAN(newValue);
  
  if (stricmp(field, "NTPenable")==0)      settingNTPenable = EVALBOOLEAN(newValue);
  if (stricmp(field, "NTPtimezone")==0)    {
    settingNTPtimezone = String(newValue);
    startNTP();  // update timezone if changed
  }
  if (stricmp(field, "LEDblink")==0)      settingLEDblink = EVALBOOLEAN(newValue);
  if (stricmp(field, "GPIOSENSORSenabled") == 0)
  {
    settingGPIOSENSORSenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO SENSORS will search for sensors on pin GPIO%d!\r\n\n", settingGPIOSENSORSpin);
  }
  if (stricmp(field, "GPIOSENSORSpin") == 0)    
  {
    settingGPIOSENSORSpin = atoi(newValue);
    Debugln();
    DebugTf("Need reboot before GPIO SENSORS will use new pin GPIO%d!\r\n\n", settingGPIOSENSORSpin);
  }

  //finally update write settings
  writeSettings(false);

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
