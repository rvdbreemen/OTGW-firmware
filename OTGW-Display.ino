#include "OTGW-Display.h"
#include "Wire.h"

OTGW_Display::~OTGW_Display()
{
  if ( u8g2 ) {
    delete u8g2;
  }
}

void OTGW_Display::draw_welcome()
{

    u8g2->drawFrame(0,0,128,64);
    u8g2->sendBuffer();
}

OTGW_Display::OTGW_Display()
{
}

void OTGW_Display::begin()
{
  // Start looking for devices.
  Wire.beginTransmission(0x8d);
  int error = Wire.endTransmission();
  if ( error == 0 ) {
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C (U8G2_R0);

    // Set some defaults
    u8g2->setFont(u8g2_font_logisoso22_tf);
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

}

void OTGW_Display::tick()
{
  // If no display is available, lets not do anything.
  if ( u8g2 == nullptr ) {
    return;
  }
  u8g2->drawFrame(0,0,128,64);

  if ( _message.length() > 0 ) {
    draw_message ();
  } else {
    draw_main_screen ();
  }

  // Send towards screen.
  u8g2->sendBuffer();
}

void OTGW_Display::message(const String &str )
{
  _message = str;
  this->tick();
}
