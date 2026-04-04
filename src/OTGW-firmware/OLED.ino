/*
***************************************************************************
**  Program  : OLED.ino
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  OLED display module for OTGW-firmware.
**  Works on both ESP8266 (PIC-based OTGW) and ESP32-S3 (OTGW32).
**  Drives a 128x64 SSD1306 I2C OLED display with runtime detection.
**  Shows OpenTherm status, temperatures, and system info on 4 pages
**  cycled via the boot/config button.
**
**  Design:
**  - Runtime I2C probe at 0x3C — if no display, all code is skipped
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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Stringification helper for _VERSION_PRERELEASE (which is a bare token, not quoted)
#define OLED_STR_HELPER(x) #x
#define OLED_STR(x)        OLED_STR_HELPER(x)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t  OLED_ADDR         = 0x3C;
static constexpr uint8_t  OLED_WIDTH        = 128;
static constexpr uint8_t  OLED_HEIGHT       = 64;
static constexpr uint32_t OLED_REFRESH_MS   = 1000;   // 1 Hz refresh
static constexpr uint32_t OLED_TIMEOUT_MS   = 30000;  // auto-off after 30s
static constexpr uint8_t  OLED_NUM_PAGES    = 5;      // 0=off, 1-4=content pages
static constexpr uint32_t OLED_DEBOUNCE_MS  = 300;

// 16x16 flame icon (PROGMEM) — shown on boot splash and when flame is on
static const uint8_t oled_flame_bmp[] PROGMEM = {
  0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x03, 0x80,
  0x07, 0x80, 0x0f, 0xc0, 0x1f, 0xe0, 0x3f, 0xf0,
  0x3f, 0xf0, 0x7f, 0xf8, 0x77, 0xf8, 0x73, 0xf8,
  0xff, 0xf8, 0xff, 0xf8, 0x3f, 0xf0, 0x1f, 0xe0
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Adafruit_SSD1306 oledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool     oledPresent       = false;
static uint8_t  oledPage          = 0;     // 0=off, 1-4=content
static uint32_t oledLastActivity  = 0;     // millis of last button press
static uint32_t oledLastRefresh   = 0;     // millis of last screen draw
static volatile bool oledButtonPressed = false;
static uint32_t oledLastDebounce  = 0;

// ---------------------------------------------------------------------------
// Button ISR — sets flag for main loop
// ---------------------------------------------------------------------------
static void IRAM_ATTR oledButtonISR() {
  oledButtonPressed = true;
}

// ---------------------------------------------------------------------------
// probeOLED — check if SSD1306 responds on I2C
// ---------------------------------------------------------------------------
static bool probeOLED() {
  Wire.beginTransmission(OLED_ADDR);
  return (Wire.endTransmission() == 0);
}

// ---------------------------------------------------------------------------
// drawHeader — common header line with title and separator
// ---------------------------------------------------------------------------
static void drawHeader(const char* title) {
  oledDisplay.setTextSize(1);
  oledDisplay.setTextColor(SSD1306_WHITE);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print(title);
  // Page indicator on the right
  oledDisplay.setCursor(110, 0);
  oledDisplay.print(oledPage);
  oledDisplay.print('/');
  oledDisplay.print(OLED_NUM_PAGES - 1);
  oledDisplay.drawLine(0, 9, OLED_WIDTH - 1, 9, SSD1306_WHITE);
}

// ---------------------------------------------------------------------------
// Page 1: Dashboard — key temperatures + flame/CH/DHW status
// ---------------------------------------------------------------------------
static void drawPageDashboard() {
  drawHeader("Dashboard");
  char buf[22];

  bool flameOn  = (OTcurrentSystemState.SlaveStatus & 0x08) != 0;
  bool chActive = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;
  bool dhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0;

  // Row 1: Flame icon + modulation, CH/DHW status
  oledDisplay.setCursor(0, 12);
  if (flameOn) {
    oledDisplay.drawBitmap(0, 12, oled_flame_bmp, 16, 16, SSD1306_WHITE);
    oledDisplay.setCursor(20, 14);
    snprintf_P(buf, sizeof(buf), PSTR("Mod: %.0f%%"), OTcurrentSystemState.RelModLevel);
    oledDisplay.print(buf);
  } else {
    oledDisplay.print(F("Flame: off"));
  }
  oledDisplay.setCursor(86, 14);
  if (chActive) oledDisplay.print(F("CH"));
  if (chActive && dhwActive) oledDisplay.print('/');
  if (dhwActive) oledDisplay.print(F("DHW"));

  // Row 2: Setpoint and boiler temp
  oledDisplay.setCursor(0, 30);
  snprintf_P(buf, sizeof(buf), PSTR("Set:%.1f  Boil:%.1f"), OTcurrentSystemState.TSet, OTcurrentSystemState.Tboiler);
  oledDisplay.print(buf);

  // Row 3: Return and DHW temp
  oledDisplay.setCursor(0, 40);
  snprintf_P(buf, sizeof(buf), PSTR("Ret:%.1f  DHW :%.1f"), OTcurrentSystemState.Tret, OTcurrentSystemState.Tdhw);
  oledDisplay.print(buf);

  // Row 4: Pressure and outside temp
  oledDisplay.setCursor(0, 50);
  snprintf_P(buf, sizeof(buf), PSTR("Bar:%.1f  Out :%.1f"), OTcurrentSystemState.CHPressure, OTcurrentSystemState.Toutside);
  oledDisplay.print(buf);
}

// ---------------------------------------------------------------------------
// Page 2: Heating Circuit 1
// ---------------------------------------------------------------------------
static void drawPageHC1() {
  drawHeader("Heating Circuit 1");
  char buf[22];

  oledDisplay.setCursor(0, 14);
  snprintf_P(buf, sizeof(buf), PSTR("Room setp: %.1f C"), OTcurrentSystemState.TrSet);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 26);
  snprintf_P(buf, sizeof(buf), PSTR("Room temp: %.1f C"), OTcurrentSystemState.Tr);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 38);
  snprintf_P(buf, sizeof(buf), PSTR("Flow temp: %.1f C"), OTcurrentSystemState.Tboiler);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 50);
  snprintf_P(buf, sizeof(buf), PSTR("Ret. temp: %.1f C"), OTcurrentSystemState.Tret);
  oledDisplay.print(buf);
}

// ---------------------------------------------------------------------------
// Page 3: Heating Circuit 2
// ---------------------------------------------------------------------------
static void drawPageHC2() {
  drawHeader("Heating Circuit 2");
  char buf[22];

  oledDisplay.setCursor(0, 14);
  snprintf_P(buf, sizeof(buf), PSTR("Room setp: %.1f C"), OTcurrentSystemState.TrSetCH2);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 26);
  snprintf_P(buf, sizeof(buf), PSTR("Room temp: %.1f C"), OTcurrentSystemState.TRoomCH2);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 38);
  snprintf_P(buf, sizeof(buf), PSTR("Flow temp: %.1f C"), OTcurrentSystemState.TflowCH2);
  oledDisplay.print(buf);

  oledDisplay.setCursor(0, 50);
  snprintf_P(buf, sizeof(buf), PSTR("DHW  temp: %.1f C"), OTcurrentSystemState.Tdhw);
  oledDisplay.print(buf);
}

// ---------------------------------------------------------------------------
// Page 4: System Status
// ---------------------------------------------------------------------------
static void drawPageSystem() {
  drawHeader("System Status");

  // Network
  oledDisplay.setCursor(0, 14);
  if (isNetworkUp()) {
    oledDisplay.print(networkModeName());
    oledDisplay.print(F(": "));
    if (state.net.eMode == NET_ETHERNET) {
      oledDisplay.print(F("Wired"));
    } else {
      String ssid = WiFi.SSID();
      if (ssid.length() > 14) ssid = ssid.substring(0, 14);
      oledDisplay.print(ssid);
    }
    oledDisplay.setCursor(0, 24);
    oledDisplay.print(F("IP: "));
    oledDisplay.print(getActiveIP());
  } else {
    oledDisplay.print(F("Network: offline"));
  }

  // MQTT
  oledDisplay.setCursor(0, 36);
  oledDisplay.print(F("MQTT: "));
  oledDisplay.print(state.mqtt.bConnected ? F("connected") : F("offline"));

  // OT bus
  oledDisplay.setCursor(0, 46);
  oledDisplay.print(F("OT bus: "));
  oledDisplay.print(state.otgw.bOnline ? F("online") : F("offline"));

  // Firmware version
  oledDisplay.setCursor(0, 56);
  char buf[22];
  snprintf_P(buf, sizeof(buf), PSTR("FW %d.%d.%d-" OLED_STR(_VERSION_PRERELEASE)),
    _VERSION_MAJOR, _VERSION_MINOR, _VERSION_PATCH);
  oledDisplay.print(buf);
}

// ---------------------------------------------------------------------------
// initOLED — called from setup(), after Wire.begin()
// ---------------------------------------------------------------------------
void initOLED() {
  if (!probeOLED()) {
    DebugTln(F("OLED: No display at 0x3C — disabled"));
    oledPresent = false;
    return;
  }

  if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    DebugTln(F("OLED: SSD1306 init failed — disabled"));
    oledPresent = false;
    return;
  }

  oledPresent = true;
  oledPage = 1;
  oledLastActivity = millis();
  DebugTln(F("OLED: Display initialized (128x64 SSD1306)"));

  // Boot splash
  oledDisplay.clearDisplay();
  oledDisplay.setTextSize(2);
  oledDisplay.setTextColor(SSD1306_WHITE);
  oledDisplay.setCursor(16, 0);
#if HAS_DIRECT_OT
  oledDisplay.println(F("OTGW32"));
#else
  oledDisplay.println(F("OTGW"));
#endif
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 22);
  oledDisplay.println(F("OpenTherm Gateway"));
  oledDisplay.setCursor(0, 36);
  oledDisplay.println(F("Press button for info"));
  oledDisplay.drawBitmap(56, 46, oled_flame_bmp, 16, 16, SSD1306_WHITE);
  oledDisplay.display();

  // Button ISR for page cycling
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), oledButtonISR, FALLING);
  DebugTf(PSTR("OLED: Button ISR on GPIO %d\r\n"), PIN_BUTTON);
}

// ---------------------------------------------------------------------------
// loopOLED — called from loop(), non-blocking
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
        oledDisplay.ssd1306_command(SSD1306_DISPLAYOFF);
      } else {
        oledDisplay.ssd1306_command(SSD1306_DISPLAYON);
        oledLastRefresh = 0;  // force immediate redraw
      }
    }
  }

  // Auto-off after timeout
  if (oledPage != 0 && (now - oledLastActivity) >= OLED_TIMEOUT_MS) {
    oledDisplay.ssd1306_command(SSD1306_DISPLAYOFF);
    oledPage = 0;
    return;
  }

  if (oledPage == 0) return;

  // 1 Hz refresh
  if ((now - oledLastRefresh) < OLED_REFRESH_MS) return;
  oledLastRefresh = now;

  oledDisplay.clearDisplay();
  switch (oledPage) {
    case 1: drawPageDashboard(); break;
    case 2: drawPageHC1();       break;
    case 3: drawPageHC2();       break;
    case 4: drawPageSystem();    break;
  }
  oledDisplay.display();
}

// ---------------------------------------------------------------------------
// oledWake — external call to wake display (e.g. on network state change)
// ---------------------------------------------------------------------------
void oledWake() {
  if (!oledPresent) return;
  if (oledPage == 0) {
    oledPage = 1;
    oledDisplay.ssd1306_command(SSD1306_DISPLAYON);
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
