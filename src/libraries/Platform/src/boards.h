/*
***************************************************************************
**  Program  : boards.h
**  Version  : v2.0.0-alpha.137
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
//   -DBOARD_NODOSHOP_ESP32          (Nodoshop OTGW32 with ESP32-S3, OTDirect)
//   -DBOARD_NODOSHOP_ESP32_CLASSIC  (LOLIN S3 Mini in the OTGW Classic D1-mini
//                                    socket: PIC gateway, compile-time — no
//                                    runtime detection)
//   -DBOARD_NODOSHOP_ESP32_COMBO    (one ESP32-S3 binary: PIC *or* OTDirect,
//                                    boot-detected — ADR-127)
//
// When no board is explicitly selected, auto-detect from the platform.

// The combo board (ADR-127, reviving ADR-125 with the verified Classic-on-S3
// pin map) is a single ESP32-S3 binary that runs on EITHER an OTGW Classic PCB
// (PIC present) OR an OTGW32 (OTDirect). It reuses the full OTGW32 pin map +
// capability set as its base and layers the verified Classic pins + HAS_PIC on
// top (see the "combo delta" block at end of file). So it derives the ESP32
// base board here, BEFORE the auto-detect guard.
#if defined(BOARD_NODOSHOP_ESP32_COMBO) && !defined(BOARD_NODOSHOP_ESP32)
  #define BOARD_NODOSHOP_ESP32
#endif

#if !defined(BOARD_NODOSHOP_ESP32) && !defined(BOARD_NODOSHOP_ESP32_CLASSIC)
  #if defined(ESP32)
    #define BOARD_NODOSHOP_ESP32
  #endif
#endif

// ---------------------------------------------------------------------------
#if defined(BOARD_NODOSHOP_ESP32)
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
#define HAS_PIC_WATCHDOG  0    // No external 0x26 watchdog board on OTGW32 (TWDT instead)
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
#define HAS_REST_TX_COALESCING 1  // sync WebServer stalls ~9ms/sendContent on ESP32; coalesce into 4KB chunks (TASK-743)
#define HAS_FRAGMENTATION_AWARE_HEAP_GATE 1  // ESP32-S3: gate drip on freeHeap+maxAllocBlock, not the ESP8266 tier machine (TASK-743)
#define HW_TYPE_NAME      "otgw32"        // Static hardware-type slug / board class (ADR-113)
#define BOARD_NAME        "Nodoshop OTGW32 (ESP32-S3)"  // Display string (boardName())
#define HAS_LEDC_LED      1    // LEDs PWM-dimmed via ledc (LED_BRIGHTNESS above)

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
#elif defined(BOARD_NODOSHOP_ESP32_CLASSIC)
// ---------------------------------------------------------------------------
// OTGW Classic PCB with a LOLIN S3 Mini (ESP32-S3) in the D1-mini socket.
//
// Same Nodoshop Classic hardware as the ESP8266 board above — PIC gateway,
// 0x26 I2C watchdog, optional I2C OLED — but with an S3 Mini instead of a
// D1 mini. The S3 Mini is footprint-compatible: each Classic signal lands on
// the S3 GPIO at the matching D1-mini hole. Mapping verified against the
// official LOLIN S3 Mini pin diagram + Arduino variant pins_arduino.h
// (docs/hardware/PINOUT.md):
//
//   hole  D1-mini GPIO  S3 GPIO  signal
//   TX    1             43       PIC UART (ESP TX -> PIC RX)
//   RX    3             44       PIC UART (ESP RX <- PIC TX)
//   D1    5             36       I2C SCL (watchdog 0x26 + OLED)
//   D2    4             35       I2C SDA
//   D3    0             18       Config/reset button
//   D4    2             16       LED1
//   D5    14            12       PIC reset
//   D0    16            4        LED2
//
// This is a fixed, compile-time board: HAS_PIC=1, no OTDirect, no runtime
// hardware detection (supersedes the ADR-125 combo experiment).

#define PIN_I2C_SCL       36   // D1 hole
#define PIN_I2C_SDA       35   // D2 hole
#define PIN_BUTTON        18   // D3 hole (active LOW)
#define PIN_PIC_RST       12   // D5 hole
#define PIN_LED1          16   // D4 hole (active LOW)
#define PIN_LED2          4    // D0 hole (active LOW)
#define PIN_PIC_RX        44   // RX hole (ESP RX <- PIC TX)
#define PIN_PIC_TX        43   // TX hole (ESP TX -> PIC RX)

// Feature flags — Classic gateway capabilities on an S3 Mini
#define HAS_PIC           1    // PIC microcontroller drives the OT bus
#define HAS_PIC_WATCHDOG  1    // Classic PCB carries the external 0x26 I2C watchdog (optional SECONDARY; ESP32 TWDT is primary, ADR-135)
#define HAS_DIRECT_OT     0    // No OT-direct hardware on the Classic PCB
#define HAS_ETH_CAPABLE   0    // No W5500 on the Classic PCB
#define HAS_OLED_CAPABLE  1    // Optional I2C OLED on the Classic I2C header
// SAT runs fine on the S3 (RAM/BLE present); boiler control goes via the PIC
// command queue, which SAT already uses on this hardware class.
#define HAS_SAT              1
#define HAS_SAT_BLE          1  // S3 BLE radio present (SATble room sensors)
#define HAS_WEATHER_FORECAST 1  // S3 RAM budget allows the hourly forecast arrays
#define HAS_REST_TX_COALESCING 1  // same sync-WebServer stall as OTGW32 (TASK-743)
#define HAS_FRAGMENTATION_AWARE_HEAP_GATE 1  // ESP32-S3 heap gating (TASK-743)
#define HW_TYPE_NAME      "otgw-classic"  // Static hardware-type slug / board class (ADR-113)
#define BOARD_NAME        "Nodoshop OTGW Classic (ESP32-S3)"  // Display string (boardName())
#define HAS_LEDC_LED      1    // S3 ledc PWM dimming, same as OTGW32
// LED PWM brightness (ledc, 8-bit; active-LOW LEDs use output inversion)
#define LED_BRIGHTNESS    5

// SAT per-platform buffer sizing — ESP32-S3 SRAM budget (same as OTGW32).
#define SAT_WIN4H_SIZE          360
#define SAT_FLOW_SAMPLE_SIZE    256
#define SAT_TAIL_SAMPLE_SIZE    180
#define HCR_DAYS                30
#define HCR_INTRADAY_SIZE       1440
#define SAT_CYCLES_FILE_BUF_SIZE 4896
typedef uint16_t SAT_RING_IDX_T;

// MQTT per-platform tuning — ESP32-S3 values (same as OTGW32).
#define MQTT_DISCOVERY_HEAP_MIN   2048
#define STATUS_BURST_COOLDOWN_MS  250

// ---------------------------------------------------------------------------
#else
  #error "No board defined. Set BOARD_NODOSHOP_ESP32, BOARD_NODOSHOP_ESP32_CLASSIC or BOARD_NODOSHOP_ESP32_COMBO."
#endif

// ---------------------------------------------------------------------------
// Combo board delta (ADR-127) — applied ON TOP of the OTGW32 (ESP32-S3) base.
//
// One ESP32-S3 binary that boot-detects PIC (OTGW Classic PCB) vs OTDirect
// (OTGW32) and drives the matching subsystem. The OTGW32 peripheral pin map +
// capability flags above are the base; here we add the field-verified
// Classic-on-S3 pins (same map as BOARD_NODOSHOP_ESP32_CLASSIC above) and flip
// HAS_PIC on. Runtime selection (state.hw.eMode / isPICEnabled()) decides
// which subsystem is initialized, so each shared GPIO has one live owner per
// boot. Conflicting positions between the two physical boards:
//
//   GPIO  Classic-on-S3        OTGW32
//   12    PIC reset            SPI SCK (W5500)
//   16    LED1                 SPI RST (W5500)
//   4     LED2                 1-Wire (Dallas)
//   18    Config button        I2C SDA (OLED)
//
// The PIC UART (43/44) and the Classic I2C pair (35/36) are unused on the
// OTGW32, so the boot probe and the early 0x26 watchdog disarm are harmless
// there. Application code resolves the conflicting positions through the
// runtime accessors activeI2cSda()/activeI2cScl()/activeLed1()/activeLed2()/
// activeButton() (OTGW-firmware.h) and gates combo behaviour on
// HAS_RUNTIME_HW_DETECT, never on the raw board macro (ESP-abstraction rule).
// ---------------------------------------------------------------------------
#if defined(BOARD_NODOSHOP_ESP32_COMBO)
  // Both OT engines are compiled in; the mutual-exclusion of HAS_PIC /
  // HAS_DIRECT_OT used by the fixed boards is replaced by runtime gating.
  #undef  HAS_PIC
  #define HAS_PIC           1    // PIC serial driver (OTGWSerial) is linked in
  // The Classic PCB carries the external 0x26 I2C watchdog as an OPTIONAL
  // SECONDARY layer (the ESP32 TWDT is the primary watchdog, ADR-135); the combo
  // feeds it at runtime only when the PIC was detected (isPICEnabled()). On an
  // OTGW32 the feed is a NACKed no-op on floating pins.
  #undef  HAS_PIC_WATCHDOG
  #define HAS_PIC_WATCHDOG  1
  // HAS_DIRECT_OT stays 1 (OTDirect linked in, from the OTGW32 base).
  #define HAS_RUNTIME_HW_DETECT 1  // boot-detect PIC vs OTDirect + dual pin map

  // Override the display/board identity. HW_TYPE_NAME stays defined from the
  // base (otgw32) but is unused on the combo: hardwareTypeName() resolves the
  // slug at runtime from state.hw.eMode (ADR-127).
  #undef  BOARD_NAME
  #define BOARD_NAME        "Nodoshop OTGW Combo (ESP32-S3)"

  // ---- Classic-on-S3 pins (field-verified, same as ESP32_CLASSIC above) ----
  //   hole  D1-mini GPIO  S3 GPIO  signal
  //   TX    1             43       PIC UART (ESP TX -> PIC RX)
  //   RX    3             44       PIC UART (ESP RX <- PIC TX)
  //   D1    5             36       I2C SCL (watchdog 0x26 + OLED)
  //   D2    4             35       I2C SDA
  //   D3    0             18       Config/reset button
  //   D4    2             16       LED1
  //   D5    14            12       PIC reset
  //   D0    16            4        LED2
  #define PIN_PIC_RST          12   // D5 hole
  #define PIN_PIC_RX           44   // RX hole (ESP RX <- PIC TX)
  #define PIN_PIC_TX           43   // TX hole (ESP TX -> PIC RX)
  #define PIN_CLASSIC_I2C_SCL  36   // D1 hole (0x26 watchdog + OLED in PIC mode)
  #define PIN_CLASSIC_I2C_SDA  35   // D2 hole
  #define PIN_CLASSIC_BUTTON   18   // D3 hole (active LOW)
  #define PIN_CLASSIC_LED1     16   // D4 hole (active LOW)
  #define PIN_CLASSIC_LED2     4    // D0 hole (active LOW)
#endif

// HAS_RUNTIME_HW_DETECT: 1 only on the combo board class (ADR-127); every
// other board is a fixed compile-time class. Defined 0 here so application
// code can gate on it unconditionally.
#ifndef HAS_RUNTIME_HW_DETECT
#define HAS_RUNTIME_HW_DETECT 0
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
