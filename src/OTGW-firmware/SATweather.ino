/*
***************************************************************************
**  Module   : SATweather.ino
**  Description: Weather data integration for SAT
**
**  Part of SAT (Smart Autotune Thermostat) firmware port.
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Weather data integration via Open-Meteo API.
**  Fetches all current-conditions fields relevant for SAT thermal-load
**  calculations (temperature, feels-like, humidity, wind, cloud cover,
**  pressure, precipitation, WMO weather code, is_day) plus 24-hour hourly
**  forecasts for temperature, dew point, cloud cover, and precipitation
**  probability.  Free API, no API key required.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Constants ---
static const uint16_t WEATHER_POLL_DEFAULT_SEC = 900;  // 15 min
static const uint16_t WEATHER_POLL_MIN_SEC     = 300;  // 5 min
static const uint8_t  WEATHER_FORECAST_HOURS   = 24;

// --- Hourly forecast arrays (24 h, BSS-resident static buffers) ---
// temperature_2m: primary thermal load forecast (float — °C, precision matters)
static float    _weather_forecastTemp[WEATHER_FORECAST_HOURS];
// dew_point_2m: comfort / condensation forecast (float — °C)
static float    _weather_forecastDewPt[WEATHER_FORECAST_HOURS];
// cloud_cover: solar-gain forecast (uint8_t — % 0-100, saves 72 bytes vs float)
static uint8_t  _weather_forecastCloud[WEATHER_FORECAST_HOURS];
// precipitation_probability: scheduling guard (uint8_t — % 0-100)
static uint8_t  _weather_forecastPrecipProb[WEATHER_FORECAST_HOURS];
static uint8_t  _weather_forecastCount = 0;

// Timer — uses configurable interval, default 15 min
DECLARE_TIMER_SEC(timerWeatherPoll, WEATHER_POLL_DEFAULT_SEC, CATCH_UP_MISSED_TICKS);

//=====================================================================
//=== Lightweight JSON helpers (no ArduinoJson!) ===
//=====================================================================

// Copy a PROGMEM (PGM_P) string key into a RAM buffer.
// Required before passing to snprintf() as a %s argument — %s reads RAM.
// On ESP8266, passing a PROGMEM pointer directly as %s can cause
// unaligned flash reads (Exception 2) if the pointer is not word-aligned.
static inline void weatherCopyKey(PGM_P key, char* buf, size_t bufSize)
{
  strncpy_P(buf, key, bufSize - 1);
  buf[bufSize - 1] = '\0';
}

// Find "key": <number> in a JSON string and return the float value.
// key must be a PROGMEM string (PGM_P / PSTR).
// Handles nested objects by searching for the exact key string.
// Returns true if found and parsed successfully.
static bool weatherJsonGetFloat(const char* json, PGM_P key, float* out)
{
  if (!json || !key || !out) return false;

  // Copy key from PROGMEM to RAM — snprintf %s reads RAM, not flash.
  // Without this, snprintf_P with PSTR key causes misread on ESP8266 (PROGMEM
  // byte access requires word-aligned reads; %s assumes RAM pointer).
  char keyBuf[32];
  weatherCopyKey(key, keyBuf, sizeof(keyBuf));

  // Build search pattern: "key":
  char search[48];
  snprintf(search, sizeof(search), "\"%s\":", keyBuf);

  const char* pos = strstr(json, search);
  if (!pos) return false;

  pos += strlen(search);
  // Skip whitespace
  while (*pos == ' ' || *pos == '\t') pos++;

  char* endp = nullptr;
  float val = strtod(pos, &endp);
  if (endp == pos) return false;  // no number found

  *out = val;
  return true;
}

// Find "key":[ ... ] array in the "hourly" section and parse up to maxLen floats.
// The Open-Meteo response has duplicate keys (temperature_2m in both "current"
// and "hourly"), so we first locate the "hourly" section, then search within it.
// key must be a PROGMEM string (PGM_P / PSTR).
static bool weatherJsonGetArray(const char* json, PGM_P key, float* arr, uint8_t maxLen, uint8_t* count)
{
  if (!json || !key || !arr || !count) return false;
  *count = 0;

  // Find "hourly" section first
  const char* hourlyPos = strstr_P(json, PSTR("\"hourly\""));
  if (!hourlyPos) return false;

  // Copy key from PROGMEM to RAM (same reason as weatherJsonGetFloat)
  char keyBuf[32];
  weatherCopyKey(key, keyBuf, sizeof(keyBuf));

  // Build search pattern: "key":[
  char search[48];
  snprintf(search, sizeof(search), "\"%s\":[", keyBuf);

  const char* pos = strstr(hourlyPos, search);
  if (!pos) return false;

  pos += strlen(search);

  uint8_t n = 0;
  while (n < maxLen && *pos != '\0' && *pos != ']') {
    // Skip whitespace and commas
    while (*pos == ' ' || *pos == ',' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
    if (*pos == ']' || *pos == '\0') break;

    // Handle null values in the array
    if (strncmp_P(pos, PSTR("null"), 4) == 0) {
      arr[n++] = 0.0f;
      pos += 4;
      continue;
    }

    char* endp = nullptr;
    float val = strtod(pos, &endp);
    if (endp == pos) break;  // not a number
    arr[n++] = val;
    pos = endp;
  }

  *count = n;
  return n > 0;
}

// Variant of weatherJsonGetArray that stores values as uint8_t (0-100 integer fields
// such as precipitation_probability and cloud_cover).  Saves 3 bytes per element vs float.
// key must be a PROGMEM string (PGM_P / PSTR).
static bool weatherJsonGetArrayU8(const char* json, PGM_P key, uint8_t* arr, uint8_t maxLen, uint8_t* count)
{
  if (!json || !key || !arr || !count) return false;
  *count = 0;

  const char* hourlyPos = strstr_P(json, PSTR("\"hourly\""));
  if (!hourlyPos) return false;

  char keyBuf[32];
  weatherCopyKey(key, keyBuf, sizeof(keyBuf));

  char search[48];
  snprintf(search, sizeof(search), "\"%s\":[", keyBuf);

  const char* pos = strstr(hourlyPos, search);
  if (!pos) return false;

  pos += strlen(search);

  uint8_t n = 0;
  while (n < maxLen && *pos != '\0' && *pos != ']') {
    while (*pos == ' ' || *pos == ',' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
    if (*pos == ']' || *pos == '\0') break;

    if (strncmp_P(pos, PSTR("null"), 4) == 0) {
      arr[n++] = 0;
      pos += 4;
      continue;
    }

    char* endp = nullptr;
    long val = strtol(pos, &endp, 10);
    if (endp == pos) break;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    arr[n++] = (uint8_t)val;
    pos = endp;
  }

  *count = n;
  return n > 0;
}

// Open-Meteo API URL format string — PROGMEM to keep format string in flash.
// Request: all current fields relevant for SAT thermal load + 24-hour hourly
// forecasts for temperature, dew point, cloud cover, and precipitation probability.
// HTTP only (no TLS) — firmware never does HTTPS (ADR constraint).
static const char kWeatherUrlFmt[] PROGMEM =
  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
  "&forecast_days=1"
  "&current=temperature_2m,relative_humidity_2m,apparent_temperature"
  ",is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover"
  ",pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m"
  "&hourly=temperature_2m,relative_humidity_2m,dew_point_2m,apparent_temperature"
  ",precipitation_probability,cloud_cover,cloud_cover_low,cloud_cover_mid";

// Heap guard: bytes required *in addition* to the JSON buffer before we
// attempt malloc — leaves room for HTTPClient, WiFiClient, and background tasks.
static const size_t WEATHER_HEAP_GUARD = 2048;

//=====================================================================
//=== HTTP Fetch ===
//=====================================================================
void weatherFetch()
{
  if (!settings.sat.bWeatherEnable) return;
  // Need valid coordinates (not both zero)
  if (settings.sat.fWeatherLat == 0.0f && settings.sat.fWeatherLon == 0.0f) {
    DebugTln(F("Weather: skipping fetch, no coordinates configured"));
    return;
  }

  // Build URL — expanded format string needs a larger buffer (512 bytes)
  char url[512];
  snprintf_P(url, sizeof(url), kWeatherUrlFmt,
             settings.sat.fWeatherLat, settings.sat.fWeatherLon);

  DebugTf(PSTR("Weather: fetching %s\r\n"), url);

  // WiFiClient and HTTPClient are resolved via the platform include chain:
  //   ESP8266: platform_esp8266.h → <ESP8266HTTPClient.h> (provides WiFiClient, HTTPClient)
  //   ESP32:   platform_esp32.h   → <HTTPClient.h>        (provides WiFiClient, HTTPClient)
  // Both expose the same http.begin(WiFiClient&, url) API, so no #if guard is needed here.
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);   // 5s timeout — ESP8266 HW WDT fires at ~8s; stay well within margin

  if (!http.begin(client, url)) {
    DebugTln(F("Weather: http.begin() failed"));
    state.sat.weather.iFetchErrors++;
    return;
  }

  yield();
  int httpCode = http.GET();
  yield();

  if (httpCode == 200) {
    // Use 5120-byte heap buffer — expanded API response (15 current + 8×24h hourly)
    // is estimated at ~3-4 KB; 5120 provides comfortable margin.
    // Heap guard: require buffer + 2048 bytes free to leave room for other tasks.
    constexpr size_t WEATHER_BUF_SIZE = 5120;
    if (platformFreeHeap() < (WEATHER_BUF_SIZE + WEATHER_HEAP_GUARD)) {
      DebugTln(F("Weather: free heap too low for fetch, skipping"));
      state.sat.weather.iFetchErrors++;
      http.end();
      return;
    }
    char* json = (char*) malloc(WEATHER_BUF_SIZE);
    if (!json) {
      DebugTln(F("Weather: malloc failed, skipping"));
      state.sat.weather.iFetchErrors++;
      http.end();
      return;
    }
    WiFiClient* stream = http.getStreamPtr();
    size_t total = 0;
    while (stream && stream->connected() && total < WEATHER_BUF_SIZE - 1) {
      int avail = stream->available();
      if (avail <= 0) {
        if (http.connected()) { yield(); delay(1); continue; }
        break;
      }
      int toRead = (int)(WEATHER_BUF_SIZE - 1 - total);
      if (toRead > avail) toRead = avail;
      int got = stream->readBytes(json + total, toRead);
      if (got <= 0) break;
      total += (size_t)got;
      yield();
    }
    json[total] = '\0';

    // --- Parse "current" section ---
    // Locate "current" block to avoid matching duplicate keys in "hourly"
    const char* currentPos = strstr_P(json, PSTR("\"current\""));
    if (currentPos) {
      float val = 0.0f;

      if (weatherJsonGetFloat(currentPos, PSTR("temperature_2m"),      &val)) state.sat.weather.fTemperature   = val;
      if (weatherJsonGetFloat(currentPos, PSTR("apparent_temperature"), &val)) state.sat.weather.fApparentTemp   = val;
      if (weatherJsonGetFloat(currentPos, PSTR("relative_humidity_2m"), &val)) state.sat.weather.fHumidity       = val;
      if (weatherJsonGetFloat(currentPos, PSTR("wind_speed_10m"),       &val)) state.sat.weather.fWindSpeed      = val;
      if (weatherJsonGetFloat(currentPos, PSTR("wind_direction_10m"),   &val)) state.sat.weather.fWindDirection  = val;
      if (weatherJsonGetFloat(currentPos, PSTR("wind_gusts_10m"),       &val)) state.sat.weather.fWindGusts      = val;
      if (weatherJsonGetFloat(currentPos, PSTR("cloud_cover"),          &val)) state.sat.weather.fCloudCover     = val;
      if (weatherJsonGetFloat(currentPos, PSTR("pressure_msl"),         &val)) state.sat.weather.fPressureMsl    = val;
      if (weatherJsonGetFloat(currentPos, PSTR("precipitation"),        &val)) state.sat.weather.fPrecipitation  = val;
      if (weatherJsonGetFloat(currentPos, PSTR("rain"),                 &val)) state.sat.weather.fRain           = val;
      if (weatherJsonGetFloat(currentPos, PSTR("snowfall"),             &val)) state.sat.weather.fSnowfall       = val;

      // WMO weather codes are integers by specification; truncating cast is correct.
      if (weatherJsonGetFloat(currentPos, PSTR("weather_code"), &val)) state.sat.weather.iWeatherCode = (uint16_t)val;

      // is_day: API returns exactly 0 or 1; use > 0.0f to treat 1 as true.
      if (weatherJsonGetFloat(currentPos, PSTR("is_day"), &val)) state.sat.weather.bIsDay = (val > 0.0f);
    }

    // --- Parse hourly forecast arrays ---
    uint8_t cnt = 0;
    weatherJsonGetArray(json, PSTR("temperature_2m"),
                        _weather_forecastTemp, WEATHER_FORECAST_HOURS, &cnt);
    _weather_forecastCount = cnt;

    weatherJsonGetArray(json, PSTR("dew_point_2m"),
                        _weather_forecastDewPt, WEATHER_FORECAST_HOURS, &cnt);

    weatherJsonGetArrayU8(json, PSTR("cloud_cover"),
                          _weather_forecastCloud, WEATHER_FORECAST_HOURS, &cnt);

    weatherJsonGetArrayU8(json, PSTR("precipitation_probability"),
                          _weather_forecastPrecipProb, WEATHER_FORECAST_HOURS, &cnt);

    state.sat.weather.bValid = true;
    state.sat.weather.iLastUpdateMs = millis();

    DebugTf(PSTR("Weather: %.1fC (feels %.1fC), %d%% RH, %.1f km/h wind, %d%% cloud, WMO %d, %d h forecast, %u bytes\r\n"),
      state.sat.weather.fTemperature,
      state.sat.weather.fApparentTemp,
      (int)state.sat.weather.fHumidity,
      state.sat.weather.fWindSpeed,
      (int)state.sat.weather.fCloudCover,
      (int)state.sat.weather.iWeatherCode,
      _weather_forecastCount,
      (unsigned)total);

    free(json);
  } else {
    DebugTf(PSTR("Weather: HTTP %d\r\n"), httpCode);
    state.sat.weather.iFetchErrors++;
  }

  http.end();
  feedWatchDog();
}

//=====================================================================
//=== Loop — called from main loop (timer-guarded) ===
//=====================================================================
void weatherLoop()
{
  if (!settings.sat.bWeatherEnable) return;
  if (!DUE(timerWeatherPoll)) return;
  weatherFetch();
}

//=====================================================================
//=== REST API: GET /api/v2/sat/weather ===
//=====================================================================
void weatherSendStatusJSON()
{
  sendStartJsonMap(F(""));
  sendJsonMapEntry(F("enabled"),      settings.sat.bWeatherEnable);
  sendJsonMapEntry(F("valid"),        state.sat.weather.bValid);

  {
    char tmpBuf[10];
    dtostrf(state.sat.weather.fTemperature,   1, 1, tmpBuf);
    sendJsonMapEntry(F("temperature"),         tmpBuf);
    dtostrf(state.sat.weather.fApparentTemp,  1, 1, tmpBuf);
    sendJsonMapEntry(F("apparent_temp"),       tmpBuf);
    dtostrf(state.sat.weather.fHumidity,      1, 0, tmpBuf);
    sendJsonMapEntry(F("humidity"),            tmpBuf);
    dtostrf(state.sat.weather.fWindSpeed,     1, 1, tmpBuf);
    sendJsonMapEntry(F("wind_speed"),          tmpBuf);
    dtostrf(state.sat.weather.fWindDirection, 1, 0, tmpBuf);
    sendJsonMapEntry(F("wind_direction"),      tmpBuf);
    dtostrf(state.sat.weather.fWindGusts,     1, 1, tmpBuf);
    sendJsonMapEntry(F("wind_gusts"),          tmpBuf);
    dtostrf(state.sat.weather.fCloudCover,    1, 0, tmpBuf);
    sendJsonMapEntry(F("cloud_cover"),         tmpBuf);
    dtostrf(state.sat.weather.fPressureMsl,   1, 1, tmpBuf);
    sendJsonMapEntry(F("pressure_msl"),        tmpBuf);
    dtostrf(state.sat.weather.fPrecipitation, 1, 1, tmpBuf);
    sendJsonMapEntry(F("precipitation"),       tmpBuf);
    dtostrf(state.sat.weather.fRain,          1, 1, tmpBuf);
    sendJsonMapEntry(F("rain"),                tmpBuf);
    dtostrf(state.sat.weather.fSnowfall,      1, 1, tmpBuf);
    sendJsonMapEntry(F("snowfall"),            tmpBuf);
    sendJsonMapEntry(F("weather_code"),        (int32_t)state.sat.weather.iWeatherCode);
    sendJsonMapEntry(F("is_day"),              state.sat.weather.bIsDay);
    dtostrf(settings.sat.fWeatherLat,         1, 4, tmpBuf);
    sendJsonMapEntry(F("latitude"),            tmpBuf);
    dtostrf(settings.sat.fWeatherLon,         1, 4, tmpBuf);
    sendJsonMapEntry(F("longitude"),           tmpBuf);
  }

  sendJsonMapEntry(F("fetch_errors"), (int32_t)state.sat.weather.iFetchErrors);

  // Age in seconds
  if (state.sat.weather.bValid && state.sat.weather.iLastUpdateMs > 0) {
    uint32_t ageSec = (millis() - state.sat.weather.iLastUpdateMs) / 1000;
    sendJsonMapEntry(F("age_seconds"), (int32_t)ageSec);
  } else {
    sendJsonMapEntry(F("age_seconds"), (int32_t)-1);
  }

  // 24-hour temperature forecast array
  {
    char entryBuf[300];
    size_t pos = snprintf_P(entryBuf, sizeof(entryBuf), PSTR("\"forecast_temp\":["));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      if (i > 0 && pos < sizeof(entryBuf) - 9) entryBuf[pos++] = ',';
      char tmpBuf[8];
      dtostrf(_weather_forecastTemp[i], 1, 1, tmpBuf);
      size_t len = strlen(tmpBuf);
      if (pos + len < sizeof(entryBuf) - 2) { memcpy(entryBuf + pos, tmpBuf, len); pos += len; }
    }
    entryBuf[pos++] = ']'; entryBuf[pos] = '\0';
    sendBeforenext(); httpServer.sendContent(entryBuf);
  }

  // 24-hour dew point forecast array
  {
    char entryBuf[300];
    size_t pos = snprintf_P(entryBuf, sizeof(entryBuf), PSTR("\"forecast_dewpt\":["));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      if (i > 0 && pos < sizeof(entryBuf) - 9) entryBuf[pos++] = ',';
      char tmpBuf[8];
      dtostrf(_weather_forecastDewPt[i], 1, 1, tmpBuf);
      size_t len = strlen(tmpBuf);
      if (pos + len < sizeof(entryBuf) - 2) { memcpy(entryBuf + pos, tmpBuf, len); pos += len; }
    }
    entryBuf[pos++] = ']'; entryBuf[pos] = '\0';
    sendBeforenext(); httpServer.sendContent(entryBuf);
  }

  // 24-hour cloud cover forecast (%)
  {
    char entryBuf[200];
    size_t pos = snprintf_P(entryBuf, sizeof(entryBuf), PSTR("\"forecast_cloud\":["));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      if (i > 0 && pos < sizeof(entryBuf) - 5) entryBuf[pos++] = ',';
      char tmpBuf[5];
      snprintf(tmpBuf, sizeof(tmpBuf), "%d", (int)_weather_forecastCloud[i]);
      size_t len = strlen(tmpBuf);
      if (pos + len < sizeof(entryBuf) - 2) { memcpy(entryBuf + pos, tmpBuf, len); pos += len; }
    }
    entryBuf[pos++] = ']'; entryBuf[pos] = '\0';
    sendBeforenext(); httpServer.sendContent(entryBuf);
  }

  // 24-hour precipitation probability (%)
  {
    char entryBuf[200];
    size_t pos = snprintf_P(entryBuf, sizeof(entryBuf), PSTR("\"forecast_precip_prob\":["));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      if (i > 0 && pos < sizeof(entryBuf) - 5) entryBuf[pos++] = ',';
      char tmpBuf[5];
      snprintf(tmpBuf, sizeof(tmpBuf), "%d", (int)_weather_forecastPrecipProb[i]);
      size_t len = strlen(tmpBuf);
      if (pos + len < sizeof(entryBuf) - 2) { memcpy(entryBuf + pos, tmpBuf, len); pos += len; }
    }
    entryBuf[pos++] = ']'; entryBuf[pos] = '\0';
    sendBeforenext(); httpServer.sendContent(entryBuf);
  }

  sendEndJsonMap(F(""));
}

//=====================================================================
//=== MQTT Publish ===
//=====================================================================
void weatherPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bWeatherEnable || !state.sat.weather.bValid) return;

  char valBuf[16];

  dtostrf(state.sat.weather.fTemperature,  1, 1, valBuf);
  sendMQTTData(F("sat/weather/temperature"),  valBuf, false);

  dtostrf(state.sat.weather.fApparentTemp, 1, 1, valBuf);
  sendMQTTData(F("sat/weather/apparent_temp"), valBuf, false);

  dtostrf(state.sat.weather.fHumidity,     1, 0, valBuf);
  sendMQTTData(F("sat/weather/humidity"),  valBuf, false);

  dtostrf(state.sat.weather.fWindSpeed,    1, 1, valBuf);
  sendMQTTData(F("sat/weather/wind_speed"), valBuf, false);

  dtostrf(state.sat.weather.fWindDirection, 1, 0, valBuf);
  sendMQTTData(F("sat/weather/wind_direction"), valBuf, false);

  dtostrf(state.sat.weather.fWindGusts,    1, 1, valBuf);
  sendMQTTData(F("sat/weather/wind_gusts"), valBuf, false);

  dtostrf(state.sat.weather.fCloudCover,   1, 0, valBuf);
  sendMQTTData(F("sat/weather/cloud_cover"), valBuf, false);

  dtostrf(state.sat.weather.fPressureMsl,  1, 1, valBuf);
  sendMQTTData(F("sat/weather/pressure_msl"), valBuf, false);

  dtostrf(state.sat.weather.fPrecipitation, 1, 1, valBuf);
  sendMQTTData(F("sat/weather/precipitation"), valBuf, false);

  dtostrf(state.sat.weather.fRain,         1, 1, valBuf);
  sendMQTTData(F("sat/weather/rain"),      valBuf, false);

  dtostrf(state.sat.weather.fSnowfall,     1, 1, valBuf);
  sendMQTTData(F("sat/weather/snowfall"),  valBuf, false);

  snprintf(valBuf, sizeof(valBuf), "%d", (int)state.sat.weather.iWeatherCode);
  sendMQTTData(F("sat/weather/weather_code"), valBuf, false);

  snprintf(valBuf, sizeof(valBuf), "%d", state.sat.weather.bIsDay ? 1 : 0);
  sendMQTTData(F("sat/weather/is_day"),    valBuf, false);
}

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
