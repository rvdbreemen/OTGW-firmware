# OTGW Pinout Reference

Canonical GPIO map for every supported OTGW hardware variant. Four configurations:

1. **OTGW Classic (PIC)** — Wemos D1 mini (ESP8266) + PIC microcontroller.
2. **OTGW32** — OT-Thing OTGW32 PCB, ESP32-S3, native OTDirect (no PIC).
3. **esp32-classic (S3 in Classic socket)** — LOLIN S3 Mini dropped into the
   Classic D1-mini socket; fixed compile-time PIC build (ADR-126).
4. **esp32-combo (ADR-127)** — one ESP32-S3 binary that carries the OTGW32 map
   (table 2) *and* the Classic-on-S3 map (table 3) and boot-detects which board
   it is running on. See section 4.

## Sources of truth

| Configuration | Authority | File |
|---|---|---|
| OTGW Classic (PIC) | **dev branch pin defines** | `dev:src/OTGW-firmware/OTGW-firmware.h` (`#define I2CSCL D1` …) |
| OTGW32 (OTDirect) | **OT-Thing OTGW32 hwdef** (`#ifdef NODO`) | `boards.h` → `BOARD_NODOSHOP_ESP32` |
| esp32-classic | ADR-126 standalone section | `boards.h` → `BOARD_NODOSHOP_ESP32_CLASSIC` |
| esp32-combo | ADR-127 combo delta | `boards.h` → `BOARD_NODOSHOP_ESP32_COMBO` |

The S3 Mini / S3 Mini Pro are **pin-compatible with the Wemos D1 mini footprint**:
a shield (or the OTGW Classic socket) drives the same physical holes, only the GPIO
behind each hole changes. That is what makes the esp32-classic build possible.

---

## 1. OTGW Classic + PIC — Wemos D1 mini (ESP8266)

Truth = dev branch `OTGW-firmware.h`:

```c
#define I2CSCL D1   // GPIO5
#define I2CSDA D2   // GPIO4
#define BUTTON D3   // GPIO0
#define PICRST D5   // GPIO14
#define LED1   D4   // GPIO2
#define LED2   D0   // GPIO16
OTGWSerial OTGWSerial(PICRST, LED2);   // PIC UART = hardware UART0 (RX/TX), reset on PICRST
```

| Signal | D1-mini hole | ESP8266 GPIO | Notes |
|---|---|---|---|
| PIC serial RX | RX | GPIO3 | UART0, ESP RX ← PIC TX |
| PIC serial TX | TX | GPIO1 | UART0, ESP TX → PIC RX |
| PIC reset | D5 | GPIO14 | `PICRST` |
| I2C SCL | D1 | GPIO5 | watchdog 0x26 + OLED |
| I2C SDA | D2 | GPIO4 | |
| Config / reset button | D3 | GPIO0 | active LOW, pull-up |
| LED1 (OT) | D4 | GPIO2 | active LOW (onboard LED) |
| LED2 (status) | D0 | GPIO16 | active LOW; also PIC-flash progress LED |

Full Wemos D1 mini footprint reference: `D0=16, D1=5, D2=4, D3=0, D4=2, D5=14,
D6=12, D7=13, D8=15, TX=1, RX=3, A0=ADC`.

---

## 2. OTGW32 — OT-Thing PCB (ESP32-S3, OTDirect)

Truth = OT-Thing `hwdef.h` (`#ifdef NODO`), mirrored into `boards.h`
`BOARD_NODOSHOP_ESP32`. No PIC: OT protocol is driven directly by OTDirect.

| Signal | S3 GPIO | Notes |
|---|---|---|
| OT master IN (from boiler) | 21 | OTDirect master |
| OT master OUT (to boiler) | 1 | |
| OT slave IN (from thermostat) | 6 | OTDirect slave |
| OT slave OUT (to thermostat) | 7 | |
| I2C SCL | 17 | OLED (SSD1306 @0x3C) |
| I2C SDA | 18 | |
| 1-Wire (Dallas) | 4 | |
| Step-up enable (18V OT bus) | 10 | HIGH = enable |
| Bypass relay | 47 | |
| W5500 SPI SCK | 12 | Ethernet |
| W5500 SPI MISO | 13 | |
| W5500 SPI MOSI | 11 | |
| W5500 CS | 14 | |
| W5500 INT | 15 | |
| W5500 RST | 16 | |
| LED1 (OT red) | 2 | active LOW |
| LED2 (status) | 8 | active LOW |
| LED green (OT) | 48 | active HIGH |
| Boot button | 0 | active LOW |
| Config button | 9 | active LOW |

---

## 3. esp32-classic — S3 Mini in the Classic D1-mini socket (ADR-126)

Fixed compile-time PIC build (`BOARD_NODOSHOP_ESP32_CLASSIC`): the same Classic
PCB as table 1, with the S3 Mini GPIO behind each D1-mini hole. No OTDirect, no
runtime detection (ADR-126).

### 3a. Footprint mapping (D1-mini hole → S3 Mini GPIO)

High-confidence rows are the boot-critical signals, cross-checked against OT-Thing's
own W5500 wiring (`SCK/MISO/MOSI = 12/13/11`, which lands on the D5/D6/D7 holes —
an independent confirmation of the footprint).

| D1-mini hole | function | ESP8266 GPIO | S3 Mini GPIO | Confidence |
|---|---|---|---|---|
| TX | UART0 TX | 1 | **43** | high (LOLIN "TX") |
| RX | UART0 RX | 3 | **44** | high (LOLIN "RX") |
| D1 | I2C SCL | 5 | **36** | high (LOLIN "SCL") |
| D2 | I2C SDA | 4 | **35** | high (LOLIN "SDA") |
| D5 | SPI SCK | 14 | **12** | high (= OT-Thing W5500 SCK) |
| D6 | SPI MISO | 12 | **13** | high (LOLIN "MISO") |
| D7 | SPI MOSI | 13 | **11** | high (LOLIN "MOSI") |
| D8 | SPI SS | 15 | **10** | high (LOLIN "SS", variant `pins_arduino.h`) |
| D0 | IO (LED2) | 16 | **4** | high (S3 Mini pin diagram, outer row) |
| D3 | IO (button) | 0 | **18** | high (S3 Mini pin diagram, outer row) |
| D4 | IO (LED1) | 2 | **16** | high (S3 Mini pin diagram, outer row) |
| A0 | ADC | A0 | **2** | high (S3 Mini pin diagram, outer row) |
| RST | reset | RST | EN | high (S3 Mini pin diagram) |

All rows confirmed against the official LOLIN S3 Mini pin diagram
(`s3_mini_v1.0.0_4_16x9.jpg`, wemos.cc) — the **outer** pin row is the
D1-mini-compatible footprint; the inner row carries extra S3 GPIOs.
Cross-checked against the Arduino core variant
(`variants/lolin_s3_mini/pins_arduino.h`: TX=43, RX=44, SDA=35, SCL=36,
SCK=12, MISO=13, MOSI=11, SS=10).

### 3b. esp32-classic pin map (`boards.h` → `BOARD_NODOSHOP_ESP32_CLASSIC`)

| Macro | S3 GPIO | From hole | Classic signal |
|---|---|---|---|
| `PIN_PIC_RST` | **12** | D5 | PIC reset |
| `PIN_PIC_RX` | **44** | RX | PIC UART (ESP RX ← PIC TX) |
| `PIN_PIC_TX` | **43** | TX | PIC UART (ESP TX → PIC RX) |
| `PIN_I2C_SCL` | **36** | D1 | I2C SCL (watchdog 0x26 + OLED) |
| `PIN_I2C_SDA` | **35** | D2 | I2C SDA |
| `PIN_BUTTON` | **18** | D3 | Config/reset button |
| `PIN_LED1` | **16** | D4 | LED1 (active LOW) |
| `PIN_LED2` | **4** | D0 | LED2 (active LOW) |

Capabilities: `HAS_PIC=1`, `HAS_PIC_WATCHDOG=1` (the Classic PCB carries the
external 0x26 I2C watchdog and this build feeds it), `HAS_DIRECT_OT=0`,
`HAS_ETH_CAPABLE=0`, `HAS_OLED_CAPABLE=1`, SAT/BLE/weather enabled with the
ESP32 buffer sizing.

---

## 4. esp32-combo — one binary, boot-detected (ADR-127)

`BOARD_NODOSHOP_ESP32_COMBO` derives the full OTGW32 map (table 2) as its base
and layers the verified Classic-on-S3 pins (table 3b) on top under the
`PIN_CLASSIC_*` / `PIN_PIC_*` macros. Boot detection (PIC-probe-first,
persisted in `settings.iBoardMode`) decides which map is live; runtime
accessors (`activeI2cSda/Scl()`, `activeLed1/2()`, `activeButton()`) resolve
the conflicting positions.

Conflicting GPIO positions between the two physical boards:

| GPIO | Classic-on-S3 | OTGW32 |
|---|---|---|
| 12 | PIC reset | W5500 SPI SCK |
| 16 | LED1 | W5500 RST |
| 4 | LED2 | 1-Wire (Dallas) |
| 18 | Config button | I2C SDA (OLED) |

The PIC UART (43/44) and the Classic I2C pair (35/36) are unused on the
OTGW32, so the boot probe and the early 0x26 watchdog disarm are harmless
there. Pre-detection default is the **Classic** map (the 0x26 disarm is the
safety-critical consumer); after detection `applyResolvedComboPins()` re-pins
I2C, the ledc LED channels and the OLED button onto the live map.

Capabilities: `HAS_PIC=1`, `HAS_DIRECT_OT=1`, `HAS_PIC_WATCHDOG=1` (0x26 feed
runtime-gated on PIC mode; ESP32 TWDT always on), `HAS_RUNTIME_HW_DETECT=1`,
OTGW32 peripheral set (Ethernet/OLED/SAT/BLE/weather).

Historical combo rationale: `docs/hardware/combo-esp32-s3-pinout.md`,
ADR-125 → ADR-126 → revived as ADR-127.
