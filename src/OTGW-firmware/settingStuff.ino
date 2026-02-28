/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v1.2.0-beta
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
// Streaming JSON write helpers — write each field directly to the open file.
// No heap allocation; cMsg is used only as a scratch buffer for numeric formatting.
// All key literals are in PROGMEM (PGM_P).
// Forward declarations with default parameters
void wStrF(File &f, PGM_P key, const char *val, bool last = false);
void wBoolF(File &f, PGM_P key, bool val, bool last = false);
void wIntF(File &f, PGM_P key, long val, bool last = false);
void applySettingFromFile(const char *key, const char *val);
void parseSettingsLine();

void wStrF(File &f, PGM_P key, const char *val, bool last) {
  f.print(F("  \""));
  f.print(FPSTR(key));
  f.print(F("\": \""));
  for (const char *p = val; *p; p++) {
    if      (*p == '"')  { f.write('\\'); f.write('"');  }
    else if (*p == '\\') { f.write('\\'); f.write('\\'); }
    else if (*p == '\n') { f.print(F("\\n")); }
    else if (*p == '\r') { f.print(F("\\r")); }
    else if (*p == '\t') { f.print(F("\\t")); }
    else                 { f.write((uint8_t)*p); }
  }
  f.print(last ? F("\"\r\n") : F("\",\r\n"));
}

void wBoolF(File &f, PGM_P key, bool val, bool last) {
  f.print(F("  \""));
  f.print(FPSTR(key));
  f.print(val ? F("\": true") : F("\": false"));
  f.print(last ? F("\r\n") : F(",\r\n"));
}

void wIntF(File &f, PGM_P key, long val, bool last) {
  f.print(F("  \""));
  f.print(FPSTR(key));
  f.print(F("\": "));
  snprintf_P(cMsg, sizeof(cMsg), PSTR("%ld"), val);
  f.print(cMsg);
  f.print(last ? F("\r\n") : F(",\r\n"));
}

//=======================================================================
void writeSettings(bool show)
{
  DebugTf(PSTR("[Settings] Writing to [%s] ..\r\n"), SETTINGS_FILE);
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (!file)
  {
    DebugTf(PSTR("[Settings] Error: open(%s, 'w') FAILED!!! --> Bailout\r\n"), SETTINGS_FILE);
    return;
  }
  yield();

  file.print(F("{\r\n"));
  wStrF (file, PSTR("hostname"),                settingHostname);
  wBoolF(file, PSTR("MQTTenable"),              settingMQTTenable);
  wStrF (file, PSTR("MQTTbroker"),              settingMQTTbroker);
  wIntF (file, PSTR("MQTTbrokerPort"),          settingMQTTbrokerPort);
  wStrF (file, PSTR("MQTTuser"),                settingMQTTuser);
  wStrF (file, PSTR("MQTTpasswd"),              settingMQTTpasswd);
  wStrF (file, PSTR("MQTTtoptopic"),            settingMQTTtopTopic);
  wStrF (file, PSTR("MQTThaprefix"),            settingMQTThaprefix);
  wStrF (file, PSTR("MQTTuniqueid"),            settingMQTTuniqueid);
  wBoolF(file, PSTR("MQTTOTmessage"),           settingMQTTOTmessage);
  wBoolF(file, PSTR("MQTTharebootdetection"),   settingMQTTharebootdetection);
  wBoolF(file, PSTR("NTPenable"),               settingNTPenable);
  wStrF (file, PSTR("NTPtimezone"),             settingNTPtimezone);
  wStrF (file, PSTR("NTPhostname"),             settingNTPhostname);
  wBoolF(file, PSTR("NTPsendtime"),             settingNTPsendtime);
  wBoolF(file, PSTR("LEDblink"),                settingLEDblink);
  wBoolF(file, PSTR("darktheme"),               settingDarkTheme);
  wBoolF(file, PSTR("ui_autoscroll"),           settingUIAutoScroll);
  wBoolF(file, PSTR("ui_timestamps"),           settingUIShowTimestamp);
  wBoolF(file, PSTR("ui_capture"),              settingUICaptureMode);
  wBoolF(file, PSTR("ui_autoscreenshot"),       settingUIAutoScreenshot);
  wBoolF(file, PSTR("ui_autodownloadlog"),      settingUIAutoDownloadLog);
  wBoolF(file, PSTR("ui_autoexport"),           settingUIAutoExport);
  wIntF (file, PSTR("ui_graphtimewindow"),      settingUIGraphTimeWindow);
  wBoolF(file, PSTR("GPIOSENSORSenabled"),      settingGPIOSENSORSenabled);
  wBoolF(file, PSTR("GPIOSENSORSlegacyformat"), settingGPIOSENSORSlegacyformat);
  wIntF (file, PSTR("GPIOSENSORSpin"),          settingGPIOSENSORSpin);
  wIntF (file, PSTR("GPIOSENSORSinterval"),     settingGPIOSENSORSinterval);
  wBoolF(file, PSTR("S0COUNTERenabled"),        settingS0COUNTERenabled);
  wIntF (file, PSTR("S0COUNTERpin"),            settingS0COUNTERpin);
  wIntF (file, PSTR("S0COUNTERdebouncetime"),   settingS0COUNTERdebouncetime);
  wIntF (file, PSTR("S0COUNTERpulsekw"),        settingS0COUNTERpulsekw);
  wIntF (file, PSTR("S0COUNTERinterval"),       settingS0COUNTERinterval);
  wBoolF(file, PSTR("OTGWcommandenable"),       settingOTGWcommandenable);
  wStrF (file, PSTR("OTGWcommands"),            settingOTGWcommands);
  wBoolF(file, PSTR("GPIOOUTPUTSenabled"),      settingGPIOOUTPUTSenabled);
  wIntF (file, PSTR("GPIOOUTPUTSpin"),          settingGPIOOUTPUTSpin);
  wIntF (file, PSTR("GPIOOUTPUTStriggerBit"),   settingGPIOOUTPUTStriggerBit);
  wBoolF(file, PSTR("WebhookEnabled"),          settingWebhookEnabled);
  wStrF (file, PSTR("WebhookURLon"),            settingWebhookURLon);
  wStrF (file, PSTR("WebhookURLoff"),           settingWebhookURLoff);
  wIntF (file, PSTR("WebhookTriggerBit"),       settingWebhookTriggerBit, true); // last field
  file.print(F("}\r\n"));

  file.close();
  DebugTf(PSTR("[Settings] State: Settings saved successfully to %s\r\n"), SETTINGS_FILE);

} // writeSettings()


//=======================================================================
// Streaming JSON read helpers — parse one key-value pair from cMsg in-place.

// Apply one key/value pair loaded from the settings file to the in-memory settings.
// rawVal is already unescaped (for string values) or the raw token (bool/int).
void applySettingFromFile(const char *key, const char *val) {
  if (strcasecmp_P(key, PSTR("hostname")) == 0)                { strlcpy(settingHostname, val, sizeof(settingHostname)); return; }
  if (strcasecmp_P(key, PSTR("MQTTenable")) == 0)              { settingMQTTenable = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("MQTTbroker")) == 0)              { strlcpy(settingMQTTbroker, val, sizeof(settingMQTTbroker)); return; }
  if (strcasecmp_P(key, PSTR("MQTTbrokerPort")) == 0)          { settingMQTTbrokerPort = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("MQTTuser")) == 0)                { strlcpy(settingMQTTuser, val, sizeof(settingMQTTuser)); return; }
  if (strcasecmp_P(key, PSTR("MQTTpasswd")) == 0)              { strlcpy(settingMQTTpasswd, val, sizeof(settingMQTTpasswd)); return; }
  if (strcasecmp_P(key, PSTR("MQTTtoptopic")) == 0)            { strlcpy(settingMQTTtopTopic, val, sizeof(settingMQTTtopTopic)); return; }
  if (strcasecmp_P(key, PSTR("MQTThaprefix")) == 0)            { strlcpy(settingMQTThaprefix, val, sizeof(settingMQTThaprefix)); return; }
  if (strcasecmp_P(key, PSTR("MQTTuniqueid")) == 0)            { strlcpy(settingMQTTuniqueid, val, sizeof(settingMQTTuniqueid)); return; }
  if (strcasecmp_P(key, PSTR("MQTTOTmessage")) == 0)           { settingMQTTOTmessage = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("MQTTharebootdetection")) == 0)   { settingMQTTharebootdetection = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("NTPenable")) == 0)               { settingNTPenable = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("NTPtimezone")) == 0)             { strlcpy(settingNTPtimezone, val, sizeof(settingNTPtimezone)); return; }
  if (strcasecmp_P(key, PSTR("NTPhostname")) == 0)             { strlcpy(settingNTPhostname, val, sizeof(settingNTPhostname)); return; }
  if (strcasecmp_P(key, PSTR("NTPsendtime")) == 0)             { settingNTPsendtime = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("LEDblink")) == 0)                { settingLEDblink = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("darktheme")) == 0)               { settingDarkTheme = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_autoscroll")) == 0)           { settingUIAutoScroll = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_timestamps")) == 0)           { settingUIShowTimestamp = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_capture")) == 0)              { settingUICaptureMode = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_autoscreenshot")) == 0)       { settingUIAutoScreenshot = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_autodownloadlog")) == 0)      { settingUIAutoDownloadLog = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_autoexport")) == 0)           { settingUIAutoExport = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("ui_graphtimewindow")) == 0)      { settingUIGraphTimeWindow = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOSENSORSenabled")) == 0)      { settingGPIOSENSORSenabled = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOSENSORSlegacyformat")) == 0) { settingGPIOSENSORSlegacyformat = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOSENSORSpin")) == 0)          { settingGPIOSENSORSpin = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOSENSORSinterval")) == 0)     { settingGPIOSENSORSinterval = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("S0COUNTERenabled")) == 0)        { settingS0COUNTERenabled = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("S0COUNTERpin")) == 0)            { settingS0COUNTERpin = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("S0COUNTERdebouncetime")) == 0)   { settingS0COUNTERdebouncetime = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("S0COUNTERpulsekw")) == 0)        { settingS0COUNTERpulsekw = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("S0COUNTERinterval")) == 0)       { settingS0COUNTERinterval = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("OTGWcommandenable")) == 0)       { settingOTGWcommandenable = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("OTGWcommands")) == 0)            { strlcpy(settingOTGWcommands, val, sizeof(settingOTGWcommands)); return; }
  if (strcasecmp_P(key, PSTR("GPIOOUTPUTSenabled")) == 0)      { settingGPIOOUTPUTSenabled = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOOUTPUTSpin")) == 0)          { settingGPIOOUTPUTSpin = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("GPIOOUTPUTStriggerBit")) == 0)   { settingGPIOOUTPUTStriggerBit = atoi(val); return; }
  if (strcasecmp_P(key, PSTR("WebhookEnabled")) == 0)          { settingWebhookEnabled = EVALBOOLEAN(val); return; }
  if (strcasecmp_P(key, PSTR("WebhookURLon")) == 0)            { strlcpy(settingWebhookURLon, val, sizeof(settingWebhookURLon)); return; }
  if (strcasecmp_P(key, PSTR("WebhookURLoff")) == 0)           { strlcpy(settingWebhookURLoff, val, sizeof(settingWebhookURLoff)); return; }
  if (strcasecmp_P(key, PSTR("WebhookTriggerBit")) == 0)       { settingWebhookTriggerBit = atoi(val); return; }
}

// Parse one JSON line from cMsg and dispatch to applySettingFromFile().
// Handles:  "key": "string value",   "key": true,   "key": 123
// Modifies cMsg in-place (null-terminates key; unescapes string values).
void parseSettingsLine() {
  char *p = cMsg;
  while (*p == ' ' || *p == '\t') p++; // skip leading whitespace
  if (*p != '"') return;               // not a key-value line ({, }, empty)
  p++;                                 // skip opening '"'
  const char *key = p;
  while (*p && *p != '"') p++;         // find closing '"' of key
  if (!*p) return;
  *p++ = '\0';                         // null-terminate key
  while (*p == ':' || *p == ' ') p++;  // skip ': '

  const char *val;
  if (*p == '"') {
    // String value — unescape in-place
    p++;
    char *out = p;
    val = out;
    while (*p && *p != '"') {
      if (*p == '\\' && *(p + 1)) {
        p++;
        switch (*p) {
          case 'n':  *out++ = '\n'; break;
          case 'r':  *out++ = '\r'; break;
          case 't':  *out++ = '\t'; break;
          default:   *out++ = *p;  break; // handles \" and \\
        }
        p++;
      } else {
        *out++ = *p++;
      }
    }
    *out = '\0';
  } else {
    // Bool or integer — strip trailing ',' '\r' '\n'
    val = p;
    while (*p && *p != ',' && *p != '\r' && *p != '\n') p++;
    *p = '\0';
  }
  applySettingFromFile(key, val);
}

//=======================================================================
void readSettings(bool show)
{
  DebugTf(PSTR(" %s ..\r\n"), SETTINGS_FILE);
  if (!LittleFS.exists(SETTINGS_FILE))
  {  //create settings file if it does not exist yet.
    DebugTln(F(" .. file not found! --> created file!"));
    writeSettings(show);
    readSettings(false); //now it should work...
    return;
  }

  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file)
  {
    DebugTln(F("[Settings] Error: could not open settings file, using defaults."));
    return;
  }

  // Stream each line; parseSettingsLine() dispatches key/value to applySettingFromFile()
  while (file.available()) {
    int n = file.readBytesUntil('\n', cMsg, sizeof(cMsg) - 1);
    cMsg[n] = '\0';
    if (n > 0 && cMsg[n - 1] == '\r') cMsg[--n] = '\0';
    parseSettingsLine();
  }
  file.close();

  // Post-processing: apply defaults for any missing or empty values
  if (strlen(settingHostname) == 0) strlcpy(settingHostname, _HOSTNAME, sizeof(settingHostname));

  char *trimmedUser = trimwhitespace(settingMQTTuser);
  if (trimmedUser != settingMQTTuser) memmove(settingMQTTuser, trimmedUser, strlen(trimmedUser) + 1);
  char *trimmedPasswd = trimwhitespace(settingMQTTpasswd);
  if (trimmedPasswd != settingMQTTpasswd) memmove(settingMQTTpasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);

  if (strlen(settingMQTTtopTopic) == 0 || strcmp_P(settingMQTTtopTopic, PSTR("null")) == 0) {
    strlcpy(settingMQTTtopTopic, _HOSTNAME, sizeof(settingMQTTtopTopic));
    for (int i = 0; settingMQTTtopTopic[i]; i++) settingMQTTtopTopic[i] = tolower(settingMQTTtopTopic[i]);
  }
  if (strlen(settingMQTThaprefix) == 0 || strcmp_P(settingMQTThaprefix, PSTR("null")) == 0)
    strlcpy(settingMQTThaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settingMQTThaprefix));
  if (strlen(settingMQTTuniqueid) == 0 || strcmp_P(settingMQTTuniqueid, PSTR("null")) == 0)
    strlcpy(settingMQTTuniqueid, getUniqueId(), sizeof(settingMQTTuniqueid));
  if (strlen(settingNTPtimezone) == 0 || strcmp_P(settingNTPtimezone, PSTR("null")) == 0)
    strlcpy(settingNTPtimezone, "Europe/Amsterdam", sizeof(settingNTPtimezone));
  if (strlen(settingNTPhostname) == 0 || strcmp_P(settingNTPhostname, PSTR("null")) == 0)
    strlcpy(settingNTPhostname, NTP_HOST_DEFAULT, sizeof(settingNTPhostname));
  if (strcmp_P(settingOTGWcommands, PSTR("null")) == 0) settingOTGWcommands[0] = 0;

  CHANGE_INTERVAL_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS);
  CHANGE_INTERVAL_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS);

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf(PSTR("Hostname              : %s\r\n"), CSTR(settingHostname));
    Debugf(PSTR("MQTT enabled          : %s\r\n"), CBOOLEAN(settingMQTTenable));
    Debugf(PSTR("MQTT broker           : %s\r\n"), CSTR(settingMQTTbroker));
    Debugf(PSTR("MQTT port             : %d\r\n"), settingMQTTbrokerPort);
    Debugf(PSTR("MQTT username         : %s\r\n"), CSTR(settingMQTTuser));
    Debugf(PSTR("MQTT password set     : %s\r\n"), CBOOLEAN(settingMQTTpasswd[0] != '\0'));
    Debugf(PSTR("MQTT toptopic         : %s\r\n"), CSTR(settingMQTTtopTopic));
    Debugf(PSTR("MQTT uniqueid         : %s\r\n"), CSTR(settingMQTTuniqueid));
    Debugf(PSTR("MQTT separate sources : %s\r\n"), CBOOLEAN(settingMQTTSeparateSources));
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
    Debugf(PSTR("Webhook enabled       : %s\r\n"), CBOOLEAN(settingWebhookEnabled));
    Debugf(PSTR("Webhook URL ON        : %s\r\n"), CSTR(settingWebhookURLon));
    Debugf(PSTR("Webhook URL OFF       : %s\r\n"), CSTR(settingWebhookURLoff));
    Debugf(PSTR("Webhook Trigger Bit   : %d\r\n"), settingWebhookTriggerBit);
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
  if (strcasecmp_P(field, PSTR("MQTTseparatesources"))==0) settingMQTTSeparateSources = EVALBOOLEAN(newValue);
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
  if (strcasecmp_P(field, PSTR("webhookenable")) == 0 ||
      strcasecmp_P(field, PSTR("WebhookEnabled")) == 0) {
    settingWebhookEnabled = EVALBOOLEAN(newValue);
  }
  if (strcasecmp_P(field, PSTR("WebhookURLon")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurlon")) == 0)   strlcpy(settingWebhookURLon, newValue, sizeof(settingWebhookURLon));
  if (strcasecmp_P(field, PSTR("WebhookURLoff")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurloff")) == 0)  strlcpy(settingWebhookURLoff, newValue, sizeof(settingWebhookURLoff));
  if (strcasecmp_P(field, PSTR("WebhookTriggerBit")) == 0 ||
      strcasecmp_P(field, PSTR("webhooktriggerbit")) == 0) settingWebhookTriggerBit = constrain(atoi(newValue), 0, 15);

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
