/* 
***************************************************************************  
**  Program  : OTGW-Display.ino
**  Version  : v1.5.1-beta.4
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Original concept by Dave Davenport (PR #178)
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#include "OTGW-Display.h"

// I2C addresses to probe (most small OLEDs sit on 0x3C; some on 0x3D)
static const uint8_t OLED_ADDR_PRIMARY   = 0x3C;
static const uint8_t OLED_ADDR_SECONDARY = 0x3D;

// ─── Constructor / destructor ───────────────────────────────────────────────

OTGW_Display::OTGW_Display()
{
  _message[0] = '\0';
}

OTGW_Display::~OTGW_Display()
{
  delete u8g2;
}

// ─── begin() ────────────────────────────────────────────────────────────────

/**
 * Probe the I2C bus and, if a display is found, initialise U8g2.
 * Must be called after Wire.begin().  A missing display is silently skipped.
 */
void OTGW_Display::begin()
{
  if (!settings.display.bEnabled) {
    DebugTln(F("[display] disabled in settings, skipping"));
    return;
  }

  // Probe both common OLED addresses
  uint8_t foundAddr = 0;
  Wire.beginTransmission(OLED_ADDR_PRIMARY);
  if (Wire.endTransmission() == 0) {
    foundAddr = OLED_ADDR_PRIMARY;
  } else {
    Wire.beginTransmission(OLED_ADDR_SECONDARY);
    if (Wire.endTransmission() == 0) {
      foundAddr = OLED_ADDR_SECONDARY;
    }
  }

  if (foundAddr == 0) {
    DebugTln(F("[display] no I2C OLED found at 0x3C/0x3D"));
    return;
  }

  DebugTf(PSTR("[display] OLED detected at 0x%02X, type=%u\r\n"),
          foundAddr, settings.display.iType);

  // Create the appropriate U8G2 driver.
  // Full-buffer mode (suffix _F_) is used for simple, direct clearBuffer/sendBuffer calls.
  if (settings.display.iType == DISPLAY_TYPE_SH1106) {
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  } else {
    // Default: SSD1306
    u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  }

  if (!u8g2) {
    DebugTln(F("[display] ERROR: failed to allocate U8g2 object"));
    return;
  }

  u8g2->begin();
  u8g2->setPowerSave(0);
  // Lower contrast to extend OLED panel lifetime
  u8g2->setContrast(32);
  u8g2->setFont(u8g2_font_t0_14b_mr);
  u8g2->setFontRefHeightExtendedText();
  u8g2->setFontPosTop();
  u8g2->setFontDirection(0);
  u8g2->setDrawColor(1);

  draw_welcome();
  DebugTln(F("[display] initialised OK"));
}

// ─── tick() ─────────────────────────────────────────────────────────────────

/**
 * Advance to the next page and redraw.  Call every ~5 s.
 */
void OTGW_Display::tick()
{
  if (!u8g2) return;

  u8g2->clearBuffer();
  draw_chrome();

  switch (page) {
    case Pages::DISPLAY_IP:
      draw_display_page_ip();
      page = Pages::DISPLAY_TIME;
      break;
    case Pages::DISPLAY_TIME:
      draw_display_page_time();
      page = Pages::DISPLAY_UPTIME;
      break;
    case Pages::DISPLAY_UPTIME:
      draw_display_page_uptime();
      page = Pages::DISPLAY_VERSION;
      break;
    case Pages::DISPLAY_VERSION:
      draw_display_page_version();
      page = Pages::DISPLAY_IP;
      break;
    case Pages::DISPLAY_MESSAGE:
      draw_display_page_message();
      // Stay on message page until cleared
      break;
  }

  draw_status_column();
  u8g2->sendBuffer();
}

// ─── message() ──────────────────────────────────────────────────────────────

/**
 * Show a temporary full-screen message.  Pass "" to resume normal rotation.
 */
void OTGW_Display::message(const char *str)
{
  strlcpy(_message, str ? str : "", sizeof(_message));
  if (_message[0] != '\0') {
    page = Pages::DISPLAY_MESSAGE;
  } else {
    page = Pages::DISPLAY_IP;
  }
  tick(); // Force immediate redraw
}

// ─── Private drawing helpers ─────────────────────────────────────────────────

/**
 * Draw the outer border and the divider for the status column.
 * Content area: x=4..107, y=0..63.  Status column: x=109..127.
 */
void OTGW_Display::draw_chrome()
{
  u8g2->drawFrame(0, 0, 128, 64);
  u8g2->drawVLine(108, 0, 64);
}

/**
 * Draw boiler status in the right-hand 20×64 px column.
 * Shows "F" when the flame is active (slave status bit 3).
 * Shows modulation level as a vertical bar.
 */
void OTGW_Display::draw_status_column()
{
  // Flame status: bit 3 of slave status byte (OpenTherm spec, MsgID 0)
  bool flameOn = (OTcurrentSystemState.SlaveStatus & 0x08) != 0;

  u8g2->setFont(u8g2_font_t0_14b_mr);
  u8g2->drawStr(112, 4, flameOn ? "F" : "-");

  // Modulation bar: RelModLevel 0-100 % → 0-44 px bar height (bottom-up)
  float modPct = OTcurrentSystemState.RelModLevel;
  if (modPct < 0.0f) modPct = 0.0f;
  if (modPct > 100.0f) modPct = 100.0f;
  uint8_t barH = (uint8_t)(modPct * 44.0f / 100.0f);
  if (barH > 0) {
    u8g2->drawBox(111, 63 - barH, 14, barH);
  }

  // Reset font for content pages
  u8g2->setFont(u8g2_font_t0_14b_mr);
}

// ─── Page: welcome screen ────────────────────────────────────────────────────

void OTGW_Display::draw_welcome()
{
  u8g2->clearBuffer();
  u8g2->drawFrame(0, 0, 128, 64);
  u8g2->drawUTF8(4, 4, "OTGW booting..");
  char buf[20];
  snprintf_P(buf, sizeof(buf), PSTR("v%s"), _SEMVER_CORE);
  u8g2->drawUTF8(4, 22, buf);
  u8g2->sendBuffer();
}

// ─── Page: IP address ────────────────────────────────────────────────────────

void OTGW_Display::draw_display_page_ip()
{
  u8g2->drawUTF8(4, 4, "IP address");
  char buf[20];
  IPAddress ip = WiFi.localIP();
  snprintf_P(buf, sizeof(buf), PSTR("%d.%d.%d.%d"),
             ip[0], ip[1], ip[2], ip[3]);
  u8g2->drawUTF8(4, 24, buf);
}

// ─── Page: Date/Time ─────────────────────────────────────────────────────────

void OTGW_Display::draw_display_page_time()
{
  u8g2->drawUTF8(4, 4, "Date / Time");

  time_t now = time(nullptr);
  if (now < 946684800L) {
    // NTP not yet synced (before 2000-01-01)
    u8g2->drawUTF8(4, 24, "Syncing NTP..");
    return;
  }

  TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);

  char buf[20];
  snprintf_P(buf, sizeof(buf), PSTR("%04d-%02d-%02d"),
             myTime.year(), myTime.month(), myTime.day());
  u8g2->drawUTF8(4, 24, buf);

  snprintf_P(buf, sizeof(buf), PSTR("%02d:%02d:%02d"),
             myTime.hour(), myTime.minute(), myTime.second());
  u8g2->drawUTF8(4, 44, buf);
}

// ─── Page: Uptime ────────────────────────────────────────────────────────────

void OTGW_Display::draw_display_page_uptime()
{
  u8g2->drawUTF8(4, 4, "Uptime");

  uint32_t secs  = state.uptime.iSeconds;
  uint32_t days  = secs / 86400UL; secs -= days * 86400UL;
  uint16_t hours = secs / 3600U;   secs -= hours * 3600U;
  uint16_t mins  = secs / 60U;     secs -= mins * 60U;

  char buf[20];
  snprintf_P(buf, sizeof(buf), PSTR("%3ud %02u:%02u:%02u"),
             (unsigned)days, (unsigned)hours, (unsigned)mins, (unsigned)secs);
  u8g2->drawUTF8(4, 24, buf);
}

// ─── Page: Firmware version ───────────────────────────────────────────────────

void OTGW_Display::draw_display_page_version()
{
  u8g2->drawUTF8(4, 4, "OTGW version");
  char buf[20];
  snprintf_P(buf, sizeof(buf), PSTR("%s"), _SEMVER_CORE);
  u8g2->drawUTF8(4, 24, buf);
}

// ─── Page: Message ───────────────────────────────────────────────────────────

void OTGW_Display::draw_display_page_message()
{
  // _message is already clipped to 63 chars; just render it.
  u8g2->drawUTF8(4, 4, _message);
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
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/
