#include "OTGW-Display.h"
#include "ESP8266WiFi.h"
#include "Wire.h"
#include "Debug.h"
#include "clib/u8g2.h"
#include "OTGW-Core.h"

OTGW_Display::~OTGW_Display()
{
  if ( u8g2 ) {
    delete u8g2;
  }
}
void OTGW_Display::draw_welcome()
{

    u8g2->drawFrame(0,0,128,64);
    u8g2->drawUTF8(4,7, "OTGW Booting..");
    char buf[16];
    snprintf(buf, sizeof(buf), "Version %s", _VERSION );
    u8g2->drawUTF8(4,24, buf );
    u8g2->sendBuffer();
}

OTGW_Display::OTGW_Display()
{
}

void OTGW_Display::begin()
{
  Wire.beginTransmission(0x3c);
  int error = Wire.endTransmission();
  if ( error == 0 ) {
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C (U8G2_R0);
  } else {
    // No display found.
  }


  if ( u8g2 != nullptr ) {
    // Initialize u8g2
    u8g2->beginSimple();
    // Disable power save.
    u8g2->setPowerSave(0);
    // Lower the contrast, this will extend the lifetime of the
    // OLED screen.
    u8g2->setContrast(1);

    // Set some defaults
    u8g2->setFont(u8g2_font_t0_14b_mr);
    u8g2->setFontRefHeightExtendedText();
    u8g2->setDrawColor(1);
    u8g2->setFontPosTop();
    u8g2->setFontDirection(0);
    draw_welcome ();
  }
}

void OTGW_Display::draw_message()
{
  u8g2->drawUTF8(4,7, _message.c_str());

}

void OTGW_Display::draw_display_ip()
{
	char buf[32];
	snprintf(buf, sizeof(buf), "IP");
  u8g2->drawUTF8(4,4, buf);
	snprintf(buf, sizeof(buf), "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  u8g2->drawUTF8(4,24, buf);
}

void OTGW_Display::draw_display_time()
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

void OTGW_Display::draw_display_uptime()
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
void OTGW_Display::draw_display_version()
{
  u8g2->drawUTF8(4,4, "OTGW version");
  char buf[12];
  snprintf(buf, sizeof(buf), "%s", _VERSION);
  u8g2->drawUTF8(4,24, buf);
}


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

  if ( _message.length() > 0 ) {
    draw_message ();
  } else {
    
    switch ( page ) {
      case OTGW_Display::Pages::DISPLAY_IP:
        draw_display_ip ();
        page = OTGW_Display::Pages::DISPLAY_TIME;
        break;
      case OTGW_Display::Pages::DISPLAY_TIME:
        draw_display_time ();
        page = OTGW_Display::Pages::DISPLAY_UPTIME;
        break;
      case OTGW_Display::Pages::DISPLAY_UPTIME:
        draw_display_uptime ();
        page = OTGW_Display::Pages::DISPLAY_VERSION;
        break;
      case OTGW_Display::Pages::DISPLAY_VERSION:
        draw_display_version ();
        page = OTGW_Display::Pages::DISPLAY_IP;
        break;
    }
  }
  if ( ((OTdata.valueLB) & 0x08) ) {
    u8g2->drawUTF8( 112, 2, "F");
  } else {
    u8g2->drawUTF8( 112, 2, "X");
  }

  // Send internal buffer towards screen.
  u8g2->sendBuffer();
}

void OTGW_Display::message(const String &str )
{
  _message = str;
  this->tick();
}
