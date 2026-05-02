/*
***************************************************************************
**  Module   : SATweather.ino
**  Description: Weather data integration for SAT
**
**  Part of SAT (Smart Autotune Thermostat) firmware port.
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Weather data via Open-Meteo API (free, no API key required).
**  HTTP only — firmware never does HTTPS (ADR constraint).
**
**  ESP8266: minimal request — 5 current fields only (temperature, feels-like,
**           humidity, wind speed, cloud cover).  Response ~450 bytes.
**           Stream-parsed with no heap allocation (no malloc).
**
**  ESP32:   full request — all 15 current fields + 24-hour hourly forecast
**           arrays (temperature, dew point, cloud cover, precipitation
**           probability).  Response ~3-4 KB.  Also stream-parsed.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Constants ---
static const uint16_t WEATHER_POLL_DEFAULT_SEC = 900;  // 15 min default
static const uint16_t WEATHER_POLL_MIN_SEC     = 300;  // 5 min minimum

// Hourly forecast arrays — ESP32 only.
// ESP8266 does not request hourly data; omitting saves ~240 bytes of BSS.
#ifndef ESP8266
static const uint8_t  WEATHER_FORECAST_HOURS   = 24;
// temperature_2m: primary thermal load forecast (float — °C)
static float    _weather_forecastTemp[WEATHER_FORECAST_HOURS];
// dew_point_2m: comfort / condensation forecast (float — °C)
static float    _weather_forecastDewPt[WEATHER_FORECAST_HOURS];
// cloud_cover: solar-gain forecast (uint8_t — % 0-100, saves 72 bytes vs float)
static uint8_t  _weather_forecastCloud[WEATHER_FORECAST_HOURS];
// precipitation_probability: scheduling guard (uint8_t — % 0-100)
static uint8_t  _weather_forecastPrecipProb[WEATHER_FORECAST_HOURS];
static uint8_t  _weather_forecastCount = 0;
#endif  // ifndef ESP8266

// Timer — fires every 15 min.  SKIP_MISSED_TICKS: if the loop was busy and a
// tick was missed, fire once then restart the full 15-min window.  Never
// fire multiple times in a row to "catch up" — that would burn through the
// OWM free-tier quota (1 000 calls/day) on a backlogged loop.
DECLARE_TIMER_SEC(timerWeatherPoll, WEATHER_POLL_DEFAULT_SEC, SKIP_MISSED_TICKS);

//=====================================================================
//=== API URL format strings (PROGMEM) ===
//=====================================================================

#ifdef ESP8266
// Minimal current-conditions request: only the 5 fields SAT actually uses.
// Produces ~450-byte response — stream-parsed with no heap allocation.
// HTTP only (no TLS) — firmware never does HTTPS (ADR constraint).
static const char kWeatherUrlFmt[] PROGMEM =
  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
  "&current=temperature_2m,relative_humidity_2m,apparent_temperature"
  ",wind_speed_10m,cloud_cover";
#else
// Full data set for ESP32: all 15 current fields + 24-hour hourly forecasts.
static const char kWeatherUrlFmt[] PROGMEM =
  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
  "&forecast_days=1"
  "&current=temperature_2m,relative_humidity_2m,apparent_temperature"
  ",is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover"
  ",pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m"
  "&hourly=temperature_2m,relative_humidity_2m,dew_point_2m,apparent_temperature"
  ",precipitation_probability,cloud_cover,cloud_cover_low,cloud_cover_mid";
#endif  // ifdef ESP8266

//=====================================================================
//=== Streaming JSON parser — no heap allocation ===
//=====================================================================
// Single-pass character-by-character parser for the Open-Meteo flat JSON.
// Uses WiFiClient::peek() for one-byte lookahead so number terminators
// (commas, brackets) stay unconsumed in the stream for the outer loop.
//
// Peak stack use: ~60 bytes (keyBuf + numBuf).  No malloc at all.
// The WiFiClient TCP receive buffer (~1 460 bytes) holds unread bytes
// while each character is processed one at a time.

// Read one byte from stream; block up to 5 s.  Returns -1 on timeout.
static int wstreamGet(WiFiClient* s, HTTPClient* h)
{
  uint32_t start = millis();
  while (!s->available()) {
    if (millis() - start > 5000UL) return -1;  // millis()-wrap-safe
    if (!h->connected())           return -1;
    yield();
  }
  return s->read();
}

// Peek at next byte without consuming; block up to 5 s.  Returns -1 on timeout.
static int wstreamPeek(WiFiClient* s, HTTPClient* h)
{
  uint32_t start = millis();
  while (!s->available()) {
    if (millis() - start > 5000UL) return -1;  // millis()-wrap-safe
    if (!h->connected())           return -1;
    yield();
  }
  return s->peek();
}

// Consume stream up to and including the closing '"'.
// Opening '"' must already have been consumed.  Handles \" escapes.
static void wstreamSkipString(WiFiClient* s, HTTPClient* h)
{
  int c;
  while ((c = wstreamGet(s, h)) >= 0) {
    if (c == '"') return;
    if (c == '\\') wstreamGet(s, h);  // skip escaped char
  }
}

// Consume a complete JSON array (opening '[' already consumed).
static void wstreamSkipArray(WiFiClient* s, HTTPClient* h)
{
  int depth = 1, c;
  while ((c = wstreamGet(s, h)) >= 0 && depth > 0) {
    if      (c == '[') depth++;
    else if (c == ']') depth--;
    else if (c == '"') wstreamSkipString(s, h);
    if (depth > 0) yield();
  }
}

// Read a JSON number from stream.  'firstChar' is the first digit or '-'
// already consumed.  Uses peek() so the terminating delimiter (',', ']',
// '}', etc.) remains unconsumed for the outer loop to process.
static float wstreamReadNumber(WiFiClient* s, HTTPClient* h, char firstChar)
{
  char numBuf[20];
  int  numLen = 0;
  numBuf[numLen++] = firstChar;
  while (numLen < (int)sizeof(numBuf) - 1) {   // leave room for '\0' at [sizeof-1]
    int nc = wstreamPeek(s, h);
    if (nc < 0) break;
    char c = (char)nc;
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) break;
    s->read();           // confirmed number char — consume it
    numBuf[numLen++] = c;
  }
  numBuf[numLen] = '\0';
  return (float)strtod(numBuf, nullptr);
}

// Single-pass streaming parse of the Open-Meteo JSON response.
// Populates state.sat.weather from the "current":{} section (all platforms).
// On ESP32 also fills _weather_forecastXxx[] from the "hourly":{} section.
static void weatherParseStream(WiFiClient* stream, HTTPClient* http)
{
  char keyBuf[48];
  int  keyLen;
  int  depth        = 0;
  bool inCurrent    = false;
  bool inHourly     = false;
  int  currentDepth = 0;
  int  hourlyDepth  = 0;

#ifndef ESP8266
  float*   arrFloat = nullptr;
  uint8_t* arrU8    = nullptr;
  uint8_t  arrMax   = 0;
  uint8_t  arrPos   = 0;
#endif

  int ci;
  while ((ci = wstreamGet(stream, http)) >= 0) {
    char c = (char)ci;

    if (c == '{') { depth++; continue; }
    if (c == '}') {
      if (--depth < currentDepth) inCurrent = false;
      if (  depth < hourlyDepth)  inHourly  = false;
      continue;
    }
    if (c != '"') continue;  // commas, whitespace — skip

    // ── Collect key string ──
    keyLen = 0;
    while ((ci = wstreamGet(stream, http)) >= 0 && (char)ci != '"') {
      if (keyLen < (int)sizeof(keyBuf) - 1) keyBuf[keyLen++] = (char)ci;
    }
    keyBuf[keyLen] = '\0';

    // ── Skip to ':' ──
    while ((ci = wstreamGet(stream, http)) >= 0 && (char)ci != ':');
    if (ci < 0) break;

    // ── First non-whitespace char of value ──
    while ((ci = wstreamGet(stream, http)) >= 0 &&
           ((char)ci == ' ' || (char)ci == '\t' || (char)ci == '\n' || (char)ci == '\r'));
    if (ci < 0) break;
    c = (char)ci;

    // ── Dispatch on value type ──
    if (c == '"') {
      wstreamSkipString(stream, http);

    } else if (c == '{') {
      depth++;
      if      (strcmp_P(keyBuf, PSTR("current")) == 0) { inCurrent = true; currentDepth = depth; }
      else if (strcmp_P(keyBuf, PSTR("hourly"))  == 0) { inHourly  = true; hourlyDepth  = depth; }

    } else if (c == '[') {
#ifndef ESP8266
      if (inHourly) {
        arrFloat = nullptr; arrU8 = nullptr; arrMax = 0; arrPos = 0;
        if      (strcmp_P(keyBuf, PSTR("temperature_2m"))            == 0) { arrFloat = _weather_forecastTemp;        arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("dew_point_2m"))              == 0) { arrFloat = _weather_forecastDewPt;       arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("cloud_cover"))               == 0) { arrU8    = _weather_forecastCloud;       arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("precipitation_probability")) == 0) { arrU8    = _weather_forecastPrecipProb;  arrMax = WEATHER_FORECAST_HOURS; }

        if (arrFloat || arrU8) {
          while ((ci = wstreamGet(stream, http)) >= 0) {
            c = (char)ci;
            if (c == ']') break;
            if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
            if (c == 'n') {                                         // null
              for (int i = 0; i < 3; i++) wstreamGet(stream, http);  // consume 'ull' after 'n'
              if (arrPos < arrMax) { if (arrFloat) arrFloat[arrPos++] = 0.0f; else arrU8[arrPos++] = 0; }
              continue;
            }
            if ((c >= '0' && c <= '9') || c == '-') {
              float val = wstreamReadNumber(stream, http, c);
              if (arrPos < arrMax) {
                if (arrFloat) arrFloat[arrPos++] = val;
                else          arrU8[arrPos++]    = (val < 0 ? 0 : val > 255 ? 255 : (uint8_t)(int)val);
              }
            }
          }
          if (arrFloat == _weather_forecastTemp) _weather_forecastCount = arrPos;
        } else {
          wstreamSkipArray(stream, http);
        }
      } else {
        wstreamSkipArray(stream, http);
      }
#else
      wstreamSkipArray(stream, http);
#endif  // ifndef ESP8266

    } else if ((c >= '0' && c <= '9') || c == '-') {
      // ── Scalar number ──
      float val = wstreamReadNumber(stream, http, c);
      if (inCurrent) {
        // Core SAT fields — both platforms
        if      (strcmp_P(keyBuf, PSTR("temperature_2m"))       == 0) state.sat.weather.fTemperature  = val;
        else if (strcmp_P(keyBuf, PSTR("apparent_temperature")) == 0) state.sat.weather.fApparentTemp  = val;
        else if (strcmp_P(keyBuf, PSTR("relative_humidity_2m")) == 0) state.sat.weather.fHumidity      = val;
        else if (strcmp_P(keyBuf, PSTR("wind_speed_10m"))       == 0) state.sat.weather.fWindSpeed     = val;
        else if (strcmp_P(keyBuf, PSTR("cloud_cover"))          == 0) state.sat.weather.fCloudCover    = val;
#ifndef ESP8266
        // Extended fields — ESP32 only (not requested in ESP8266 URL)
        else if (strcmp_P(keyBuf, PSTR("wind_direction_10m"))   == 0) state.sat.weather.fWindDirection = val;
        else if (strcmp_P(keyBuf, PSTR("wind_gusts_10m"))       == 0) state.sat.weather.fWindGusts     = val;
        else if (strcmp_P(keyBuf, PSTR("pressure_msl"))         == 0) state.sat.weather.fPressureMsl   = val;
        else if (strcmp_P(keyBuf, PSTR("precipitation"))        == 0) state.sat.weather.fPrecipitation = val;
        else if (strcmp_P(keyBuf, PSTR("rain"))                 == 0) state.sat.weather.fRain          = val;
        else if (strcmp_P(keyBuf, PSTR("snowfall"))             == 0) state.sat.weather.fSnowfall      = val;
        else if (strcmp_P(keyBuf, PSTR("weather_code"))         == 0) state.sat.weather.iWeatherCode   = (uint16_t)val;
        else if (strcmp_P(keyBuf, PSTR("is_day"))               == 0) state.sat.weather.bIsDay         = (val > 0.0f);
#endif  // ifndef ESP8266
      }
    }
    // else: true/false/null at top level — not needed for weather fields

    yield();
  }
}

//=====================================================================
//=== HTTP Fetch ===
//=====================================================================
void weatherFetch()
{
  if (!settings.sat.bWeatherEnable) return;
  if (settings.sat.fWeatherLat == 0.0f && settings.sat.fWeatherLon == 0.0f) {
    DebugTln(F("Weather: skipping fetch, no coordinates configured"));
    return;
  }

  // WiFiClient and HTTPClient are resolved via the platform include chain:
  //   ESP8266: platform_esp8266.h → <ESP8266HTTPClient.h>
  //   ESP32:   platform_esp32.h   → <HTTPClient.h>
  // Both expose the same http.begin(WiFiClient&, url) API.
  char url[512];
  snprintf_P(url, sizeof(url), kWeatherUrlFmt,
             settings.sat.fWeatherLat, settings.sat.fWeatherLon);

  DebugTf(PSTR("Weather: fetching %s\r\n"), url);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);  // 5s — ESP8266 HW WDT fires at ~8s; stay within margin

  if (!http.begin(client, url)) {
    DebugTln(F("Weather: http.begin() failed"));
    state.sat.weather.iFetchErrors++;
    return;
  }

  yield();
  int httpCode = http.GET();
  yield();

  if (httpCode == 200) {
    // Stream-parse directly — no heap allocation needed.
    WiFiClient* stream = http.getStreamPtr();
    weatherParseStream(stream, &http);

    state.sat.weather.bValid       = true;
    state.sat.weather.iLastUpdateMs = millis();

#ifdef ESP8266
    DebugTf(PSTR("Weather: %.1fC (feels %.1fC), %d%% RH, %.1f km/h wind, %d%% cloud\r\n"),
      state.sat.weather.fTemperature,
      state.sat.weather.fApparentTemp,
      (int)state.sat.weather.fHumidity,
      state.sat.weather.fWindSpeed,
      (int)state.sat.weather.fCloudCover);
#else
    DebugTf(PSTR("Weather: %.1fC (feels %.1fC), %d%% RH, %.1f km/h wind, %d%% cloud, WMO %d, %d h forecast\r\n"),
      state.sat.weather.fTemperature,
      state.sat.weather.fApparentTemp,
      (int)state.sat.weather.fHumidity,
      state.sat.weather.fWindSpeed,
      (int)state.sat.weather.fCloudCover,
      (int)state.sat.weather.iWeatherCode,
      _weather_forecastCount);
#endif  // ifdef ESP8266

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
  // 5-minute startup gate: give the OT bus time to report Toutside before
  // making the first weather API call.
  if (millis() < (WEATHER_POLL_MIN_SEC * 1000UL)) return;
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
    // Core SAT fields — both platforms
    dtostrf(state.sat.weather.fTemperature,   1, 1, tmpBuf);
    sendJsonMapEntry(F("temperature"),         tmpBuf);
    dtostrf(state.sat.weather.fApparentTemp,  1, 1, tmpBuf);
    sendJsonMapEntry(F("apparent_temp"),       tmpBuf);
    dtostrf(state.sat.weather.fHumidity,      1, 0, tmpBuf);
    sendJsonMapEntry(F("humidity"),            tmpBuf);
    dtostrf(state.sat.weather.fWindSpeed,     1, 1, tmpBuf);
    sendJsonMapEntry(F("wind_speed"),          tmpBuf);
    dtostrf(state.sat.weather.fCloudCover,    1, 0, tmpBuf);
    sendJsonMapEntry(F("cloud_cover"),         tmpBuf);
#ifndef ESP8266
    // Extended fields — ESP32 only
    dtostrf(state.sat.weather.fWindDirection, 1, 0, tmpBuf);
    sendJsonMapEntry(F("wind_direction"),      tmpBuf);
    dtostrf(state.sat.weather.fWindGusts,     1, 1, tmpBuf);
    sendJsonMapEntry(F("wind_gusts"),          tmpBuf);
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
#endif  // ifndef ESP8266
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

#ifndef ESP8266
  // 24-hour forecast arrays — ESP32 only
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
#endif  // ifndef ESP8266

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

  // Core SAT fields — both platforms
  dtostrf(state.sat.weather.fTemperature,  1, 1, valBuf);
  sendMQTTData(F("sat/weather/temperature"),  valBuf, false);

  dtostrf(state.sat.weather.fApparentTemp, 1, 1, valBuf);
  sendMQTTData(F("sat/weather/apparent_temp"), valBuf, false);

  dtostrf(state.sat.weather.fHumidity,     1, 0, valBuf);
  sendMQTTData(F("sat/weather/humidity"),  valBuf, false);

  dtostrf(state.sat.weather.fWindSpeed,    1, 1, valBuf);
  sendMQTTData(F("sat/weather/wind_speed"), valBuf, false);

  dtostrf(state.sat.weather.fCloudCover,   1, 0, valBuf);
  sendMQTTData(F("sat/weather/cloud_cover"), valBuf, false);

#ifndef ESP8266
  // Extended fields — ESP32 only
  dtostrf(state.sat.weather.fWindDirection, 1, 0, valBuf);
  sendMQTTData(F("sat/weather/wind_direction"), valBuf, false);

  dtostrf(state.sat.weather.fWindGusts,    1, 1, valBuf);
  sendMQTTData(F("sat/weather/wind_gusts"), valBuf, false);

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
#endif  // ifndef ESP8266
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
