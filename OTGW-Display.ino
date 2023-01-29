#include "OTGW-Display.h"
#include "ESP8266WiFi.h"
#include "Wire.h"
#include "Debug.h"
#include "clib/u8g2.h"

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
    u8g2->drawUTF8(4,20, buf );
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
    u8g2->setFont(u8g2_font_cu12_tf);
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

void OTGW_Display::draw_main_screen()
{
	char buf[32];
	snprintf(buf, sizeof(buf), "IP: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  u8g2->drawUTF8(4,7, buf);
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

  if ( _message.length() > 0 ) {
    draw_message ();
  } else {
    draw_main_screen ();
  }

  // Send internal buffer towards screen.
  u8g2->sendBuffer();
}

void OTGW_Display::message(const String &str )
{
  _message = str;
  this->tick();
}
