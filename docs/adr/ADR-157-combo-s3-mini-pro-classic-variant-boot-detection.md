# ADR-157 Combo board: add the LOLIN S3 Mini Pro as a third boot-detected Classic variant

## Status

Proposed, 2026-06-28

Amends ADR-127 (combo ESP32-S3 single binary, runtime PIC/OTDirect boot
detection). ADR-127 stays in force; this ADR extends its runtime-detection
machinery with a second detection axis. Guideline-level (per ADR-080):
enforcement posture is identical to ADR-127 — the existing ESP-abstraction
boundary gate (`evaluate.py::check_esp_abstraction_boundary`) enforces the
flag-based layering and no new dedicated gate is introduced. If
module-selection regressions appear, a `check_combo_runtime_selection` gate
should be added and both ADRs promoted to binding.

## Context

ADR-127 makes the `esp32-combo` binary boot-detect **one** axis: PIC present
→ OTGW Classic (the `PIN_CLASSIC_*` map), no PIC → OTGW32 (the `PIN_*` map).
The discriminator is the PIC on UART 43/44, which is independent of the
safety-critical I2C bus.

The NodoShop OTGW Classic socket accepts any D1-mini-footprint ESP module. So
far only the LOLIN S3 Mini has been mapped (`esp32-classic` / the combo's
`PIN_CLASSIC_*`). The **LOLIN S3 Mini Pro** is footprint-compatible too, but it
reroutes its 2×8 headers to free GPIO 33–36 for its on-board 0.85" TFT, so the
GPIO behind each D1-mini hole differs from the plain S3 Mini. Verified against
the WEMOS `S3_MINI_PRO` schematic (P2/P3 "Pins" sheet) and the Arduino variant
`lolin_s3_mini_pro/pins_arduino.h`:

| D1-mini hole | Classic signal | S3 Mini GPIO | **S3 Mini Pro GPIO** |
|---|---|---|---|
| TX | PIC UART TX | 43 | 43 |
| RX | PIC UART RX | 44 | 44 |
| D1 | I2C SCL (0x26 watchdog + OLED) | 36 | **11** |
| D2 | I2C SDA | 35 | **12** |
| D3 | button | 18 | **13** |
| D4 | LED1 | 16 | **14** |
| D5 | PIC reset | 12 | **40** |
| D0 | LED2 | 4 | **41** |

Only the PIC UART (43/44) is identical. The I2C bus — which carries the
safety-critical 0x26 watchdog — moves to 11/12, so the existing combo cannot be
flashed onto a Pro: its pre-detection 0x26 disarm would go out on the wrong
bus, and every Classic GPIO except the UART would be wrong.

The S3 Mini Pro and the plain S3 Mini carry **identical ESP32-S3 silicon**, so
chip-ID / eFuse cannot tell them apart. The only reliable discriminator is the
Pro's on-board **QMI8658C 6-axis IMU**, present on the Pro's I2C bus (11/12) and
absent on the plain S3 Mini. A LOLIN module is only ever dropped into the
**Classic** socket (the OTGW32 carries its own soldered ESP32-S3, never a LOLIN
Pro), so an IMU hit collapses both axes at once: IMU found ⇒ Classic + PIC + Pro
pin map.

The maintainer chose (2026-06-28) to add the Pro to the combo binary rather than
ship only a separate fixed target, so a single asset serves all three physical
configurations. A separate deterministic `esp32-classic-pro` fixed target may
still be shipped alongside for users who prefer no runtime detection; it is out
of scope for this ADR.

## Decision

Implemented on `claude/esp32-s3-mini-pro-support-0i6uu9` (combo extension); this
ADR records the design as built.

### 1. Third pin map in the combo delta (`boards.h`)

The combo block adds a `PIN_CLASSIC_PRO_*` set (RST 40, I2C 11/12, button 13,
LED1 14, LED2 41) alongside the existing OTGW32 (`PIN_*`) and Classic-S3-Mini
(`PIN_CLASSIC_*`) maps, plus the IMU discriminator constants
(`PRO_IMU_I2C_ADDR_LO/HI`, `PRO_IMU_WHOAMI_REG`, `PRO_IMU_WHOAMI_VALUE`). No new
build target or asset: the same `esp32-combo` binary.

### 2. Three-way runtime pin resolver (`OTGW-firmware.h`)

`comboClassicPinsActive()` (bool) is generalised to `comboActivePinMap()`
returning `enum ComboPinMap { COMBO_MAP_OTGW32, COMBO_MAP_CLASSIC,
COMBO_MAP_CLASSIC_PRO }`. All `active*()` accessors switch on it, and a new
`activePicRst()` is added (PIC reset differs 12↔40 between the two Classic
modules). `comboClassicPinsActive()` is retained as a convenience
(`!= COMBO_MAP_OTGW32`). On the fixed boards everything still folds to the
single compile-time map (`HAS_RUNTIME_HW_DETECT=0`), so the indirection costs
the fixed targets nothing — ADR-127's bounded-indirection property is preserved.

### 3. Persisted selector / manual override extended

`settings.iBoardMode` gains value **3 = force PIC (S3 Mini Pro)** alongside
0=auto, 1=PIC (S3 Mini), 2=OTDirect. The telnet/REST/Web-UI plumbing
(`settingStuff.ino`, `restAPI.ino`, `data/index.js`) accepts 0..3. The auto path
caches `iBoardMode=3` when the IMU probe flags a Pro, so later boots skip the
probe.

### 4. Early IMU probe before the watchdog disarm (`OTGW-firmware.ino`)

`probeProImu()` brings up I2C on the Pro bus (11/12) and reads the QMI8658C
WHO_AM_I at both SA0 addresses (0x6A/0x6B), accepting only the chip-id byte
(0x05) — so a stray device cannot produce a false positive. It runs at the very
top of `setup()`, **before** the 0x26 disarm, because the Pro's watchdog lives
on the Pro bus. The probe is a passive read; on a plain S3 Mini those GPIOs are
MOSI/PIC-reset and on an OTGW32 they are W5500 SPI, so a brief probe there is
benign (it precedes both the PIC and W5500 init). The result is held in
`state.hw.bClassicPro`, which `comboActivePinMap()` consults in the auto path.

### 5. PIC reset / progress-LED re-pinning (`OTGWSerial`)

The `OTGWSerial` global ctor binds the S3 Mini reset/LED pins (12/4) at
global-init, before detection. `setResetPin()` / `setProgressLed()` setters are
added and called in the detection block before `detectPIC()` drives them, so a
Pro resets the PIC on GPIO 40, not 12. The PIC UART (RX/TX 43/44) is identical
on both modules and needs no re-pinning (TASK-862 precedent for the ctor args).

### 6. Diagnostics

`/bootdetect.log` and the `log_e` boot line gain a `pro=` field and report the
**resolved** PIC-reset and I2C pins (`activePicRst()`, `activeI2cSda/Scl()`)
instead of the fixed Classic-S3-Mini constants.

## Consequences

- One `esp32-combo` asset now serves OTGW32, OTGW Classic + S3 Mini, and OTGW
  Classic + S3 Mini Pro. No extra build target or partition table.
- The combo now juggles three overlapping pin maps; the Pro's I2C/button/LED1
  (11/12/13/14) overlap the OTGW32 W5500 SPI pins, resolved by the same
  one-owner-per-boot mutual exclusion ADR-127 already relies on (Ethernet inits
  only in OTDirect mode). The three-way resolver is the single point where a
  mistake maps to a wrong-driven pin.
- Edge case: a *cached* Pro (`iBoardMode=3`) whose early IMU probe glitches will
  see the pre-settings disarm fall to the S3 Mini bus for one boot; after
  `readSettings()` the override re-pins to 11/12. The 0x26 watchdog is the
  optional secondary (ADR-135; the ESP32 TWDT is primary), so this is tolerable.
- The TFT, RGB LED, IR receiver and IMU on the Pro are otherwise unused by the
  firmware. PIC reset shares GPIO 40 with the TFT SCK, so a reset shows brief
  noise on the (unused) display — cosmetic only.
- **Field verification required** (cannot be self-verified without hardware):
  on a real S3 Mini Pro the IMU probe must return the correct verdict (and not
  false-positive on a plain S3 Mini / OTGW32), the PIC must come up on 43/44 with
  reset on 40, and the 0x26 watchdog + OLED must work on 11/12.

## Related

- ADR-127 — combo single binary, runtime PIC/OTDirect boot detection (amended here)
- ADR-126 — fixed `esp32-classic` build (the S3 Mini Classic map this reuses)
- ADR-135 — TWDT primary, external 0x26 optional secondary watchdog
- ADR-113 — hardware-type codepath selection (Pro stays the `otgw-classic` slug)
- ADR-080 — binding-ADR / CI-gate meta-rule (guideline-level declaration above)
- `docs/hardware/PINOUT.md` §4 — combo pin maps
