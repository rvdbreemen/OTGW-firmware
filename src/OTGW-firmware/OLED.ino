/*
***************************************************************************
**  Program  : OLED.ino
**  Version  : v2.0.0-alpha.4
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  OLED display module for OTGW-firmware.
**  Works on both ESP8266 (PIC-based OTGW) and ESP32-S3 (OTGW32).
**  Drives a 128x64 SSD1306 I2C OLED display with runtime detection.
**  Shows OpenTherm status, temperatures, and system info on 4 pages
**  cycled via the boot/config button.
**
**  Uses SSD1306Ascii (text-only, no framebuffer) to save ~1KB RAM
**  compared to Adafruit_SSD1306 which requires a 1024-byte framebuffer.
**
**  Design:
**  - Runtime I2C probe at 0x3C - if no display, all code is skipped
**  - Reads values directly from OTcurrentSystemState (no JSON layer)
**  - Button ISR on PIN_BUTTON with software debounce
**  - Auto-off after 30s of inactivity
**  - 1 Hz display refresh (non-blocking, cooperative)
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#if HAS_OLED_CAPABLE

#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Stringification helper for _VERSION_PRERELEASE (which is a bare token, not quoted)
#define OLED_STR_HELPER(x) #x
#define OLED_STR(x)        OLED_STR_HELPER(x)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t  OLED_ADDR         = 0x3C;
static constexpr uint32_t OLED_REFRESH_MS   = 1000;   // 1 Hz refresh
static constexpr uint32_t OLED_TIMEOUT_MS   = 30000;  // auto-off after 30s
static constexpr uint8_t  OLED_NUM_PAGES    = 5;      // 0=off, 1-4=content pages
static constexpr uint32_t OLED_DEBOUNCE_MS  = 300;

// SSD1306Ascii uses row-based positioning (rows of 8 pixels for System5x7 font).
// 128x64 display = 21 chars wide x 8 rows with System5x7 (6px wide, 8px tall).
static constexpr uint8_t  OLED_COLS         = 21;   // chars per line (128/6)
static constexpr uint8_t  OLED_ROWS         = 8;    // text rows (64/8)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static SSD1306AsciiWire oledDisplay;
static bool     oledPresent       = false;
static uint8_t  oledPage          = 0;     // 0=off, 1-4=content
static uint32_t oledLastActivity  = 0;     // millis of last button press
static uint32_t oledLastRefresh   = 0;     // millis of last screen draw
static volatile bool oledButtonPressed = false;
static uint32_t oledLastDebounce  = 0;

// Shared formatting buffer (small, on module level to avoid per-function stack cost)
static char oledBuf[22];

// ---------------------------------------------------------------------------
// Button ISR - sets flag for main loop
// ---------------------------------------------------------------------------
static void IRAM_ATTR oledButtonISR() {
  oledButtonPressed = true;
}

// ---------------------------------------------------------------------------
// probeOLED - check if SSD1306 responds on I2C
// ---------------------------------------------------------------------------
static bool probeOLED() {
  Wire.beginTransmission(OLED_ADDR);
  return (Wire.endTransmission() == 0);
}

// ---------------------------------------------------------------------------
// drawHeader - common header line with title and page indicator
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
// Page 1: Dashboard - key temperatures + flame/CH/DHW status
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
    snprintf_P(oledBuf, sizeof(oledBuf), PSTR("* Mod: %.0f%%"), OTcurrentSystemState.RelModLevel);
    oledDisplay.print(oledBuf);
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
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Set:%.1f Boil:%.1f"), OTcurrentSystemState.TSet, OTcurrentSystemState.Tboiler);
  oledDisplay.print(oledBuf);

  // Row 5: Return and DHW temp
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Ret:%.1f DHW :%.1f"), OTcurrentSystemState.Tret, OTcurrentSystemState.Tdhw);
  oledDisplay.print(oledBuf);

  // Row 6: Pressure and outside temp
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Bar:%.1f Out :%.1f"), OTcurrentSystemState.CHPressure, OTcurrentSystemState.Toutside);
  oledDisplay.print(oledBuf);
}

// ---------------------------------------------------------------------------
// Page 2: Heating Circuit 1
// ---------------------------------------------------------------------------
static void drawPageHC1() {
  drawHeader(F("Heating Circuit 1"));

  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Room setp: %.1f C"), OTcurrentSystemState.TrSet);
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  if (isnan(OTcurrentSystemState.Tr)) {
    strlcpy_P(oledBuf, PSTR("Room temp: -- C"), sizeof(oledBuf));
  } else {
    snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Room temp: %.1f C"), OTcurrentSystemState.Tr);
  }
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Flow temp: %.1f C"), OTcurrentSystemState.Tboiler);
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Ret. temp: %.1f C"), OTcurrentSystemState.Tret);
  oledDisplay.print(oledBuf);
}

// ---------------------------------------------------------------------------
// Page 3: Heating Circuit 2
// ---------------------------------------------------------------------------
static void drawPageHC2() {
  drawHeader(F("Heating Circuit 2"));

  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Room setp: %.1f C"), OTcurrentSystemState.TrSetCH2);
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Room temp: %.1f C"), OTcurrentSystemState.TRoomCH2);
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Flow temp: %.1f C"), OTcurrentSystemState.TflowCH2);
  oledDisplay.print(oledBuf);

  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("DHW  temp: %.1f C"), OTcurrentSystemState.Tdhw);
  oledDisplay.print(oledBuf);
}

// ---------------------------------------------------------------------------
// Page 4: System Status
// ---------------------------------------------------------------------------
static void drawPageSystem() {
  drawHeader(F("System Status"));

  // Network
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  if (isNetworkUp()) {
#if defined(_VERSION_PRERELEASE)
    if (state.net.bAPFallback) {
      oledDisplay.print(F("AP MODE: "));
      // Truncate SSID to fit display
      char apSSID[15];
      strlcpy(apSSID, state.net.sAPSSID, sizeof(apSSID));
      oledDisplay.print(apSSID);
      oledDisplay.setRow(3);
      oledDisplay.setCol(0);
      oledDisplay.print(F("IP: 192.168.4.1"));
    } else
#endif
    {
      oledDisplay.print(networkModeName());
      oledDisplay.print(F(": "));
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
      if (state.net.eMode == NET_ETHERNET) {
        oledDisplay.print(F("Wired"));
      } else
#endif
      {
        String ssid = WiFi.SSID();
        if (ssid.length() > 14) ssid = ssid.substring(0, 14);
        oledDisplay.print(ssid);
      }
      oledDisplay.setRow(3);
      oledDisplay.setCol(0);
      oledDisplay.print(F("IP: "));
      oledDisplay.print(getActiveIP());
    }
  } else {
    oledDisplay.print(F("Network: offline"));
  }

  // MQTT
  oledDisplay.setRow(5);
  oledDisplay.setCol(0);
  oledDisplay.print(F("MQTT: "));
  oledDisplay.print(state.mqtt.bConnected ? F("connected") : F("offline"));

  // OT bus
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.print(F("OT bus: "));
  oledDisplay.print(state.otBus.bOnline ? F("online") : F("offline"));

  // Firmware version
  oledDisplay.setRow(7);
  oledDisplay.setCol(0);
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("FW %d.%d.%d-" OLED_STR(_VERSION_PRERELEASE)),
    _VERSION_MAJOR, _VERSION_MINOR, _VERSION_PATCH);
  oledDisplay.print(oledBuf);
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
  DebugTln(F("OLED: Display initialized (128x64 SSD1306Ascii)"));

  // Boot splash
  oledDisplay.clear();
  oledDisplay.set2X();
  oledDisplay.setRow(0);
  oledDisplay.setCol(16);
#if HAS_DIRECT_OT
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

  // Button ISR for page cycling
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), oledButtonISR, FALLING);
  DebugTf(PSTR("OLED: Button ISR on GPIO %d\r\n"), PIN_BUTTON);
}

// ---------------------------------------------------------------------------
// loopOLED - called from loop(), non-blocking
// ---------------------------------------------------------------------------
void loopOLED() {
  if (!oledPresent) return;

  uint32_t now = millis();

  // Handle button press (debounced)
  if (oledButtonPressed) {
    oledButtonPressed = false;
    if ((now - oledLastDebounce) >= OLED_DEBOUNCE_MS) {
      oledLastDebounce = now;
      oledLastActivity = now;

      oledPage = (oledPage + 1) % OLED_NUM_PAGES;
      if (oledPage == 0) {
        oledDisplay.ssd1306WriteCmd(0xAE);  // DISPLAYOFF
      } else {
        oledDisplay.ssd1306WriteCmd(0xAF);  // DISPLAYON
        oledLastRefresh = 0;  // force immediate redraw
      }
    }
  }

  // Auto-off after timeout
  if (oledPage != 0 && (now - oledLastActivity) >= OLED_TIMEOUT_MS) {
    oledDisplay.ssd1306WriteCmd(0xAE);  // DISPLAYOFF
    oledPage = 0;
    return;
  }

  if (oledPage == 0) return;

  // 1 Hz refresh
  if ((now - oledLastRefresh) < OLED_REFRESH_MS) return;
  oledLastRefresh = now;

  oledDisplay.clear();
  switch (oledPage) {
    case 1: drawPageDashboard(); break;
    case 2: drawPageHC1();       break;
    case 3: drawPageHC2();       break;
    case 4: drawPageSystem();    break;
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
  }
  oledLastActivity = millis();
}

#endif // HAS_OLED_CAPABLE

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
