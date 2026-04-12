/*
***************************************************************************
**  Program  : boards.h
**  Version  : v2.0.0-beta
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
//   -DBOARD_NODOSHOP_ESP8266   (current Nodoshop OTGW with D1 mini)
//   -DBOARD_NODOSHOP_ESP32     (future Nodoshop OTGW with ESP32)
//
// When no board is explicitly selected, auto-detect from the platform.

#if !defined(BOARD_NODOSHOP_ESP8266) && !defined(BOARD_NODOSHOP_ESP32)
  #if defined(ESP8266)
    #define BOARD_NODOSHOP_ESP8266
  #elif defined(ESP32)
    #define BOARD_NODOSHOP_ESP32
  #endif
#endif

// ---------------------------------------------------------------------------
#if defined(BOARD_NODOSHOP_ESP8266)
// ---------------------------------------------------------------------------
// Nodoshop OTGW v1.x — Wemos D1 mini (ESP8266)
//
//  D0 = GPIO16  LED2 (active LOW)
//  D1 = GPIO5   I2C SCL (watchdog + sensors)
//  D2 = GPIO4   I2C SDA
//  D3 = GPIO0   Config/reset button (active LOW, has pull-up)
//  D4 = GPIO2   LED1 (active LOW, also onboard LED)
//  D5 = GPIO14  PIC reset line

#define PIN_I2C_SCL       5    // D1
#define PIN_I2C_SDA       4    // D2
#define PIN_BUTTON        0    // D3
#define PIN_PIC_RST       14   // D5
#define PIN_LED1          2    // D4
#define PIN_LED2          16   // D0

// Feature flags for ESP8266 Nodoshop OTGW
#define HAS_PIC           1    // Has PIC microcontroller for OpenTherm gateway
#define HAS_DIRECT_OT     0    // No direct OT master (uses PIC)
#define HAS_ETH_CAPABLE   0    // No Ethernet support

// ---------------------------------------------------------------------------
#elif defined(BOARD_NODOSHOP_ESP32)
// ---------------------------------------------------------------------------
// Nodoshop OTGW v2.x — ESP32 (hypothetical pinout, adjust to actual board)
//
// The ESP32 board is expected to use the same functional layout:
//   I2C for watchdog, two LEDs, one button, PIC reset line.
// GPIO numbers below are sensible defaults — update when the hardware ships.

#warning "ESP32 pin definitions are hypothetical — verify against actual hardware before deploying"

#define PIN_I2C_SCL       22   // Standard ESP32 I2C SCL
#define PIN_I2C_SDA       21   // Standard ESP32 I2C SDA
#define PIN_BUTTON        0    // BOOT button (active LOW, pull-up)
#define PIN_PIC_RST       14   // PIC reset line
#define PIN_LED1          2    // Onboard LED on many ESP32 dev boards
#define PIN_LED2          4    // Secondary LED

// ESP32 needs explicit Serial1 RX/TX pins for PIC communication
#define PIN_PIC_RX        16   // UART1 RX — connects to PIC TX
#define PIN_PIC_TX        17   // UART1 TX — connects to PIC RX

// OT-Direct master pins (ESP32 direct OpenTherm without PIC)
#define PIN_OT_MASTER_IN  32   // OpenTherm master input
#define PIN_OT_MASTER_OUT 33   // OpenTherm master output

// OT-Direct slave pins (gateway mode: listen to thermostat)
#define PIN_OT_SLAVE_IN   25   // OpenTherm slave input (thermostat signal)
#define PIN_OT_SLAVE_OUT  26   // OpenTherm slave output (response to thermostat)

// Step-up converter enable (18V OT bus power)
#define PIN_STEPUP_ENABLE 27   // HIGH = enable 18V step-up for OT bus

// SPI pins for W5500 Ethernet module
#define PIN_SPI_CS        5    // W5500 chip select
#define PIN_SPI_INT       34   // W5500 interrupt (input only on ESP32)
#define PIN_SPI_RST       15   // W5500 reset
#define PIN_SPI_SCK       18   // SPI clock
#define PIN_SPI_MISO      19   // SPI MISO
#define PIN_SPI_MOSI      23   // SPI MOSI

// Feature flags for ESP32 Nodoshop OTGW32
#define HAS_PIC           1    // Has PIC microcontroller for OpenTherm gateway
#define HAS_DIRECT_OT     1    // Can also do direct OT master (without PIC)
#define HAS_ETH_CAPABLE   1    // Has Ethernet support (W5500 or similar)

// ---------------------------------------------------------------------------
#else
  #error "No board defined. Set BOARD_NODOSHOP_ESP8266 or BOARD_NODOSHOP_ESP32."
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
