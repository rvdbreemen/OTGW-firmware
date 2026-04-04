/*
***************************************************************************
**  Program  : boards.h
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Board-specific pin maps.  Each supported hardware variant defines its
**  own GPIO assignments here.  Application code uses the PIN_* constants
**  and never references Dx aliases or raw GPIO numbers directly.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef BOARDS_H
#define BOARDS_H

// ---- Board selection ------------------------------------------------------
// Build flags should define exactly one BOARD_* macro, e.g.:
//   -DBOARD_NODOSHOP_ESP8266   (current Nodoshop OTGW with D1 mini + PIC)
//   -DBOARD_NODOSHOP_OTGW32    (Nodoshop OTGW32 with ESP32-S3, direct GPIO OT)
//
// When no board is explicitly selected, auto-detect from the platform.

#if !defined(BOARD_NODOSHOP_ESP8266) && !defined(BOARD_NODOSHOP_OTGW32)
  #if defined(ESP8266)
    #define BOARD_NODOSHOP_ESP8266
  #elif defined(ESP32)
    #define BOARD_NODOSHOP_OTGW32
  #endif
#endif

// ---- Feature flags --------------------------------------------------------
// Each board sets these to indicate hardware capabilities.
// HAS_PIC       — PIC16F co-processor present (UART-based OT gateway)
// HAS_DIRECT_OT — Direct GPIO OpenTherm via opentherm_library (no PIC)

// ---------------------------------------------------------------------------
#if defined(BOARD_NODOSHOP_ESP8266)
// ---------------------------------------------------------------------------
// Nodoshop OTGW v1.x — Wemos D1 mini (ESP8266) + PIC16F gateway
//
//  D0 = GPIO16  LED2 (active LOW)
//  D1 = GPIO5   I2C SCL (watchdog + sensors)
//  D2 = GPIO4   I2C SDA
//  D3 = GPIO0   Config/reset button (active LOW, has pull-up)
//  D4 = GPIO2   LED1 (active LOW, also onboard LED)
//  D5 = GPIO14  PIC reset line

#define HAS_PIC           1
#define HAS_DIRECT_OT     0
#define HAS_OLED_CAPABLE  1    // probed at runtime via I2C scan

#define PIN_I2C_SCL       5    // D1
#define PIN_I2C_SDA       4    // D2
#define PIN_BUTTON        0    // D3
#define PIN_PIC_RST       14   // D5
#define PIN_LED1          2    // D4
#define PIN_LED2          16   // D0

// ---------------------------------------------------------------------------
#elif defined(BOARD_NODOSHOP_OTGW32)
// ---------------------------------------------------------------------------
// Nodoshop OTGW32 — ESP32-S3, direct GPIO OpenTherm (no PIC)
//
// Pin assignments sourced from OT-Thing NODO (ESP32-S3) variant.
// TODO: Verify against actual Nodoshop OTGW32 hardware before production use.

#define HAS_PIC           0
#define HAS_DIRECT_OT     1
#define HAS_OLED_CAPABLE  1    // probed at runtime via I2C scan
#define HAS_ETH_CAPABLE   1    // probed at runtime via SPI (W5500)

// OpenTherm GPIO pins
#define PIN_OT_MASTER_IN  21   // OT master receive (from boiler)
#define PIN_OT_MASTER_OUT 1    // OT master transmit (to boiler)
#define PIN_OT_SLAVE_IN   6    // OT slave receive (from thermostat)
#define PIN_OT_SLAVE_OUT  7    // OT slave transmit (to thermostat)
#define PIN_STEPUP_ENABLE 10   // 24V step-up converter enable
#define PIN_BYPASS_RELAY  47   // Bypass relay (thermostat direct to boiler)

// I2C (OLED, sensors)
#define PIN_I2C_SCL       17
#define PIN_I2C_SDA       18

// LEDs, button, 1-wire
#define PIN_STATUS_LED    8
#define PIN_OT_RED_LED    2
#define PIN_OT_GREEN_LED  48
#define PIN_BUTTON        9
#define PIN_1WIRE         4

// Map generic LED names used by existing code
#define PIN_LED1          PIN_STATUS_LED
#define PIN_LED2          PIN_OT_RED_LED

// ---------------------------------------------------------------------------
#else
  #error "No board defined. Set BOARD_NODOSHOP_ESP8266 or BOARD_NODOSHOP_OTGW32."
#endif

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

#endif // BOARDS_H
