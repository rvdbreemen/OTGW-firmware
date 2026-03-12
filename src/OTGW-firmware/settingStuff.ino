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
    startMDNS(settings.sHostname);
    startLLMNR(settings.sHostname);
  }
  if ((pendingSideEffects & SIDE_EFFECT_MQTT) && settings.mqtt.bEnable) {
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
  if (strcasecmp_P(caller, PSTR("sensor")) != 0 && pin == settings.sensors.iPin && settings.sensors.iPin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by SENSORS\r\n"), pin);
    conflict = true;
  }
  if (strcasecmp_P(caller, PSTR("s0")) != 0 && pin == settings.s0.iPin && settings.s0.iPin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by S0 Counter\r\n"), pin);
    conflict = true;
  }
  if (strcasecmp_P(caller, PSTR("output")) != 0 && pin == settings.outputs.iPin && settings.outputs.iPin >= 0) {
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
  // Use global cMsg as escape scratch — no heap allocation.
  // writeSettings() holds no yield() between calls, so cMsg cannot be clobbered mid-write.
  escapeJsonStringTo(value, cMsg, sizeof(cMsg));
  file.printf_P(PSTR("  \"%S\": \"%s\"%s\n"),
                reinterpret_cast<PGM_P>(key),
                cMsg,
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
  writeJsonStringKV(file, F("hostname"), settings.sHostname, true);
  writeJsonBoolKV(file, F("MQTTenable"), settings.mqtt.bEnable, true);
  writeJsonStringKV(file, F("MQTTbroker"), settings.mqtt.sBroker, true);
  writeJsonIntKV(file, F("MQTTbrokerPort"), settings.mqtt.iBrokerPort, true);
  writeJsonStringKV(file, F("MQTTuser"), settings.mqtt.sUser, true);
  writeJsonStringKV(file, F("MQTTpasswd"), settings.mqtt.sPasswd, true);
  writeJsonStringKV(file, F("MQTTtoptopic"), settings.mqtt.sTopTopic, true);
  writeJsonStringKV(file, F("MQTThaprefix"), settings.mqtt.sHaprefix, true);
  writeJsonStringKV(file, F("MQTTuniqueid"), settings.mqtt.sUniqueid, true);
  writeJsonBoolKV(file, F("MQTTOTmessage"), settings.mqtt.bOTmessage, true);
  writeJsonIntKV(file, F("MQTTinterval"), settings.mqtt.iInterval, true);
  writeJsonBoolKV(file, F("MQTTseparatesources"), settings.mqtt.bSeparateSources, true);
  writeJsonBoolKV(file, F("MQTTharebootdetection"), settings.mqtt.bHaRebootDetect, true);
  writeJsonBoolKV(file, F("NTPenable"), settings.ntp.bEnable, true);
  writeJsonStringKV(file, F("NTPtimezone"), settings.ntp.sTimezone, true);
  writeJsonStringKV(file, F("NTPhostname"), settings.ntp.sHostname, true);
  writeJsonBoolKV(file, F("NTPsendtime"), settings.ntp.bSendtime, true);
  writeJsonBoolKV(file, F("LEDblink"), settings.bLEDblink, true);
  writeJsonBoolKV(file, F("darktheme"), settings.bDarkTheme, true);
  writeJsonBoolKV(file, F("ui_autoscroll"), settings.ui.bAutoScroll, true);
  writeJsonBoolKV(file, F("ui_timestamps"), settings.ui.bShowTimestamp, true);
  writeJsonBoolKV(file, F("ui_capture"), settings.ui.bCaptureMode, true);
  writeJsonBoolKV(file, F("ui_autoscreenshot"), settings.ui.bAutoScreenshot, true);
  writeJsonBoolKV(file, F("ui_autodownloadlog"), settings.ui.bAutoDownloadLog, true);
  writeJsonBoolKV(file, F("ui_autoexport"), settings.ui.bAutoExport, true);
  writeJsonIntKV(file, F("ui_graphtimewindow"), settings.ui.iGraphTimeWindow, true);
  writeJsonBoolKV(file, F("GPIOSENSORSenabled"), settings.sensors.bEnabled, true);
  writeJsonBoolKV(file, F("GPIOSENSORSlegacyformat"), settings.sensors.bLegacyFormat, true);
  writeJsonIntKV(file, F("GPIOSENSORSpin"), settings.sensors.iPin, true);
  writeJsonIntKV(file, F("GPIOSENSORSinterval"), settings.sensors.iInterval, true);
  writeJsonBoolKV(file, F("S0COUNTERenabled"), settings.s0.bEnabled, true);
  writeJsonIntKV(file, F("S0COUNTERpin"), settings.s0.iPin, true);
  writeJsonIntKV(file, F("S0COUNTERdebouncetime"), settings.s0.iDebounceTime, true);
  writeJsonIntKV(file, F("S0COUNTERpulsekw"), settings.s0.iPulsekw, true);
  writeJsonIntKV(file, F("S0COUNTERinterval"), settings.s0.iInterval, true);
  writeJsonBoolKV(file, F("OTGWcommandenable"), settings.otgw.bEnable, true);
  writeJsonStringKV(file, F("OTGWcommands"), settings.otgw.sCommands, true);
  writeJsonBoolKV(file, F("GPIOOUTPUTSenabled"), settings.outputs.bEnabled, true);
  writeJsonIntKV(file, F("GPIOOUTPUTSpin"), settings.outputs.iPin, true);
  writeJsonIntKV(file, F("GPIOOUTPUTStriggerBit"), settings.outputs.iTriggerBit, true);
  writeJsonBoolKV(file, F("WebhookEnabled"), settings.webhook.bEnabled, true);
  writeJsonStringKV(file, F("WebhookURLon"), settings.webhook.sURLon, true);
  writeJsonStringKV(file, F("WebhookURLoff"), settings.webhook.sURLoff, true);
  writeJsonIntKV(file, F("WebhookTriggerBit"), settings.webhook.iTriggerBit, true);
  writeJsonStringKV(file, F("WebhookPayload"), settings.webhook.sPayload, true);
  writeJsonStringKV(file, F("WebhookContentType"), settings.webhook.sContentType, false);
  file.print(F("}\n"));
  Debugln(F("\r\n[Settings] State: File write complete, closing file"));
  file.close();  // Close write handle before any subsequent read
  DebugTf(PSTR("[Settings] State: Settings saved successfully to %s\r\n"), SETTINGS_FILE);

  if (show) {
    DebugTln(F("\r\n[Settings] JSON content:"));
    File showFile = LittleFS.open(SETTINGS_FILE, "r");
    while (showFile && showFile.available()) {
      TelnetStream.write(showFile.read());
    }
    if (showFile) showFile.close();
  }

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
  // Own line buffer — prevents cMsg clobber if readSettings() is called from an
  // HTTP handler where file.readBytesUntil() calls yield() internally, which
  // could allow writeSettings() → writeJsonStringKV() to overwrite cMsg mid-parse.
  char lineBuf[256];
  char keyBuf[64];
  char valueBuf[201]; // must fit the largest setting value (WebhookPayload: 201 bytes)

  while (file.available()) {
    size_t len = file.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
    lineBuf[len] = '\0';
    if (len == (sizeof(lineBuf) - 1)) {
      // Line was longer than lineBuf — discard remainder and skip it.
      while (file.available()) {
        char discardBuf[32];
        size_t chunkLen = file.readBytesUntil('\n', discardBuf, sizeof(discardBuf) - 1);
        if (chunkLen < (sizeof(discardBuf) - 1)) break;
        yield();
      }
      continue;
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
  if (strlen(settings.sHostname) == 0) strlcpy(settings.sHostname, _HOSTNAME, sizeof(settings.sHostname));

  char *trimmedUser = trimwhitespace(settings.mqtt.sUser);
  if (trimmedUser != settings.mqtt.sUser) memmove(settings.mqtt.sUser, trimmedUser, strlen(trimmedUser) + 1);
  char *trimmedPasswd = trimwhitespace(settings.mqtt.sPasswd);
  if (trimmedPasswd != settings.mqtt.sPasswd) memmove(settings.mqtt.sPasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);

  if (strlen(settings.mqtt.sTopTopic) == 0 || strcmp_P(settings.mqtt.sTopTopic, PSTR("null")) == 0) {
    strlcpy(settings.mqtt.sTopTopic, _HOSTNAME, sizeof(settings.mqtt.sTopTopic));
    for (int i = 0; settings.mqtt.sTopTopic[i]; i++) settings.mqtt.sTopTopic[i] = tolower(settings.mqtt.sTopTopic[i]);
  }
  if (strlen(settings.mqtt.sHaprefix) == 0 || strcmp_P(settings.mqtt.sHaprefix, PSTR("null")) == 0)
    strlcpy(settings.mqtt.sHaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settings.mqtt.sHaprefix));
  if (strlen(settings.mqtt.sUniqueid) == 0 || strcmp_P(settings.mqtt.sUniqueid, PSTR("null")) == 0)
    strlcpy(settings.mqtt.sUniqueid, getUniqueId(), sizeof(settings.mqtt.sUniqueid));
  if (strlen(settings.ntp.sTimezone) == 0 || strcmp_P(settings.ntp.sTimezone, PSTR("null")) == 0)
    strlcpy(settings.ntp.sTimezone, "Europe/Amsterdam", sizeof(settings.ntp.sTimezone));
  if (strlen(settings.ntp.sHostname) == 0 || strcmp_P(settings.ntp.sHostname, PSTR("null")) == 0)
    strlcpy(settings.ntp.sHostname, NTP_HOST_DEFAULT, sizeof(settings.ntp.sHostname));
  if (strcmp_P(settings.otgw.sCommands, PSTR("null")) == 0) settings.otgw.sCommands[0] = 0;

  CHANGE_INTERVAL_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
  CHANGE_INTERVAL_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf(PSTR("Hostname              : %s\r\n"), CSTR(settings.sHostname));
    Debugf(PSTR("MQTT enabled          : %s\r\n"), CBOOLEAN(settings.mqtt.bEnable));
    Debugf(PSTR("MQTT broker           : %s\r\n"), CSTR(settings.mqtt.sBroker));
    Debugf(PSTR("MQTT port             : %d\r\n"), settings.mqtt.iBrokerPort);
    Debugf(PSTR("MQTT username         : %s\r\n"), CSTR(settings.mqtt.sUser));
    Debugf(PSTR("MQTT password set     : %s\r\n"), CBOOLEAN(settings.mqtt.sPasswd[0] != '\0'));
    Debugf(PSTR("MQTT toptopic         : %s\r\n"), CSTR(settings.mqtt.sTopTopic));
    Debugf(PSTR("MQTT uniqueid         : %s\r\n"), CSTR(settings.mqtt.sUniqueid));
    Debugf(PSTR("MQTT separate sources : %s\r\n"), CBOOLEAN(settings.mqtt.bSeparateSources));
    Debugf(PSTR("MQTT interval         : %d\r\n"), settings.mqtt.iInterval);
    Debugf(PSTR("HA prefix             : %s\r\n"), CSTR(settings.mqtt.sHaprefix));
    Debugf(PSTR("HA reboot detection   : %s\r\n"), CBOOLEAN(settings.mqtt.bHaRebootDetect));
    Debugf(PSTR("NTP enabled           : %s\r\n"), CBOOLEAN(settings.ntp.bEnable));
    Debugf(PSTR("NPT timezone          : %s\r\n"), CSTR(settings.ntp.sTimezone));
    Debugf(PSTR("NPT hostname          : %s\r\n"), CSTR(settings.ntp.sHostname));
    Debugf(PSTR("NPT send time         : %s\r\n"), CBOOLEAN(settings.ntp.bSendtime));
    Debugf(PSTR("Led Blink             : %s\r\n"), CBOOLEAN(settings.bLEDblink));
    Debugf(PSTR("GPIO Sensors          : %s\r\n"), CBOOLEAN(settings.sensors.bEnabled));
    Debugf(PSTR("GPIO Sen. Legacy      : %s\r\n"), CBOOLEAN(settings.sensors.bLegacyFormat));
    Debugf(PSTR("GPIO Sen. Pin         : %d\r\n"), settings.sensors.iPin);
    Debugf(PSTR("GPIO Interval         : %d\r\n"), settings.sensors.iInterval);
    Debugf(PSTR("S0 Counter            : %s\r\n"), CBOOLEAN(settings.s0.bEnabled));
    Debugf(PSTR("S0 Counter Pin        : %d\r\n"), settings.s0.iPin);
    Debugf(PSTR("S0 Counter Debouncetime:%d\r\n"), settings.s0.iDebounceTime);
    Debugf(PSTR("S0 Counter Pulses/kw  : %d\r\n"), settings.s0.iPulsekw);
    Debugf(PSTR("S0 Counter Interval   : %d\r\n"), settings.s0.iInterval);
    Debugf(PSTR("OTGW boot cmd enabled : %s\r\n"), CBOOLEAN(settings.otgw.bEnable));
    Debugf(PSTR("OTGW boot cmd         : %s\r\n"), CSTR(settings.otgw.sCommands));
    Debugf(PSTR("GPIO Outputs          : %s\r\n"), CBOOLEAN(settings.outputs.bEnabled));
    Debugf(PSTR("GPIO Out. Pin         : %d\r\n"), settings.outputs.iPin);
    Debugf(PSTR("GPIO Out. Trg. Bit    : %d\r\n"), settings.outputs.iTriggerBit);
    Debugf(PSTR("Webhook enabled       : %s\r\n"), CBOOLEAN(settings.webhook.bEnabled));
    Debugf(PSTR("Webhook URL ON        : %s\r\n"), CSTR(settings.webhook.sURLon));
    Debugf(PSTR("Webhook URL OFF       : %s\r\n"), CSTR(settings.webhook.sURLoff));
    Debugf(PSTR("Webhook Trigger Bit   : %d\r\n"), settings.webhook.iTriggerBit);
    Debugf(PSTR("Webhook Payload       : %s\r\n"), CSTR(settings.webhook.sPayload));
    Debugf(PSTR("Webhook ContentType   : %s\r\n"), CSTR(settings.webhook.sContentType));
  }

  Debugln(F("-\r\n"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{ //do not just trust the caller to do the right thing, server side validation is here!
  DebugTf(PSTR("-> field[%s], newValue[%s]\r\n"), field, newValue);

  if (strcasecmp_P(field, PSTR("hostname"))==0) 
  { //make sure we have a valid hostname here...
    strlcpy(settings.sHostname, newValue, sizeof(settings.sHostname));
    if (strlen(settings.sHostname)==0) snprintf_P(settings.sHostname, sizeof(settings.sHostname), PSTR("OTGW-%06x"), (unsigned int)ESP.getChipId());
    
    //strip away anything beyond the dot
    char *dot = strchr(settings.sHostname, '.');
    if (dot) *dot = '\0';
    
    // Defer MDNS/LLMNR and MQTT restart to flushSettings()
    pendingSideEffects |= SIDE_EFFECT_MDNS | SIDE_EFFECT_MQTT;

    Debugln();
    DebugTf(PSTR("Need reboot before new %s.local will be available!\r\n\n"), settings.sHostname);
  }
  
  if (strcasecmp_P(field, PSTR("MQTTenable"))==0)      settings.mqtt.bEnable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("MQTTbroker")) == 0)    strlcpy(settings.mqtt.sBroker, newValue, sizeof(settings.mqtt.sBroker));
  if (strcasecmp_P(field, PSTR("MQTTbrokerPort"))==0) {
    int port = atoi(newValue);
    if (port < 1 || port > 65535) { DebugTf(PSTR("WARNING: MQTTbrokerPort %d out of range 1-65535, ignored\r\n"), port); }
    else settings.mqtt.iBrokerPort = port;
  }
  if (strcasecmp_P(field, PSTR("MQTTuser"))==0) {
    strlcpy(settings.mqtt.sUser, newValue, sizeof(settings.mqtt.sUser));
    // Trim leading/trailing whitespace from username
    char* trimmedUser = trimwhitespace(settings.mqtt.sUser);
    if (trimmedUser != settings.mqtt.sUser) {
      memmove(settings.mqtt.sUser, trimmedUser, strlen(trimmedUser) + 1);
    }
  }
  if (strcasecmp_P(field, PSTR("MQTTpasswd"))==0){
    if ( newValue && strcasecmp_P(newValue, PSTR("notthepassword")) != 0 ){
      strlcpy(settings.mqtt.sPasswd, newValue, sizeof(settings.mqtt.sPasswd));
      // Trim leading/trailing whitespace from password
      char* trimmedPasswd = trimwhitespace(settings.mqtt.sPasswd);
      if (trimmedPasswd != settings.mqtt.sPasswd) {
        memmove(settings.mqtt.sPasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);
      }
    }
  }
  if (strcasecmp_P(field, PSTR("MQTTtoptopic"))==0)    {
    strlcpy(settings.mqtt.sTopTopic, newValue, sizeof(settings.mqtt.sTopTopic));
    if (strlen(settings.mqtt.sTopTopic)==0)    {
      strlcpy(settings.mqtt.sTopTopic, _HOSTNAME, sizeof(settings.mqtt.sTopTopic));
      for(int i = 0; settings.mqtt.sTopTopic[i]; i++) settings.mqtt.sTopTopic[i] = tolower(settings.mqtt.sTopTopic[i]);
    }
  }
  if (strcasecmp_P(field, PSTR("MQTThaprefix"))==0)    {
    strlcpy(settings.mqtt.sHaprefix, newValue, sizeof(settings.mqtt.sHaprefix));
    if (strlen(settings.mqtt.sHaprefix)==0)    strlcpy(settings.mqtt.sHaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settings.mqtt.sHaprefix));
  }
  if (strcasecmp_P(field, PSTR("MQTTharebootdetection"))==0)      settings.mqtt.bHaRebootDetect = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("MQTTuniqueid")) == 0)  {
    strlcpy(settings.mqtt.sUniqueid, newValue, sizeof(settings.mqtt.sUniqueid));     
    if (strlen(settings.mqtt.sUniqueid) == 0)   strlcpy(settings.mqtt.sUniqueid, getUniqueId(), sizeof(settings.mqtt.sUniqueid));
  }
  if (strcasecmp_P(field, PSTR("MQTTOTmessage"))==0)   settings.mqtt.bOTmessage = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("MQTTinterval"))==0) {
    int val = atoi(newValue);
    if (val < 0 || val > 65535) { DebugTf(PSTR("WARNING: MQTTinterval %d out of range 0-65535, ignored\r\n"), val); }
    else settings.mqtt.iInterval = (uint16_t)val;
  }
  if (strcasecmp_P(field, PSTR("MQTTseparatesources"))==0) settings.mqtt.bSeparateSources = EVALBOOLEAN(newValue);
  if (strstr_P(field, PSTR("mqtt")) != NULL)        pendingSideEffects |= SIDE_EFFECT_MQTT; // defer MQTT restart to flushSettings()
  
  if (strcasecmp_P(field, PSTR("NTPenable"))==0)      settings.ntp.bEnable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("NTPhostname"))==0)    {
    strlcpy(settings.ntp.sHostname, newValue, sizeof(settings.ntp.sHostname)); 
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  if (strcasecmp_P(field, PSTR("NTPtimezone"))==0)    {
    strlcpy(settings.ntp.sTimezone, newValue, sizeof(settings.ntp.sTimezone));
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  if (strcasecmp_P(field, PSTR("NTPsendtime"))==0)    settings.ntp.bSendtime = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("LEDblink"))==0)      settings.bLEDblink = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("darktheme"))==0)     settings.bDarkTheme = EVALBOOLEAN(newValue);
  
  if (strcasecmp_P(field, PSTR("ui_autoscroll"))==0)      settings.ui.bAutoScroll = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_timestamps"))==0)      settings.ui.bShowTimestamp = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_capture"))==0)         settings.ui.bCaptureMode = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autoscreenshot"))==0)  settings.ui.bAutoScreenshot = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autodownloadlog"))==0) settings.ui.bAutoDownloadLog = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_autoexport"))==0)      settings.ui.bAutoExport = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("ui_graphtimewindow"))==0) {
    int val = atoi(newValue);
    settings.ui.iGraphTimeWindow = constrain(val, 1, 1440);
  }

  if (strcasecmp_P(field, PSTR("GPIOSENSORSenabled")) == 0)
  {
    settings.sensors.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO SENSORS will search for sensors on pin GPIO%d!\r\n\n"), settings.sensors.iPin);
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSlegacyformat")) == 0)
  {
    settings.sensors.bLegacyFormat = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Updated GPIO Sensors Legacy Format to %s\r\n\n"), CBOOLEAN(settings.sensors.bLegacyFormat));
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: GPIOSENSORSpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, PSTR("sensor"))) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.sensors.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before GPIO SENSORS will use new pin GPIO%d!\r\n\n"), settings.sensors.iPin);
    }
  }
  if (strcasecmp_P(field, PSTR("GPIOSENSORSinterval")) == 0) {
    int val = atoi(newValue);
    settings.sensors.iInterval = constrain(val, 1, 3600);
    CHANGE_INTERVAL_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERenabled")) == 0)
  {
    settings.s0.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before S0 Counter starts counting on pin GPIO%d!\r\n\n"), settings.s0.iPin);
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: S0COUNTERpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, PSTR("s0"))) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.s0.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before S0 Counter will use new pin GPIO%d!\r\n\n"), settings.s0.iPin);
    }
  }
  if (strcasecmp_P(field, PSTR("S0COUNTERdebouncetime")) == 0) { int val = atoi(newValue); settings.s0.iDebounceTime = constrain(val, 0, 1000); }
  if (strcasecmp_P(field, PSTR("S0COUNTERpulsekw")) == 0)      { int val = atoi(newValue); settings.s0.iPulsekw = constrain(val, 1, 100000); }

  if (strcasecmp_P(field, PSTR("S0COUNTERinterval")) == 0) {
    int val = atoi(newValue);
    settings.s0.iInterval = constrain(val, 1, 3600);
    CHANGE_INTERVAL_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);
  }
  if (strcasecmp_P(field, PSTR("OTGWcommandenable"))==0)    settings.otgw.bEnable = EVALBOOLEAN(newValue);
  if (strcasecmp_P(field, PSTR("OTGWcommands"))==0)         strlcpy(settings.otgw.sCommands, newValue, sizeof(settings.otgw.sCommands));
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTSenabled")) == 0)
  {
    settings.outputs.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will be enabled on pin GPIO%d!\r\n\n"), settings.outputs.iPin);
  }
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTSpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: GPIOOUTPUTSpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, PSTR("output"))) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.outputs.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new pin GPIO%d!\r\n\n"), settings.outputs.iPin);
    }
  }
  if (strcasecmp_P(field, PSTR("GPIOOUTPUTStriggerBit")) == 0)
  {
    int val = atoi(newValue);
    settings.outputs.iTriggerBit = constrain(val, 0, 15);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new trigger bit %d!\r\n\n"), settings.outputs.iTriggerBit);
  }
  if (strcasecmp_P(field, PSTR("webhookenable")) == 0 ||
      strcasecmp_P(field, PSTR("WebhookEnabled")) == 0) {
    settings.webhook.bEnabled = EVALBOOLEAN(newValue);
  }
  if (strcasecmp_P(field, PSTR("WebhookURLon")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurlon")) == 0)   strlcpy(settings.webhook.sURLon, newValue, sizeof(settings.webhook.sURLon));
  if (strcasecmp_P(field, PSTR("WebhookURLoff")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurloff")) == 0)  strlcpy(settings.webhook.sURLoff, newValue, sizeof(settings.webhook.sURLoff));
  if (strcasecmp_P(field, PSTR("WebhookTriggerBit")) == 0 ||
      strcasecmp_P(field, PSTR("webhooktriggerbit")) == 0) settings.webhook.iTriggerBit = constrain(atoi(newValue), 0, 15);
  if (strcasecmp_P(field, PSTR("WebhookPayload")) == 0 ||
      strcasecmp_P(field, PSTR("webhookpayload")) == 0)    strlcpy(settings.webhook.sPayload, newValue, sizeof(settings.webhook.sPayload));
  if (strcasecmp_P(field, PSTR("WebhookContentType")) == 0 ||
      strcasecmp_P(field, PSTR("webhookcontenttype")) == 0) strlcpy(settings.webhook.sContentType, newValue, sizeof(settings.webhook.sContentType));

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
