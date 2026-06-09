# OTGW Pinout Reference

Canonical GPIO map for every supported OTGW hardware variant. Three configurations:

1. **OTGW Classic (PIC)** — Wemos D1 mini (ESP8266) + PIC microcontroller.
2. **OTGW32** — OT-Thing OTGW32 PCB, ESP32-S3, native OTDirect (no PIC).
3. **Combo (S3 in Classic socket)** — LOLIN S3 Mini / S3 Mini Pro dropped into the
   Classic D1-mini socket; one binary boot-detects PIC vs OTDirect (ADR-125).

## Sources of truth

| Configuration | Authority | File |
|---|---|---|
| OTGW Classic (PIC) | **dev branch pin defines** | `dev:src/OTGW-firmware/OTGW-firmware.h` (`#define I2CSCL D1` …) |
| OTGW32 (OTDirect) | **OT-Thing OTGW32 hwdef** (`#ifdef NODO`) | `boards.h` → `BOARD_NODOSHOP_ESP32` |
| Combo | ADR-125 delta on the OTGW32 base | `boards.h` → `BOARD_NODOSHOP_ESP32_COMBO` |

The S3 Mini / S3 Mini Pro are **pin-compatible with the Wemos D1 mini footprint**:
a shield (or the OTGW Classic socket) drives the same physical holes, only the GPIO
behind each hole changes. That is what makes the combo board possible.

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

## 3. Combo — S3 Mini in the Classic D1-mini socket (ADR-125)

One ESP32-S3 binary that runs on **either** PCB. It uses the OTGW32 map (table 2)
as its base and layers PIC pins on top; **runtime detection** picks which subsystem
is live, so a shared GPIO has exactly one owner per boot.

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
| D8 | SPI SS | 15 | _TBD_ | confirm from S3 Mini diagram |
| D0 | IO | 16 | _TBD_ | confirm |
| D3 | IO (boot) | 0 | _TBD_ | confirm |
| D4 | IO (LED) | 2 | _TBD_ | confirm |
| A0 | ADC | A0 | _TBD_ | confirm |

The _TBD_ rows are LED/button/ADC holes — not boot-critical. Fill them from the
LOLIN S3 Mini diagram before relying on the on-device LEDs/button in PIC mode.

### 3b. PIC-mode pins (`BOARD_NODOSHOP_ESP32_COMBO` delta)

| Combo macro | S3 GPIO | From hole |
|---|---|---|
| `PIN_PIC_RST` | **12** | D5 |
| `PIN_PIC_RX` | **44** | RX |
| `PIN_PIC_TX` | **43** | TX |
| `PIN_PIC_I2C_SCL` | **36** | D1 |
| `PIN_PIC_I2C_SDA` | **35** | D2 |

### 3c. Conflict check (PIC pins vs OTGW32 map)

PIC-mode pins `{12, 43, 44, 35, 36}` vs OTGW32 pins
`{0,1,2,4,6,7,8,9,10,11,13,14,15,16,17,18,21,47,48}`:

- **12** overlaps W5500-SCK — intentional, runtime-gated (one owner per mode).
- **43, 44, 35, 36** do not overlap — distinct GPIOs, no contention.

### 3d. Boot verification

The combo logs detection to the USB/IDF console (ERROR level):

```
[combo] detect: eMode=1 picEnabled=1 boardMode=1 (RST=12 RX=44 TX=43 I2C(pic)=35/36)
```

- `picEnabled=1, eMode=1` → PIC found → Classic mode. Correct.
- `picEnabled=0, eMode=2` → PIC not found → either a pin is wrong or the wiring
  differs; re-check against table 3a/3b.

For the full combo rationale see `docs/hardware/combo-esp32-s3-pinout.md` and ADR-125.
