/*
***************************************************************************
**  Program  : boards.h
**  Version  : v2.0.0-alpha.126
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
// SAT feature flags (ESP-abstraction Tier 0, TASK-740; see
// docs/audits/2026-05-28-esp-abstraction-leak-audit.md §"Tier 0").
// Tier 0 only declares these; the raw #if defined(ESP32) gates are
// switched over in Tiers 1-3. Values reflect current ESP8266 behaviour.
#define HAS_SAT              0  // Smart Auto Thermostat is an ESP32/OTGW32 capability
#define HAS_SAT_BLE          0  // No BLE radio on ESP8266 (SATble.ino is ESP32-only)
#define HAS_WEATHER_FORECAST 0  // Basic weather only; hourly forecast is ESP32-only (independent of HAS_SAT)
#define HW_TYPE_NAME      "otgw-classic"  // Static hardware-type slug / board class (ADR-113)

// SAT per-platform buffer sizing (ESP-abstraction Tier 3, TASK-743). These are
// compile-time board-capacity tunings (SRAM budget), so they live with the
// board's HAS_* flags rather than scattered behind raw #if defined(ESP8266) in
// the SAT .ino files. ESP8266 keeps the tight (~40 KB DRAM) sizes.
#define SAT_WIN4H_SIZE          30   // rolling 4h cycle window (~2h at 4-min avg cycle)
#define SAT_FLOW_SAMPLE_SIZE    64   // per-cycle flow sample ring (256 B SRAM, ~5min @5s)
#define SAT_TAIL_SAMPLE_SIZE    SAT_FLOW_SAMPLE_SIZE  // end-of-cycle tail = 64s @1Hz
#define HCR_DAYS                7    // heating-curve daily-median ring (one week)
#define HCR_INTRADAY_SIZE       96   // intra-day samples (15-min intervals, one day)
#define SAT_CYCLES_FILE_BUF_SIZE 2560  // sat_cycles.json read buffer (30 records)
// Ring head/count index width: all ESP8266 SAT rings are <=96 slots, so a byte
// counter suffices and saves SRAM.
typedef uint8_t SAT_RING_IDX_T;

// MQTT per-platform tuning (ESP-abstraction Tier 3, TASK-743).
#define MQTT_DISCOVERY_HEAP_MIN   3000   // discovery-publish heap floor (WARNING tier, ~80KB total)
#define STATUS_BURST_COOLDOWN_MS  2000   // post-burst drip pause (ADR-088; <3s Status cadence)

// ---------------------------------------------------------------------------
#elif defined(BOARD_NODOSHOP_ESP32)
// ---------------------------------------------------------------------------
// Nodoshop OTGW32 — ESP32-S3 (4 MB flash)
//
// Pin assignments from OT-Thing-OTGW32/Firmware/include/hwdef.h (#ifdef NODO).
// This is the Nodoshop PCB variant of the OT-Thing design.
//
// No PIC: OT protocol is handled directly by OTDirect running on the ESP32-S3.
// The Nodoshop OTGW WiFi (ESP8266 + PIC) is a different product entirely.

// Buttons
#define PIN_BUTTON        0    // Boot button (active LOW, pull-up)
#define PIN_CONFIG_BUTTON 9    // Config button (active LOW)

// LEDs (all active LOW except PIN_LED_GREEN which is active HIGH)
#define PIN_LED1          2    // OT Red LED (active LOW)
#define PIN_LED2          8    // Status LED (active LOW)
#define PIN_LED_GREEN     48   // OT Green LED (active HIGH)

// I2C (OLED display)
#define PIN_I2C_SCL       17
#define PIN_I2C_SDA       18

// OpenTherm Direct — master (boiler side)
#define PIN_OT_MASTER_IN  21   // OT master input (from boiler)
#define PIN_OT_MASTER_OUT 1    // OT master output (to boiler)

// OpenTherm Direct — slave (thermostat side)
#define PIN_OT_SLAVE_IN   6    // OT slave input (from thermostat)
#define PIN_OT_SLAVE_OUT  7    // OT slave output (to thermostat)

// Step-up converter (18V OT bus power)
#define PIN_STEPUP_ENABLE 10   // HIGH = enable 18V step-up

// Dallas 1-Wire temperature sensor
#define PIN_1WIRE         4

// Bypass relay
#define PIN_BYPASS_RELAY  47

// SPI pins for W5500 Ethernet module
#define PIN_SPI_CS        14   // W5500 chip select
#define PIN_SPI_INT       15   // W5500 interrupt
#define PIN_SPI_RST       16   // W5500 reset
#define PIN_SPI_SCK       12   // SPI clock
#define PIN_SPI_MISO      13   // SPI MISO
#define PIN_SPI_MOSI      11   // SPI MOSI

// LED PWM brightness (ledc, 8-bit resolution, active-LOW LEDs use output inversion)
// 5/255 ≈ 2% on-time → dim glow, same as OT-Thing firmware LED_BRIGHTNESS
#define LED_BRIGHTNESS    5

// Feature flags for Nodoshop OTGW32
#define HAS_PIC           0    // No PIC: OT protocol handled by OTDirect (ESP32-S3 native)
#define HAS_DIRECT_OT     1    // Direct OT master/slave via OTDirect library
#define HAS_ETH_CAPABLE   1    // Has W5500 Ethernet module
#define HAS_OLED_CAPABLE  1    // Onboard 128x64 SSD1306 I2C OLED (runtime probe at 0x3C)
// SAT feature flags (ESP-abstraction Tier 0, TASK-740; see
// docs/audits/2026-05-28-esp-abstraction-leak-audit.md §"Tier 0").
// Tier 0 only declares these; the raw #if defined(ESP32) gates are
// switched over in Tiers 1-3. Values reflect current ESP32 behaviour.
#define HAS_SAT              1  // Smart Auto Thermostat runs on OTGW32 (OTDirect boiler control)
#define HAS_SAT_BLE          1  // ESP32-S3 BLE radio present (SATble.ino room-sensor support)
#define HAS_WEATHER_FORECAST 1  // Full weather incl. 24h hourly forecast arrays (independent of HAS_SAT)
#define HW_TYPE_NAME      "otgw32"        // Static hardware-type slug / board class (ADR-113)

// SAT per-platform buffer sizing (ESP-abstraction Tier 3, TASK-743). ESP32-S3
// has ample SRAM, so the SAT history buffers run much deeper than on ESP8266.
// Ring indices that exceed 255 slots need uint16_t counters; see SAT_RING_IDX_T.
#define SAT_WIN4H_SIZE          360  // rolling 4h cycle window (12h of 2-min cycle history)
#define SAT_FLOW_SAMPLE_SIZE    256  // per-cycle flow sample ring (1024 B SRAM, better p90/p10)
#define SAT_TAIL_SAMPLE_SIZE    180  // end-of-cycle tail = 180s @1Hz
#define HCR_DAYS                30   // heating-curve daily-median ring (4-week trend)
#define HCR_INTRADAY_SIZE       1440 // intra-day samples (per-minute, one day)
#define SAT_CYCLES_FILE_BUF_SIZE 4896  // sat_cycles.json read buffer (60 records)
// Ring head/count index width: ESP32 rings reach 1440 slots, so the index
// counters need a 16-bit type.
typedef uint16_t SAT_RING_IDX_T;

// MQTT per-platform tuning (ESP-abstraction Tier 3, TASK-743).
#define MQTT_DISCOVERY_HEAP_MIN   2048   // discovery-publish heap floor (larger DRAM budget)
#define STATUS_BURST_COOLDOWN_MS  250    // post-burst drip pause (ADR-088; more heap headroom)

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
