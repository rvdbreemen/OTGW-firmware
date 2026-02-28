/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v1.3.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <ctype.h>

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
static bool parseJsonKVLine(const char* line, char* keyOut, size_t keyOutSize, char* valueOut, size_t valueOutSize)
{
  if (!line || !keyOut || keyOutSize == 0 || !valueOut || valueOutSize == 0) return false;
  keyOut[0] = '\0';
  valueOut[0] = '\0';

  const char* keyStart = strchr(line, '"');
  if (!keyStart) return false;
  keyStart++;
  const char* keyEnd = keyStart;
  while (*keyEnd) {
    if (*keyEnd == '\\') {
      if (*(keyEnd + 1) == '\0') return false;
      keyEnd += 2;
      continue;
    }
    if (*keyEnd == '"') break;
    keyEnd++;
  }
  if (*keyEnd != '"') return false;
  size_t keyLen = static_cast<size_t>(keyEnd - keyStart);
  if (keyLen == 0 || keyLen >= keyOutSize) return false;
  memcpy(keyOut, keyStart, keyLen);
  keyOut[keyLen] = '\0';

  const char* p = keyEnd + 1;
  while (*p && isspace(static_cast<unsigned char>(*p))) p++;
  if (*p != ':') return false;
  p++;
  while (*p && isspace(static_cast<unsigned char>(*p))) p++;

  if (*p == '"') {
    p++;
    size_t n = 0;
    while (*p && n + 1 < valueOutSize) {
      if (*p == '\\') {
        if (*(p + 1) == '\0') return false;
        p++;
        switch (*p) {
          case '"': valueOut[n++] = '"'; break;
          case '\\': valueOut[n++] = '\\'; break;
          case '/': valueOut[n++] = '/'; break;
          case 'b': valueOut[n++] = '\b'; break;
          case 'f': valueOut[n++] = '\f'; break;
          case 'n': valueOut[n++] = '\n'; break;
          case 'r': valueOut[n++] = '\r'; break;
          case 't': valueOut[n++] = '\t'; break;
          default: valueOut[n++] = *p; break;
        }
        p++;
        continue;
      }
      if (*p == '"') break;
      valueOut[n++] = *p++;
    }
    valueOut[n] = '\0';
    return true;
  }

  const char* start = p;
  while (*p && *p != ',' && *p != '}' && !isspace(static_cast<unsigned char>(*p))) p++;
  size_t len = static_cast<size_t>(p - start);
  if (len == 0) return false;
  if (len >= valueOutSize) len = valueOutSize - 1;
  memcpy(valueOut, start, len);
  valueOut[len] = '\0';
  return true;
}

static void writeJsonStringKV(File& file, const __FlashStringHelper* key, const char* value, bool withComma)
{
  String escaped = escapeJsonString(value);
  file.printf_P(PSTR("  \"%S\": \"%s\"%s\n"),
                reinterpret_cast<PGM_P>(key),
                escaped.c_str(),
                withComma ? "," : "");
}

static void writeJsonBoolKV(File& file, const __FlashStringHelper* key, bool value, bool withComma)
{
  file.printf_P(PSTR("  \"%S\": %s%s\n"),
                reinterpret_cast<PGM_P>(key),
                value ? "true" : "false",
                withComma ? "," : "");
}

static void writeJsonIntKV(File& file, const __FlashStringHelper* key, int value, bool withComma)
{
  file.printf_P(PSTR("  \"%S\": %d%s\n"),
                reinterpret_cast<PGM_P>(key),
                value,
                withComma ? "," : "");
}

//=======================================================================
void writeSettings(bool show)
{

  DebugTf(PSTR("[Settings] State: writeSettings called (show=%s)\r\n"), show ? "true" : "false");
  DebugTf(PSTR("[Settings] Writing to [%s] ..\r\n"), SETTINGS_FILE);
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (!file)
  {
    DebugTf(PSTR("[Settings] Error: open(%s, 'w') FAILED!!! --> Bailout\r\n"), SETTINGS_FILE);
    return;
  }
  yield();

  DebugT(F("[Settings] State: Writing JSON settings... "));

  file.print(F("{\n"));
  writeJsonStringKV(file, F("hostname"), settingHostname, true);
  writeJsonBoolKV(file, F("MQTTenable"), settingMQTTenable, true);
  writeJsonStringKV(file, F("MQTTbroker"), settingMQTTbroker, true);
  writeJsonIntKV(file, F("MQTTbrokerPort"), settingMQTTbrokerPort, true);
  writeJsonStringKV(file, F("MQTTuser"), settingMQTTuser, true);
  writeJsonStringKV(file, F("MQTTpasswd"), settingMQTTpasswd, true);
  writeJsonStringKV(file, F("MQTTtoptopic"), settingMQTTtopTopic, true);
  writeJsonStringKV(file, F("MQTThaprefix"), settingMQTThaprefix, true);
  writeJsonStringKV(file, F("MQTTuniqueid"), settingMQTTuniqueid, true);
  writeJsonBoolKV(file, F("MQTTOTmessage"), settingMQTTOTmessage, true);
  writeJsonBoolKV(file, F("MQTTseparatesources"), settingMQTTSeparateSources, true);
  writeJsonBoolKV(file, F("MQTTharebootdetection"), settingMQTTharebootdetection, true);
  writeJsonBoolKV(file, F("NTPenable"), settingNTPenable, true);
  writeJsonStringKV(file, F("NTPtimezone"), settingNTPtimezone, true);
  writeJsonStringKV(file, F("NTPhostname"), settingNTPhostname, true);
  writeJsonBoolKV(file, F("NTPsendtime"), settingNTPsendtime, true);
  writeJsonBoolKV(file, F("LEDblink"), settingLEDblink, true);
  writeJsonBoolKV(file, F("darktheme"), settingDarkTheme, true);
  writeJsonBoolKV(file, F("ui_autoscroll"), settingUIAutoScroll, true);
  writeJsonBoolKV(file, F("ui_timestamps"), settingUIShowTimestamp, true);
  writeJsonBoolKV(file, F("ui_capture"), settingUICaptureMode, true);
  writeJsonBoolKV(file, F("ui_autoscreenshot"), settingUIAutoScreenshot, true);
  writeJsonBoolKV(file, F("ui_autodownloadlog"), settingUIAutoDownloadLog, true);
  writeJsonBoolKV(file, F("ui_autoexport"), settingUIAutoExport, true);
  writeJsonIntKV(file, F("ui_graphtimewindow"), settingUIGraphTimeWindow, true);
  writeJsonBoolKV(file, F("GPIOSENSORSenabled"), settingGPIOSENSORSenabled, true);
  writeJsonBoolKV(file, F("GPIOSENSORSlegacyformat"), settingGPIOSENSORSlegacyformat, true);
  writeJsonIntKV(file, F("GPIOSENSORSpin"), settingGPIOSENSORSpin, true);
  writeJsonIntKV(file, F("GPIOSENSORSinterval"), settingGPIOSENSORSinterval, true);
  writeJsonBoolKV(file, F("S0COUNTERenabled"), settingS0COUNTERenabled, true);
  writeJsonIntKV(file, F("S0COUNTERpin"), settingS0COUNTERpin, true);
  writeJsonIntKV(file, F("S0COUNTERdebouncetime"), settingS0COUNTERdebouncetime, true);
  writeJsonIntKV(file, F("S0COUNTERpulsekw"), settingS0COUNTERpulsekw, true);
  writeJsonIntKV(file, F("S0COUNTERinterval"), settingS0COUNTERinterval, true);
  writeJsonBoolKV(file, F("OTGWcommandenable"), settingOTGWcommandenable, true);
  writeJsonStringKV(file, F("OTGWcommands"), settingOTGWcommands, true);
  writeJsonBoolKV(file, F("GPIOOUTPUTSenabled"), settingGPIOOUTPUTSenabled, true);
  writeJsonIntKV(file, F("GPIOOUTPUTSpin"), settingGPIOOUTPUTSpin, true);
  writeJsonIntKV(file, F("GPIOOUTPUTStriggerBit"), settingGPIOOUTPUTStriggerBit, true);
  writeJsonBoolKV(file, F("WebhookEnabled"), settingWebhookEnabled, true);
  writeJsonStringKV(file, F("WebhookURLon"), settingWebhookURLon, true);
  writeJsonStringKV(file, F("WebhookURLoff"), settingWebhookURLoff, true);
  writeJsonIntKV(file, F("WebhookTriggerBit"), settingWebhookTriggerBit, true);
  writeJsonStringKV(file, F("WebhookPayload"), settingWebhookPayload, true);
  writeJsonStringKV(file, F("WebhookContentType"), settingWebhookContentType, false);
  file.print(F("}\n"));

  if (show) {
    DebugTln(F("\r\n[Settings] JSON content:"));
    File showFile = LittleFS.open(SETTINGS_FILE, "r");
    while (showFile && showFile.available()) {
      TelnetStream.write(showFile.read());
    }
    if (showFile) showFile.close();
  }
  Debugln(F("\r\n[Settings] State: File write complete, closing file"));
  file.close();
  DebugTf(PSTR("[Settings] State: Settings saved successfully to %s\r\n"), SETTINGS_FILE);

} // writeSettings()


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
  if (!file) {
    DebugTln(F("Failed to open settings file, use existing defaults."));
    return;
  }
  if (file.size() == 0) {
    file.close();
    DebugTln(F("Settings file is empty, use existing defaults."));
    return;
  }
  static const size_t JSON_LINE_BUF = 256;
  char lineBuf[JSON_LINE_BUF];
  char keyBuf[64];
  char valueBuf[201]; // must fit the largest setting value (WebhookPayload: 201 bytes)

  while (file.available()) {
    size_t len = file.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
    lineBuf[len] = '\0';
    if (len == (sizeof(lineBuf) - 1)) {
      char discardBuf[32];
      while (file.available()) {
        size_t chunkLen = file.readBytesUntil('\n', discardBuf, sizeof(discardBuf) - 1);
        if (chunkLen < (sizeof(discardBuf) - 1)) break;
        yield();
      }
    }

    if (parseJsonKVLine(lineBuf, keyBuf, sizeof(keyBuf), valueBuf, sizeof(valueBuf))) {
      updateSetting(keyBuf, valueBuf);
    }
  }
  file.close();

  // Loading from file must NOT trigger a rewrite or service restarts —
  // clear any dirty/side-effect state set by updateSetting() above.
  settingsDirty = false;
  pendingSideEffects = 0;

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
    Debugf(PSTR("Webhook Payload       : %s\r\n"), CSTR(settingWebhookPayload));
    Debugf(PSTR("Webhook ContentType   : %s\r\n"), CSTR(settingWebhookContentType));
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
    char *dot = strchr(settingHostname, '.');
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
  if (strcasecmp_P(field, PSTR("WebhookPayload")) == 0 ||
      strcasecmp_P(field, PSTR("webhookpayload")) == 0)    strlcpy(settingWebhookPayload, newValue, sizeof(settingWebhookPayload));
  if (strcasecmp_P(field, PSTR("WebhookContentType")) == 0 ||
      strcasecmp_P(field, PSTR("webhookcontenttype")) == 0) strlcpy(settingWebhookContentType, newValue, sizeof(settingWebhookContentType));

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
