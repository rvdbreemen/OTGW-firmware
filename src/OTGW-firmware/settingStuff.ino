/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v1.1.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//=======================================================================
// Deferred settings write support (Finding #23: reduce flash wear + service restarts)
// Side-effect bitmask flags
#define SIDE_EFFECT_MQTT   0x01
#define SIDE_EFFECT_NTP    0x02
#define SIDE_EFFECT_MDNS   0x04
static bool    settingsDirty = false;
static uint8_t pendingSideEffects = 0;

//=======================================================================
void flushSettings()
{
  if (!settingsDirty) return;

  DebugTln(F("[Settings] Flushing deferred settings write..."));
  writeSettings(false);
  settingsDirty = false;

  // Apply deferred side effects — exactly once per service per save batch
  if (pendingSideEffects & SIDE_EFFECT_MDNS) {
    DebugTln(F("[Settings] Restarting MDNS/LLMNR (deferred)"));
    startMDNS(settingHostname);
    startLLMNR(settingHostname);
  }
  if ((pendingSideEffects & SIDE_EFFECT_MQTT) && settingMQTTenable) {
    DebugTln(F("[Settings] Restarting MQTT (deferred)"));
    startMQTT();
  }
  if (pendingSideEffects & SIDE_EFFECT_NTP) {
    DebugTln(F("[Settings] Restarting NTP (deferred)"));
    startNTP();
  }
  pendingSideEffects = 0;
}

//=======================================================================
// GPIO conflict detection (Finding #27)
// Returns true if the requested pin is already used by another feature.
// 'caller' identifies which feature is requesting the pin (e.g. "sensor", "s0", "output")
bool checkGPIOConflict(int pin, PGM_P caller)
{
  if (pin < 0) return false; // disabled / not set

  bool conflict = false;
  // Check against each configurable GPIO (excluding 'caller' itself)
  if (strcasecmp_P(caller, PSTR("sensor")) != 0 && pin == settingGPIOSENSORSpin && settingGPIOSENSORSpin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by SENSORS\r\n"), pin);
    conflict = true;
  }
  if (strcasecmp_P(caller, PSTR("s0")) != 0 && pin == settingS0COUNTERpin && settingS0COUNTERpin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by S0 Counter\r\n"), pin);
    conflict = true;
  }
  if (strcasecmp_P(caller, PSTR("output")) != 0 && pin == settingGPIOOUTPUTSpin && settingGPIOOUTPUTSpin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by GPIO OUTPUTS\r\n"), pin);
    conflict = true;
  }
  return conflict;
}

//=======================================================================
void writeSettings(bool show) 
{

  //let's use JSON to write the setting file
  DebugTf(PSTR("[Settings] State: writeSettings called (show=%s)\r\n"), show ? "true" : "false");
  DebugTf(PSTR("[Settings] Writing to [%s] ..\r\n"), SETTINGS_FILE);
  File file = LittleFS.open(SETTINGS_FILE, "w"); // open for reading and writing
  if (!file) 
  {
    DebugTf(PSTR("[Settings] Error: open(%s, 'w') FAILED!!! --> Bailout\r\n"), SETTINGS_FILE);
    return;
  }
  yield();

  DebugT(F("[Settings] State: Serializing settings to JSON... "));

  // Capacity reduced back to 1536 bytes (Dallas labels now in separate file)
  DynamicJsonDocument doc(1536);
  JsonObject root  = doc.to<JsonObject>();
  root[F("hostname")] = settingHostname;
  root[F("MQTTenable")] = settingMQTTenable;
  root[F("MQTTbroker")] = settingMQTTbroker;
  root[F("MQTTbrokerPort")] = settingMQTTbrokerPort;
  root[F("MQTTuser")] = settingMQTTuser;
  root[F("MQTTpasswd")] = settingMQTTpasswd;
  root[F("MQTTtoptopic")] = settingMQTTtopTopic;
  root[F("MQTThaprefix")] = settingMQTThaprefix;
  root[F("MQTTuniqueid")] = settingMQTTuniqueid;
  root[F("MQTTOTmessage")] = settingMQTTOTmessage;
  root[F("MQTTharebootdetection")]= settingMQTTharebootdetection;  
  root[F("NTPenable")] = settingNTPenable;
  root[F("NTPtimezone")] = settingNTPtimezone;
  root[F("NTPhostname")] = settingNTPhostname;
  root[F("NTPsendtime")] = settingNTPsendtime;
  root[F("LEDblink")] = settingLEDblink;
  root[F("darktheme")] = settingDarkTheme;
  root[F("ui_autoscroll")] = settingUIAutoScroll;
  root[F("ui_timestamps")] = settingUIShowTimestamp;
  root[F("ui_capture")] = settingUICaptureMode;
  root[F("ui_autoscreenshot")] = settingUIAutoScreenshot;
  root[F("ui_autodownloadlog")] = settingUIAutoDownloadLog;
  root[F("ui_autoexport")] = settingUIAutoExport;
  root[F("ui_graphtimewindow")] = settingUIGraphTimeWindow;
  root[F("GPIOSENSORSenabled")] = settingGPIOSENSORSenabled;
  root[F("GPIOSENSORSlegacyformat")] = settingGPIOSENSORSlegacyformat;
  root[F("GPIOSENSORSpin")] = settingGPIOSENSORSpin;
  root[F("GPIOSENSORSinterval")] = settingGPIOSENSORSinterval;
  root[F("S0COUNTERenabled")] = settingS0COUNTERenabled;
  root[F("S0COUNTERpin")] = settingS0COUNTERpin;
  root[F("S0COUNTERdebouncetime")] = settingS0COUNTERdebouncetime;
  root[F("S0COUNTERpulsekw")] = settingS0COUNTERpulsekw;
  root[F("S0COUNTERinterval")] = settingS0COUNTERinterval;
  root[F("OTGWcommandenable")] = settingOTGWcommandenable;
  root[F("OTGWcommands")] = settingOTGWcommands;
  root[F("GPIOOUTPUTSenabled")] = settingGPIOOUTPUTSenabled;
  root[F("GPIOOUTPUTSpin")] = settingGPIOOUTPUTSpin;
  root[F("GPIOOUTPUTStriggerBit")] = settingGPIOOUTPUTStriggerBit;
  // Dallas sensor labels now stored in /dallas_labels.json (not in settings.json)

  serializeJsonPretty(root, file);
  if (show) {
    DebugTln(F("\r\n[Settings] JSON content:"));
    serializeJsonPretty(root, TelnetStream); //Debug stream ;-)
  }
  Debugln(F("\r\n[Settings] State: File write complete, closing file"));
  file.close();
  DebugTf(PSTR("[Settings] State: Settings saved successfully to %s\r\n"), SETTINGS_FILE);  

} // writeSettings()


//=======================================================================
void readSettings(bool show) 
{
  // Open file for reading
  File file =  LittleFS.open(SETTINGS_FILE, "r");

  DebugTf(PSTR(" %s ..\r\n"), SETTINGS_FILE);
  if (!LittleFS.exists(SETTINGS_FILE)) 
  {  //create settings file if it does not exist yet.
    DebugTln(F(" .. file not found! --> created file!"));
    writeSettings(show);
    readSettings(false); //now it should work...
    return;
  }

  // Deserialize the JSON document
  // Use DynamicJsonDocument to eliminate stack overflow risk (moved from stack to heap)
  // Capacity reduced back to 1536 bytes (Dallas labels now in separate file)
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    DebugTln(F("Failed to read file, use existing defaults."));
    DebugTf(PSTR("Settings Deserialisation error:  %s \r\n"), error.c_str());
    return;
  }

  // Copy values from the JsonDocument to the Config 
  strlcpy(settingHostname, doc[F("hostname")] | "", sizeof(settingHostname));
  if (strlen(settingHostname)==0) strlcpy(settingHostname, _HOSTNAME, sizeof(settingHostname));

  settingMQTTenable       = doc[F("MQTTenable")]|settingMQTTenable;
  strlcpy(settingMQTTbroker, doc[F("MQTTbroker")] | "", sizeof(settingMQTTbroker));
  
  settingMQTTbrokerPort   = doc[F("MQTTbrokerPort")] | settingMQTTbrokerPort; //default port
  strlcpy(settingMQTTuser, doc[F("MQTTuser")] | "", sizeof(settingMQTTuser));
  // Trim leading/trailing whitespace from username
  char* trimmedUser = trimwhitespace(settingMQTTuser);
  if (trimmedUser != settingMQTTuser) {
    memmove(settingMQTTuser, trimmedUser, strlen(trimmedUser) + 1);
  }
  strlcpy(settingMQTTpasswd, doc[F("MQTTpasswd")] | "", sizeof(settingMQTTpasswd));
  // Trim leading/trailing whitespace from password
  char* trimmedPasswd = trimwhitespace(settingMQTTpasswd);
  if (trimmedPasswd != settingMQTTpasswd) {
    memmove(settingMQTTpasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);
  }
  
  strlcpy(settingMQTTtopTopic, doc[F("MQTTtoptopic")] | "", sizeof(settingMQTTtopTopic));
  if (strlen(settingMQTTtopTopic)==0 || strcmp_P(settingMQTTtopTopic, PSTR("null"))==0) {
    strlcpy(settingMQTTtopTopic, _HOSTNAME, sizeof(settingMQTTtopTopic));
    for(int i=0; settingMQTTtopTopic[i]; i++) settingMQTTtopTopic[i] = tolower(settingMQTTtopTopic[i]);
  }
  
  strlcpy(settingMQTThaprefix, doc[F("MQTThaprefix")] | "", sizeof(settingMQTThaprefix));
  if (strlen(settingMQTThaprefix)==0 || strcmp_P(settingMQTThaprefix, PSTR("null"))==0) strlcpy(settingMQTThaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settingMQTThaprefix));
  
  settingMQTTharebootdetection = doc[F("MQTTharebootdetection")]|settingMQTTharebootdetection;	  
  
  strlcpy(settingMQTTuniqueid, doc[F("MQTTuniqueid")] | "", sizeof(settingMQTTuniqueid));
  if (strlen(settingMQTTuniqueid)==0 || strcmp_P(settingMQTTuniqueid, PSTR("null"))==0) strlcpy(settingMQTTuniqueid, getUniqueId(), sizeof(settingMQTTuniqueid));

  settingMQTTOTmessage    = doc[F("MQTTOTmessage")]|settingMQTTOTmessage;
  settingNTPenable        = doc[F("NTPenable")]; 
  
  strlcpy(settingNTPtimezone, doc[F("NTPtimezone")] | "", sizeof(settingNTPtimezone));
  if (strlen(settingNTPtimezone)==0 || strcmp_P(settingNTPtimezone, PSTR("null"))==0)  strlcpy(settingNTPtimezone, "Europe/Amsterdam", sizeof(settingNTPtimezone)); //default to amsterdam timezone
  
  strlcpy(settingNTPhostname, doc[F("NTPhostname")] | "", sizeof(settingNTPhostname));
  if (strlen(settingNTPhostname)==0 || strcmp_P(settingNTPhostname, PSTR("null"))==0)  strlcpy(settingNTPhostname, NTP_HOST_DEFAULT, sizeof(settingNTPhostname));  
  settingNTPsendtime      = doc[F("NTPsendtime")]|settingNTPsendtime;
  settingLEDblink         = doc[F("LEDblink")]|settingLEDblink;
  settingDarkTheme        = doc[F("darktheme")]|settingDarkTheme;
  settingUIAutoScroll      = doc[F("ui_autoscroll")] | settingUIAutoScroll;
  settingUIShowTimestamp   = doc[F("ui_timestamps")] | settingUIShowTimestamp;
  settingUICaptureMode     = doc[F("ui_capture")] | settingUICaptureMode;
  settingUIAutoScreenshot  = doc[F("ui_autoscreenshot")] | settingUIAutoScreenshot;
  settingUIAutoDownloadLog = doc[F("ui_autodownloadlog")] | settingUIAutoDownloadLog;
  settingUIAutoExport      = doc[F("ui_autoexport")] | settingUIAutoExport;
  settingUIGraphTimeWindow = doc[F("ui_graphtimewindow")] | settingUIGraphTimeWindow;
  settingGPIOSENSORSenabled = doc[F("GPIOSENSORSenabled")] | settingGPIOSENSORSenabled;
  settingGPIOSENSORSlegacyformat = doc[F("GPIOSENSORSlegacyformat")] | settingGPIOSENSORSlegacyformat;
  settingGPIOSENSORSpin = doc[F("GPIOSENSORSpin")] | settingGPIOSENSORSpin;
  settingGPIOSENSORSinterval = doc[F("GPIOSENSORSinterval")] | settingGPIOSENSORSinterval;
  CHANGE_INTERVAL_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS); 
  settingS0COUNTERenabled = doc[F("S0COUNTERenabled")] | settingS0COUNTERenabled;
  settingS0COUNTERpin = doc[F("S0COUNTERpin")] | settingS0COUNTERpin;
  settingS0COUNTERdebouncetime = doc[F("S0COUNTERdebouncetime")] | settingS0COUNTERdebouncetime;
  settingS0COUNTERpulsekw = doc[F("S0COUNTERpulsekw")] | settingS0COUNTERpulsekw;
  settingS0COUNTERinterval = doc[F("S0COUNTERinterval")] | settingS0COUNTERinterval;
  CHANGE_INTERVAL_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS); 
  settingOTGWcommandenable = doc[F("OTGWcommandenable")] | settingOTGWcommandenable;
  strlcpy(settingOTGWcommands, doc[F("OTGWcommands")] | "", sizeof(settingOTGWcommands));
  if (strcmp_P(settingOTGWcommands, PSTR("null"))==0) settingOTGWcommands[0] = 0;
  settingGPIOOUTPUTSenabled = doc[F("GPIOOUTPUTSenabled")] | settingGPIOOUTPUTSenabled;
  settingGPIOOUTPUTSpin = doc[F("GPIOOUTPUTSpin")] | settingGPIOOUTPUTSpin;
  settingGPIOOUTPUTStriggerBit = doc[F("GPIOOUTPUTStriggerBit")] | settingGPIOOUTPUTStriggerBit;
  
  // Dallas sensor labels now stored in /dallas_labels.json (not in settings.json)

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf(PSTR("Hostname              : %s\r\n"), CSTR(settingHostname));
    Debugf(PSTR("MQTT enabled          : %s\r\n"), CBOOLEAN(settingMQTTenable));
    Debugf(PSTR("MQTT broker           : %s\r\n"), CSTR(settingMQTTbroker));
    Debugf(PSTR("MQTT port             : %d\r\n"), settingMQTTbrokerPort);
    Debugf(PSTR("MQTT username         : %s\r\n"), CSTR(settingMQTTuser));
    Debugf(PSTR("MQTT password         : %s\r\n"), CSTR(settingMQTTpasswd));
    Debugf(PSTR("MQTT toptopic         : %s\r\n"), CSTR(settingMQTTtopTopic));
    Debugf(PSTR("MQTT uniqueid         : %s\r\n"), CSTR(settingMQTTuniqueid));
    Debugf(PSTR("HA prefix             : %s\r\n"), CSTR(settingMQTThaprefix));
    Debugf(PSTR("HA reboot detection   : %s\r\n"), CBOOLEAN(settingMQTTharebootdetection));
    Debugf(PSTR("NTP enabled           : %s\r\n"), CBOOLEAN(settingNTPenable));
    Debugf(PSTR("NPT timezone          : %s\r\n"), CSTR(settingNTPtimezone));
    Debugf(PSTR("NPT hostname          : %s\r\n"), CSTR(settingNTPhostname));
    Debugf(PSTR("NPT send time         : %s\r\n"), CBOOLEAN(settingNTPsendtime));
    Debugf(PSTR("Led Blink             : %s\r\n"), CBOOLEAN(settingLEDblink));
    Debugf(PSTR("GPIO Sensors          : %s\r\n"), CBOOLEAN(settingGPIOSENSORSenabled));
    Debugf(PSTR("GPIO Sen. Legacy      : %s\r\n"), CBOOLEAN(settingGPIOSENSORSlegacyformat));
    Debugf(PSTR("GPIO Sen. Pin         : %d\r\n"), settingGPIOSENSORSpin);
    Debugf(PSTR("GPIO Interval         : %d\r\n"), settingGPIOSENSORSinterval);
    Debugf(PSTR("S0 Counter            : %s\r\n"), CBOOLEAN(settingS0COUNTERenabled));
    Debugf(PSTR("S0 Counter Pin        : %d\r\n"), settingS0COUNTERpin);
    Debugf(PSTR("S0 Counter Debouncetime:%d\r\n"), settingS0COUNTERdebouncetime);
    Debugf(PSTR("S0 Counter Pulses/kw  : %d\r\n"), settingS0COUNTERpulsekw);
    Debugf(PSTR("S0 Counter Interval   : %d\r\n"), settingS0COUNTERinterval);
    Debugf(PSTR("OTGW boot cmd enabled : %s\r\n"), CBOOLEAN(settingOTGWcommandenable));
    Debugf(PSTR("OTGW boot cmd         : %s\r\n"), CSTR(settingOTGWcommands));
    Debugf(PSTR("GPIO Outputs          : %s\r\n"), CBOOLEAN(settingGPIOOUTPUTSenabled));
    Debugf(PSTR("GPIO Out. Pin         : %d\r\n"), settingGPIOOUTPUTSpin);
    Debugf(PSTR("GPIO Out. Trg. Bit    : %d\r\n"), settingGPIOOUTPUTStriggerBit);
    }
  
  Debugln(F("-\r\n"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{ //do not just trust the caller to do the right thing, server side validation is here!
  DebugTf(PSTR("-> field[%s], newValue[%s]\r\n"), field, newValue);

  if (strcasecmp_P(field, PSTR("hostname"))==0) 
  { //make sure we have a valid hostname here...
    strlcpy(settingHostname, newValue, sizeof(settingHostname));
    if (strlen(settingHostname)==0) snprintf_P(settingHostname, sizeof(settingHostname), PSTR("OTGW-%06x"), (unsigned int)ESP.getChipId());
    
    //strip away anything beyond the dot
    char *dot = strchr(settingMQTTtopTopic, '.');
    if (dot) *dot = '\0';
    
    // Defer MDNS/LLMNR and MQTT restart to flushSettings()
    pendingSideEffects |= SIDE_EFFECT_MDNS | SIDE_EFFECT_MQTT;

    Debugln();
    DebugTf(PSTR("Need reboot before new %s.local will be available!\r\n\n"), settingHostname);
  }
  
  if (strcasecmp_P(field, PSTR("MQTTenable"))==0)      settingMQTTenable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("MQTTbroker")) == 0)    strlcpy(settingMQTTbroker, newValue, sizeof(settingMQTTbroker));
  if (strcasecmp_P(field, PSTR("MQTTbrokerPort"))==0)  settingMQTTbrokerPort = atoi(newValue);
  if (strcasecmp_P(field, PSTR("MQTTuser"))==0) {
    strlcpy(settingMQTTuser, newValue, sizeof(settingMQTTuser));
    // Trim leading/trailing whitespace from username
    char* trimmedUser = trimwhitespace(settingMQTTuser);
    if (trimmedUser != settingMQTTuser) {
      memmove(settingMQTTuser, trimmedUser, strlen(trimmedUser) + 1);
    }
  }
  if (strcasecmp_P(field, PSTR("MQTTpasswd"))==0){
    if ( newValue && strcasecmp_P(newValue, PSTR("notthepassword")) != 0 ){
      strlcpy(settingMQTTpasswd, newValue, sizeof(settingMQTTpasswd));
      // Trim leading/trailing whitespace from password
      char* trimmedPasswd = trimwhitespace(settingMQTTpasswd);
      if (trimmedPasswd != settingMQTTpasswd) {
        memmove(settingMQTTpasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);
      }
    }
  }
  if (strcasecmp_P(field, PSTR("MQTTtoptopic"))==0)    {
    strlcpy(settingMQTTtopTopic, newValue, sizeof(settingMQTTtopTopic));
    if (strlen(settingMQTTtopTopic)==0)    {
      strlcpy(settingMQTTtopTopic, _HOSTNAME, sizeof(settingMQTTtopTopic));
      for(int i = 0; settingMQTTtopTopic[i]; i++) settingMQTTtopTopic[i] = tolower(settingMQTTtopTopic[i]);
    }
  }
  if (strcasecmp_P(field, PSTR("MQTThaprefix"))==0)    {
    strlcpy(settingMQTThaprefix, newValue, sizeof(settingMQTThaprefix));
    if (strlen(settingMQTThaprefix)==0)    strlcpy(settingMQTThaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settingMQTThaprefix));
  }
  if (strcasecmp_P(field, PSTR("MQTTharebootdetection"))==0)      settingMQTTharebootdetection = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("MQTTuniqueid")) == 0)  {
    strlcpy(settingMQTTuniqueid, newValue, sizeof(settingMQTTuniqueid));     
    if (strlen(settingMQTTuniqueid) == 0)   strlcpy(settingMQTTuniqueid, getUniqueId(), sizeof(settingMQTTuniqueid));
  }
  if (strcasecmp_P(field, PSTR("MQTTOTmessage"))==0)   settingMQTTOTmessage = EVALBOOLEAN(newValue);
  if (strstr_P(field, PSTR("mqtt")) != NULL)        pendingSideEffects |= SIDE_EFFECT_MQTT; // defer MQTT restart to flushSettings()
  
  if (strcasecmp_P(field, PSTR("NTPenable"))==0)      settingNTPenable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("NTPhostname"))==0)    {
    strlcpy(settingNTPhostname, newValue, sizeof(settingNTPhostname)); 
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  if (strcasecmp_P(field, PSTR("NTPtimezone"))==0)    {
    strlcpy(settingNTPtimezone, newValue, sizeof(settingNTPtimezone));
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  if (strcasecmp_P(field, PSTR("NTPsendtime"))==0)    settingNTPsendtime = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("LEDblink"))==0)      settingLEDblink = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("darktheme"))==0)     settingDarkTheme = EVALBOOLEAN(newValue);
  
  if (strcasecmp_P(field, PSTR("ui_autoscroll"))==0)      settingUIAutoScroll = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_timestamps"))==0)      settingUIShowTimestamp = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_capture"))==0)         settingUICaptureMode = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autoscreenshot"))==0)  settingUIAutoScreenshot = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autodownloadlog"))==0) settingUIAutoDownloadLog = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autoexport"))==0)      settingUIAutoExport = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_graphtimewindow"))==0) settingUIGraphTimeWindow = atoi(newValue);

  if (strcasecmp_P(field, PSTR("GPIOSENSORSenabled")) == 0)
  {
    settingGPIOSENSORSenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO SENSORS will search for sensors on pin GPIO%d!\r\n\n"), settingGPIOSENSORSpin);
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSlegacyformat")) == 0)
  {
    settingGPIOSENSORSlegacyformat = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Updated GPIO Sensors Legacy Format to %s\r\n\n"), CBOOLEAN(settingGPIOSENSORSlegacyformat));
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSpin")) == 0)    
  {
    int newPin = atoi(newValue);
    if (checkGPIOConflict(newPin, PSTR("sensor"))) {
      DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
    }
    settingGPIOSENSORSpin = newPin;
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO SENSORS will use new pin GPIO%d!\r\n\n"), settingGPIOSENSORSpin);
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSinterval")) == 0) {
    settingGPIOSENSORSinterval = atoi(newValue);
    CHANGE_INTERVAL_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS); 
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERenabled")) == 0)
  {
    settingS0COUNTERenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before S0 Counter starts counting on pin GPIO%d!\r\n\n"), settingS0COUNTERpin);
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERpin")) == 0)    
  {
    int newPin = atoi(newValue);
    if (checkGPIOConflict(newPin, PSTR("s0"))) {
      DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
    }
    settingS0COUNTERpin = newPin;
    Debugln();
    DebugTf(PSTR("Need reboot before S0 Counter will use new pin GPIO%d!\r\n\n"), settingS0COUNTERpin);
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERdebouncetime")) == 0) settingS0COUNTERdebouncetime = atoi(newValue);
  if (strcasecmp_P(field, PSTR("S0COUNTERpulsekw")) == 0)      settingS0COUNTERpulsekw = atoi(newValue);

  if (strcasecmp_P(field, PSTR("S0COUNTERinterval")) == 0) {
    settingS0COUNTERinterval = atoi(newValue);
    CHANGE_INTERVAL_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS); 
  }
  if (strcasecmp_P(field, PSTR("OTGWcommandenable"))==0)    settingOTGWcommandenable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("OTGWcommands"))==0)         strlcpy(settingOTGWcommands, newValue, sizeof(settingOTGWcommands));
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTSenabled")) == 0)
  {
    settingGPIOOUTPUTSenabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will be enabled on pin GPIO%d!\r\n\n"), settingGPIOOUTPUTSenabled);
  }
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTSpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (checkGPIOConflict(newPin, PSTR("output"))) {
      DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
    }
    settingGPIOOUTPUTSpin = newPin;
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new pin GPIO%d!\r\n\n"), settingGPIOOUTPUTSpin);
  }
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTStriggerBit")) == 0)
  {
    settingGPIOOUTPUTStriggerBit = atoi(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new trigger bit %d!\r\n\n"), settingGPIOOUTPUTStriggerBit);
  }

  // Mark settings dirty and restart debounce timer — actual write + service
  // restarts are deferred to flushSettings() which runs from loop() timer.
  // This coalesces multiple field updates into a single flash write and at most
  // one restart per service (Finding #23: reduce flash wear + MQTT churn).
  settingsDirty = true;
  RESTART_TIMER(timerFlushSettings);

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
