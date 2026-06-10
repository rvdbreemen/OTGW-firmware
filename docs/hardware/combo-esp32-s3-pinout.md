> **SUPERSEDED (ADR-126, 2026-06-10):** the combo board and its runtime
> detection are retired. The S3-in-Classic configuration is now the fixed
> compile-time `esp32-classic` build; the footprint mapping below remains
> valid and is mirrored in `PINOUT.md`.

# Combo ESP32-S3 pinout — OTGW Classic (PIC) and OTGW32 (OTDirect)

Reference pin map for the **combo board** (ADR-125): a single ESP32-S3 binary
that runs on **either** an OTGW Classic PCB (Wemos D1-mini socket + PIC) **or**
an OTGW32 (OT-Thing, native OTDirect). The S3 board (LOLIN **S3 Mini** / **S3
Mini Pro**) is **pin-compatible with the Wemos D1 mini footprint**, so when it
sits in the OTGW Classic socket each signal lands on the S3 GPIO at the matching
footprint hole.

Sources of truth:
- **OTGW Classic (PIC):** dev branch `boards.h` → `BOARD_NODOSHOP_ESP8266`, plus
  the Wemos D1 mini pin table.
- **OTGW32 (OTDirect):** OT-Thing OTGW32 hardware → `boards.h`
  → `BOARD_NODOSHOP_ESP32`.
- **S3 Mini ↔ D1 mini:** footprint compatibility (shield signals land on the
  same holes). Cross-checked: OT-Thing's W5500 `SCK/MISO/MOSI = 12/13/11`
  equals the S3 Mini's SPI at the D1-mini D5/D6/D7 holes — an independent
  confirmation of the footprint mapping.

---

## 1. Wemos D1 mini footprint → S3 Mini GPIO

The OTGW Classic was designed around the D1 mini. The S3 Mini drops into the same
holes; the right-hand column is the ESP32-S3 GPIO at each hole.

| D1-mini hole | D1-mini function | ESP8266 GPIO (Classic) | S3 Mini GPIO | Confidence |
|---|---|---|---|---|
| TX | UART0 TX | GPIO1 | **43** | high (LOLIN "TX") |
| RX | UART0 RX | GPIO3 | **44** | high (LOLIN "RX") |
| D1 | I2C SCL | GPIO5 | **36** | high (LOLIN "SCL") |
| D2 | I2C SDA | GPIO4 | **35** | high (LOLIN "SDA") |
| D5 | SPI SCK | GPIO14 | **12** | high (LOLIN "SCK" = OT-Thing W5500 SCK) |
| D6 | SPI MISO | GPIO12 | **13** | high (LOLIN "MISO") |
| D7 | SPI MOSI | GPIO13 | **11** | high (LOLIN "MOSI") |
| D8 | SPI SS | GPIO15 | **10** | high (LOLIN "SS", variant `pins_arduino.h`) |
| D0 | IO (LED2) | GPIO16 | **4** | high (S3 Mini pin diagram, outer row) |
| D3 | IO (button) | GPIO0 | **18** | high (S3 Mini pin diagram, outer row) |
| D4 | IO (LED1) | GPIO2 | **16** | high (S3 Mini pin diagram, outer row) |
| A0 | ADC | A0 | **2** | high (S3 Mini pin diagram, outer row) |
| RST | reset | RST | EN | high (S3 Mini pin diagram) |

All rows confirmed against the official LOLIN S3 Mini pin diagram
(`s3_mini_v1.0.0_4_16x9.jpg`, wemos.cc): the **outer** pin row is the
D1-mini-compatible footprint, the inner row carries extra S3 GPIOs. Also
cross-checked against the Arduino core variant
(`variants/lolin_s3_mini/pins_arduino.h`).

---

## 2. OTGW Classic — what the socket actually drives

From dev `boards.h` (`BOARD_NODOSHOP_ESP8266`) + the hole each uses:

| Classic signal | D1-mini hole | ESP8266 GPIO | → S3 Mini GPIO (combo, PIC mode) |
|---|---|---|---|
| PIC reset (`PIN_PIC_RST`) | D5 | 14 | **12** |
| PIC serial RX (`PIN_PIC_RX`) | RX | 3 | **44** |
| PIC serial TX (`PIN_PIC_TX`) | TX | 1 | **43** |
| I2C SCL (watchdog 0x26 + OLED) | D1 | 5 | **36** |
| I2C SDA | D2 | 4 | **35** |
| Button | D3 | 0 | **18** (= OTGW32 I2C SDA — needs runtime gating if wired) |
| LED1 | D4 | 2 | **16** (= OTGW32 W5500 RST — needs runtime gating if wired) |
| LED2 | D0 | 16 | **4** (= OTGW32 1-Wire — needs runtime gating if wired) |

> Note: the Classic PCB also carries the external **0x26 I2C watchdog** on the
> I2C bus. The combo currently runs the ESP32 Task Watchdog instead
> (`HAS_PIC_WATCHDOG=0`); feeding the 0x26 chip in PIC mode is a known follow-up.

---

## 3. OTGW32 (OT-Thing) — ESP32-S3 native, OTDirect

From `boards.h` (`BOARD_NODOSHOP_ESP32`). This is NOT a D1-mini footprint; it is
the OT-Thing OTGW32 PCB. The combo uses these in **OTDirect mode**.

| OTGW32 signal | S3 GPIO |
|---|---|
| OT master IN / OUT | 21 / 1 |
| OT slave IN / OUT | 6 / 7 |
| I2C SCL / SDA (OLED) | 17 / 18 |
| W5500 SPI SCK/MISO/MOSI | 12 / 13 / 11 |
| W5500 CS / INT / RST | 14 / 15 / 16 |
| 1-Wire (Dallas) | 4 |
| Step-up enable | 10 |
| Bypass relay | 47 |
| LED1 / LED2 / LED_GREEN | 2 / 8 / 48 |
| Boot / Config button | 0 / 9 |

---

## 4. Resulting combo pin map (boards.h `BOARD_NODOSHOP_ESP32_COMBO`)

The combo = OTGW32 base map **+** the PIC delta. Both subsystems' pins are
compiled in; **runtime detection** (`state.hw.eMode`) decides which is active,
so a GPIO shared between the two maps (e.g. **12** = W5500-SCK in OTDirect mode
*and* PIC-reset in PIC mode) has exactly one live owner per boot.

| Combo macro | Value (S3 GPIO) | Used in mode | From |
|---|---|---|---|
| `PIN_PIC_RST` | **12** | PIC | D5 hole |
| `PIN_PIC_RX` | **44** | PIC | RX hole |
| `PIN_PIC_TX` | **43** | PIC | TX hole |
| `PIN_PIC_I2C_SCL` | **36** | PIC | D1 hole |
| `PIN_PIC_I2C_SDA` | **35** | PIC | D2 hole |
| `PIN_I2C_SCL` / `PIN_I2C_SDA` | 17 / 18 | OTDirect | OT-Thing |
| `PIN_OT_MASTER_IN/OUT` | 21 / 1 | OTDirect | OT-Thing |
| `PIN_OT_SLAVE_IN/OUT` | 6 / 7 | OTDirect | OT-Thing |
| W5500 SPI (CS/INT/RST/SCK/MISO/MOSI) | 14/15/16/12/13/11 | OTDirect | OT-Thing |

### Conflict check (PIC mode pins vs OTGW32 map)
PIC-mode pins 12/43/44/35/36 vs OTGW32 pins {0,1,2,4,6,7,8,9,10,11,13,14,15,16,
17,18,21,47,48}:
- **12** overlaps W5500-SCK — intentional, runtime-gated (different owner per mode).
- **43, 44, 35, 36** do not overlap the OTGW32 map — distinct GPIOs, no contention.

---

## 5. Verification

The combo logs its detection result to the USB/IDF console at boot (ERROR level):

```
[combo] detect: eMode=1 picEnabled=1 boardMode=1 (RST=12 RX=44 TX=43 I2C(pic)=35/36)
```

- `picEnabled=1, eMode=1` → PIC found on these pins (Classic mode). Correct.
- `picEnabled=0, eMode=2` → PIC not found → either a pin is still wrong or the
  PIC wiring differs; re-check against this table.
