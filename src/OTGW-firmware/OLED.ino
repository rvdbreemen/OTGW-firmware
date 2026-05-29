/*
***************************************************************************
**  Program  : OLED.ino
**  Version  : v2.0.0-alpha.88
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  OLED display module for OTGW-firmware.
**  Works on both ESP8266 (PIC-based OTGW) and ESP32-S3 (OTGW32).
**  Drives a 128x64 SSD1306 I2C OLED display with runtime detection.
**  Shows network status, OT-Direct status, device info and temperatures
**  on 5 pages cycled via the boot button (GPIO 0 = PIN_BUTTON).
**
**  Uses SSD1306Ascii (text-only, no framebuffer) to save ~1KB RAM
**  compared to Adafruit_SSD1306 which requires a 1024-byte framebuffer.
**
**  Design:
**  - Runtime I2C probe at 0x3C - if no display, all code is skipped
**  - Reads values directly from OTcurrentSystemState (no JSON layer)
**  - Button ISR on PIN_BUTTON (GPIO 0) via FreeRTOS queue with debounce
**  - Auto-off after 30s of inactivity; button press wakes from off
**  - 5 s display refresh (non-blocking, cooperative); clear() only on
**    page change (SSD1306Ascii overwrites in place)
**
**  Pages (0=off, 1-5=content):
**  1 Network   - transport, IP, MQTT broker + status
**  2 OT Status - hardware mode, bus state, flame, temps
**  3 Device    - firmware version, hostname, uptime, heap
**  4 Dashboard - key temps + flame/CH/DHW at a glance
**  5 Heating   - HC1 + HC2 room setpoints and flow temps
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#if defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE

#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

#if defined(ESP32)
  #include <driver/gpio.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/queue.h>
  #include <esp_timer.h>
#endif

// Stringification helper for _VERSION_PRERELEASE (which is a bare token, not quoted)
#define OLED_STR_HELPER(x) #x
#define OLED_STR(x)        OLED_STR_HELPER(x)

// Button pin: boot button (GPIO 0) on OTGW32. Active LOW, pull-up.
#define OLED_BUTTON_PIN PIN_BUTTON

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t  OLED_ADDR         = 0x3C;
static constexpr uint32_t OLED_REFRESH_MS   = 5000;   // 5 s refresh (readable, low I2C load)
static constexpr uint32_t OLED_TIMEOUT_MS   = 30000;  // auto-off after 30s
static constexpr uint8_t  OLED_NUM_PAGES    = 6;      // 0=off, 1-5=content pages
static constexpr int64_t  OLED_DEBOUNCE_US  = 300000; // 300 ms (microseconds)

// SSD1306Ascii uses row-based positioning (rows of 8 pixels for System5x7 font).
// 128x64 display = 21 chars wide x 8 rows with System5x7 (6px wide, 8px tall).
static constexpr uint8_t  OLED_COLS         = 21;   // chars per line (128/6)
static constexpr uint8_t  OLED_ROWS         = 8;    // text rows (64/8)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static SSD1306AsciiWire oledDisplay;
static bool     oledPresent       = false;
static uint8_t  oledPage          = 0;     // 0=off, 1-5=content
static uint32_t oledLastActivity  = 0;     // millis of last button press
static uint32_t oledLastRefresh   = 0;     // millis of last screen draw
static uint8_t  oledLastRenderedPage = 0xFF; // track page-change for selective clear()
static char     oledCachedSSID[33] = "";   // cached WiFi SSID (avoid String in hot path)

#if defined(ESP32)
static QueueHandle_t _oledBtnQueue = nullptr;
#endif

// Shared formatting buffer (small, on module level to avoid per-function stack cost)
static char oledBuf[22];

// ---------------------------------------------------------------------------
// Button ISR - push timestamp into queue; debounce happens in loopOLED()
// ---------------------------------------------------------------------------
#if defined(ESP32)
static void IRAM_ATTR oledButtonISR(void* /*arg*/) {
  if (gpio_get_level((gpio_num_t)OLED_BUTTON_PIN) == 0) {
    int64_t t = esp_timer_get_time();
    xQueueSendFromISR(_oledBtnQueue, &t, nullptr);
  }
}
#endif

// ---------------------------------------------------------------------------
// fmtFloatOrDash - render float into oledBuf, or "---" if NaN
// Returns pointer to oledBuf for chained print().
// ---------------------------------------------------------------------------
static const char* fmtFloatOrDash(float val) {
  if (isnan(val)) {
    strlcpy(oledBuf, "---", sizeof(oledBuf));
  } else {
    snprintf_P(oledBuf, sizeof(oledBuf), PSTR("%.1f"), val);
  }
  return oledBuf;
}

// ---------------------------------------------------------------------------
// probeOLED - check if SSD1306 responds on I2C
// ---------------------------------------------------------------------------
static bool probeOLED() {
  Wire.beginTransmission(OLED_ADDR);
  return (Wire.endTransmission() == 0);
}

// ---------------------------------------------------------------------------
// drawHeader - common header row with title and page indicator
// ---------------------------------------------------------------------------
static void drawHeader(const __FlashStringHelper* title) {
  oledDisplay.setRow(0);
  oledDisplay.setCol(0);
  oledDisplay.print(title);
  // Page indicator on the right (col 108 = 18 chars * 6px)
  oledDisplay.setCol(108);
  oledDisplay.print(oledPage);
  oledDisplay.print('/');
  oledDisplay.print(OLED_NUM_PAGES - 1);
  // Separator line: fill row 1 with dashes
  oledDisplay.setRow(1);
  oledDisplay.setCol(0);
  for (uint8_t i = 0; i < OLED_COLS; i++) oledDisplay.print('-');
}

// ---------------------------------------------------------------------------
// Page 1: Network status - transport, IP, MQTT
// ---------------------------------------------------------------------------
static void drawPageNetwork() {
  drawHeader(F("Network"));

  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  if (isNetworkUp()) {
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
    if (state.net.eMode == NET_ETHERNET) {
      oledDisplay.print(F("Ethernet"));
    } else
#endif
    {
      oledDisplay.print(F("WiFi: "));
      // Use cached SSID (refreshed in loopOLED) to avoid String alloc in hot path.
      char ssidBuf[15];
      strlcpy(ssidBuf, oledCachedSSID, sizeof(ssidBuf));
      oledDisplay.print(ssidBuf);
    }

    oledDisplay.setRow(3);
    oledDisplay.setCol(0);
    oledDisplay.print(F("IP: "));
    oledDisplay.print(getActiveIP());

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
    if (state.net.eMode != NET_ETHERNET) {
#endif
      oledDisplay.setRow(4);
      oledDisplay.setCol(0);
      snprintf_P(oledBuf, sizeof(oledBuf), PSTR("RSSI: %d dBm"), (int)WiFi.RSSI());
      oledDisplay.print(oledBuf);
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
    }
#endif
  } else {
    oledDisplay.print(F("Network: offline"));
  }

  // MQTT status + broker
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.print(state.mqtt.bConnected ? F("MQTT: connected") : F("MQTT: offline"));

  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  // Truncate broker to fit
  char brokerBuf[OLED_COLS + 1];
  strlcpy(brokerBuf, settings.mqtt.sBroker, sizeof(brokerBuf));
  oledDisplay.print(brokerBuf);
}

// ---------------------------------------------------------------------------
// Page 2: OT-Direct / OpenTherm bus status
// ---------------------------------------------------------------------------
static void drawPageOTStatus() {
  drawHeader(F("OT Status"));

  // Hardware mode
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Mode: "));
  oledDisplay.print(hardwareModeName());

  // Bus online/offline
  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Bus:  "));
  oledDisplay.print(state.otBus.bOnline ? F("online") : F("offline"));

  // Flame and modulation
  bool flameOn = (OTcurrentSystemState.SlaveStatus & 0x08) != 0;
  oledDisplay.setRow(4);
  oledDisplay.setCol(0);
  if (flameOn) {
    float mod = OTcurrentSystemState.RelModLevel;
    if (isnan(mod)) {
      oledDisplay.print(F("Flame:on  Mod:---"));
    } else {
      snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Flame:on  Mod:%.0f%%"), mod);
      oledDisplay.print(oledBuf);
    }
  } else {
    oledDisplay.print(F("Flame: off"));
  }

  // CH / DHW active flags
  bool chActive  = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;
  bool dhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0;
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.print(F("CH:"));
  oledDisplay.print(chActive  ? F("on  ") : F("off "));
  oledDisplay.print(F("DHW:"));
  oledDisplay.print(dhwActive ? F("on")   : F("off"));

  // Setpoint and boiler temp - guard each float independently
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Set:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TSet));
  oledDisplay.print(F(" Boil:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tboiler));
}

// ---------------------------------------------------------------------------
// Page 3: Device status - version, hostname, uptime, heap
// ---------------------------------------------------------------------------
static void drawPageDevice() {
  drawHeader(F("Device"));

  // Firmware version
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("FW %d.%d.%d-" OLED_STR(_VERSION_PRERELEASE)),
    _VERSION_MAJOR, _VERSION_MINOR, _VERSION_PATCH);
  oledDisplay.print(oledBuf);

  // Hostname (truncated to fit)
  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  char hostBuf[OLED_COLS + 1];
  strlcpy(hostBuf, settings.sHostname, sizeof(hostBuf));
  oledDisplay.print(hostBuf);

  // Uptime: format as DDd HHh MMm
  uint32_t secs = state.uptime.iSeconds;
  uint32_t days = secs / 86400;
  uint32_t hrs  = (secs % 86400) / 3600;
  uint32_t mins = (secs % 3600) / 60;
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Up: %ud %02uh %02um"), (unsigned)days, (unsigned)hrs, (unsigned)mins);
  oledDisplay.print(oledBuf);

  // Free heap and reboot count
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Heap: %u B"), (unsigned)ESP.getFreeHeap());
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(7);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Reboots: %u"), (unsigned)state.uptime.iRebootCount);
  oledDisplay.print(oledBuf);
}

// ---------------------------------------------------------------------------
// Page 4: Dashboard - key temperatures + flame/CH/DHW status
// ---------------------------------------------------------------------------
static void drawPageDashboard() {
  drawHeader(F("Dashboard"));

  bool flameOn  = (OTcurrentSystemState.SlaveStatus & 0x08) != 0;
  bool chActive = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;
  bool dhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0;

  // Row 2: Flame + modulation, CH/DHW status
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  if (flameOn) {
    float mod = OTcurrentSystemState.RelModLevel;
    if (isnan(mod)) {
      oledDisplay.print(F("* Mod: ---"));
    } else {
      snprintf_P(oledBuf, sizeof(oledBuf), PSTR("* Mod: %.0f%%"), mod);
      oledDisplay.print(oledBuf);
    }
  } else {
    oledDisplay.print(F("Flame: off"));
  }
  oledDisplay.setCol(90);
  if (chActive) oledDisplay.print(F("CH"));
  if (chActive && dhwActive) oledDisplay.print('/');
  if (dhwActive) oledDisplay.print(F("DHW"));

  // Row 4: Setpoint and boiler temp
  oledDisplay.setRow(4);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Set:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TSet));
  oledDisplay.print(F(" Boil:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tboiler));

  // Row 5: Return and DHW temp
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Ret:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tret));
  oledDisplay.print(F(" DHW :"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tdhw));

  // Row 6: Pressure and outside temp
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Bar:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.CHPressure));
  oledDisplay.print(F(" Out :"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Toutside));
}

// ---------------------------------------------------------------------------
// Page 5: Heating circuits - HC1 and HC2 setpoints / flow temps
// ---------------------------------------------------------------------------
static void drawPageHeating() {
  drawHeader(F("Heating"));

  // HC1
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  oledDisplay.print(F("HC1:"));
  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  oledDisplay.print(F(" Set:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TrSet));
  oledDisplay.print(F(" Rm:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tr));
  oledDisplay.setRow(4);
  oledDisplay.setCol(0);
  oledDisplay.print(F(" Flow:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tboiler));
  oledDisplay.print(F(" Ret:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.Tret));

  // HC2
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.print(F("HC2:"));
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.print(F(" Set:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TrSetCH2));
  oledDisplay.print(F(" Rm:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TRoomCH2));
  oledDisplay.setRow(7);
  oledDisplay.setCol(0);
  oledDisplay.print(F(" Flow:"));
  oledDisplay.print(fmtFloatOrDash(OTcurrentSystemState.TflowCH2));
}

// ---------------------------------------------------------------------------
// initOLED - called from setup(), after Wire.begin()
// ---------------------------------------------------------------------------
void initOLED() {
  if (!probeOLED()) {
    DebugTln(F("OLED: No display at 0x3C - disabled"));
    oledPresent = false;
    return;
  }

  oledDisplay.begin(&Adafruit128x64, OLED_ADDR);
  oledDisplay.setFont(System5x7);
  oledDisplay.displayRemap(false);

  oledPresent = true;
  oledPage = 1;
  oledLastActivity = millis();
  oledLastRenderedPage = 0xFF;  // force initial clear+render

  // Advertise OLED presence in runtime state
  state.hw.bOLEDPresent = true;

  DebugTln(F("OLED: Display initialized (128x64 SSD1306Ascii)"));

  // Boot splash
  oledDisplay.clear();
  oledDisplay.set2X();
  oledDisplay.setRow(0);
  oledDisplay.setCol(16);
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  oledDisplay.println(F("OTGW32"));
#else
  oledDisplay.println(F("OTGW"));
#endif
  oledDisplay.set1X();
  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  oledDisplay.println(F("OpenTherm Gateway"));
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.println(F("Press button for info"));

  // Button ISR for page cycling — ESP-IDF GPIO API + FreeRTOS queue.
  // Queue carries the press timestamp (us); debounce happens in loopOLED().
#if defined(ESP32)
  _oledBtnQueue = xQueueCreate(10, sizeof(int64_t));
  gpio_set_direction((gpio_num_t)OLED_BUTTON_PIN, GPIO_MODE_INPUT);
  // Boot button has an external pull-up; ensure internal is also enabled for safety.
  gpio_set_pull_mode((gpio_num_t)OLED_BUTTON_PIN, GPIO_PULLUP_ONLY);
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  gpio_set_intr_type((gpio_num_t)OLED_BUTTON_PIN, GPIO_INTR_NEGEDGE);
  gpio_intr_enable((gpio_num_t)OLED_BUTTON_PIN);
  gpio_isr_handler_add((gpio_num_t)OLED_BUTTON_PIN, oledButtonISR, nullptr);
  DebugTf(PSTR("OLED: Button ISR on GPIO %d (FreeRTOS queue)\r\n"), OLED_BUTTON_PIN);
#else
  // Non-ESP32 fallback: legacy Arduino API (HAS_OLED_CAPABLE is currently
  // ESP32-only, so this is belt-and-braces only).
  pinMode(OLED_BUTTON_PIN, INPUT_PULLUP);
  DebugTf(PSTR("OLED: Button on GPIO %d (no ISR — non-ESP32 build)\r\n"), OLED_BUTTON_PIN);
#endif
}

// ---------------------------------------------------------------------------
// loopOLED - called from loop(), non-blocking
// ---------------------------------------------------------------------------
void loopOLED() {
  if (!oledPresent) return;

  uint32_t now = millis();

  // ----- Button drain w/ debounce (debounce check BEFORE state mutation) -----
#if defined(ESP32)
  static int64_t _lastBtnTime = 0;
  int64_t c = 0;
  while (_oledBtnQueue && xQueueReceive(_oledBtnQueue, &c, 0)) {
    if ((c - _lastBtnTime) < OLED_DEBOUNCE_US) {
      _lastBtnTime = c;
      continue;  // bounce — drop without mutating page state
    }
    _lastBtnTime = c;
    oledLastActivity = now;

    if (oledPage == 0) {
      // Wake from off: go to page 1 and turn display on
      oledPage = 1;
      oledDisplay.ssd1306WriteCmd(0xAF);  // DISPLAYON
      oledLastRefresh = 0;                // force immediate redraw
    } else {
      // Cycle to next page; wrap past last content page to 0=off.
      // OLED_NUM_PAGES = 6 (slots: 0=off, 1..5 content), so 5 -> 0.
      oledPage = (oledPage + 1) % OLED_NUM_PAGES;
      if (oledPage == 0) {
        oledDisplay.ssd1306WriteCmd(0xAE);  // DISPLAYOFF
      } else {
        oledDisplay.ssd1306WriteCmd(0xAF);  // DISPLAYON
        oledLastRefresh = 0;                // force immediate redraw
      }
    }
  }
#endif

  // ----- SSID cache maintenance (avoid String in hot path, ADR-004) -----
  if (WiFi.isConnected()) {
    if (oledCachedSSID[0] == '\0') {
      strlcpy(oledCachedSSID, WiFi.SSID().c_str(), sizeof(oledCachedSSID));
    }
  } else {
    oledCachedSSID[0] = '\0';
  }

  // Auto-off after timeout
  if (oledPage != 0 && (now - oledLastActivity) >= OLED_TIMEOUT_MS) {
    oledDisplay.ssd1306WriteCmd(0xAE);  // DISPLAYOFF
    oledPage = 0;
    return;
  }

  if (oledPage == 0) return;

  // Throttle render cadence (OLED_REFRESH_MS)
  if ((now - oledLastRefresh) < OLED_REFRESH_MS) return;
  oledLastRefresh = now;

  // clear() only on page change — SSD1306Ascii overwrites text in place,
  // so per-second clear() just causes flicker and wastes I2C cycles.
  if (oledPage != oledLastRenderedPage) {
    oledDisplay.clear();
    oledLastRenderedPage = oledPage;
  }

  switch (oledPage) {
    case 1: drawPageNetwork();   break;
    case 2: drawPageOTStatus();  break;
    case 3: drawPageDevice();    break;
    case 4: drawPageDashboard(); break;
    case 5: drawPageHeating();   break;
  }
}

// ---------------------------------------------------------------------------
// oledWake - external call to wake display (e.g. on network state change)
// ---------------------------------------------------------------------------
void oledWake() {
  if (!oledPresent) return;
  if (oledPage == 0) {
    oledPage = 1;
    oledDisplay.ssd1306WriteCmd(0xAF);  // DISPLAYON
    oledLastRefresh = 0;
  }
  oledLastActivity = millis();
}

#endif // defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE

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
