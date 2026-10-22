/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v2.0.0-alpha.450
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.                                                            
***************************************************************************      
*/

#include <ctype.h>

//=======================================================================
// Deferred settings write support (Finding #23: reduce flash wear + service restarts)
// Side-effect bitmask flags
#define SIDE_EFFECT_MQTT   0x01
#define SIDE_EFFECT_NTP    0x02
#define SIDE_EFFECT_MDNS   0x04
#define SIDE_EFFECT_OTGWSTREAM 0x08
static bool    settingsDirty = false;
static uint8_t pendingSideEffects = 0;

static bool isHttpPasswordPlaceholder(const char* value)
{
  if (!value) return false;

  if (strcasecmp_P(value, PSTR("notthepassword")) == 0 ||
      strcasecmp_P(value, PSTR("notthispassword")) == 0) {
    return true;
  }

  const size_t prefixLen = sizeof("password=") - 1;
  if (strncasecmp_P(value, PSTR("password="), prefixLen) != 0) {
    return false;
  }

  const char* lengthPart = value + prefixLen;
  if (*lengthPart == '\0') return false;

  while (*lengthPart) {
    if (!isdigit(static_cast<unsigned char>(*lengthPart))) return false;
    lengthPart++;
  }

  return true;
}

//=======================================================================
// Clear the dirty flag and pending side-effects without writing or restarting services.
// Call this after a direct writeSettings() to prevent the deferred-flush timer from
// triggering unnecessary service restarts (e.g. during the OTA reboot window).
void settingsMarkClean()
{
  settingsDirty = false;
  pendingSideEffects = 0;
}

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
  if (pendingSideEffects & SIDE_EFFECT_OTGWSTREAM) {
    DebugTln(F("[Settings] Applying legacy port 25238 setting (deferred)"));
    applyLegacyPort25238Setting();
  }
  pendingSideEffects = 0;
}

//=======================================================================
// GPIO conflict detection (Finding #27)
// Returns true if the requested pin is already used by another feature.
// 'caller' identifies which feature is requesting the pin (e.g. "sensor", "s0", "output")
bool checkGPIOConflict(int pin, GPIOConflictCaller caller)
{
  if (pin < 0) return false; // disabled / not set

  bool conflict = false;
  // Check against each configurable GPIO (excluding 'caller' itself)
  if (caller != GPIOConflictCaller::Sensor && pin == settings.sensors.iPin && settings.sensors.iPin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by SENSORS\r\n"), pin);
    conflict = true;
  }
  if (caller != GPIOConflictCaller::S0 && pin == settings.s0.iPin && settings.s0.iPin >= 0) {
    DebugTf(PSTR("GPIO conflict: pin %d already used by S0 Counter\r\n"), pin);
    conflict = true;
  }
  if (caller != GPIOConflictCaller::Output && pin == settings.outputs.iPin && settings.outputs.iPin >= 0) {
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

// settings.ini is written one key per line. The helpers put the separator before
// every entry but the first, so appending a key or leaving one out under #if can
// never drop a comma (TASK-1244). writeSettings() resets the flag and holds no
// yield() between calls.
static bool settingsJsonFirstEntry = true;

static const char* settingsJsonSeparator()
{
  const char* sep = settingsJsonFirstEntry ? "" : ",\n";
  settingsJsonFirstEntry = false;
  return sep;
}

static void writeJsonStringKV(File& file, const __FlashStringHelper* key, const char* value)
{
  // Use global cMsg as escape scratch — no heap allocation.
  // writeSettings() holds no yield() between calls, so cMsg cannot be clobbered mid-write.
  escapeJsonStringTo(value, cMsg, sizeof(cMsg));
  file.printf_P(PSTR("%s  \"%S\": \"%s\""),
                settingsJsonSeparator(),
                reinterpret_cast<PGM_P>(key),
                cMsg);
}

static void writeJsonBoolKV(File& file, const __FlashStringHelper* key, bool value)
{
  file.printf_P(PSTR("%s  \"%S\": %s"),
                settingsJsonSeparator(),
                reinterpret_cast<PGM_P>(key),
                value ? "true" : "false");
}

static void writeJsonIntKV(File& file, const __FlashStringHelper* key, int value)
{
  file.printf_P(PSTR("%s  \"%S\": %d"),
                settingsJsonSeparator(),
                reinterpret_cast<PGM_P>(key),
                value);
}

static void writeJsonFloatKV(File& file, const __FlashStringHelper* key, float value)
{
  char valBuf[16];
  dtostrf(value, 1, 2, valBuf);
  file.printf_P(PSTR("%s  \"%S\": %s"),
                settingsJsonSeparator(),
                reinterpret_cast<PGM_P>(key),
                valBuf);
}

// Indexed key such as SATblemac3 (keyBase in PROGMEM, index appended).
static void writeJsonIndexedStringKV(File& file, PGM_P keyBase, unsigned index, const char* value)
{
  escapeJsonStringTo(value, cMsg, sizeof(cMsg));
  file.printf_P(PSTR("%s  \"%S%u\": \"%s\""),
                settingsJsonSeparator(),
                keyBase,
                index,
                cMsg);
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

  DebugT(F("[Settings] State: Writing JSON settings... "));
  settingsJsonFirstEntry = true;
  file.print(F("{\n"));
  writeJsonStringKV(file, F("hostname"), settings.sHostname);
  writeJsonStringKV(file, F("httppasswd"), settings.sHTTPpasswd);
  writeJsonStringKV(file, F("DeviceManufacturer"), settings.device.sManufacturer);
  writeJsonStringKV(file, F("DeviceModel"), settings.device.sModel);
  writeJsonBoolKV(file, F("MQTTenable"), settings.mqtt.bEnable);
  writeJsonStringKV(file, F("MQTTbroker"), settings.mqtt.sBroker);
  writeJsonIntKV(file, F("MQTTbrokerPort"), settings.mqtt.iBrokerPort);
  writeJsonStringKV(file, F("MQTTuser"), settings.mqtt.sUser);
  writeJsonStringKV(file, F("MQTTpasswd"), settings.mqtt.sPasswd);
  writeJsonStringKV(file, F("MQTTtoptopic"), settings.mqtt.sTopTopic);
  writeJsonStringKV(file, F("MQTThaprefix"), settings.mqtt.sHaprefix);
  writeJsonStringKV(file, F("MQTTuniqueid"), settings.mqtt.sUniqueid);
  writeJsonBoolKV(file, F("MQTTOTmessage"), settings.mqtt.bOTmessage);
  writeJsonBoolKV(file, F("MQTTonChangePublishing"), settings.mqtt.bOnChangePublishing);
  writeJsonIntKV(file, F("MQTTinterval"), settings.mqtt.iInterval);
  writeJsonBoolKV(file, F("MQTTseparatesources"), settings.mqtt.bSeparateSources);
  writeJsonBoolKV(file, F("LegacyPort25238Enabled"), settings.mqtt.bLegacyPort25238Enabled);
  writeJsonBoolKV(file, F("MQTTharebootdetection"), settings.mqtt.bHaRebootDetect);
  writeJsonBoolKV(file, F("MQTTdiscoveryAutoVerify"), settings.mqtt.bDiscoveryAutoVerify);
  writeJsonBoolKV(file, F("MQTTuseLegacyOtTopics"), settings.mqtt.bUseLegacyOtTopics);
  writeJsonBoolKV(file, F("MQTTlastPublishedLegacy"), settings.mqtt.bLastPublishedLegacy);  // TASK-648 Task 6: topology stamp
  writeJsonBoolKV(file, F("MQTTthermostatDupsCleared"), settings.mqtt.bThermostatDupsCleared);  // TASK-1201: one-shot thermostat_ clear done
  writeJsonBoolKV(file, F("NTPenable"), settings.ntp.bEnable);
  writeJsonStringKV(file, F("NTPtimezone"), settings.ntp.sTimezone);
  writeJsonStringKV(file, F("NTPhostname"), settings.ntp.sHostname);
  writeJsonBoolKV(file, F("NTPsendtime"), settings.ntp.bSendtime);
  writeJsonBoolKV(file, F("LEDblink"), settings.bLEDblink);
  writeJsonBoolKV(file, F("darktheme"), settings.bDarkTheme);
  writeJsonBoolKV(file, F("nightlyrestart"), settings.bNightlyRestart);
  writeJsonIntKV(file, F("nightlyrestarthour"), settings.iRestartHour);
  writeJsonIntKV(file, F("boardmode"), settings.iBoardMode);  // ADR-127 combo board-mode cache/override
  writeJsonBoolKV(file, F("ui_onboarded"), settings.ui.bOnboarded);
  writeJsonBoolKV(file, F("sat_onboarded"), settings.ui.bSatOnboarded);
  writeJsonBoolKV(file, F("GPIOSENSORSenabled"), settings.sensors.bEnabled);
  writeJsonBoolKV(file, F("GPIOSENSORSlegacyformat"), settings.sensors.bLegacyFormat);
  writeJsonIntKV(file, F("GPIOSENSORSpin"), settings.sensors.iPin);
  writeJsonIntKV(file, F("GPIOSENSORSinterval"), settings.sensors.iInterval);
  writeJsonBoolKV(file, F("S0COUNTERenabled"), settings.s0.bEnabled);
  writeJsonIntKV(file, F("S0COUNTERpin"), settings.s0.iPin);
  writeJsonIntKV(file, F("S0COUNTERdebouncetime"), settings.s0.iDebounceTime);
  writeJsonIntKV(file, F("S0COUNTERpulsekw"), settings.s0.iPulsekw);
  writeJsonIntKV(file, F("S0COUNTERinterval"), settings.s0.iInterval);
  writeJsonBoolKV(file, F("OTGWcommandenable"), settings.picBoot.bEnable);
  writeJsonStringKV(file, F("OTGWcommands"), settings.picBoot.sCommands);
  writeJsonBoolKV(file, F("GPIOOUTPUTSenabled"), settings.outputs.bEnabled);
  writeJsonIntKV(file, F("GPIOOUTPUTSpin"), settings.outputs.iPin);
  writeJsonIntKV(file, F("GPIOOUTPUTStriggerBit"), settings.outputs.iTriggerBit);
  writeJsonBoolKV(file, F("WebhookEnabled"), settings.webhook.bEnabled);
  writeJsonStringKV(file, F("WebhookURLon"), settings.webhook.sURLon);
  writeJsonStringKV(file, F("WebhookURLoff"), settings.webhook.sURLoff);
  writeJsonIntKV(file, F("WebhookTriggerBit"), settings.webhook.iTriggerBit);
  writeJsonStringKV(file, F("WebhookPayload"), settings.webhook.sPayload);
  writeJsonStringKV(file, F("WebhookContentType"), settings.webhook.sContentType);
  // --- SAT settings ---
  writeJsonBoolKV(file, F("SATenabled"), settings.sat.bEnabled);
  writeJsonIntKV(file, F("SATsystem"), settings.sat.iHeatingSystem);
  writeJsonIntKV(file, F("SATsource"), settings.sat.iHeatingSource);
  writeJsonIntKV(file, F("SAThpcycle"), settings.sat.iHpCycleSeconds);
  writeJsonFloatKV(file, F("SATtargettemp"), settings.sat.fTargetTemp);
  writeJsonFloatKV(file, F("SATcoefficient"), settings.sat.fHeatingCurveCoeff);
  writeJsonFloatKV(file, F("SATdeadband"), settings.sat.fDeadband);
  writeJsonIntKV(file, F("SATinterval"), settings.sat.iControlInterval);
  writeJsonBoolKV(file, F("SATexternaltemp"), settings.sat.bUseExternalTemp);
  writeJsonFloatKV(file, F("SATpresetcomfort"), settings.sat.fPresetComfort);
  writeJsonFloatKV(file, F("SATpreseteco"), settings.sat.fPresetEco);
  writeJsonFloatKV(file, F("SATpresetaway"), settings.sat.fPresetAway);
  writeJsonFloatKV(file, F("SATpresetsleep"), settings.sat.fPresetSleep);
  writeJsonFloatKV(file, F("SATpresetactivity"), settings.sat.fPresetActivity);
  writeJsonFloatKV(file, F("SATpresethome"), settings.sat.fPresetHome);
  writeJsonBoolKV(file, F("SATpwmautoswitch"), settings.sat.bPwmAutoSwitch);
  writeJsonIntKV(file, F("SATmaxmodulation"), settings.sat.iMaxRelModulation);
  writeJsonFloatKV(file, F("SATovershootmargin"), settings.sat.fOvershootMargin);
  writeJsonFloatKV(file, F("SATmodsupdelay"), settings.sat.fModSupDelay);
  writeJsonFloatKV(file, F("SATmodsupoffset"), settings.sat.fModSupOffset);
  writeJsonFloatKV(file, F("SATdhwsetpoint"), settings.sat.fDhwSetpoint);
  writeJsonBoolKV(file, F("SATdhwenabled"), settings.sat.bDhwEnabled);
  writeJsonBoolKV(file, F("SATdhwenable"), settings.sat.bDhwEnable);
  writeJsonBoolKV(file, F("SATpushsetpoint"), settings.sat.bPushSetpoint);
  writeJsonBoolKV(file, F("SATfollowthermostat"), settings.sat.bFollowThermostat);
  writeJsonFloatKV(file, F("SATflameoffset"), settings.sat.fFlameOffOffset);
  writeJsonBoolKV(file, F("SATwindowdetect"), settings.sat.bWindowDetection);
  writeJsonIntKV(file, F("SATwindowminsec"), settings.sat.iWindowMinOpenSec);
  writeJsonFloatKV(file, F("SATtempstep"), settings.sat.fTargetTempStep);
  writeJsonBoolKV(file, F("SATforcepwm"), settings.sat.bForcePWM);
  writeJsonBoolKV(file, F("SATovershootprotection"), settings.sat.bOvershootProtection);
  writeJsonFloatKV(file, F("SATflowoffset"), settings.sat.fFlowOffset);
  writeJsonFloatKV(file, F("SATminpressure"), settings.sat.fMinPressure);
  writeJsonFloatKV(file, F("SATmaxpressure"), settings.sat.fMaxPressure);
  writeJsonFloatKV(file, F("SATmaxpressdrop"), settings.sat.fMaxPressureDrop);
  writeJsonIntKV(file, F("SATmanufacturer"), settings.sat.iManufacturer);
  writeJsonBoolKV(file, F("SATweatherenable"), settings.sat.bWeatherEnable);
  writeJsonFloatKV(file, F("SATweatherlat"), settings.sat.fWeatherLat);
  writeJsonFloatKV(file, F("SATweatherlon"), settings.sat.fWeatherLon);
  writeJsonIntKV(file, F("SATweatherinterval"), settings.sat.iWeatherInterval);
  writeJsonFloatKV(file, F("SATboilercapacity"), settings.sat.fBoilerCapacity);
  writeJsonFloatKV(file, F("SATdhwcapacity"), settings.sat.fDhwCapacity);
  writeJsonFloatKV(file, F("SATboilerratedkw"), settings.sat.fBoilerRatedKW);
  writeJsonFloatKV(file, F("SATboilerefficiency"), settings.sat.fBoilerEfficiency);
  writeJsonBoolKV(file, F("SATpresetsync"), settings.sat.bPresetSync);
  writeJsonStringKV(file, F("SATpresetsynctopic"), settings.sat.sPresetSyncTopic);
  writeJsonBoolKV(file, F("SATsimulation"), settings.sat.bSimulation);
  writeJsonFloatKV(file, F("SATsimheatrate"), settings.sat.fSimHeatRate);
  writeJsonFloatKV(file, F("SATsimcoolrate"), settings.sat.fSimCoolRate);
  writeJsonFloatKV(file, F("SATthermalcoeff"), settings.sat.fThermalCoeff);
  writeJsonBoolKV(file, F("SATsolargain"), settings.sat.bSolarGainEnable);
  writeJsonFloatKV(file, F("SATsolarminrise"), settings.sat.fSolarMinRiseRate);
  writeJsonFloatKV(file, F("SATsolaroffset"), settings.sat.fSolarSetpointOffset);
  writeJsonFloatKV(file, F("SATsolarminelev"), settings.sat.fSolarMinElevation);
  writeJsonBoolKV(file, F("SATsummersimmer"), settings.sat.bSummerSimmer);
  writeJsonFloatKV(file, F("SATsummerthreshold"), settings.sat.fSummerThreshold);
  writeJsonIntKV(file, F("SATsummerminhours"), settings.sat.iSummerMinHours);
  writeJsonBoolKV(file, F("SATcomfortadjust"), settings.sat.bComfortAdjust);
  writeJsonFloatKV(file, F("SATcomforthumidity"), settings.sat.fComfortHumidity);
  writeJsonFloatKV(file, F("SATcomfortmaxoffset"), settings.sat.fComfortMaxOffset);
  writeJsonBoolKV(file, F("SATmultiarea"), settings.sat.bMultiArea);
  writeJsonIntKV(file, F("SATmultiareacount"), settings.sat.iMultiAreaCount);
  writeJsonFloatKV(file, F("SATareaweight0"), settings.sat.fAreaWeight[0]);
  writeJsonFloatKV(file, F("SATareaweight1"), settings.sat.fAreaWeight[1]);
  writeJsonFloatKV(file, F("SATareaweight2"), settings.sat.fAreaWeight[2]);
  writeJsonFloatKV(file, F("SATareaweight3"), settings.sat.fAreaWeight[3]);
  writeJsonBoolKV(file, F("SATautotune"), settings.sat.bAutoTune);
  writeJsonFloatKV(file, F("SATautotunerate"), settings.sat.fAutoTuneRate);
  // SAT Python parity settings (Task #82)
  writeJsonIntKV(file, F("SATsensormaxage"), settings.sat.iSensorMaxAgeS);
  writeJsonBoolKV(file, F("SATerrormon"), settings.sat.bErrorMonitoring);
  writeJsonFloatKV(file, F("SATautogains"), settings.sat.fAutoGainsValue);
  writeJsonIntKV(file, F("SATheatingmode"), settings.sat.iHeatingMode);
  writeJsonIntKV(file, F("SATcyclesperhour"), settings.sat.iCyclesPerHour);
  writeJsonFloatKV(file, F("SATvalveoffset"), settings.sat.fValveOffset);
  writeJsonBoolKV(file, F("SATthermalcomfort"), settings.sat.bThermalComfort);
  writeJsonIntKV(file, F("SAThumiditytimeout"), settings.sat.iHumidityTimeoutS);
  writeJsonBoolKV(file, F("SATsolarfreezeint"), settings.sat.bSolarFreezeIntegral);
  writeJsonIntKV(file, F("SATflushtreshold"), settings.sat.iSatFlushThresholdH);
  // Multi-zone PID (Task #233)
  writeJsonIntKV(file, F("SATzonecount"), settings.sat.iZoneCount);
  writeJsonIntKV(file, F("SATzonetimeout"), settings.sat.iZoneTimeoutS);
  writeJsonFloatKV(file, F("SATzoneheadroom"), settings.sat.fZoneAggregationHeadroom);
  // TASK-587: DS18B20 sensor-to-SAT-area mapping (area 0..3)
  writeJsonStringKV(file, F("SATsensorarea0"), settings.sat.sSensorArea[0]);
  writeJsonStringKV(file, F("SATsensorarea1"), settings.sat.sSensorArea[1]);
  writeJsonStringKV(file, F("SATsensorarea2"), settings.sat.sSensorArea[2]);
  writeJsonStringKV(file, F("SATsensorarea3"), settings.sat.sSensorArea[3]);
  // PV-surplus setpoint boost (TASK-640)
  writeJsonBoolKV (file, F("SATpvboostenabled"),        settings.sat.bPvBoostEnabled);
  writeJsonIntKV  (file, F("SATpvboostthresholdw"),     settings.sat.iPvBoostThresholdW);
  writeJsonIntKV  (file, F("SATpvboostholds"),          settings.sat.iPvBoostHoldS);
  writeJsonFloatKV(file, F("SATpvboostdeltac"),         settings.sat.fPvBoostDeltaC);
  writeJsonFloatKV(file, F("SATpvboostmaxindoorc"),     settings.sat.fPvBoostMaxIndoorC);
  writeJsonIntKV  (file, F("SATpvboostmaxdurationmin"), settings.sat.iPvBoostMaxDurationMin);
  // BLE temperature sensor (Task #20). ESP-abstraction Tier 2 (TASK-742):
  // serialised unconditionally (fields exist on both platforms) so settings.json
  // round-trips between ESP8266 and ESP32; on ESP8266 these write zero/empty.
  writeJsonBoolKV(file, F("SATbleenable"), settings.sat.bBleEnable);
  writeJsonBoolKV(file, F("SATbleriskack"), settings.sat.bBleRiskAck);
  writeJsonBoolKV(file, F("SATblefailover"), settings.sat.bBleFailover);
  writeJsonStringKV(file, F("SATblemac"), settings.sat.sBleMAC);
  writeJsonIntKV(file, F("SATbleinterval"), settings.sat.iBleInterval);
  // TASK-895: name-prefix filter + ingestion toggle. The prefix is user text;
  // writeJsonStringKV escapes it (pre-escaping into cMsg ran away in place, TASK-1245).
  writeJsonStringKV(file, F("SATblenameprefix"), settings.sat.sBleNamePrefix);
  writeJsonBoolKV(file, F("SATblenamefilteringest"), settings.sat.bBleNameFilterIngest);
  // TASK-508: BLE roster — 8 × {mac, label, bindkey} + count. Indexed-key pattern
  // mirrors fAreaWeight precedent. Empty slots serialise as "" — readers
  // treat that as unused.
  for (uint8_t i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    writeJsonIndexedStringKV(file, PSTR("SATblemac"), i, settings.sat.sBleMac[i]);
    writeJsonIndexedStringKV(file, PSTR("SATblelabel"), i, settings.sat.sBleLabel[i]);
    // TASK-930 Phase 2: per-slot MiBeacon bindkey (32 hex).
    // Persisted plaintext to settings.ini under the trusted-LAN model, like the
    // MQTT/HTTP passwords; it is masked on the REST read path and in logs.
    writeJsonIndexedStringKV(file, PSTR("SATblebindkey"), i, settings.sat.sBleBindkey[i]);
  }
  writeJsonIntKV(file, F("SATblerostercount"), settings.sat.iBleRosterCount);
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  // --- OT-direct settings (OTGW32/OT-Thing only) ---
  writeJsonIntKV(file, F("OTDmode"), settings.otd.iMode);
  writeJsonBoolKV(file, F("OTDautodetect"), settings.otd.bAutoDetect);
  writeJsonFloatKV(file, F("OTDsetbacktemp"), settings.otd.fSetbackTemp);
  writeJsonIntKV(file, F("OTDsetbacktimeout"), settings.otd.iSetbackTimeout);
  writeJsonBoolKV(file, F("OTDenableslave"), settings.otd.bEnableSlave);
  writeJsonBoolKV(file, F("OTDsummermode"), settings.otd.bSummerMode);
  writeJsonBoolKV(file, F("OTDfailsafe"), settings.otd.bFailSafe);
  writeJsonIntKV(file, F("OTDmsginterval"), settings.otd.iMsgInterval);
  // TASK-933 P2: persist the user-declared bypass-relay-fitted flag so it survives
  // a reboot (the POST dispatch + GET emit already existed; without this line the
  // flag reset to false every boot and the bypass-mode logic could never stay on).
  writeJsonBoolKV(file, F("OTDhasbypassrelay"), settings.otd.bHasBypassRelay);
  // --- TASK-183: PI room compensation + heating curve ---
  writeJsonIntKV(file, F("OTDchmode"), settings.otd.iCHMode);
  writeJsonFloatKV(file, F("OTDflowtemp"), settings.otd.fFlowTemp);
  writeJsonFloatKV(file, F("OTDflowmax"), settings.otd.fFlowMax);
  writeJsonFloatKV(file, F("OTDroomsetpoint"), settings.otd.fRoomSetpoint);
  writeJsonFloatKV(file, F("OTDgradient"), settings.otd.fGradient);
  writeJsonFloatKV(file, F("OTDexponent"), settings.otd.fExponent);
  writeJsonFloatKV(file, F("OTDoffset"), settings.otd.fOffset);
  writeJsonBoolKV(file, F("OTDroomcomp"), settings.otd.bRoomCompEnabled);
  writeJsonFloatKV(file, F("OTDkp"), settings.otd.fKp);
  writeJsonFloatKV(file, F("OTDki"), settings.otd.fKi);
  writeJsonFloatKV(file, F("OTDkboost"), settings.otd.fKboost);
  // --- TASK-582: CH hysteresis deadband ---
  writeJsonBoolKV(file, F("OTDhysteresisenable"), settings.otd.bHysteresisEnable);
  writeJsonFloatKV(file, F("OTDhysteresis"), settings.otd.fHysteresis);
  // --- TASK-584: ventilation override persistence ---
  writeJsonBoolKV(file, F("OTDventenable"), settings.otd.bVentEnable);
  writeJsonBoolKV(file, F("OTDopenbypass"), settings.otd.bOpenBypass);
  writeJsonBoolKV(file, F("OTDautobypass"), settings.otd.bAutoBypass);
  writeJsonBoolKV(file, F("OTDfreeventenable"), settings.otd.bFreeVentEnable);
  writeJsonIntKV(file, F("OTDventsetpoint"), settings.otd.iVentSetpoint);
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // Ethernet static IP (OTGW32 only)
  writeJsonBoolKV(file, F("ETHstaticip"), settings.eth.bStaticIP);
  writeJsonStringKV(file, F("ETHipaddress"), settings.eth.sIPaddress);
  writeJsonStringKV(file, F("ETHgateway"), settings.eth.sGateway);
  writeJsonStringKV(file, F("ETHsubnet"), settings.eth.sSubnet);
  writeJsonStringKV(file, F("ETHdns"), settings.eth.sDNS);
#endif
  // WiFi static IP (empty sStaticIp = DHCP)
  writeJsonStringKV(file, F("WifiStaticIP"), settings.wifi.sStaticIp);
  writeJsonStringKV(file, F("WifiSubnet"),   settings.wifi.sSubnet);
  writeJsonStringKV(file, F("WifiGateway"),  settings.wifi.sGateway);
  writeJsonStringKV(file, F("WifiDns1"),     settings.wifi.sDns1);
  writeJsonStringKV(file, F("WifiDns2"),     settings.wifi.sDns2);
  file.print(F("\n}\n"));
  Debugln(F("\r\n[Settings] State: File write complete, closing file"));
  file.close();  // Close write handle before any subsequent read
  DebugTf(PSTR("[Settings] State: Settings saved successfully to %s\r\n"), SETTINGS_FILE);
  safebootSyncNvs(settings.sHostname, settings.sHTTPpasswd);  // ADR-191: safeboot reads these from NVS

  if (show) {
    DebugTln(F("\r\n[Settings] JSON content:"));
    File showFile = LittleFS.open(SETTINGS_FILE, "r");
    while (showFile && showFile.available()) {
      debugTelnet.write(showFile.read());
    }
    if (showFile) showFile.close();
  }

} // writeSettings()


//=======================================================================
// TASK-648: set true while parsing if the file carries the MQTTuseLegacyOtTopics key.
// Absent on a 1.x.x config (predates ADR-106) -> readSettings defaults to legacy topics.
static bool g_sawLegacyTopicsKey = false;
static bool g_sawSatFollowKey = false;   // TASK-1259: set when SATfollowthermostat is parsed; its absence means "default to the push-setpoint value"
static bool g_sawOnboardedKey = false;   // TASK-997: set when ui_onboarded is parsed; its absence marks a pre-feature (existing) install

void readSettings(bool show)
{
  DebugTf(PSTR(" %s ..\r\n"), SETTINGS_FILE);
  g_sawLegacyTopicsKey = false;  // reset per parse; updateSetting() sets it when the key is seen
  g_sawOnboardedKey = false;     // TASK-997: same sentinel pattern for the first-time-setup flag
  g_sawSatFollowKey = false;     // TASK-1259: same sentinel pattern for SATfollowthermostat
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
  static char lineBuf[256];
  static char keyBuf[64];
  static char valueBuf[201]; // must fit the largest setting value (WebhookPayload: 201 bytes)

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
  // TASK-648: topic naming (bUseLegacyOtTopics) and device topology (bLegacyMode) are
  // INDEPENDENT axes. Device topology defaults to modern five-device for everyone.
  // Topic naming defaults to LEGACY for a 1.x.x upgrade (its config predates ADR-106, so
  // the MQTTuseLegacyOtTopics key was absent from the file) and to NEW for fresh installs
  // and existing 2.0.0 configs (which carry the key, written by writeSettings). The key's
  // presence is its own sentinel: g_sawLegacyTopicsKey is set when the key is parsed.
  if (!g_sawLegacyTopicsKey) {
    settings.mqtt.bUseLegacyOtTopics = true;   // 1.x.x upgrade: keep old topic names so existing automations survive
    settingsDirty = true;                      // deferred persist; the key then exists on the next boot
    DebugTln(F("[Settings] 1.x.x upgrade detected (no MQTTuseLegacyOtTopics key): defaulting to legacy OT topics. Opt in to new topics in settings."));
  }
  // TASK-997: first-time-setup wizard runs ONLY on a genuinely fresh device. A fresh
  // install has no settings file, so writeSettings() above created it WITH ui_onboarded
  // (=false) and the re-read sees the key -> onboard. An EXISTING install that predates
  // this feature parsed a file WITHOUT the key -> migrate it to onboarded so the wizard
  // never appears for already-configured users ("only the first time ever").
  if (!g_sawOnboardedKey) {
    settings.ui.bOnboarded = true;
    settingsDirty = true;              // persist the key so this migration runs once
    DebugTln(F("[Settings] existing install (no ui_onboarded key): marking as already onboarded."));
  }
  // TASK-1259: a unit that has no stored SATfollowthermostat yet follows the thermostat exactly when it
  // pushes the SAT target to it (Sergeant D: on when push-setpoint is on). Applied here, after the whole
  // file is read, because SATpushsetpoint may come later in the file than the follow key would. The
  // derived value is persisted by the deferred flush, so from then on the stored value wins and a later
  // change of push-setpoint does not move it.
  if (!g_sawSatFollowKey) {
    settings.sat.bFollowThermostat = settings.sat.bPushSetpoint;
    settingsDirty = true;
    DebugTf(PSTR("[Settings] no SATfollowthermostat key: default %s (= push-setpoint)\r\n"), CBOOLEAN(settings.sat.bFollowThermostat));
  }
  // TASK-648 Task 6: NO stamp seeding here. bLastPublishedLegacy=false on a
  // legacy upgrade boot means "stamp says modern, but mode is now legacy" —
  // that IS a real migration that needs cleanup. Let the first boot do one
  // benign migration pass (empty-publish to nonexistent modern topics = broker
  // no-op) and stamp true on drain completion. Seeding would silently suppress
  // the migration check and leave modern ghosts after a real switch.
  // See TASK-648 Task 6 design notes for the full invariant.
  if (strlen(settings.ntp.sTimezone) == 0 || strcmp_P(settings.ntp.sTimezone, PSTR("null")) == 0)
    strlcpy(settings.ntp.sTimezone, "Europe/Amsterdam", sizeof(settings.ntp.sTimezone));
  if (strlen(settings.ntp.sHostname) == 0 || strcmp_P(settings.ntp.sHostname, PSTR("null")) == 0)
    strlcpy(settings.ntp.sHostname, NTP_HOST_DEFAULT, sizeof(settings.ntp.sHostname));
  if (strcmp_P(settings.picBoot.sCommands, PSTR("null")) == 0) settings.picBoot.sCommands[0] = 0;

  // ADR-116 one-time migration: an on-change config that still carries the legacy
  // interval=0 (pre-this-release, where the key is absent so the flag defaults true)
  // is bumped to a 60s heartbeat. Persist via the deferred flushSettings() path in
  // loop() — NOT a write here — so the "loading must not rewrite" invariant holds.
  if (settings.mqtt.bOnChangePublishing && settings.mqtt.iInterval == 0) {
    DebugTf(PSTR("[Settings] Migrating MQTTinterval 0 -> %u (on-change publishing default)\r\n"),
            MQTT_DEFAULT_PUBLISH_INTERVAL_SEC);
    settings.mqtt.iInterval = MQTT_DEFAULT_PUBLISH_INTERVAL_SEC;
    settingsDirty = true;  // deferred write; no boot-time writeSettings(), no service restart
  }

  CHANGE_INTERVAL_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
  CHANGE_INTERVAL_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);

  DebugTln(F(" .. done\r\n"));

  if (show) {
    Debugln(F("\r\n==== read Settings ===================================================\r"));
    Debugf(PSTR("Hostname              : %s\r\n"), CSTR(settings.sHostname));
    Debugf(PSTR("HTTP password         : %s\r\n"), settings.sHTTPpasswd[0] ? "***" : "(not set)");
    Debugf(PSTR("MQTT enabled          : %s\r\n"), CBOOLEAN(settings.mqtt.bEnable));
    Debugf(PSTR("MQTT broker           : %s\r\n"), CSTR(settings.mqtt.sBroker));
    Debugf(PSTR("MQTT port             : %d\r\n"), settings.mqtt.iBrokerPort);
    Debugf(PSTR("MQTT username         : %s\r\n"), CSTR(settings.mqtt.sUser));
    Debugf(PSTR("MQTT password set     : %s\r\n"), CBOOLEAN(settings.mqtt.sPasswd[0] != '\0'));
    Debugf(PSTR("MQTT toptopic         : %s\r\n"), CSTR(settings.mqtt.sTopTopic));
    Debugf(PSTR("MQTT uniqueid         : %s\r\n"), CSTR(settings.mqtt.sUniqueid));
    Debugf(PSTR("MQTT publish on change: %s\r\n"), CBOOLEAN(settings.mqtt.bOnChangePublishing));
    Debugf(PSTR("MQTT separate sources : %s\r\n"), CBOOLEAN(settings.mqtt.bSeparateSources));
    Debugf(PSTR("Legacy port 25238     : %s\r\n"), CBOOLEAN(settings.mqtt.bLegacyPort25238Enabled));
    Debugf(PSTR("MQTT interval         : %d\r\n"), settings.mqtt.iInterval);
    Debugf(PSTR("HA prefix             : %s\r\n"), CSTR(settings.mqtt.sHaprefix));
    Debugf(PSTR("HA reboot detection   : %s\r\n"), CBOOLEAN(settings.mqtt.bHaRebootDetect));
    Debugf(PSTR("Discovery auto-verify : %s\r\n"), CBOOLEAN(settings.mqtt.bDiscoveryAutoVerify));
    Debugf(PSTR("Use legacy OT topics  : %s\r\n"), CBOOLEAN(settings.mqtt.bUseLegacyOtTopics));
    Debugf(PSTR("NTP enabled           : %s\r\n"), CBOOLEAN(settings.ntp.bEnable));
    Debugf(PSTR("NPT timezone          : %s\r\n"), CSTR(settings.ntp.sTimezone));
    Debugf(PSTR("NPT hostname          : %s\r\n"), CSTR(settings.ntp.sHostname));
    Debugf(PSTR("NPT send time         : %s\r\n"), CBOOLEAN(settings.ntp.bSendtime));
    Debugf(PSTR("Led Blink             : %s\r\n"), CBOOLEAN(settings.bLEDblink));
    Debugf(PSTR("Nightly Restart       : %s (hour=%d)\r\n"), CBOOLEAN(settings.bNightlyRestart), settings.iRestartHour);
    Debugf(PSTR("Board Mode (ADR-127)  : %d (0=auto,1=pic,2=otdirect,3=pic-pro)\r\n"), settings.iBoardMode);
    Debugf(PSTR("GPIO Sensors          : %s\r\n"), CBOOLEAN(settings.sensors.bEnabled));
    Debugf(PSTR("GPIO Sen. Legacy      : %s\r\n"), CBOOLEAN(settings.sensors.bLegacyFormat));
    Debugf(PSTR("GPIO Sen. Pin         : %d\r\n"), settings.sensors.iPin);
    Debugf(PSTR("GPIO Interval         : %d\r\n"), settings.sensors.iInterval);
    Debugf(PSTR("S0 Counter            : %s\r\n"), CBOOLEAN(settings.s0.bEnabled));
    Debugf(PSTR("S0 Counter Pin        : %d\r\n"), settings.s0.iPin);
    Debugf(PSTR("S0 Counter Debouncetime:%d\r\n"), settings.s0.iDebounceTime);
    Debugf(PSTR("S0 Counter Pulses/kw  : %d\r\n"), settings.s0.iPulsekw);
    Debugf(PSTR("S0 Counter Interval   : %d\r\n"), settings.s0.iInterval);
    Debugf(PSTR("OTGW boot cmd enabled : %s\r\n"), CBOOLEAN(settings.picBoot.bEnable));
    Debugf(PSTR("OTGW boot cmd         : %s\r\n"), CSTR(settings.picBoot.sCommands));
    Debugf(PSTR("GPIO Outputs          : %s\r\n"), CBOOLEAN(settings.outputs.bEnabled));
    Debugf(PSTR("GPIO Out. Pin         : %d\r\n"), settings.outputs.iPin);
    Debugf(PSTR("GPIO Out. Trg. Bit    : %d\r\n"), settings.outputs.iTriggerBit);
    Debugf(PSTR("Webhook enabled       : %s\r\n"), CBOOLEAN(settings.webhook.bEnabled));
    Debugf(PSTR("Webhook URL ON        : %s\r\n"), CSTR(settings.webhook.sURLon));
    Debugf(PSTR("Webhook URL OFF       : %s\r\n"), CSTR(settings.webhook.sURLoff));
    Debugf(PSTR("Webhook Trigger Bit   : %d\r\n"), settings.webhook.iTriggerBit);
    Debugf(PSTR("Webhook Payload       : %s\r\n"), CSTR(settings.webhook.sPayload));
    Debugf(PSTR("Webhook ContentType   : %s\r\n"), CSTR(settings.webhook.sContentType));
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
    Debugf(PSTR("OTD Mode              : %d\r\n"), settings.otd.iMode);
    Debugf(PSTR("OTD Auto-detect       : %s\r\n"), CBOOLEAN(settings.otd.bAutoDetect));
    Debugf(PSTR("OTD Setback temp      : "));
    { char tb[8]; dtostrf(settings.otd.fSetbackTemp, 1, 1, tb); Debugf(PSTR("%s\r\n"), tb); }
    Debugf(PSTR("OTD Setback timeout   : %d\r\n"), settings.otd.iSetbackTimeout);
    Debugf(PSTR("OTD Enable slave      : %s\r\n"), CBOOLEAN(settings.otd.bEnableSlave));
#endif
  }

  Debugln(F("-\r\n"));

  safebootSyncNvs(settings.sHostname, settings.sHTTPpasswd);  // ADR-191: safeboot reads these from NVS
} // readSettings()


//=======================================================================
// Parse a float setting and clamp it to [lo, hi]. A non-finite input ("nan",
// "inf") keeps the current value: constrain() passes NaN through, and a NaN
// would be written as 'nan' into settings.ini and the REST JSON (TASK-1243).
static float clampSettingFloat(const char* newValue, float current, float lo, float hi)
{
  float v = strtof(newValue, nullptr);
  if (!isfinite(v)) {
    DebugTf(PSTR("[Settings] Ignoring non-finite value [%s], keeping %.2f\r\n"), newValue, current);
    return current;
  }
  return constrain(v, lo, hi);
}

//=======================================================================
void updateSetting(const char *field, const char *newValue)
{ //do not just trust the caller to do the right thing, server side validation is here!
  // Mask password fields in debug log to avoid leaking credentials
  // (TASK-930: + per-slot MiBeacon bindkey secret, SATblebindkeyN)
  // SATweatherapikey is no longer a setting (TASK-1280), but an older settings.ini
  // still carries the OpenWeatherMap key; it is ignored on load and stays masked here.
  if (strcasecmp_P(field, PSTR("httppasswd")) == 0 ||
      strcasecmp_P(field, PSTR("MQTTpasswd")) == 0 ||
      strcasecmp_P(field, PSTR("SATweatherapikey")) == 0 ||
      strncasecmp_P(field, PSTR("SATblebindkey"), 13) == 0) {
    DebugTf(PSTR("-> field[%s], newValue[***]\r\n"), field);
  } else {
    DebugTf(PSTR("-> field[%s], newValue[%s]\r\n"), field, newValue);
  }

  // TASK-564: per-field no-op detection. SergeantD's alpha.3 telnet log showed
  // 6 full /settings.ini rewrites in 14 s for a single SAT/BLE form interaction
  // because identical-value writes (satblemac flushed twice with the same empty
  // value; satexternaltemp toggled false→true→false→true) all marked the
  // settings dirty. The per-platform fingerprint strategy (ESP8266 CRC32
  // sentinel vs ESP32 full-struct snapshot) lives behind platformSettingsNoop*
  // shims (ADR-113 / TASK-756: no raw platform #if in application code).
  platformSettingsNoopCapture(&settings, sizeof(settings));
  const uint8_t pendingSideEffectsSnapshot = pendingSideEffects;

  if (strcasecmp_P(field, PSTR("hostname"))==0)
  { //make sure we have a valid hostname here...
    strlcpy(settings.sHostname, newValue, sizeof(settings.sHostname));
    if (strlen(settings.sHostname)==0) snprintf_P(settings.sHostname, sizeof(settings.sHostname), PSTR("OTGW-%06x"), (unsigned int)platformChipId());
    
    //strip away anything beyond the dot
    char *dot = strchr(settings.sHostname, '.');
    if (dot) *dot = '\0';
    
    // Defer MDNS/LLMNR and MQTT restart to flushSettings()
    pendingSideEffects |= SIDE_EFFECT_MDNS | SIDE_EFFECT_MQTT;

    Debugln();
    DebugTf(PSTR("Need reboot before new %s.local will be available!\r\n\n"), settings.sHostname);
  }

  else if (strcasecmp_P(field, PSTR("DeviceManufacturer")) == 0) {
    strlcpy(settings.device.sManufacturer, newValue, sizeof(settings.device.sManufacturer));
  }
  else if (strcasecmp_P(field, PSTR("DeviceModel")) == 0) {
    strlcpy(settings.device.sModel, newValue, sizeof(settings.device.sModel));
  }
  else if (strcasecmp_P(field, PSTR("httppasswd")) == 0) {
    // Only update if not the placeholder value.
    if (newValue && !isHttpPasswordPlaceholder(newValue)) {
      strlcpy(settings.sHTTPpasswd, newValue, sizeof(settings.sHTTPpasswd));
      // Trim leading/trailing whitespace — trailing spaces are easy to enter in the UI
      char* trimmed = trimwhitespace(settings.sHTTPpasswd);
      if (trimmed != settings.sHTTPpasswd) memmove(settings.sHTTPpasswd, trimmed, strlen(trimmed) + 1);
      // Update OTA update server credentials immediately
      if (settings.sHTTPpasswd[0] != '\0') {
        httpUpdater.updateCredentials("admin", settings.sHTTPpasswd);
      } else {
        httpUpdater.updateCredentials("", "");
      }
    }
  }
  else if (strcasecmp_P(field, PSTR("MQTTenable"))==0)      settings.mqtt.bEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTbroker")) == 0)    strlcpy(settings.mqtt.sBroker, newValue, sizeof(settings.mqtt.sBroker));
  else if (strcasecmp_P(field, PSTR("MQTTbrokerPort"))==0) {
    int port = atoi(newValue);
    if (port < 1 || port > 65535) { DebugTf(PSTR("WARNING: MQTTbrokerPort %d out of range 1-65535, ignored\r\n"), port); }
    else settings.mqtt.iBrokerPort = port;
  }
  else if (strcasecmp_P(field, PSTR("MQTTuser"))==0) {
    strlcpy(settings.mqtt.sUser, newValue, sizeof(settings.mqtt.sUser));
    // Trim leading/trailing whitespace from username
    char* trimmedUser = trimwhitespace(settings.mqtt.sUser);
    if (trimmedUser != settings.mqtt.sUser) {
      memmove(settings.mqtt.sUser, trimmedUser, strlen(trimmedUser) + 1);
    }
  }
  else if (strcasecmp_P(field, PSTR("MQTTpasswd"))==0){
    if (newValue && !isHttpPasswordPlaceholder(newValue)) {
      strlcpy(settings.mqtt.sPasswd, newValue, sizeof(settings.mqtt.sPasswd));
      // Trim leading/trailing whitespace from password
      char* trimmedPasswd = trimwhitespace(settings.mqtt.sPasswd);
      if (trimmedPasswd != settings.mqtt.sPasswd) {
        memmove(settings.mqtt.sPasswd, trimmedPasswd, strlen(trimmedPasswd) + 1);
      }
    }
  }
  else if (strcasecmp_P(field, PSTR("MQTTtoptopic"))==0)    {
    strlcpy(settings.mqtt.sTopTopic, newValue, sizeof(settings.mqtt.sTopTopic));
    if (strlen(settings.mqtt.sTopTopic)==0)    {
      strlcpy(settings.mqtt.sTopTopic, _HOSTNAME, sizeof(settings.mqtt.sTopTopic));
      for(int i = 0; settings.mqtt.sTopTopic[i]; i++) settings.mqtt.sTopTopic[i] = tolower(settings.mqtt.sTopTopic[i]);
    }
  }
  else if (strcasecmp_P(field, PSTR("MQTThaprefix"))==0)    {
    strlcpy(settings.mqtt.sHaprefix, newValue, sizeof(settings.mqtt.sHaprefix));
    if (strlen(settings.mqtt.sHaprefix)==0)    strlcpy(settings.mqtt.sHaprefix, HOME_ASSISTANT_DISCOVERY_PREFIX, sizeof(settings.mqtt.sHaprefix));
  }
  else if (strcasecmp_P(field, PSTR("MQTTharebootdetection"))==0)      settings.mqtt.bHaRebootDetect = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTdiscoveryAutoVerify"))==0)    settings.mqtt.bDiscoveryAutoVerify = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTuseLegacyOtTopics"))==0) {
    // TASK-648: this key being present marks a config that already knows the topic-naming
    // axis (fresh install / existing 2.0.0), so the 1.x.x-upgrade legacy default is NOT applied.
    g_sawLegacyTopicsKey = true;
    // ADR-106: detect transition and arm cleanup of the OTHER label-set's retained discovery topics.
    const bool oldVal = settings.mqtt.bUseLegacyOtTopics;
    const bool newVal = EVALBOOLEAN(newValue);
    settings.mqtt.bUseLegacyOtTopics = newVal;
    if (oldVal != newVal) {
      armTopicCleanupOnLegacyToggle(newVal);
      // Republish all discovery configs under the new label set (their topics/object_ids
      // carry the chosen label). This is the topic-naming axis only; device topology
      // (bLegacyMode) is unaffected, so the topology migration is not triggered here.
      markAllMQTTConfigPending();
    }
  }
  else if (strcasecmp_P(field, PSTR("MQTTuniqueid")) == 0)  {
    strlcpy(settings.mqtt.sUniqueid, newValue, sizeof(settings.mqtt.sUniqueid));
    if (strlen(settings.mqtt.sUniqueid) == 0)   strlcpy(settings.mqtt.sUniqueid, getUniqueId(), sizeof(settings.mqtt.sUniqueid));
  }
  else if (strcasecmp_P(field, PSTR("MQTTOTmessage"))==0)   settings.mqtt.bOTmessage = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTonChangePublishing"))==0) settings.mqtt.bOnChangePublishing = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTinterval"))==0) {
    int val = atoi(newValue);
    if (val < 0 || val > 65535) { DebugTf(PSTR("WARNING: MQTTinterval %d out of range 0-65535, ignored\r\n"), val); }
    else settings.mqtt.iInterval = (uint16_t)val;
  }
  else if (strcasecmp_P(field, PSTR("MQTTseparatesources"))==0) settings.mqtt.bSeparateSources = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("MQTTlastPublishedLegacy"))==0) settings.mqtt.bLastPublishedLegacy = EVALBOOLEAN(newValue);  // TASK-648 Task 6
  else if (strcasecmp_P(field, PSTR("MQTTthermostatDupsCleared"))==0) settings.mqtt.bThermostatDupsCleared = EVALBOOLEAN(newValue);  // TASK-1201
  else if (strcasecmp_P(field, PSTR("LegacyPort25238Enabled"))==0) {
    settings.mqtt.bLegacyPort25238Enabled = EVALBOOLEAN(newValue);
    pendingSideEffects |= SIDE_EFFECT_OTGWSTREAM;
  }
  else if (strcasecmp_P(field, PSTR("NTPenable"))==0)      settings.ntp.bEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("NTPhostname"))==0)    {
    strlcpy(settings.ntp.sHostname, newValue, sizeof(settings.ntp.sHostname));
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  else if (strcasecmp_P(field, PSTR("NTPtimezone"))==0)    {
    strlcpy(settings.ntp.sTimezone, newValue, sizeof(settings.ntp.sTimezone));
    pendingSideEffects |= SIDE_EFFECT_NTP; // defer NTP restart to flushSettings()
  }
  else if (strcasecmp_P(field, PSTR("NTPsendtime"))==0)    settings.ntp.bSendtime = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("LEDblink"))==0)      settings.bLEDblink = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("darktheme"))==0)     settings.bDarkTheme = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("nightlyrestart"))==0)     settings.bNightlyRestart = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("nightlyrestarthour"))==0) { int h = atoi(newValue); settings.iRestartHour = (h >= 0 && h <= 23) ? h : 4; }
  else if (strcasecmp_P(field, PSTR("boardmode"))==0)          { int m = atoi(newValue); settings.iBoardMode = (m >= 0 && m <= 3) ? (uint8_t)m : 0; }  // ADR-127/157

  else if (strcasecmp_P(field, PSTR("ui_onboarded"))==0)    { settings.ui.bOnboarded = EVALBOOLEAN(newValue); g_sawOnboardedKey = true; }  // TASK-997: key presence = existing install (see readSettings migration)
  else if (strcasecmp_P(field, PSTR("sat_onboarded"))==0)     settings.ui.bSatOnboarded = EVALBOOLEAN(newValue);  // TASK-1012: no g_sawKey migration — defaults false so existing SAT users are prompted once (client gates on satenabled)

  else if (strcasecmp_P(field, PSTR("GPIOSENSORSenabled")) == 0)
  {
    settings.sensors.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO SENSORS will search for sensors on pin GPIO%d!\r\n\n"), settings.sensors.iPin);
  }
  else if (strcasecmp_P(field, PSTR("GPIOSENSORSlegacyformat")) == 0)
  {
    settings.sensors.bLegacyFormat = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Updated GPIO Sensors Legacy Format to %s\r\n\n"), CBOOLEAN(settings.sensors.bLegacyFormat));
  }
  else if (strcasecmp_P(field, PSTR("GPIOSENSORSpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: GPIOSENSORSpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, GPIOConflictCaller::Sensor)) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.sensors.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before GPIO SENSORS will use new pin GPIO%d!\r\n\n"), settings.sensors.iPin);
    }
  }
  else if (strcasecmp_P(field, PSTR("GPIOSENSORSinterval")) == 0) {
    int val = atoi(newValue);
    settings.sensors.iInterval = constrain(val, 1, 3600);
    CHANGE_INTERVAL_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
  }
  else if (strcasecmp_P(field, PSTR("S0COUNTERenabled")) == 0)
  {
    settings.s0.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before S0 Counter starts counting on pin GPIO%d!\r\n\n"), settings.s0.iPin);
  }
  else if (strcasecmp_P(field, PSTR("S0COUNTERpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: S0COUNTERpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, GPIOConflictCaller::S0)) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.s0.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before S0 Counter will use new pin GPIO%d!\r\n\n"), settings.s0.iPin);
    }
  }
  else if (strcasecmp_P(field, PSTR("S0COUNTERdebouncetime")) == 0) { int val = atoi(newValue); settings.s0.iDebounceTime = constrain(val, 0, 1000); }
  else if (strcasecmp_P(field, PSTR("S0COUNTERpulsekw")) == 0)      { int val = atoi(newValue); settings.s0.iPulsekw = constrain(val, 1, 65535); }

  else if (strcasecmp_P(field, PSTR("S0COUNTERinterval")) == 0) {
    int val = atoi(newValue);
    settings.s0.iInterval = constrain(val, 1, 3600);
    CHANGE_INTERVAL_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);
  }
  else if (strcasecmp_P(field, PSTR("OTGWcommandenable"))==0)    settings.picBoot.bEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTGWcommands"))==0)         strlcpy(settings.picBoot.sCommands, newValue, sizeof(settings.picBoot.sCommands));
  else if (strcasecmp_P(field, PSTR("GPIOOUTPUTSenabled")) == 0)
  {
    settings.outputs.bEnabled = EVALBOOLEAN(newValue);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will be enabled on pin GPIO%d!\r\n\n"), settings.outputs.iPin);
  }
  else if (strcasecmp_P(field, PSTR("GPIOOUTPUTSpin")) == 0)
  {
    int newPin = atoi(newValue);
    if (newPin < 0 || newPin > 16) { DebugTf(PSTR("WARNING: GPIOOUTPUTSpin %d out of range 0-16, ignored\r\n"), newPin); }
    else {
      if (checkGPIOConflict(newPin, GPIOConflictCaller::Output)) {
        DebugTf(PSTR("WARNING: GPIO%d conflicts with another enabled feature!\r\n"), newPin);
      }
      settings.outputs.iPin = newPin;
      Debugln();
      DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new pin GPIO%d!\r\n\n"), settings.outputs.iPin);
    }
  }
  else if (strcasecmp_P(field, PSTR("GPIOOUTPUTStriggerBit")) == 0)
  {
    int val = atoi(newValue);
    settings.outputs.iTriggerBit = constrain(val, 0, 15);
    Debugln();
    DebugTf(PSTR("Need reboot before GPIO OUTPUTS will use new trigger bit %d!\r\n\n"), settings.outputs.iTriggerBit);
  }
  else if (strcasecmp_P(field, PSTR("webhookenable")) == 0 ||
      strcasecmp_P(field, PSTR("WebhookEnabled")) == 0) {
    settings.webhook.bEnabled = EVALBOOLEAN(newValue);
  }
  else if (strcasecmp_P(field, PSTR("WebhookURLon")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurlon")) == 0) {
    strlcpy(settings.webhook.sURLon, newValue, sizeof(settings.webhook.sURLon));
    if (strlen(newValue) >= sizeof(settings.webhook.sURLon)) {
      DebugTf(PSTR("Warning: webhook URL truncated [%s]\r\n"), field);
    }
  }
  else if (strcasecmp_P(field, PSTR("WebhookURLoff")) == 0 ||
      strcasecmp_P(field, PSTR("webhookurloff")) == 0) {
    strlcpy(settings.webhook.sURLoff, newValue, sizeof(settings.webhook.sURLoff));
    if (strlen(newValue) >= sizeof(settings.webhook.sURLoff)) {
      DebugTf(PSTR("Warning: webhook URL truncated [%s]\r\n"), field);
    }
  }
  else if (strcasecmp_P(field, PSTR("WebhookTriggerBit")) == 0 ||
      strcasecmp_P(field, PSTR("webhooktriggerbit")) == 0) settings.webhook.iTriggerBit = constrain(atoi(newValue), 0, 15);
  else if (strcasecmp_P(field, PSTR("WebhookPayload")) == 0 ||
      strcasecmp_P(field, PSTR("webhookpayload")) == 0)    strlcpy(settings.webhook.sPayload, newValue, sizeof(settings.webhook.sPayload));
  else if (strcasecmp_P(field, PSTR("WebhookContentType")) == 0 ||
      strcasecmp_P(field, PSTR("webhookcontenttype")) == 0) strlcpy(settings.webhook.sContentType, newValue, sizeof(settings.webhook.sContentType));
  // --- SAT settings ---
  else if (strcasecmp_P(field, PSTR("SATenabled")) == 0) {
    bool wasEnabled = settings.sat.bEnabled;
    settings.sat.bEnabled = EVALBOOLEAN(newValue);
    // Only disable on actual enabled→disabled transition, and not during boot
    if (wasEnabled && !settings.sat.bEnabled && state.bSetupComplete) {
      satDisable();
    }
    // Re-enabling from any path resumes a tripped SAT (TASK-1159). Not during the
    // boot-time file parse: a trip does not survive a reboot anyway.
    if (!wasEnabled && settings.sat.bEnabled && state.bSetupComplete) {
      satClearSafetyTrip();
    }
  }
  else if (strcasecmp_P(field, PSTR("SATsystem")) == 0)          settings.sat.iHeatingSystem = constrain(atoi(newValue), 0, 2);  // 0=auto,1=radiators,2=underfloor
  else if (strcasecmp_P(field, PSTR("SATsource")) == 0)          settings.sat.iHeatingSource = constrain(atoi(newValue), 0, 3);  // 0=auto,1=gas_boiler,2=heat_pump,3=hybrid (TASK-891.8)
  else if (strcasecmp_P(field, PSTR("SAThpcycle")) == 0)         settings.sat.iHpCycleSeconds = constrain(atoi(newValue), 1800, 2400);  // heat-pump cycle: 1800=2/hr, 2400=1.5/hr
  else if (strcasecmp_P(field, PSTR("SATtargettemp")) == 0)      settings.sat.fTargetTemp = clampSettingFloat(newValue, settings.sat.fTargetTemp, 5.0f, 30.0f);
  else if (strcasecmp_P(field, PSTR("SATcoefficient")) == 0)     settings.sat.fHeatingCurveCoeff = clampSettingFloat(newValue, settings.sat.fHeatingCurveCoeff, 0.1f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATdeadband")) == 0)        settings.sat.fDeadband = clampSettingFloat(newValue, settings.sat.fDeadband, 0.05f, 2.0f);
  else if (strcasecmp_P(field, PSTR("SATinterval")) == 0) {
    settings.sat.iControlInterval = constrain(atoi(newValue), 10, 300);
    CHANGE_INTERVAL_SEC(timerSATControl, settings.sat.iControlInterval);
  }
  else if (strcasecmp_P(field, PSTR("SATexternaltemp")) == 0)    settings.sat.bUseExternalTemp = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATpresetcomfort")) == 0)   settings.sat.fPresetComfort = clampSettingFloat(newValue, settings.sat.fPresetComfort, 15.0f, 28.0f);
  else if (strcasecmp_P(field, PSTR("SATpreseteco")) == 0)       settings.sat.fPresetEco = clampSettingFloat(newValue, settings.sat.fPresetEco, 10.0f, 22.0f);
  else if (strcasecmp_P(field, PSTR("SATpresetaway")) == 0)      settings.sat.fPresetAway = clampSettingFloat(newValue, settings.sat.fPresetAway, 5.0f, 18.0f);
  else if (strcasecmp_P(field, PSTR("SATpresetsleep")) == 0)     settings.sat.fPresetSleep = clampSettingFloat(newValue, settings.sat.fPresetSleep, 5.0f, 25.0f);
  else if (strcasecmp_P(field, PSTR("SATpresetactivity")) == 0)  settings.sat.fPresetActivity = clampSettingFloat(newValue, settings.sat.fPresetActivity, 5.0f, 20.0f);
  else if (strcasecmp_P(field, PSTR("SATpresethome")) == 0)      settings.sat.fPresetHome = clampSettingFloat(newValue, settings.sat.fPresetHome, 10.0f, 25.0f);
  else if (strcasecmp_P(field, PSTR("SATpwmautoswitch")) == 0)   settings.sat.bPwmAutoSwitch = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATmaxmodulation")) == 0)   settings.sat.iMaxRelModulation = constrain(atoi(newValue), 0, 100);
  else if (strcasecmp_P(field, PSTR("SATovershootmargin")) == 0) settings.sat.fOvershootMargin = clampSettingFloat(newValue, settings.sat.fOvershootMargin, 0.5f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATmodsupdelay")) == 0)    settings.sat.fModSupDelay = clampSettingFloat(newValue, settings.sat.fModSupDelay, 0.0f, 120.0f);
  else if (strcasecmp_P(field, PSTR("SATmodsupoffset")) == 0)   settings.sat.fModSupOffset = clampSettingFloat(newValue, settings.sat.fModSupOffset, 0.0f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATdhwsetpoint")) == 0)    settings.sat.fDhwSetpoint = clampSettingFloat(newValue, settings.sat.fDhwSetpoint, 0.0f, 60.0f);
  else if (strcasecmp_P(field, PSTR("SATdhwenabled")) == 0)     settings.sat.bDhwEnabled = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATdhwenable")) == 0) {
    // TASK-516: master DHW enable. Persist always; only emit HW= when boiler
    // reports MsgID 3 HB3=1 (storage tank). HB3 sits in bit 11 of the uint16
    // SlaveConfigMemberIDcode (= bit 3 of the high byte). On combi boilers
    // (HB3=0) we silently keep the setting in NVRAM but never push HW=.
    bool newVal = EVALBOOLEAN(newValue);
    bool changed = (settings.sat.bDhwEnable != newVal);
    settings.sat.bDhwEnable = newVal;
    if (changed && (OTcurrentSystemState.SlaveConfigMemberIDcode & 0x0800)) {
      const char *cmd = newVal ? "HW=1" : "HW=0";
      addCommandToQueue(cmd, strlen(cmd), true);
      DebugTf(PSTR("TASK-516: dhw_enable -> %s, queued %s\r\n"), CBOOLEAN(newVal), cmd);
    }
  }
  else if (strcasecmp_P(field, PSTR("SATpushsetpoint")) == 0)   settings.sat.bPushSetpoint = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATfollowthermostat")) == 0) { settings.sat.bFollowThermostat = EVALBOOLEAN(newValue); g_sawSatFollowKey = true; }  // TASK-1259: key presence = stored value (see readSettings default)
  else if (strcasecmp_P(field, PSTR("SATflameoffset")) == 0)    settings.sat.fFlameOffOffset = clampSettingFloat(newValue, settings.sat.fFlameOffOffset, 0.0f, 30.0f);
  else if (strcasecmp_P(field, PSTR("SATwindowdetect")) == 0)  settings.sat.bWindowDetection = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATwindowminsec")) == 0)  settings.sat.iWindowMinOpenSec = constrain(atoi(newValue), 10, 600);
  else if (strcasecmp_P(field, PSTR("SATtempstep")) == 0)     settings.sat.fTargetTempStep = clampSettingFloat(newValue, settings.sat.fTargetTempStep, 0.1f, 1.0f);
  else if (strcasecmp_P(field, PSTR("SATforcepwm")) == 0)     settings.sat.bForcePWM = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATovershootprotection")) == 0) settings.sat.bOvershootProtection = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATflowoffset")) == 0)   settings.sat.fFlowOffset = clampSettingFloat(newValue, settings.sat.fFlowOffset, 0.5f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATminpressure")) == 0)  settings.sat.fMinPressure = clampSettingFloat(newValue, settings.sat.fMinPressure, 0.0f, 3.0f);
  else if (strcasecmp_P(field, PSTR("SATmaxpressure")) == 0)  settings.sat.fMaxPressure = clampSettingFloat(newValue, settings.sat.fMaxPressure, 1.0f, 4.0f);
  else if (strcasecmp_P(field, PSTR("SATmaxpressdrop")) == 0) settings.sat.fMaxPressureDrop = clampSettingFloat(newValue, settings.sat.fMaxPressureDrop, 0.05f, 2.0f);
  else if (strcasecmp_P(field, PSTR("SATmanufacturer")) == 0) settings.sat.iManufacturer = constrain(atoi(newValue), 0, SAT_MFR_COUNT - 1);
  else if (strcasecmp_P(field, PSTR("SATweatherenable")) == 0) settings.sat.bWeatherEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATweatherlat")) == 0)    settings.sat.fWeatherLat = clampSettingFloat(newValue, settings.sat.fWeatherLat, -90.0f, 90.0f);
  else if (strcasecmp_P(field, PSTR("SATweatherlon")) == 0)    settings.sat.fWeatherLon = clampSettingFloat(newValue, settings.sat.fWeatherLon, -180.0f, 180.0f);
  else if (strcasecmp_P(field, PSTR("SATweatherinterval")) == 0) {
    // Hard floor at 900 s (15 min) to stay well inside the Open-Meteo
    // free-tier rate limit.
    settings.sat.iWeatherInterval = constrain(atoi(newValue), 900, 3600);
    CHANGE_INTERVAL_SEC(timerWeatherPoll, settings.sat.iWeatherInterval);
  }
  else if (strcasecmp_P(field, PSTR("SATboilercapacity")) == 0) settings.sat.fBoilerCapacity = clampSettingFloat(newValue, settings.sat.fBoilerCapacity, 1.0f, 100.0f);
  else if (strcasecmp_P(field, PSTR("SATdhwcapacity")) == 0) settings.sat.fDhwCapacity = clampSettingFloat(newValue, settings.sat.fDhwCapacity, 0.0f, 100.0f);
  else if (strcasecmp_P(field, PSTR("SATboilerratedkw")) == 0)   settings.sat.fBoilerRatedKW = clampSettingFloat(newValue, settings.sat.fBoilerRatedKW, 0.0f, 200.0f);
  else if (strcasecmp_P(field, PSTR("SATboilerefficiency")) == 0) settings.sat.fBoilerEfficiency = clampSettingFloat(newValue, settings.sat.fBoilerEfficiency, 0.5f, 1.0f);
  else if (strcasecmp_P(field, PSTR("SATpresetsync")) == 0) settings.sat.bPresetSync = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATpresetsynctopic")) == 0) strlcpy(settings.sat.sPresetSyncTopic, newValue, sizeof(settings.sat.sPresetSyncTopic));
  else if (strcasecmp_P(field, PSTR("SATsimulation")) == 0) {
    bool wantSim = EVALBOOLEAN(newValue);
    // TASK-795 §4.2: availability gate. Simulation is a boiler-absent bench
    // mode. Refuse to enable it while a real boiler is on the bus, regardless
    // of the request source (MQTT, settings restore, REST — REST also returns
    // HTTP 409 earlier for a clearer client error). This is the single
    // chokepoint that every writer funnels through.
    if (wantSim && satBoilerHardwarePresent()) {
      DebugTln(F("SAT-SIM: enable rejected — boiler hardware present"));
      sendEventToWebSocket_P('!', PSTR("SAT-SIM: enable rejected, boiler present"));
      settings.sat.bSimulation = false;
    } else {
      settings.sat.bSimulation = wantSim;
      if (settings.sat.bSimulation) {
        state.sat.iSimLastUpdateMs = 0;  // reset on enable
        state.sat.bSimWarmupDone = false;
      }
    }
  }
  else if (strcasecmp_P(field, PSTR("SATsimheatrate")) == 0) settings.sat.fSimHeatRate = clampSettingFloat(newValue, settings.sat.fSimHeatRate, 0.01f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATsimcoolrate")) == 0) settings.sat.fSimCoolRate = clampSettingFloat(newValue, settings.sat.fSimCoolRate, 0.01f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATthermalcoeff")) == 0) settings.sat.fThermalCoeff = clampSettingFloat(newValue, settings.sat.fThermalCoeff, 0.005f, 0.3f);
  else if (strcasecmp_P(field, PSTR("SATsolargain")) == 0) settings.sat.bSolarGainEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATsolarminrise")) == 0) settings.sat.fSolarMinRiseRate = clampSettingFloat(newValue, settings.sat.fSolarMinRiseRate, 0.1f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATsolaroffset")) == 0) settings.sat.fSolarSetpointOffset = clampSettingFloat(newValue, settings.sat.fSolarSetpointOffset, 0.5f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATsolarminelev")) == 0) settings.sat.fSolarMinElevation = clampSettingFloat(newValue, settings.sat.fSolarMinElevation, -10.0f, 45.0f);
  else if (strcasecmp_P(field, PSTR("SATsummersimmer")) == 0) settings.sat.bSummerSimmer = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATsummerthreshold")) == 0) settings.sat.fSummerThreshold = clampSettingFloat(newValue, settings.sat.fSummerThreshold, 5.0f, 35.0f);
  else if (strcasecmp_P(field, PSTR("SATsummerminhours")) == 0) settings.sat.iSummerMinHours = constrain(atoi(newValue), 1, 48);
  else if (strcasecmp_P(field, PSTR("SATcomfortadjust")) == 0) settings.sat.bComfortAdjust = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATcomforthumidity")) == 0) settings.sat.fComfortHumidity = clampSettingFloat(newValue, settings.sat.fComfortHumidity, 10.0f, 90.0f);
  else if (strcasecmp_P(field, PSTR("SATcomfortmaxoffset")) == 0) settings.sat.fComfortMaxOffset = clampSettingFloat(newValue, settings.sat.fComfortMaxOffset, 0.0f, 3.0f);
  else if (strcasecmp_P(field, PSTR("SATmultiarea")) == 0) settings.sat.bMultiArea = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATmultiareacount")) == 0) settings.sat.iMultiAreaCount = constrain(atoi(newValue), 0, 4);
  else if (strcasecmp_P(field, PSTR("SATareaweight0")) == 0) settings.sat.fAreaWeight[0] = clampSettingFloat(newValue, settings.sat.fAreaWeight[0], 0.0f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATareaweight1")) == 0) settings.sat.fAreaWeight[1] = clampSettingFloat(newValue, settings.sat.fAreaWeight[1], 0.0f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATareaweight2")) == 0) settings.sat.fAreaWeight[2] = clampSettingFloat(newValue, settings.sat.fAreaWeight[2], 0.0f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATareaweight3")) == 0) settings.sat.fAreaWeight[3] = clampSettingFloat(newValue, settings.sat.fAreaWeight[3], 0.0f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATautotune")) == 0) settings.sat.bAutoTune = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATautotunerate")) == 0) settings.sat.fAutoTuneRate = clampSettingFloat(newValue, settings.sat.fAutoTuneRate, 0.005f, 0.1f);
  // SAT Python parity settings (Task #82)
  else if (strcasecmp_P(field, PSTR("SATsensormaxage")) == 0)  settings.sat.iSensorMaxAgeS = constrain((uint32_t)atol(newValue), 60UL, 86400UL);
  else if (strcasecmp_P(field, PSTR("SATerrormon")) == 0)      settings.sat.bErrorMonitoring = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATautogains")) == 0)     settings.sat.fAutoGainsValue = clampSettingFloat(newValue, settings.sat.fAutoGainsValue, 0.1f, 10.0f);
  else if (strcasecmp_P(field, PSTR("SATheatingmode")) == 0)   settings.sat.iHeatingMode = constrain(atoi(newValue), 0, 1);
  else if (strcasecmp_P(field, PSTR("SATcyclesperhour")) == 0) settings.sat.iCyclesPerHour = constrain(atoi(newValue), 2, 6);
  else if (strcasecmp_P(field, PSTR("SATvalveoffset")) == 0)   settings.sat.fValveOffset = clampSettingFloat(newValue, settings.sat.fValveOffset, -1.0f, 1.0f);
  else if (strcasecmp_P(field, PSTR("SATthermalcomfort")) == 0) settings.sat.bThermalComfort = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SAThumiditytimeout")) == 0) settings.sat.iHumidityTimeoutS = (uint16_t)constrain(atoi(newValue), 60, 65535);
  else if (strcasecmp_P(field, PSTR("SATsolarfreezeint")) == 0) settings.sat.bSolarFreezeIntegral = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATflushtreshold")) == 0)  settings.sat.iSatFlushThresholdH = (uint16_t)constrain(atoi(newValue), 1, 720);
  // Multi-zone PID (Task #233)
  else if (strcasecmp_P(field, PSTR("SATzonecount")) == 0)    settings.sat.iZoneCount = (uint8_t)constrain(atoi(newValue), 1, 4);
  else if (strcasecmp_P(field, PSTR("SATzonetimeout")) == 0)  settings.sat.iZoneTimeoutS = (uint16_t)constrain(atoi(newValue), 30, 3600);
  else if (strcasecmp_P(field, PSTR("SATzoneheadroom")) == 0) settings.sat.fZoneAggregationHeadroom = clampSettingFloat(newValue, settings.sat.fZoneAggregationHeadroom, 0.0f, 15.0f);
  // TASK-587: DS18B20 sensor-to-SAT-area mapping
  else if (strcasecmp_P(field, PSTR("SATsensorarea0")) == 0) strlcpy(settings.sat.sSensorArea[0], newValue, sizeof(settings.sat.sSensorArea[0]));
  else if (strcasecmp_P(field, PSTR("SATsensorarea1")) == 0) strlcpy(settings.sat.sSensorArea[1], newValue, sizeof(settings.sat.sSensorArea[1]));
  else if (strcasecmp_P(field, PSTR("SATsensorarea2")) == 0) strlcpy(settings.sat.sSensorArea[2], newValue, sizeof(settings.sat.sSensorArea[2]));
  else if (strcasecmp_P(field, PSTR("SATsensorarea3")) == 0) strlcpy(settings.sat.sSensorArea[3], newValue, sizeof(settings.sat.sSensorArea[3]));
  // PV-surplus setpoint boost (TASK-640)
  else if (strcasecmp_P(field, PSTR("SATpvboostenabled")) == 0) {
    settings.sat.bPvBoostEnabled = (strcasecmp_P(newValue, PSTR("true")) == 0 || atoi(newValue) != 0);
    if (!settings.sat.bPvBoostEnabled) {
      state.sat.bPvBoostActive = false;
      state.sat.fPvBoostAppliedC = 0.0f;
      state.sat.iPvBoostStartedMs = 0;
    }
  }
  else if (strcasecmp_P(field, PSTR("SATpvboostthresholdw")) == 0)     settings.sat.iPvBoostThresholdW = (uint16_t)constrain(atoi(newValue), 100, 10000);
  else if (strcasecmp_P(field, PSTR("SATpvboostholds")) == 0)          settings.sat.iPvBoostHoldS = (uint16_t)constrain(atoi(newValue), 30, 600);
  else if (strcasecmp_P(field, PSTR("SATpvboostdeltac")) == 0)         settings.sat.fPvBoostDeltaC = clampSettingFloat(newValue, settings.sat.fPvBoostDeltaC, 0.5f, 5.0f);
  else if (strcasecmp_P(field, PSTR("SATpvboostmaxindoorc")) == 0)     settings.sat.fPvBoostMaxIndoorC = clampSettingFloat(newValue, settings.sat.fPvBoostMaxIndoorC, 18.0f, 28.0f);
  else if (strcasecmp_P(field, PSTR("SATpvboostmaxdurationmin")) == 0) settings.sat.iPvBoostMaxDurationMin = (uint16_t)constrain(atoi(newValue), 30, 1440);
  // --- BLE temperature sensor settings (Task #20) ---
  // ESP-abstraction Tier 2 (TASK-742): parsed unconditionally so a settings.json
  // written on either platform round-trips; harmless no-op fields on ESP8266.
  else if (strcasecmp_P(field, PSTR("SATbleenable")) == 0)  settings.sat.bBleEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATbleriskack")) == 0) settings.sat.bBleRiskAck = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATblefailover")) == 0) settings.sat.bBleFailover = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("SATblemac")) == 0)      strlcpy(settings.sat.sBleMAC, newValue, sizeof(settings.sat.sBleMAC));
  else if (strcasecmp_P(field, PSTR("SATbleinterval")) == 0) settings.sat.iBleInterval = constrain(atoi(newValue), 10, 300);
  // TASK-895: name-prefix filter + ingestion toggle. Exact-match keys placed
  // before the SATblemacN/SATblelabelN prefix branches below; "SATblename..."
  // shares no 9/11-char prefix with "SATblemac"/"SATblelabel" so no collision.
  else if (strcasecmp_P(field, PSTR("SATblenameprefix")) == 0)      strlcpy(settings.sat.sBleNamePrefix, newValue, sizeof(settings.sat.sBleNamePrefix));
  else if (strcasecmp_P(field, PSTR("SATblenamefilteringest")) == 0) settings.sat.bBleNameFilterIngest = EVALBOOLEAN(newValue);
  // TASK-508: roster slot keys — SATblemacN / SATblelabelN with N=0..7.
  // Prefix-match + digit suffix to avoid 17 individual else-if branches.
  // The exact-match SATblemac above takes precedence (legacy selected MAC).
  else if (strncasecmp_P(field, PSTR("SATblemac"), 9) == 0 && isdigit((unsigned char)field[9])) {
    int idx = atoi(field + 9);
    if (idx >= 0 && idx < SAT_BLE_MAX_ROSTER) {
      // Validate: empty (slot cleared) OR 17-char colon-separated hex MAC
      bool valid = (newValue[0] == '\0');
      if (!valid && strlen(newValue) == 17) {
        valid = true;
        for (int p = 0; valid && p < 17; p++) {
          if (p == 2 || p == 5 || p == 8 || p == 11 || p == 14) {
            valid = (newValue[p] == ':');
          } else {
            valid = isxdigit((unsigned char)newValue[p]);
          }
        }
      }
      if (valid) {
        strlcpy(settings.sat.sBleMac[idx], newValue, sizeof(settings.sat.sBleMac[idx]));
        // Canonicalise to uppercase so onResult comparisons via strcasecmp
        // match the BLE-stack-emitted form.
        for (int p = 0; settings.sat.sBleMac[idx][p]; p++) {
          settings.sat.sBleMac[idx][p] = toupper((unsigned char)settings.sat.sBleMac[idx][p]);
        }
      }
    }
  }
  else if (strncasecmp_P(field, PSTR("SATblelabel"), 11) == 0 && isdigit((unsigned char)field[11])) {
    int idx = atoi(field + 11);
    if (idx >= 0 && idx < SAT_BLE_MAX_ROSTER) {
      strlcpy(settings.sat.sBleLabel[idx], newValue, sizeof(settings.sat.sBleLabel[idx]));
    }
  }
  // TASK-930 Phase 2: per-slot MiBeacon bindkey. Accept empty (clear) or EXACTLY
  // 32 hex chars; store lowercase. Malformed input is ignored (slot unchanged).
  // No 9/11-char prefix collision with SATblemac/SATblelabel (differ at index 6).
  else if (strncasecmp_P(field, PSTR("SATblebindkey"), 13) == 0 && isdigit((unsigned char)field[13])) {
    int idx = atoi(field + 13);
    if (idx >= 0 && idx < SAT_BLE_MAX_ROSTER) {
      size_t n = strlen(newValue);
      bool ok = (n == 0);
      if (n == 32) {
        ok = true;
        for (size_t p = 0; p < 32; p++) { if (!isxdigit((unsigned char)newValue[p])) { ok = false; break; } }
      }
      if (ok) {
        strlcpy(settings.sat.sBleBindkey[idx], newValue, sizeof(settings.sat.sBleBindkey[idx]));
        for (int p = 0; settings.sat.sBleBindkey[idx][p]; p++) {
          settings.sat.sBleBindkey[idx][p] = tolower((unsigned char)settings.sat.sBleBindkey[idx][p]);
        }
      }
    }
  }
  else if (strcasecmp_P(field, PSTR("SATblerostercount")) == 0) {
    settings.sat.iBleRosterCount = (uint8_t)constrain(atoi(newValue), 0, SAT_BLE_MAX_ROSTER);
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  // --- OT-direct settings ---
  else if (strcasecmp_P(field, PSTR("OTDmode")) == 0)           settings.otd.iMode = constrain(atoi(newValue), 0, 4);
  else if (strcasecmp_P(field, PSTR("OTDautodetect")) == 0)    settings.otd.bAutoDetect = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDsetbacktemp")) == 0)    settings.otd.fSetbackTemp = clampSettingFloat(newValue, settings.otd.fSetbackTemp, 1.0f, 30.0f);
  else if (strcasecmp_P(field, PSTR("OTDsetbacktimeout")) == 0) settings.otd.iSetbackTimeout = constrain(atoi(newValue), 5, 255);
  else if (strcasecmp_P(field, PSTR("OTDenableslave")) == 0)   settings.otd.bEnableSlave = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDsummermode")) == 0)    settings.otd.bSummerMode = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDfailsafe")) == 0)      settings.otd.bFailSafe = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDmsginterval")) == 0)   settings.otd.iMsgInterval = constrain(atoi(newValue), 100, 1275);
  else if (strcasecmp_P(field, PSTR("OTDhasbypassrelay")) == 0) settings.otd.bHasBypassRelay = EVALBOOLEAN(newValue);
  // --- TASK-183: PI room compensation + heating curve ---
  else if (strcasecmp_P(field, PSTR("OTDchmode")) == 0)         settings.otd.iCHMode = constrain(atoi(newValue), 0, 2);
  else if (strcasecmp_P(field, PSTR("OTDflowtemp")) == 0)       settings.otd.fFlowTemp = clampSettingFloat(newValue, settings.otd.fFlowTemp, 5.0f, 90.0f);
  else if (strcasecmp_P(field, PSTR("OTDflowmax")) == 0)        settings.otd.fFlowMax = clampSettingFloat(newValue, settings.otd.fFlowMax, 20.0f, 90.0f);
  else if (strcasecmp_P(field, PSTR("OTDroomsetpoint")) == 0)   settings.otd.fRoomSetpoint = clampSettingFloat(newValue, settings.otd.fRoomSetpoint, 5.0f, 30.0f);
  else if (strcasecmp_P(field, PSTR("OTDgradient")) == 0)       settings.otd.fGradient = clampSettingFloat(newValue, settings.otd.fGradient, 0.1f, 5.0f);
  else if (strcasecmp_P(field, PSTR("OTDexponent")) == 0)       settings.otd.fExponent = clampSettingFloat(newValue, settings.otd.fExponent, 0.5f, 2.0f);
  else if (strcasecmp_P(field, PSTR("OTDoffset")) == 0)         settings.otd.fOffset = clampSettingFloat(newValue, settings.otd.fOffset, -10.0f, 10.0f);
  else if (strcasecmp_P(field, PSTR("OTDroomcomp")) == 0)       settings.otd.bRoomCompEnabled = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDkp")) == 0)             settings.otd.fKp = clampSettingFloat(newValue, settings.otd.fKp, 0.0f, 20.0f);
  else if (strcasecmp_P(field, PSTR("OTDki")) == 0)             settings.otd.fKi = clampSettingFloat(newValue, settings.otd.fKi, 0.0f, 5.0f);
  else if (strcasecmp_P(field, PSTR("OTDkboost")) == 0)         settings.otd.fKboost = clampSettingFloat(newValue, settings.otd.fKboost, 0.0f, 10.0f);
  // --- TASK-582: CH hysteresis deadband ---
  else if (strcasecmp_P(field, PSTR("OTDhysteresisenable")) == 0) settings.otd.bHysteresisEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDhysteresis")) == 0)       settings.otd.fHysteresis = clampSettingFloat(newValue, settings.otd.fHysteresis, 0.0f, 2.0f);
  // --- TASK-584: ventilation override persistence ---
  else if (strcasecmp_P(field, PSTR("OTDventenable")) == 0)      settings.otd.bVentEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDopenbypass")) == 0)      settings.otd.bOpenBypass = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDautobypass")) == 0)      settings.otd.bAutoBypass = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDfreeventenable")) == 0)  settings.otd.bFreeVentEnable = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("OTDventsetpoint")) == 0)    settings.otd.iVentSetpoint = (uint8_t)constrain(atoi(newValue), 0, 100);
#endif
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // Ethernet static IP (OTGW32 only)
  else if (strcasecmp_P(field, PSTR("ETHstaticip")) == 0)       settings.eth.bStaticIP = EVALBOOLEAN(newValue);
  else if (strcasecmp_P(field, PSTR("ETHipaddress")) == 0)    { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.eth.sIPaddress, newValue, sizeof(settings.eth.sIPaddress)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  else if (strcasecmp_P(field, PSTR("ETHgateway")) == 0)      { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.eth.sGateway, newValue, sizeof(settings.eth.sGateway)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  else if (strcasecmp_P(field, PSTR("ETHsubnet")) == 0)       { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.eth.sSubnet, newValue, sizeof(settings.eth.sSubnet)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  else if (strcasecmp_P(field, PSTR("ETHdns")) == 0)          { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.eth.sDNS, newValue, sizeof(settings.eth.sDNS)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
#endif
  // WiFi static IP. Empty string = DHCP. Parse-validate non-empty values.
  else if (strcasecmp_P(field, PSTR("WifiStaticIP")) == 0) {
    if (newValue[0] == '\0') { settings.wifi.sStaticIp[0] = '\0'; }
    else { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.wifi.sStaticIp, newValue, sizeof(settings.wifi.sStaticIp)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  }
  else if (strcasecmp_P(field, PSTR("WifiSubnet")) == 0) {
    if (newValue[0] == '\0') { settings.wifi.sSubnet[0] = '\0'; }
    else { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.wifi.sSubnet, newValue, sizeof(settings.wifi.sSubnet)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  }
  else if (strcasecmp_P(field, PSTR("WifiGateway")) == 0) {
    if (newValue[0] == '\0') { settings.wifi.sGateway[0] = '\0'; }
    else { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.wifi.sGateway, newValue, sizeof(settings.wifi.sGateway)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  }
  else if (strcasecmp_P(field, PSTR("WifiDns1")) == 0) {
    if (newValue[0] == '\0') { settings.wifi.sDns1[0] = '\0'; }
    else { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.wifi.sDns1, newValue, sizeof(settings.wifi.sDns1)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  }
  else if (strcasecmp_P(field, PSTR("WifiDns2")) == 0) {
    if (newValue[0] == '\0') { settings.wifi.sDns2[0] = '\0'; }
    else { IPAddress t; if (t.fromString(newValue)) strlcpy(settings.wifi.sDns2, newValue, sizeof(settings.wifi.sDns2)); else DebugTf(PSTR("Invalid IP '%s', ignored\r\n"), newValue); }
  }

  // Side-effect checks — independent if's, multiple can fire
  // Every MQTT key starts with "MQTT"; case-insensitive, because REST passes the name as sent (TASK-1240)
  if (strncasecmp_P(field, PSTR("mqtt"), 4) == 0)   pendingSideEffects |= SIDE_EFFECT_MQTT; // defer MQTT restart to flushSettings()

  // TASK-564: no-op detection. If the cascade above produced no change to the
  // OTGWSettings struct, this update is identical to the current state — skip
  // the dirty mark, restore pendingSideEffects to its pre-call value, and do
  // NOT restart the debounce timer (which would have triggered a wasted flash
  // rewrite). Verified by smoke test toggling satblemac twice with the same
  // empty value: only one would-be flush fires.
  if (platformSettingsNoopUnchanged(&settings, sizeof(settings))) {
    pendingSideEffects = pendingSideEffectsSnapshot;
    DebugTf(PSTR("[Settings] no-op skip: field[%s] equals current value, no flush scheduled\r\n"), field);
    return;
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
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
****************************************************************************
*/
