#include "OTGW-Display.h"
#include "ESP8266WiFi.h"
#include "Wire.h"
#include "clib/u8g2.h"
#include "OTGW-Core.h"

/**
 * Deconstructor.
 */
OTGW_Display::~OTGW_Display()
{
  if ( u8g2 ) {
    delete u8g2;
  }
}

/**
 * Constructor
 */
OTGW_Display::OTGW_Display()
{
}

/**
 * This sets up the display.
 */
void OTGW_Display::begin()
{
  /**
   * Try to auto-detect if a display is connected
   * to the iic connection.
   */
  // Check 0x3c for a small oled screen.
  Wire.beginTransmission(0x3c);
  int error = Wire.endTransmission();
  if ( error == 0 ) {
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C (U8G2_R0);
  } else {
    // No display found.
  }
  // TODO: Add and test with more displays. u8g2 supports many.


  // If we found a display, set it up and configure some defaults.
  if ( u8g2 != nullptr ) {
    // Initialize u8g2
    u8g2->beginSimple();
    // Disable power save.
    u8g2->setPowerSave(0);
    // Lower the contrast, this will extend the lifetime of the
    // OLED screen.
    u8g2->setContrast(1);

    // Set some defaults:
    // Font
    u8g2->setFont(u8g2_font_t0_14b_mr);
    u8g2->setFontRefHeightExtendedText();
    u8g2->setFontPosTop();
    u8g2->setFontDirection(0);
    // Set drawing color
    u8g2->setDrawColor(1);
    // Draw the welcome screen.
    // This will be redrawn once the ::tick() is called.
    draw_welcome ();
  }
}

/**
 * Draw the welcome screen.
 */
void OTGW_Display::draw_welcome()
{
  u8g2->drawFrame(0,0,128,64);
  u8g2->drawUTF8(4,7, "OTGW Booting..");
  char buf[16];
  snprintf(buf, sizeof(buf), "Version %s", _VERSION );
  u8g2->drawUTF8(4,24, buf );
  u8g2->sendBuffer();
}

/**
 * Draw the message page.
 * ::Pages::DISPLAY_MESSAGE
 */
void OTGW_Display::draw_display_page_message()
{
  // Clean out old buffer, and reset the frame. 
  u8g2->clearBuffer();
  u8g2->drawFrame(0,0,128,64);
  u8g2->drawLine(108, 0,108, 64);
  u8g2->drawUTF8(4,7, _message.c_str());
}

/**
 * Draw the ip address page.
 * ::Pages::DISPLAY_IP
 */
void OTGW_Display::draw_display_page_ip()
{
  char buf[32];
  snprintf(buf, sizeof(buf), "IP");
  u8g2->drawUTF8(4,4, buf);
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  u8g2->drawUTF8(4,24, buf);
}

/**
 * Draw the time page.
 * ::Pages::DISPLAY_TIME
 */
void OTGW_Display::draw_display_page_time()
{

  char buf[50];
  snprintf(buf, sizeof(buf), "Time");
  u8g2->drawUTF8(4,4, buf);
  time_t now = time(nullptr);
  //Timezone based devtime
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  snprintf(buf, 49, PSTR("%04d-%02d-%02d"), myTime.year(), myTime.month(), myTime.day());

  u8g2->drawUTF8(4,24, buf);
  snprintf(buf, 49, PSTR("%02d:%02d:%02d"), myTime.hour(), myTime.minute(), myTime.second());

  u8g2->drawUTF8(4,40, buf);
}

/**
 * Draw the uptime page.
 * ::Pages::DISPLAY_UPTIME
 */
void OTGW_Display::draw_display_page_uptime()
{

  char buf[50];
  snprintf(buf, sizeof(buf), "Uptime");
  u8g2->drawUTF8(4,4, buf);
  uint32_t seconds= upTimeSeconds;
  uint32_t days = seconds/(60*60*24); 
  seconds -= days*60*60*24;
  uint16_t hours = seconds/(60*60);
  seconds -= hours*60*60;
  uint16_t minutes = seconds/(60);
  seconds -= minutes*60;
  snprintf(buf, 49, PSTR("%03dd %02d:%02d:%02d"),
      days, hours, minutes,seconds);

  u8g2->drawUTF8(4,24, buf);
}

/**
 * Draw the version page.
 * ::Pages::DISPLAY_VERSION
 */
void OTGW_Display::draw_display_page_version()
{
  u8g2->drawUTF8(4,4, "OTGW version");
  char buf[12];
  snprintf(buf, sizeof(buf), "%s", _VERSION);
  u8g2->drawUTF8(4,24, buf);
}

/**
 * Periodic update tick function.
 * This will update the page and if needed switch to the next one.
 */
void OTGW_Display::tick()
{
  // If no display is available, lets not do anything.
  if ( u8g2 == nullptr ) {
    return;
  }
  // Draw border around the screen.
  u8g2->clearBuffer();
  u8g2->drawFrame(0,0,128,64);
  u8g2->drawLine(108, 0,108, 64);

  switch ( page ) {
    case OTGW_Display::Pages::DISPLAY_IP:
      draw_display_page_ip ();
      page = OTGW_Display::Pages::DISPLAY_TIME;
      break;
    case OTGW_Display::Pages::DISPLAY_TIME:
      draw_display_page_time ();
      page = OTGW_Display::Pages::DISPLAY_UPTIME;
      break;
    case OTGW_Display::Pages::DISPLAY_UPTIME:
      draw_display_page_uptime ();
      page = OTGW_Display::Pages::DISPLAY_VERSION;
      break;
    case OTGW_Display::Pages::DISPLAY_VERSION:
      draw_display_page_version ();
      page = OTGW_Display::Pages::DISPLAY_IP;
      break;
    case OTGW_Display::Pages::DISPLAY_MESSAGE:
      draw_display_page_message ();
      page = OTGW_Display::Pages::DISPLAY_MESSAGE;
      break;
  }
  if ( ((OTdata.valueLB) & 0x08) ) {
    u8g2->drawUTF8( 112, 2, "F");
  } else {
    u8g2->drawUTF8( 112, 2, "X");
  }

  // Send internal buffer towards screen.
  u8g2->sendBuffer();
}

/**
 * Set a message that override the current page.
 */
void OTGW_Display::message(const String &str )
{
  _message = str;
  if ( _message.length() > 0 ) {
    page = OTGW_Display::Pages::DISPLAY_MESSAGE;
  } else {
    page = OTGW_Display::Pages::DISPLAY_VERSION;
  }
  // Force a page reload. Showing the message.
  this->tick();
}
