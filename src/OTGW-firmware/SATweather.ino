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
**  TERMS OF USE: GNU GPLv3. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Constants ---
static const uint16_t WEATHER_POLL_DEFAULT_SEC = 900;  // 15 min default
static const uint16_t WEATHER_POLL_MIN_SEC     = 300;  // 5 min minimum
static const uint32_t WEATHER_FIRST_RETRY_MS   = 60000UL;  // first retry until the first fetch succeeds, doubling (TASK-1319)

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

// TASK-1604: the HTTP fetch runs on a worker task (the webhook sender task since TASK-1669). The worker parses into this scratch
// copy only and never touches state.sat.weather or the live forecast arrays; the loop task
// copies it over in weatherConsumeResult() once the worker has published it (see "HTTP Fetch"
// below). The fields mirror state.sat.weather; the loop task seeds the scratch from the live
// values before it starts the worker, so a field missing from a response keeps its old value.
struct WeatherResult {
  float    fTemperature   = 0.0f;
  float    fApparentTemp  = 0.0f;
  float    fHumidity      = 0.0f;
  float    fWindSpeed     = 0.0f;
  float    fWindDirection = 0.0f;
  float    fWindGusts     = 0.0f;
  float    fCloudCover    = 0.0f;
  float    fPressureMsl   = 0.0f;
  float    fPrecipitation = 0.0f;
  float    fRain          = 0.0f;
  float    fSnowfall      = 0.0f;
  uint16_t iWeatherCode   = 0;
  bool     bIsDay         = true;
#if HAS_WEATHER_FORECAST
  float    fcTemp[WEATHER_FORECAST_HOURS];
  float    fcDewPt[WEATHER_FORECAST_HOURS];
  uint8_t  fcCloud[WEATHER_FORECAST_HOURS];
  uint8_t  fcPrecipProb[WEATHER_FORECAST_HOURS];
  uint8_t  fcCount        = 0;
#endif
  bool     bOk            = false;   // worker verdict: HTTP 200 and the stream was parsed
  float    fReqLat        = 0.0f;    // request coordinates, copied by the loop task at start
  float    fReqLon        = 0.0f;
};
static WeatherResult _wxWork;

// Timer — fires every 15 min.  SKIP_MISSED_TICKS: if the loop was busy and a
// tick was missed, fire once then restart the full 15-min window.  Never
// fire multiple times in a row to "catch up" — that would hammer the
// Open-Meteo API on a backlogged loop.
DECLARE_TIMER_SEC(timerWeatherPoll, WEATHER_POLL_DEFAULT_SEC, SKIP_MISSED_TICKS);

//=====================================================================
//=== API URL format strings (PROGMEM) ===
//=====================================================================

// The Open-Meteo URL format differs per platform (ESP8266 minimal 5
// fields, ESP32 full data set), so the constant is per-platform.
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
  while (depth > 0 && (c = wstreamGet(s, h)) >= 0) {   // test depth first: never read a byte past the closing ']'
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
      // The top-level object is closed: everything is parsed. Return now so
      // the parser does not wait for the idle-timeout on a socket that stays open.
      if (depth == 0) return;
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
        if      (strcmp_P(keyBuf, PSTR("temperature_2m"))            == 0) { arrFloat = _wxWork.fcTemp;        arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("dew_point_2m"))              == 0) { arrFloat = _wxWork.fcDewPt;       arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("cloud_cover"))               == 0) { arrU8    = _wxWork.fcCloud;       arrMax = WEATHER_FORECAST_HOURS; }
        else if (strcmp_P(keyBuf, PSTR("precipitation_probability")) == 0) { arrU8    = _wxWork.fcPrecipProb;  arrMax = WEATHER_FORECAST_HOURS; }

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
          if (arrFloat == _wxWork.fcTemp) _wxWork.fcCount = arrPos;
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
        if      (strcmp_P(keyBuf, PSTR("temperature_2m"))       == 0) _wxWork.fTemperature  = val;
        else if (strcmp_P(keyBuf, PSTR("apparent_temperature")) == 0) _wxWork.fApparentTemp  = val;
        else if (strcmp_P(keyBuf, PSTR("relative_humidity_2m")) == 0) _wxWork.fHumidity      = val;
        else if (strcmp_P(keyBuf, PSTR("wind_speed_10m"))       == 0) _wxWork.fWindSpeed     = val;
        else if (strcmp_P(keyBuf, PSTR("cloud_cover"))          == 0) _wxWork.fCloudCover    = val;
#if HAS_WEATHER_FORECAST
        // Extended fields — ESP32 only (not requested in ESP8266 URL)
        else if (strcmp_P(keyBuf, PSTR("wind_direction_10m"))   == 0) _wxWork.fWindDirection = val;
        else if (strcmp_P(keyBuf, PSTR("wind_gusts_10m"))       == 0) _wxWork.fWindGusts     = val;
        else if (strcmp_P(keyBuf, PSTR("pressure_msl"))         == 0) _wxWork.fPressureMsl   = val;
        else if (strcmp_P(keyBuf, PSTR("precipitation"))        == 0) _wxWork.fPrecipitation = val;
        else if (strcmp_P(keyBuf, PSTR("rain"))                 == 0) _wxWork.fRain          = val;
        else if (strcmp_P(keyBuf, PSTR("snowfall"))             == 0) _wxWork.fSnowfall      = val;
        else if (strcmp_P(keyBuf, PSTR("weather_code"))         == 0) _wxWork.iWeatherCode   = (uint16_t)val;
        else if (strcmp_P(keyBuf, PSTR("is_day"))               == 0) _wxWork.bIsDay         = (val > 0.0f);
#endif  // HAS_WEATHER_FORECAST
      }
    }
    // else: true/false/null at top level — not needed for weather fields

    yield();
  }
}

//=====================================================================
//=== HTTP Fetch (worker task) and handoff to the loop task ===
//=====================================================================

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

// TASK-1604 (TASK-1471 R1). The GET used to run on the loop task: with the internet blocked it
// stalled the loop for the DNS lookup (not bounded by any HTTPClient timeout) plus the connect
// timeout, at +300/360/480/720/1200 s after boot. Now a worker task (the persistent webhook sender
// task, TASK-1669) does DNS, connect, GET and parse. Ownership rules:
//   - _wxState IDLE:    nobody touches _wxWork except the loop task (seed / consume).
//   - _wxState RUNNING: only the worker touches _wxWork. The loop task starts no second worker.
//   - _wxState READY:   the worker has published the result (release store) and does nothing but
//                       delete itself; the loop task (acquire load) copies it to state.sat.weather
//                       in weatherConsumeResult() and sets IDLE.
// _wxState is only accessed through __atomic builtins. No SAT state is written from the worker.
static const uint8_t WX_IDLE    = 0;
static const uint8_t WX_RUNNING = 1;
static const uint8_t WX_READY   = 2;
static uint8_t  _wxState        = WX_IDLE;
static bool     _wxManual       = false;      // loop task only: the telnet 'w' key asked for this fetch
static uint32_t _wxLastBlockMs  = 0;          // loop task only: rate limit for the heap-critical log
// First-fetch backoff (TASK-1319), loop task only. 0 in _wxFirstRetryMs = no try yet.
static uint32_t _wxLastFirstTryMs = 0;
static uint32_t _wxFirstRetryMs   = 0;

static const uint32_t WEATHER_CONNECT_TIMEOUT_MS = 2000UL;  // TCP connect bound; DNS is bounded only by running off the loop task
static const uint32_t WEATHER_READ_TIMEOUT_MS    = 5000UL;  // HTTP header wait and stream read bound

// Worker body: HTTP GET + stream parse into _wxWork. Runs on the worker task, never on the loop.
static void weatherFetchWork()
{
  _wxWork.bOk = false;

  // WiFiClient and HTTPClient come from the platform include chain (<HTTPClient.h>).
  char url[512];
  snprintf_P(url, sizeof(url), kWeatherUrlFmt, _wxWork.fReqLat, _wxWork.fReqLon);

  DebugTf(PSTR("Weather: fetching %s\r\n"), url);

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(WEATHER_CONNECT_TIMEOUT_MS);
  http.setTimeout(WEATHER_READ_TIMEOUT_MS);  // read bound: header wait and every body byte (wstreamGet)

  if (!http.begin(client, url)) {
    DebugTln(F("Weather: http.begin() failed"));
    return;
  }

  // TASK-1605: connect by IP after begin() and before GET(), so HTTPClient reuses this
  // connection and never resolves the hostname through hostByName() (see netConnectUrl()).
  if (!netConnectUrl(client, url, WEATHER_CONNECT_TIMEOUT_MS, WEATHER_READ_TIMEOUT_MS)) {
    DebugTln(F("Weather: no connection to the weather server"));
    http.end();
    return;
  }

  // HTTP/1.0: the server closes after the body, no keep-alive tail and no
  // chunked transfer encoding, so the raw stream holds only JSON bytes.
  http.useHTTP10(true);
  yield();
  int httpCode = http.GET();
  yield();

  if (httpCode == 200) {
    // Stream-parse directly: no heap allocation needed.
    WiFiClient* stream = http.getStreamPtr();
    weatherParseStream(stream, &http);
    _wxWork.bOk = true;
  } else {
    DebugTf(PSTR("Weather: HTTP %d\r\n"), httpCode);
  }

  http.end();
}

// TASK-1669: the fetch runs on the persistent webhook sender task, which calls this between its
// jobs (webhookTaskBody). A one-shot task created and deleted per fetch lost about 132 B of
// internal heap for good on every fetch (~12 KB a day at the 900 s default interval).
static bool _wxRequested = false;   // set by the loop task (weatherRequestFetch), taken by the worker

void weatherWorkerPoll()
{
  if (!__atomic_exchange_n(&_wxRequested, false, __ATOMIC_ACQ_REL)) return;
  weatherFetchWork();
  __atomic_store_n(&_wxState, WX_READY, __ATOMIC_RELEASE);   // publish: from here the loop task owns _wxWork
}

// Loop task, worker not running: copy the live values into the scratch.
static void weatherSeedWork()
{
  _wxWork.fTemperature   = state.sat.weather.fTemperature;
  _wxWork.fApparentTemp  = state.sat.weather.fApparentTemp;
  _wxWork.fHumidity      = state.sat.weather.fHumidity;
  _wxWork.fWindSpeed     = state.sat.weather.fWindSpeed;
  _wxWork.fWindDirection = state.sat.weather.fWindDirection;
  _wxWork.fWindGusts     = state.sat.weather.fWindGusts;
  _wxWork.fCloudCover    = state.sat.weather.fCloudCover;
  _wxWork.fPressureMsl   = state.sat.weather.fPressureMsl;
  _wxWork.fPrecipitation = state.sat.weather.fPrecipitation;
  _wxWork.fRain          = state.sat.weather.fRain;
  _wxWork.fSnowfall      = state.sat.weather.fSnowfall;
  _wxWork.iWeatherCode   = state.sat.weather.iWeatherCode;
  _wxWork.bIsDay         = state.sat.weather.bIsDay;
#if HAS_WEATHER_FORECAST
  memcpy(_wxWork.fcTemp,       _weather_forecastTemp,       sizeof(_wxWork.fcTemp));
  memcpy(_wxWork.fcDewPt,      _weather_forecastDewPt,      sizeof(_wxWork.fcDewPt));
  memcpy(_wxWork.fcCloud,      _weather_forecastCloud,      sizeof(_wxWork.fcCloud));
  memcpy(_wxWork.fcPrecipProb, _weather_forecastPrecipProb, sizeof(_wxWork.fcPrecipProb));
  _wxWork.fcCount        = _weather_forecastCount;
#endif
  _wxWork.bOk            = false;
  _wxWork.fReqLat        = settings.sat.fWeatherLat;
  _wxWork.fReqLon        = settings.sat.fWeatherLon;
}

// Start the worker unless one is running or its result is not consumed yet.
// Returns 1 = worker started, 0 = not attempted (disabled, no coordinates, busy, heap critical),
// -1 = attempted and failed (task creation failed; already counted as a fetch error).
static int weatherRequestFetch()
{
  if (!settings.sat.bWeatherEnable) return 0;
  if (settings.sat.fWeatherLat == 0.0f && settings.sat.fWeatherLon == 0.0f) {
    DebugTln(F("Weather: skipping fetch, no coordinates configured"));
    return 0;
  }
  if (__atomic_load_n(&_wxState, __ATOMIC_ACQUIRE) != WX_IDLE) return 0;   // one worker at a time
  if (getHeapHealth() == HEAP_CRITICAL) {
    if (_wxLastBlockMs == 0 || (uint32_t)(millis() - _wxLastBlockMs) >= 60000UL) {
      _wxLastBlockMs = millis() | 1UL;
      DebugTln(F("Weather: heap critical, fetch not started"));
    }
    return 0;
  }
  weatherSeedWork();
  __atomic_store_n(&_wxState, WX_RUNNING, __ATOMIC_RELEASE);
  if (!webhookTaskAvailable()) {
    __atomic_store_n(&_wxState, WX_IDLE, __ATOMIC_RELEASE);
    DebugTln(F("Weather: no worker task to run the fetch"));
    state.sat.weather.iFetchErrors++;
    return -1;
  }
  __atomic_store_n(&_wxRequested, true, __ATOMIC_RELEASE);   // the webhook task picks it up within 500 ms
  return 1;
}

// Loop task: apply a published result to state.sat.weather. Returns the worker verdict.
static bool weatherApplyResult()
{
  if (!_wxWork.bOk) {
    state.sat.weather.iFetchErrors++;
    return false;
  }
  state.sat.weather.fTemperature   = _wxWork.fTemperature;
  state.sat.weather.fApparentTemp  = _wxWork.fApparentTemp;
  state.sat.weather.fHumidity      = _wxWork.fHumidity;
  state.sat.weather.fWindSpeed     = _wxWork.fWindSpeed;
  state.sat.weather.fWindDirection = _wxWork.fWindDirection;
  state.sat.weather.fWindGusts     = _wxWork.fWindGusts;
  state.sat.weather.fCloudCover    = _wxWork.fCloudCover;
  state.sat.weather.fPressureMsl   = _wxWork.fPressureMsl;
  state.sat.weather.fPrecipitation = _wxWork.fPrecipitation;
  state.sat.weather.fRain          = _wxWork.fRain;
  state.sat.weather.fSnowfall      = _wxWork.fSnowfall;
  state.sat.weather.iWeatherCode   = _wxWork.iWeatherCode;
  state.sat.weather.bIsDay         = _wxWork.bIsDay;
#if HAS_WEATHER_FORECAST
  memcpy(_weather_forecastTemp,       _wxWork.fcTemp,       sizeof(_wxWork.fcTemp));
  memcpy(_weather_forecastDewPt,      _wxWork.fcDewPt,      sizeof(_wxWork.fcDewPt));
  memcpy(_weather_forecastCloud,      _wxWork.fcCloud,      sizeof(_wxWork.fcCloud));
  memcpy(_weather_forecastPrecipProb, _wxWork.fcPrecipProb, sizeof(_wxWork.fcPrecipProb));
  _weather_forecastCount = _wxWork.fcCount;
#endif
  state.sat.weather.bValid        = true;
  state.sat.weather.iLastUpdateMs = millis();
  if (diagRec.active) diagRec.recWeather(diagX100(state.sat.weather.fTemperature), diagX100(state.sat.weather.fHumidity), diagX100(state.sat.weather.fWindSpeed));   // TASK-1665 (ADR-213)

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
  return true;
}

// First-fetch backoff (TASK-1319): a failed try waits 60 s, then doubles, capped at the poll
// interval; a success ends the backoff and restarts the poll timer. Loop task only.
static void weatherNoteFirstFetchOutcome(bool ok)
{
  if (ok) {
    _wxFirstRetryMs = 0;
    RESTART_TIMER(timerWeatherPoll);
  } else {
    const uint32_t capMs = (uint32_t)settings.sat.iWeatherInterval * 1000UL;
    _wxFirstRetryMs = (_wxFirstRetryMs == 0) ? WEATHER_FIRST_RETRY_MS : _wxFirstRetryMs * 2;
    if (capMs >= WEATHER_FIRST_RETRY_MS && _wxFirstRetryMs > capMs) _wxFirstRetryMs = capMs;
  }
}

// Loop task, every pass: consume a published result. Idempotent: a second call finds IDLE.
static void weatherConsumeResult()
{
  if (__atomic_load_n(&_wxState, __ATOMIC_ACQUIRE) != WX_READY) return;
  const bool wasValid = state.sat.weather.bValid;
  const bool ok       = weatherApplyResult();
  const bool manual   = _wxManual;
  _wxManual = false;
  __atomic_store_n(&_wxState, WX_IDLE, __ATOMIC_RELEASE);
  if (manual) {
    weatherDumpStateToTelnet();
  } else if (!wasValid) {
    weatherNoteFirstFetchOutcome(ok);
  }
}

void weatherFetch()
{
  (void)weatherRequestFetch();
}

// Public entry point for the telnet debug 'w' shortcut. Bypasses the scheduler interval but
// still honours the bWeatherEnable flag and the coordinate guard inside weatherRequestFetch().
// TASK-1604: the fetch runs on the worker task; the cached state is dumped to telnet when the
// loop task consumes the result, a few seconds later.
void weatherFetchOpenMeteoNow()
{
  DebugTln(F("Weather: manual Open-Meteo fetch triggered via debug telnet"));
  if (weatherRequestFetch() > 0) {
    _wxManual = true;
  } else {
    DebugTln(F("Weather: fetch not started (disabled, no coordinates, busy or heap critical)"));
  }
}

//=====================================================================
//=== Loop — called from main loop (timer-guarded) ===
//=====================================================================
void weatherLoop()
{
  weatherConsumeResult();   // apply a result the worker published, also when the feature was switched off meanwhile
  if (!settings.sat.bWeatherEnable) return;
  // 5-minute startup gate: give the OT bus time to report Toutside before
  // making the first weather API call.
  if (millis() < (WEATHER_POLL_MIN_SEC * 1000UL)) return;
  // If the boiler already provides the outside temperature SAT uses, avoid burning weather API calls
  // (SAT only needs weather as a fallback). TASK-1897: the same rule satResolveOutsideTemp() ranks by: MsgID 27 from a
  // live Read-Ack younger than 1 h and not 0.0. A Toutside the PIC fills from a DS18B20, or a stale one, is not used by
  // SAT, so it must not block the weather fallback either.
  if (satOtOutsideFresh(millis()) && OTcurrentSystemState.Toutside != 0.0f) return;

  // TASK-1319: until the first fetch succeeds, try right after the start-up gate
  // instead of waiting for the first poll tick (the poll timer runs from boot, so
  // that tick comes ~15 min after boot). A failed try waits 60 s, then doubles,
  // capped at the poll interval, so a unit without internet does not retry every
  // minute forever. TASK-1604: the fetch runs on a worker task, so the outcome
  // reaches the backoff in weatherConsumeResult() on a later pass.
  // Without coordinates there is nothing to fetch and no try is counted, so the
  // first fetch follows right after the coordinates are entered.
  // Once valid, the normal interval takes over, counted from that fetch.
  if (!state.sat.weather.bValid) {
    if (settings.sat.fWeatherLat == 0.0f && settings.sat.fWeatherLon == 0.0f) return;
    if (_wxFirstRetryMs != 0 && (millis() - _wxLastFirstTryMs) < _wxFirstRetryMs) return;
    const int started = weatherRequestFetch();
    if (started == 0) return;                 // busy or heap critical: ask again on the next pass
    _wxLastFirstTryMs = millis();
    if (started < 0) weatherNoteFirstFetchOutcome(false);
    return;
  }
  if (!DUE(timerWeatherPoll)) return;

  weatherFetch();
}

//=====================================================================
//=== REST API: GET /api/v2/sat/weather ===
//=====================================================================
void weatherSendStatusJSON()
{
  // ADR-141 revert / TASK-885: streaming JsonEmit replaces the JsonDocument
  // path. Each float field emits as a real JSON number via je.field(k, float)
  // (6-decimal default, trailing zeros trimmed; json_golden tolerates decimal
  // width). Bools emit as
  // real true/false, ints as bare integers. The forecast arrays stay bare
  // numbers. The original wrapper key was F("") => a flat ROOT object, so build
  // directly on the root object (no nested wrapper).
  AsyncResponseStream* strm = restBeginStreamSized("application/json", REST_SIZE_WEATHER);
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
