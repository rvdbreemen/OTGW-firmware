/*
***************************************************************************
**  Program  : OLED.ino
**  Version  : v2.0.0-alpha.324
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
**  The boot-splash flame is drawn via raw GDDRAM column writes
**  (ssd1306WriteRam), so a 16x16 bitmap needs no framebuffer.
**
**  Design:
**  - Runtime I2C probe at 0x3C - if no display, all code is skipped
**  - Reads values directly from OTcurrentSystemState (no JSON layer)
**  - Button on PIN_BUTTON (GPIO 0) polled in loopOLED() with debounce
**    (portable: works on both ESP8266 and ESP32, TASK-758)
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

#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Stringification helper for _VERSION_PRERELEASE (which is a bare token, not quoted)
#define OLED_STR_HELPER(x) #x
#define OLED_STR(x)        OLED_STR_HELPER(x)

// Button pin: boot button (GPIO 0) on OTGW32, D3-hole (GPIO 18) on the
// Classic-on-S3 boards. Active LOW, pull-up. The BUTTON alias resolves at
// runtime on the combo board (ADR-127) and to PIN_BUTTON everywhere else.
#define OLED_BUTTON_PIN BUTTON

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t  OLED_ADDR         = 0x3C;
static constexpr uint32_t OLED_REFRESH_MS   = 5000;   // 5 s refresh (readable, low I2C load)
static constexpr uint32_t OLED_TIMEOUT_MS   = 30000;  // auto-off after 30s
static constexpr uint8_t  OLED_NUM_PAGES    = 6;      // 0=off, 1-5=content pages
static constexpr uint32_t OLED_DEBOUNCE_MS  = 300;    // 300 ms button debounce

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
static uint8_t  oledLastBtnLevel  = HIGH;  // last sampled button level (active LOW)
static uint32_t oledLastBtnEdge   = 0;     // millis of last accepted button edge

// Shared formatting buffer (small, on module level to avoid per-function stack cost)
static char oledBuf[22];

// ---------------------------------------------------------------------------
// Boot-splash flame icon — 16x16, column-major, 2 pages of 16 bytes.
// SSD1306Ascii has no framebuffer / drawBitmap, but ssd1306WriteRam() writes a
// raw GDDRAM column byte (8 vertical px, LSB = top of page) at the current
// cursor and advances the column. Two such page rows render a 16x16 bitmap with
// zero extra RAM — the bytes live in flash (PROGMEM). Layout generated offline
// and round-trip verified. (TASK-750 AC#2)
// ---------------------------------------------------------------------------
static const uint8_t FLAME16[32] PROGMEM = {
  // page 0 (top 8 rows), columns 0..15
  0x00, 0x00, 0x80, 0xC0, 0x60, 0x38, 0x9E, 0x9F, 0x9F, 0x30, 0x60, 0xC0, 0x80, 0x00, 0x00, 0x00,
  // page 1 (bottom 8 rows), columns 0..15
  0x00, 0x0E, 0x1F, 0x30, 0x66, 0xEF, 0xCF, 0xCF, 0xCF, 0xEF, 0x66, 0x30, 0x1F, 0x0E, 0x00, 0x00,
};

// Draw the 16x16 flame with its top-left at pixel column `col`, text-row `row`.
static void oledDrawFlame(uint8_t col, uint8_t row) {
  for (uint8_t p = 0; p < 2; p++) {
    oledDisplay.setCursor(col, row + p);
    for (uint8_t c = 0; c < 16; c++) {
      oledDisplay.ssd1306WriteRam(pgm_read_byte(&FLAME16[p * 16 + c]));
    }
  }
}

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
// Config "how to connect" screen — shared by the AP-fallback page
// (drawPageConfig) and the one-shot config-portal screen (oledShowConfigMode).
// drawConfigHeader draws the title + separator; drawConfigBody renders
// SSID / optional password / setup URL. In drawConfigBody, url and pw may be
// nullptr (or empty) to fall back to defaults / omit the password line.
// ---------------------------------------------------------------------------
static void drawConfigHeader() {
  // Like drawHeader() but without the page counter — the config screen is not
  // part of the button-cycled page set, so "n/5" would be misleading here.
  oledDisplay.setRow(0);
  oledDisplay.setCol(0);
  oledDisplay.print(F("WiFi Setup"));
  oledDisplay.setRow(1);
  oledDisplay.setCol(0);
  for (uint8_t i = 0; i < OLED_COLS; i++) oledDisplay.print('-');
}

static void drawConfigBody(const char* ssid, const char* pw, const char* url) {
  oledDisplay.setRow(2);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Connect to WiFi:"));

  oledDisplay.setRow(3);
  oledDisplay.setCol(0);
  oledDisplay.print(F("SSID: "));
  if (ssid && ssid[0]) {
    char ssidBuf[OLED_COLS + 1];
    strlcpy(ssidBuf, ssid, sizeof(ssidBuf));
    oledDisplay.print(ssidBuf);
  } else {
    oledDisplay.print(F("---"));
  }

  uint8_t row = 4;
  if (pw && pw[0]) {
    oledDisplay.setRow(row++);
    oledDisplay.setCol(0);
    oledDisplay.print(F("PW:   "));
    char pwBuf[OLED_COLS + 1];
    strlcpy(pwBuf, pw, sizeof(pwBuf));
    oledDisplay.print(pwBuf);
  }

  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.print(F("Then open:"));
  oledDisplay.setRow(7);
  oledDisplay.setCol(0);
  if (url && url[0]) {
    char urlBuf[OLED_COLS + 1];
    strlcpy(urlBuf, url, sizeof(urlBuf));
    oledDisplay.print(urlBuf);
  } else {
    oledDisplay.print(F("http://192.168.4.1/"));
  }
}

#if defined(_VERSION_PRERELEASE)
// ---------------------------------------------------------------------------
// drawPageConfig - WiFi-setup / AP-fallback screen (BETA AP fallback only).
// Shown as the forced first page while state.net.bAPFallback is active so a
// freshly-booted, WiFi-less device tells the user how to reach the web UI.
// SSID comes from state.net.sAPSSID; password and IP mirror startAPFallback()
// in networkStuff.ino ("otgw1234" / 192.168.4.1) — kept in sync by hand.
// ---------------------------------------------------------------------------
static void drawPageConfig() {
  drawConfigHeader();
  drawConfigBody(state.net.sAPSSID, "otgw1234", "http://192.168.4.1/");
}
#endif

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
  snprintf_P(oledBuf, sizeof(oledBuf), PSTR("Heap: %u B"), (unsigned)platformFreeHeap());
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

  // Boot splash: a real 16x16 flame bitmap (top-centre) over the title and
  // setup hints. The flame is drawn via raw GDDRAM column writes (oledDrawFlame),
  // so it costs no framebuffer RAM (TASK-750 AC#2).
  oledDisplay.clear();
  oledDrawFlame(56, 0);                 // 16x16 flame centred on rows 0-1 (col 56..71)

  oledDisplay.set2X();
  oledDisplay.setRow(2);
  // ADR-125: name follows the boot-detected mode (set before initOLED()).
  // isOTDirectEnabled() is compile-time false on the fixed ESP8266 and true on
  // the fixed OTGW32, so the fixed boards render exactly as before.
  if (isOTDirectEnabled()) {
    oledDisplay.setCol(28);             // 2X "OTGW32" = 72px, centred on 128px
    oledDisplay.print(F("OTGW32"));
  } else {
    oledDisplay.setCol(40);             // 2X "OTGW" = 48px, centred
    oledDisplay.print(F("OTGW"));
  }
  oledDisplay.set1X();
  oledDisplay.setRow(4);
  oledDisplay.setCol(13);               // "OpenTherm Gateway" = 102px, centred
  oledDisplay.print(F("OpenTherm Gateway"));
  oledDisplay.setRow(6);
  oledDisplay.setCol(0);
  oledDisplay.println(F("Press button for info"));
  oledDisplay.setRow(7);
  oledDisplay.setCol(0);
  // Our WiFi wipe is a triple-reset (shouldForceWifiConfigPortal), not a
  // reset-hold, so the hint must name the right gesture for this firmware.
  oledDisplay.print(F("Triple-reset = config"));

  // Button for page cycling — polled in loopOLED() with a millis() debounce.
  // Portable Arduino API (no ISR / FreeRTOS queue), so it works identically on
  // ESP8266 and ESP32 (TASK-758). The boot button is active LOW with a pull-up.
  pinMode(OLED_BUTTON_PIN, INPUT_PULLUP);
  oledLastBtnLevel = digitalRead(OLED_BUTTON_PIN);
  DebugTf(PSTR("OLED: Button on GPIO %d (polled, debounced)\r\n"), OLED_BUTTON_PIN);
}

// ---------------------------------------------------------------------------
// loopOLED - called from loop(), non-blocking
// ---------------------------------------------------------------------------
void loopOLED() {
  if (!oledPresent) return;

  uint32_t now = millis();

  // ----- Button poll w/ debounce (HIGH->LOW edge = fresh press) -----
  // Portable replacement for the former ESP32-only ISR+queue (TASK-758): poll
  // the pin at loop() cadence and accept a falling edge only once per debounce
  // window. The page cycle is not latency-critical, so polling is reliable.
  uint8_t btnLevel = digitalRead(OLED_BUTTON_PIN);
  if (btnLevel == LOW && oledLastBtnLevel == HIGH &&
      (now - oledLastBtnEdge) >= OLED_DEBOUNCE_MS) {
    oledLastBtnEdge = now;
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
  oledLastBtnLevel = btnLevel;

  // ----- SSID cache maintenance (avoid String in hot path, ADR-004) -----
  if (WiFi.isConnected()) {
    if (oledCachedSSID[0] == '\0') {
      strlcpy(oledCachedSSID, WiFi.SSID().c_str(), sizeof(oledCachedSSID));
    }
  } else {
    oledCachedSSID[0] = '\0';
  }

  // Auto-off after timeout. Suppressed during AP fallback: the "how to connect"
  // screen must stay visible for a user who walks up long after boot.
  bool keepAwake = false;
#if defined(_VERSION_PRERELEASE)
  keepAwake = state.net.bAPFallback;
  // If AP fallback engaged while the display was off, wake it so the user sees
  // the "how to connect" screen without having to press the button.
  if (keepAwake && oledPage == 0) {
    oledPage = 1;
    oledDisplay.ssd1306WriteCmd(0xAF);  // DISPLAYON
    oledLastRefresh = 0;                // force immediate redraw
  }
#endif
  if (!keepAwake && oledPage != 0 && (now - oledLastActivity) >= OLED_TIMEOUT_MS) {
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

#if defined(_VERSION_PRERELEASE)
  // While the BETA AP fallback is active there is no usable network/OT data;
  // always show the "how to connect" screen so the user can reach the web UI.
  if (state.net.bAPFallback) {
    drawPageConfig();
    return;
  }
#endif

  switch (oledPage) {
    case 1: drawPageNetwork();   break;
    case 2: drawPageOTStatus();  break;
    case 3: drawPageDevice();    break;
    case 4: drawPageDashboard(); break;
    case 5: drawPageHeating();   break;
  }
}

// ---------------------------------------------------------------------------
// oledShowConfigMode - one-shot "how to connect" screen for the blocking
// WiFiManager config portal (first boot / triple-reset). That portal runs its
// own internal loop and never returns to loopOLED(), so the main thread must
// call this synchronously from configModeCallback() to paint the screen once.
// pw may be nullptr (the WiFiManager portal AP is open / passwordless).
// Safe no-op if the display is absent.
// ---------------------------------------------------------------------------
void oledShowConfigMode(const char* ssid, const char* pw, const char* url) {
  if (!oledPresent) return;
  oledDisplay.ssd1306WriteCmd(0xAF);  // ensure display is on
  oledDisplay.clear();
  drawConfigHeader();
  drawConfigBody(ssid, pw, url);
  // Keep this screen visible: mark it active and pin activity so the auto-off
  // timer does not blank it while the user is reading it during config.
  oledPage = 1;
  oledLastRenderedPage = 0xFF;  // force a clean redraw once loopOLED resumes
  oledLastActivity = millis();
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
