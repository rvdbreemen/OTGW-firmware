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
**  Fetches outdoor temperature, humidity, wind speed, and 24-hour
**  hourly temperature forecast.  Free API, no key required.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Constants ---
static const uint16_t WEATHER_POLL_DEFAULT_SEC = 900;  // 15 min
static const uint16_t WEATHER_POLL_MIN_SEC     = 300;  // 5 min
static const uint8_t  WEATHER_FORECAST_HOURS   = 24;

// --- State ---
static float    _weather_forecastTemp[WEATHER_FORECAST_HOURS];
static uint8_t  _weather_forecastCount = 0;

// Timer — uses configurable interval, default 15 min
DECLARE_TIMER_SEC(timerWeatherPoll, WEATHER_POLL_DEFAULT_SEC, CATCH_UP_MISSED_TICKS);

//=====================================================================
//=== Lightweight JSON helpers (no ArduinoJson!) ===
//=====================================================================

// Find "key": <number> in a JSON string and return the float value.
// Handles nested objects by searching for the exact key string.
// Returns true if found and parsed successfully.
static bool weatherJsonGetFloat(const char* json, PGM_P key, float* out)
{
  if (!json || !key || !out) return false;

  // Build search pattern: "key":
  char search[48];
  snprintf_P(search, sizeof(search), PSTR("\"%s\":"), key);

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
static bool weatherJsonGetArray(const char* json, PGM_P key, float* arr, uint8_t maxLen, uint8_t* count)
{
  if (!json || !key || !arr || !count) return false;
  *count = 0;

  // Find "hourly" section first
  const char* hourlyPos = strstr_P(json, PSTR("\"hourly\""));
  if (!hourlyPos) return false;

  // Build search pattern: "key":[
  char search[48];
  snprintf_P(search, sizeof(search), PSTR("\"%s\":["), key);

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

  // Build URL
  char url[220];
  snprintf_P(url, sizeof(url),
    PSTR("http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
         "&current=temperature_2m,relative_humidity_2m,wind_speed_10m"
         "&hourly=temperature_2m&forecast_days=1"),
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
    // Replace String payload = http.getString() (ADR-004 hot-path repeat
    // allocation every 15 min, 2-4 KB doubling growth) with a bounded heap
    // buffer. Fixed 4 KB is plenty for current + 24h forecast; if heap is
    // low we skip the fetch entirely rather than risk a failed malloc mid-loop.
    constexpr size_t WEATHER_BUF_SIZE = 4096;
    if (ESP.getFreeHeap() < (WEATHER_BUF_SIZE + 4096)) {
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

    // Parse current weather from "current" section
    float temp = 0.0f, hum = 0.0f, wind = 0.0f;

    // Find "current" section to avoid matching hourly keys
    const char* currentPos = strstr_P(json, PSTR("\"current\""));
    if (currentPos) {
      weatherJsonGetFloat(currentPos, PSTR("temperature_2m"), &temp);
      weatherJsonGetFloat(currentPos, PSTR("relative_humidity_2m"), &hum);
      weatherJsonGetFloat(currentPos, PSTR("wind_speed_10m"), &wind);

      state.sat.weather.fTemperature = temp;
      state.sat.weather.fHumidity    = hum;
      state.sat.weather.fWindSpeed   = wind;
    }

    // Parse hourly forecast array
    weatherJsonGetArray(json, PSTR("temperature_2m"),
                        _weather_forecastTemp, WEATHER_FORECAST_HOURS,
                        &_weather_forecastCount);

    state.sat.weather.bValid = true;
    state.sat.weather.iLastUpdateMs = millis();

    DebugTf(PSTR("Weather: %.1fC, %d%% humidity, %.1f km/h wind, %d forecast hours (read %u bytes)\r\n"),
      state.sat.weather.fTemperature,
      (int)state.sat.weather.fHumidity,
      state.sat.weather.fWindSpeed,
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
  sendJsonMapEntry(F("enabled"),     settings.sat.bWeatherEnable);
  sendJsonMapEntry(F("valid"),       state.sat.weather.bValid);

  {
    char tmpBuf[8];
    dtostrf(state.sat.weather.fTemperature, 1, 1, tmpBuf);
    sendJsonMapEntry(F("temperature"),   tmpBuf);
    dtostrf(state.sat.weather.fHumidity, 1, 0, tmpBuf);
    sendJsonMapEntry(F("humidity"),      tmpBuf);
    dtostrf(state.sat.weather.fWindSpeed, 1, 1, tmpBuf);
    sendJsonMapEntry(F("wind_speed"),    tmpBuf);
    dtostrf(settings.sat.fWeatherLat, 1, 4, tmpBuf);
    sendJsonMapEntry(F("latitude"),      tmpBuf);
    dtostrf(settings.sat.fWeatherLon, 1, 4, tmpBuf);
    sendJsonMapEntry(F("longitude"),     tmpBuf);
  }

  sendJsonMapEntry(F("fetch_errors"), (int32_t)state.sat.weather.iFetchErrors);

  // Age in seconds
  if (state.sat.weather.bValid && state.sat.weather.iLastUpdateMs > 0) {
    uint32_t ageSec = (millis() - state.sat.weather.iLastUpdateMs) / 1000;
    sendJsonMapEntry(F("age_seconds"), (int32_t)ageSec);
  } else {
    sendJsonMapEntry(F("age_seconds"), (int32_t)-1);
  }

  // Forecast array as inline JSON array
  // Single buffer: prefix "forecast": (12 chars) + array max ~169 chars = ~181 total, well within 300
  {
    char entryBuf[300];
    // Write the JSON key prefix first; snprintf_P returns chars written (excluding NUL)
    size_t pos = snprintf_P(entryBuf, sizeof(entryBuf), PSTR("\"forecast\":["));
    for (uint8_t i = 0; i < _weather_forecastCount && i < WEATHER_FORECAST_HOURS; i++) {
      if (i > 0 && pos < sizeof(entryBuf) - 9) entryBuf[pos++] = ',';
      char tmpBuf[8];
      dtostrf(_weather_forecastTemp[i], 1, 1, tmpBuf);
      size_t len = strlen(tmpBuf);
      if (pos + len < sizeof(entryBuf) - 2) {
        memcpy(entryBuf + pos, tmpBuf, len);
        pos += len;
      }
    }
    entryBuf[pos++] = ']';
    entryBuf[pos] = '\0';
    sendBeforenext();
    httpServer.sendContent(entryBuf);
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

  dtostrf(state.sat.weather.fTemperature, 1, 1, valBuf);
  sendMQTTData(F("sat/weather/temperature"), valBuf, false);

  dtostrf(state.sat.weather.fHumidity, 1, 0, valBuf);
  sendMQTTData(F("sat/weather/humidity"), valBuf, false);

  dtostrf(state.sat.weather.fWindSpeed, 1, 1, valBuf);
  sendMQTTData(F("sat/weather/wind_speed"), valBuf, false);
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
