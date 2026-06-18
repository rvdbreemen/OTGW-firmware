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

// OpenWeatherMap hard floor (must match plan/req): 15 minutes.
static const uint16_t WEATHER_OWM_MIN_SEC      = 900;

// Hourly forecast arrays — ESP32 only.
// ESP8266 does not request hourly data; omitting saves ~240 bytes of BSS.
#if HAS_WEATHER_FORECAST
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
#endif  // HAS_WEATHER_FORECAST

// Timer — fires every 15 min.  SKIP_MISSED_TICKS: if the loop was busy and a
// tick was missed, fire once then restart the full 15-min window.  Never
// fire multiple times in a row to "catch up" — that would burn through the
// OWM free-tier quota (1 000 calls/day) on a backlogged loop.
DECLARE_TIMER_SEC(timerWeatherPoll, WEATHER_POLL_DEFAULT_SEC, SKIP_MISSED_TICKS);

//=====================================================================
//=== API URL format strings (PROGMEM) ===
//=====================================================================

// TASK-511: Open-Meteo URL format differs per platform (ESP8266 minimal 5
// fields, ESP32 full data set), so the Open-Meteo constant is per-platform.
// OpenWeatherMap is platform-agnostic (same URL, same response shape) so the
// OWM constant lives outside the platform split. PR #559 originally defined
// kWeatherOwmUrlFmt only in the ESP8266 branch, breaking the ESP32 build.
#if !HAS_WEATHER_FORECAST
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
#endif  // !HAS_WEATHER_FORECAST

// OpenWeatherMap Current Weather API (HTTP, firmware-side). Key required.
// Same URL on both platforms; ~400-byte response.
static const char kWeatherOwmUrlFmt[] PROGMEM =
  "http://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&appid=%s&units=metric";

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
  int  currentDepth = 0;
#if HAS_WEATHER_FORECAST
  bool inHourly    = false;
  int  hourlyDepth = 0;
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
      #if HAS_WEATHER_FORECAST
      if (  depth < hourlyDepth)  inHourly  = false;
      #endif
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
      if (strcmp_P(keyBuf, PSTR("current")) == 0) { inCurrent = true; currentDepth = depth; }
      #if HAS_WEATHER_FORECAST
      else if (strcmp_P(keyBuf, PSTR("hourly"))  == 0) { inHourly  = true; hourlyDepth  = depth; }
      #endif

    } else if (c == '[') {
#if HAS_WEATHER_FORECAST
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
#endif  // HAS_WEATHER_FORECAST

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
#if HAS_WEATHER_FORECAST
        // Extended fields — ESP32 only (not requested in ESP8266 URL)
        else if (strcmp_P(keyBuf, PSTR("wind_direction_10m"))   == 0) state.sat.weather.fWindDirection = val;
        else if (strcmp_P(keyBuf, PSTR("wind_gusts_10m"))       == 0) state.sat.weather.fWindGusts     = val;
        else if (strcmp_P(keyBuf, PSTR("pressure_msl"))         == 0) state.sat.weather.fPressureMsl   = val;
        else if (strcmp_P(keyBuf, PSTR("precipitation"))        == 0) state.sat.weather.fPrecipitation = val;
        else if (strcmp_P(keyBuf, PSTR("rain"))                 == 0) state.sat.weather.fRain          = val;
        else if (strcmp_P(keyBuf, PSTR("snowfall"))             == 0) state.sat.weather.fSnowfall      = val;
        else if (strcmp_P(keyBuf, PSTR("weather_code"))         == 0) state.sat.weather.iWeatherCode   = (uint16_t)val;
        else if (strcmp_P(keyBuf, PSTR("is_day"))               == 0) state.sat.weather.bIsDay         = (val > 0.0f);
#endif  // HAS_WEATHER_FORECAST
      }
    }
    // else: true/false/null at top level — not needed for weather fields

    yield();
  }
}

//=====================================================================
//=== HTTP Fetch ===
//=====================================================================
static bool weatherShouldUseOwm()
{
  // Use OWM only when a key is configured.
  return (settings.sat.sWeatherApiKey[0] != '\0');
}

// Forward declaration so the public debug wrapper below can call the
// file-static implementation that is defined further down.
static void weatherFetchOpenMeteo();

// Structured dump of the cached weather state to the telnet debug
// console. Called after the manual fetch triggered by the 'w' shortcut.
static void weatherDumpStateToTelnet()
{
  Debugln(F("--- Weather state (Open-Meteo) ---"));
  Debugf(PSTR("  Valid         : %s\r\n"), state.sat.weather.bValid ? "YES" : "no");
  if (state.sat.weather.iLastUpdateMs != 0) {
    uint32_t ageMs = (uint32_t)(millis() - state.sat.weather.iLastUpdateMs);
    Debugf(PSTR("  Last update   : %lu ms ago\r\n"), (unsigned long)ageMs);
  } else {
    Debugln(F("  Last update   : never"));
  }
  Debugf(PSTR("  Fetch errors  : %ld\r\n"), (long)state.sat.weather.iFetchErrors);
  Debugf(PSTR("  Coords        : %.4f, %.4f\r\n"),
         settings.sat.fWeatherLat, settings.sat.fWeatherLon);
  if (!state.sat.weather.bValid) {
    Debugln(F("  (no successful fetch yet — check enable flag, coords, network)"));
    Debugln(F("---"));
    return;
  }
  Debugf(PSTR("  Day/Night     : %s\r\n"), state.sat.weather.bIsDay ? "day" : "night");
  Debugf(PSTR("  Weather code  : %u\r\n"), state.sat.weather.iWeatherCode);
  Debugf(PSTR("  Temperature   : %.2f C  (apparent: %.2f C)\r\n"),
         state.sat.weather.fTemperature, state.sat.weather.fApparentTemp);
  Debugf(PSTR("  Humidity      : %d %%\r\n"), (int)state.sat.weather.fHumidity);
  Debugf(PSTR("  Cloud cover   : %d %%\r\n"), (int)state.sat.weather.fCloudCover);
  Debugf(PSTR("  Pressure      : %.1f hPa\r\n"), state.sat.weather.fPressureMsl);
  Debugf(PSTR("  Wind speed    : %.1f km/h  (gusts: %.1f, dir: %.0f deg)\r\n"),
         state.sat.weather.fWindSpeed,
         state.sat.weather.fWindGusts,
         state.sat.weather.fWindDirection);
  Debugf(PSTR("  Precipitation : %.1f mm  (rain: %.1f, snow: %.1f)\r\n"),
         state.sat.weather.fPrecipitation,
         state.sat.weather.fRain,
         state.sat.weather.fSnowfall);
  Debugln(F("---"));
}

// Public entry point for the telnet debug 'w' shortcut. Bypasses the
// 3600-second scheduler interval but still honours the bWeatherEnable
// flag and coordinate-configured guard inside weatherFetchOpenMeteo().
// Dumps the resulting cached state in a structured block.
void weatherFetchOpenMeteoNow()
{
  DebugTln(F("Weather: manual Open-Meteo fetch triggered via debug telnet"));
  weatherFetchOpenMeteo();
  weatherDumpStateToTelnet();
}

static void weatherFetchOpenMeteo()
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

#if !HAS_WEATHER_FORECAST
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
#endif  // !HAS_WEATHER_FORECAST

  } else {
    DebugTf(PSTR("Weather: HTTP %d\r\n"), httpCode);
    state.sat.weather.iFetchErrors++;
  }

  http.end();
  feedWatchDog();
}

// Extract main.temp from OWM current-weather response.
// Minimal streaming parse: find "main":{..."temp":<num>}.
static bool weatherParseOwmTemp(WiFiClient* stream, HTTPClient* http, float* outTemp)
{
  static const char kNeedleMain[] PROGMEM = "\"main\":";
  static const char kNeedleTemp[] PROGMEM = "\"temp\":";

  const size_t mainLen = sizeof(kNeedleMain) - 1;
  const size_t tempLen = sizeof(kNeedleTemp) - 1;

  uint8_t mainPos = 0;
  bool inMain = false;
  uint8_t tempPos = 0;

  int ci;
  while ((ci = wstreamGet(stream, http)) >= 0) {
    char c = (char)ci;

    if (!inMain) {
      if (c == (char)pgm_read_byte(&kNeedleMain[mainPos])) {
        if (++mainPos >= mainLen) { inMain = true; }
      } else {
        mainPos = (c == (char)pgm_read_byte(&kNeedleMain[0])) ? 1 : 0;
      }
      continue;
    }

    if (c == (char)pgm_read_byte(&kNeedleTemp[tempPos])) {
      if (++tempPos >= tempLen) {
        // Skip whitespace after ':'
        while ((ci = wstreamGet(stream, http)) >= 0) {
          c = (char)ci;
          if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) break;
        }
        if (ci < 0) return false;
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+')) return false;
        *outTemp = wstreamReadNumber(stream, http, c);
        return true;
      }
    } else {
      tempPos = (c == (char)pgm_read_byte(&kNeedleTemp[0])) ? 1 : 0;
    }
  }

  return false;
}

static void weatherFetchOwm()
{
  if (!settings.sat.bWeatherEnable) return;
  if (!weatherShouldUseOwm()) return;
  if (settings.sat.fWeatherLat == 0.0f && settings.sat.fWeatherLon == 0.0f) {
    DebugTln(F("Weather(OWM): skipping fetch, no coordinates configured"));
    return;
  }

  char url[512];
  snprintf_P(url, sizeof(url), kWeatherOwmUrlFmt,
             settings.sat.fWeatherLat,
             settings.sat.fWeatherLon,
             settings.sat.sWeatherApiKey);

  DebugTf(PSTR("Weather(OWM): fetching %s\r\n"), url);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);

  if (!http.begin(client, url)) {
    DebugTln(F("Weather(OWM): http.begin() failed"));
    state.sat.weather.iFetchErrors++;
    return;
  }

  yield();
  int httpCode = http.GET();
  yield();

  if (httpCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    float temp;
    if (weatherParseOwmTemp(stream, &http, &temp)) {
      state.sat.weather.fTemperature  = temp;
      state.sat.weather.bValid        = true;
      state.sat.weather.iLastUpdateMs = millis();
      DebugTf(PSTR("Weather(OWM): %.1fC\r\n"), state.sat.weather.fTemperature);
    } else {
      DebugTln(F("Weather(OWM): parse failed"));
      state.sat.weather.iFetchErrors++;
    }
  } else {
    DebugTf(PSTR("Weather(OWM): HTTP %d\r\n"), httpCode);
    state.sat.weather.iFetchErrors++;
  }

  http.end();
  feedWatchDog();
}

void weatherFetch()
{
  // Prefer OWM when key is present; otherwise default to Open-Meteo.
  if (weatherShouldUseOwm()) weatherFetchOwm();
  else                       weatherFetchOpenMeteo();
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
  // If the OT bus already provides a valid outside temperature, avoid
  // burning weather API calls (SAT only needs weather as a fallback).
  // TASK-511: canonical struct is OTcurrentSystemState (see OTGW-Core.ino:3490
  // where OT message ID 27 populates this field). PR #559 originally referenced
  // a non-existent state.sat.Toutside and would not compile.
  if (OTcurrentSystemState.Toutside != 0.0f) return;
  if (!DUE(timerWeatherPoll)) return;

  // If we're using OWM, enforce a hard 15-min guard regardless of
  // user-configured interval.
  static uint32_t lastOwmFetchMs = 0;
  if (weatherShouldUseOwm()) {
    uint32_t now = millis();
    if (lastOwmFetchMs != 0 && (uint32_t)(now - lastOwmFetchMs) < (WEATHER_OWM_MIN_SEC * 1000UL)) return;
    lastOwmFetchMs = now;
  }
  weatherFetch();
}

//=====================================================================
//=== REST API: GET /api/v2/sat/weather ===
//=====================================================================
void weatherSendStatusJSON()
{
  // ADR-141 revert / TASK-885: streaming JsonEmit replaces the JsonDocument
  // path. Each float field emits as a real JSON number via je.field(k, float)
  // (3-decimal default; json_golden tolerates decimal width). Bools emit as
  // real true/false, ints as bare integers. The forecast arrays stay bare
  // numbers. The original wrapper key was F("") => a flat ROOT object, so build
  // directly on the root object (no nested wrapper).
  AsyncResponseStream* strm = restBeginStream("application/json");
  if (strm) {
    JsonEmit je(*strm);
    je.beginObject();                 // root {

    je.field(F("enabled"), settings.sat.bWeatherEnable);
    je.field(F("valid"),   state.sat.weather.bValid);

    // Core SAT fields — both platforms
    je.field(F("temperature"),   state.sat.weather.fTemperature);
    je.field(F("apparent_temp"), state.sat.weather.fApparentTemp);
    je.field(F("humidity"),      state.sat.weather.fHumidity);
    je.field(F("wind_speed"),    state.sat.weather.fWindSpeed);
    je.field(F("cloud_cover"),   state.sat.weather.fCloudCover);
#if HAS_WEATHER_FORECAST
    // Extended fields — ESP32 only
    je.field(F("wind_direction"), state.sat.weather.fWindDirection);
    je.field(F("wind_gusts"),     state.sat.weather.fWindGusts);
    je.field(F("pressure_msl"),   state.sat.weather.fPressureMsl);
    je.field(F("precipitation"),  state.sat.weather.fPrecipitation);
    je.field(F("rain"),           state.sat.weather.fRain);
    je.field(F("snowfall"),       state.sat.weather.fSnowfall);
    je.field(F("weather_code"),   (int32_t)state.sat.weather.iWeatherCode);
    je.field(F("is_day"),         state.sat.weather.bIsDay);
#endif  // HAS_WEATHER_FORECAST
    je.field(F("latitude"),  settings.sat.fWeatherLat);
    je.field(F("longitude"), settings.sat.fWeatherLon);

    je.field(F("fetch_errors"), (int32_t)state.sat.weather.iFetchErrors);

    // Age in seconds
    if (state.sat.weather.bValid && state.sat.weather.iLastUpdateMs > 0) {
      uint32_t ageSec = (millis() - state.sat.weather.iLastUpdateMs) / 1000;
      je.field(F("age_seconds"), (int32_t)ageSec);
    } else {
      je.field(F("age_seconds"), (int32_t)-1);
    }

#if HAS_WEATHER_FORECAST
    // 24-hour forecast arrays — ESP32 only. Bare numbers (cloud / precip_prob
    // are uint8_t, emitted as integers). Empty forecast (count==0) => [].
    je.beginArray(F("forecast_temp"));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      je.value(_weather_forecastTemp[i]);
    }
    je.endArray();

    je.beginArray(F("forecast_dewpt"));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      je.value(_weather_forecastDewPt[i]);
    }
    je.endArray();

    je.beginArray(F("forecast_cloud"));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      je.value(_weather_forecastCloud[i]);
    }
    je.endArray();

    je.beginArray(F("forecast_precip_prob"));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      je.value(_weather_forecastPrecipProb[i]);
    }
    je.endArray();
#endif  // HAS_WEATHER_FORECAST

    je.endObject();                   // close root
  }
  restFinalize();
}

//=====================================================================
//=== MQTT Publish ===
//=====================================================================
void weatherPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bWeatherEnable || !state.sat.weather.bValid) return;

  // ADR-111: on-change + jittered heartbeat. The outer rate-limit
  // (settings.sat.iWeatherInterval, default 900s) still applies; these
  // helpers further suppress publishes when the polled values haven't
  // changed since the last poll.
  static SATShadowF s_w_temp, s_w_apparent, s_w_humidity, s_w_wind_speed, s_w_cloud_cover;
#if HAS_WEATHER_FORECAST
  static SATShadowF s_w_wind_dir, s_w_wind_gusts, s_w_pressure_msl;
  static SATShadowF s_w_precip, s_w_rain, s_w_snowfall;
  static SATShadowI s_w_weather_code;
  static SATShadowB s_w_is_day;
#endif  // HAS_WEATHER_FORECAST

  // Core fields — both platforms
  publishIfChangedF(F("sat/weather/temperature"),   state.sat.weather.fTemperature, s_w_temp,        SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/apparent_temp"), state.sat.weather.fApparentTemp,s_w_apparent,    SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/humidity"),      state.sat.weather.fHumidity,    s_w_humidity,    SAT_EPS_DURATION,    0, false);
  publishIfChangedF(F("sat/weather/wind_speed"),    state.sat.weather.fWindSpeed,   s_w_wind_speed,  SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/cloud_cover"),   state.sat.weather.fCloudCover,  s_w_cloud_cover, SAT_EPS_DURATION,    0, false);

#if HAS_WEATHER_FORECAST
  publishIfChangedF(F("sat/weather/wind_direction"), state.sat.weather.fWindDirection, s_w_wind_dir,     SAT_EPS_DURATION,    0, false);
  publishIfChangedF(F("sat/weather/wind_gusts"),     state.sat.weather.fWindGusts,     s_w_wind_gusts,   SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/pressure_msl"),   state.sat.weather.fPressureMsl,   s_w_pressure_msl, SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/precipitation"),  state.sat.weather.fPrecipitation, s_w_precip,       SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/rain"),           state.sat.weather.fRain,          s_w_rain,         SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/weather/snowfall"),       state.sat.weather.fSnowfall,      s_w_snowfall,     SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedI(F("sat/weather/weather_code"),   (int32_t)state.sat.weather.iWeatherCode, s_w_weather_code, false);

  // is_day historically uses "1"/"0" payload (not "true"/"false").
  publishIfChangedBStr(F("sat/weather/is_day"), state.sat.weather.bIsDay, s_w_is_day, "1", "0", false);
#endif  // HAS_WEATHER_FORECAST
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
