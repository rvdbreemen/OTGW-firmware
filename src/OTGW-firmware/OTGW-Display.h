/* 
***************************************************************************  
**  Program  : OTGW-Display.h
**  Version  : v1.5.1-beta.4
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Original concept by Dave Davenport (PR #178)
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
**
**  A small module that drives an optional I2C OLED display connected to
**  the GPIO header (SDA=D2, SCL=D1) of the NodoShop OTGW board.
**
**  Supported controllers: SSD1306 128×64 (default), SH1106 128×64.
**  The display is optional — if nothing is detected on I2C address 0x3C
**  or 0x3D the module does nothing.
**
**  Display rotates through pages every tick() call:
**    IP address → Date/Time → Uptime → Version → (repeat)
**
**  Enable via settings.display.bEnabled = true.
**  Controller type via settings.display.iType (0=SSD1306, 1=SH1106).
***************************************************************************
*/

#ifndef OTGW_DISPLAY_H
#define OTGW_DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>

// Display type constants — matches settings.display.iType
#define DISPLAY_TYPE_SSD1306  0
#define DISPLAY_TYPE_SH1106   1

/**
 * OTGW_Display — thin wrapper around U8g2 for the OTGW OLED display.
 *
 * Usage:
 *   OTGW_Display display;
 *   display.begin();       // call after Wire.begin() and watchdog init
 *   display.tick();        // call periodically (every ~5 s) to rotate pages
 *   display.message("…");  // show a temporary message; clear with message("")
 */
class OTGW_Display
{
  public:
    // Pages cycled through by tick()
    enum class Pages : uint8_t {
      DISPLAY_IP,
      DISPLAY_TIME,
      DISPLAY_UPTIME,
      DISPLAY_VERSION,
      DISPLAY_MESSAGE
    };

    OTGW_Display();
    ~OTGW_Display();

    // Initialise the display.  Probes I2C; a missing display is silently ignored.
    // Must be called after Wire.begin().
    void begin();

    // Advance to the next page and redraw.  No-op when no display is attached.
    void tick();

    // Show a full-screen message.  Pass an empty string to clear and resume
    // normal page rotation.  Also forces an immediate redraw.
    void message(const char *str);

    // True when a display was successfully detected and initialised.
    bool isConnected() const { return u8g2 != nullptr; }

  private:
    void draw_welcome();
    void draw_display_page_ip();
    void draw_display_page_time();
    void draw_display_page_uptime();
    void draw_display_page_version();
    void draw_display_page_message();

    // Draws a fixed-size frame and the right-hand status column.
    void draw_chrome();

    // Draw status icon in right-hand column (flame / modulation).
    void draw_status_column();

    U8G2   *u8g2    = nullptr;
    char    _message[64];
    Pages   page    = Pages::DISPLAY_IP;
};

#endif // OTGW_DISPLAY_H
