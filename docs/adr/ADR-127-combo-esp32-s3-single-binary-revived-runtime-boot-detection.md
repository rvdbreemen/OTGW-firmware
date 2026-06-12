# ADR-127 Combo ESP32-S3 single binary revived: runtime PIC/OTDirect boot detection (supersedes ADR-126)

## Status

Accepted, 2026-06-12

Supersedes ADR-126 and revives the ADR-125 design with the root causes of its
field failures fixed. Guideline-level (per ADR-080): enforcement posture is
identical to ADR-125/126, the existing ESP-abstraction boundary gate
(`evaluate.py::check_esp_abstraction_boundary`) enforces the flag-based
layering; no new dedicated gate is introduced. If runtime-mode-selection
regressions appear, a `check_combo_runtime_selection` gate should be added and
this ADR promoted to binding.

Amends ADR-113 §1 for the combo board class only, exactly as ADR-125 did: on
`BOARD_NODOSHOP_ESP32_COMBO` the `hardware_type` slug is resolved at runtime
from the detected mode instead of the compile-time `HW_TYPE_NAME`. The two
fixed ESP32-S3 boards and the ESP8266 board keep ADR-113's compile-time slug.

## Context

ADR-125 (2026-06-08) introduced a combo ESP32-S3 binary that linked both OT
engines and boot-detected PIC vs OTDirect. ADR-126 (2026-06-10) withdrew it
after field testing (alpha.166 through alpha.172) on four concrete objections
and replaced it with the fixed `esp32-classic` build. Between 2026-06-10 and
2026-06-12 every one of those objections was root-caused and resolved:

| ADR-126 objection | Resolution | Evidence |
|---|---|---|
| Boot probe starved the WiFi captive portal on fresh flash | WiFi-portal-first boot order: WiFi provisioning runs before any PIC/OTDirect bring-up. Introduced for the combo, retained for ALL boards by ADR-126 §5, and still in place. | TASK-853, commit db976c22 |
| Pin-map fragility, silent misdetection | Root-caused 2026-06-12: the `OTGWSerial` library translation unit could not see the `boards.h` pin macros and fell back to its UART defaults GPIO16/17 instead of the real Classic-on-S3 pins 43/44, so the boot probe was electrically incapable of seeing the PIC. Fixed by passing `rxPin`/`txPin` into the `OTGWSerial` constructor at instantiation. Additionally the Classic-on-S3 pin map is no longer an assumption: ADR-126/TASK-854 field-verified it for `esp32-classic` (PIC RST=12, UART=43/44, I2C=36/35, button=18, LED1=16, LED2=4) and the combo reuses exactly that verified map. | TASK-862 / bug-119, commit 77f084b0; `docs/hardware/PINOUT.md` |
| Classic 0x26 I2C watchdog unfed in PIC mode (combo forced `HAS_PIC_WATCHDOG=0`) | The combo now sets `HAS_PIC_WATCHDOG=1` and runs a dual-path watchdog: the ESP32 Task Watchdog Timer always, plus the 0x26 feed runtime-gated on `isPICEnabled()`. A combo detected as Classic feeds the external watchdog; detected as OTGW32 it skips a chip that is not there. | `OTGW-Core.ino:842-986` (three-way `#if`) |
| Runtime-indirection surface through otherwise compile-time code | Bounded and enumerated: `activeI2cSda()/activeI2cScl()`, `activeLed1()/activeLed2()`, `activeButton()`, `comboClassicPinsActive()`, runtime `hardwareTypeName()`. Every helper folds to a compile-time constant on the fixed boards (`HAS_RUNTIME_HW_DETECT=0` branch), so the indirection costs the fixed targets nothing. | `boards.h:329-333` (flag defaults to 0), `OTGW-firmware.h:591-597` |

With the probe's electrical root cause fixed (wrong UART pins, not a flawed
detection concept), the maintainer decided on 2026-06-12 to revive the
one-binary design. The fixed builds of ADR-126 remain in place during the
transition (Decision 2).

## Decision

The following is implemented on `feature-dev-2.0.0-otgw32-esp32-sat-support`
(TASK-864); this ADR records the design as built.

### 1. Board class `BOARD_NODOSHOP_ESP32_COMBO`

A combo section in `src/libraries/Platform/src/boards.h` that derives the
OTGW32 base (`boards.h:37` defines `BOARD_NODOSHOP_ESP32` when the combo macro
is set) and layers the field-verified Classic pins on top as `PIN_CLASSIC_*` +
`PIN_PIC_*` (`boards.h:290-326`). Flags: `HAS_PIC=1`, `HAS_DIRECT_OT=1`,
`HAS_PIC_WATCHDOG=1`, `HAS_RUNTIME_HW_DETECT=1` (`boards.h:301`).
`HAS_RUNTIME_HW_DETECT` defaults to 0 everywhere else; application code gates
runtime-detect machinery on this flag, never on the raw board macro
(ESP-abstraction rule).

### 2. Transitional fourth build target `esp32-combo`

A new PlatformIO env `[env:esp32-combo]` (`platformio.ini:230`) extends
`env:esp32`; `build.py` carries it as a fourth target with asset slug
`esp32-combo` (`build.py:133-136, 2413`). Both OT engines link: the env clears
the inherited `lib_ignore = OTGWSerial` (empty `lib_ignore`), and the per-env
ctags type stubs are NOT applied because the real enums of both engines are
visible. Measured flash usage is 98.4% of the 1.875 MB app slot with `-flto`
(build of 2026-06-12).

The `esp32` and `esp32-classic` fixed targets remain until the combo is
field-validated; a later ADR or commit collapses the two fixed ESP32-S3
targets into the combo. This ADR does not retire them.

### 3. PIC-probe-first boot detection, persisted to `settings.iBoardMode`

After `startWiFi()` (portal-first order per TASK-853), `setup()` in
`OTGW-firmware.ino` resolves the hardware mode:

- `settings.iBoardMode` (0=auto, 1=pic, 2=otdirect, `OTGW-firmware.h:658`) is
  the persisted selector per ADR-051; `state.hw.eMode` stays the transient
  live mode.
- `iBoardMode != 0`: apply directly, skip probing.
- `iBoardMode == 0`: probe for the PIC first. PIC found persists
  `iBoardMode = 1` (`OTGW-firmware.ino:314`); no PIC leads to `initOTDirect()`
  and, per the ADR-125 persistence semantics confirmed by maintainer decision
  2026-06-12, `iBoardMode = 2` is persisted as soon as `initOTDirect()`
  succeeds (`OTGW-firmware.ino:320`). Degraded or inconclusive results persist
  nothing, so the next boot re-detects.
- The detection result is written to a rolling `/bootdetect.log` on LittleFS
  (`OTGW-firmware.ino:143-144, 335`) and emitted as a `log_e` console line, so
  a misdetection is diagnosable from both the filesystem and the USB console.

### 4. Post-detection pin re-mux: `applyResolvedComboPins()`

The pre-detection default is the CLASSIC pin map, because the safety-critical
consumer is the 0x26 watchdog disarm at the top of `setup()`: a Classic board
must be able to disarm its hardware watchdog before anything slow happens.
Stray I2C writes on an OTGW32 are NACKed no-ops on floating pins, so the
Classic-first default is safe on both boards. After detection,
`applyResolvedComboPins()` (`OTGW-firmware.ino:209`, called at line 328)
re-pins the I2C bus (`Wire.end()` + `Wire.begin()` on the resolved SDA/SCL),
re-attaches the ledc LED channels via `reinitLedDriver()`
(`OTGW-firmware.ino:459`), and re-runs `initOLED()` including the button
`pinMode`.

### 5. Dual-path watchdog on the combo

`OTGW-Core.ino` carries a three-way `#if` (`OTGW-Core.ino:842-986`):

1. `HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT` (esp8266, esp32-classic):
   external 0x26 feed only, unchanged.
2. `HAS_PIC_WATCHDOG && HAS_RUNTIME_HW_DETECT` (combo): ESP32 TWDT always,
   0x26 feed runtime-gated on `isPICEnabled()`.
3. `!HAS_PIC_WATCHDOG` (OTGW32): ESP32 TWDT only, unchanged.

### 6. Runtime mutual-exclusion gates

The PIC and OTDirect engines never run concurrently:

- The `handlePICSerial()` drain and the 60-second `PR=A` retry are gated on
  `!isOTDirectEnabled()` (`OTGW-Core.ino:3014, 4641`).
- `initOTDirect()` does not clobber `HW_MODE_PIC` (fixes the ADR-125 §4
  concern about `OTDirect.ino` setting the mode unconditionally).
- W5500/Ethernet is explicitly skipped in PIC mode:
  `if (!isPICEnabled()) initEthernet()` in `setup()` (OTGW-firmware.ino). The
  W5500 VERSION-register probe (`Ethernet.ino:176-192`) clocks GPIO12
  (`PIN_SPI_SCK`, `boards.h:151`), which on a Classic PCB is the PIC reset
  hole — the gate keeps the probe off the PIC, the symmetric counterpart of
  the telnet gate below. (The gate existed in the ADR-125 implementation, was
  dropped with the rest of the combo machinery in ec55a1fb, and was restored
  during TASK-864 after the ADR verification pass caught the omission.) A
  combo in degraded mode still self-limits: Classic wiring cannot answer the
  probe, `state.hw.bEthernetPresent` stays false and `loopEthernet()` returns
  immediately (`Ethernet.ino:203-204`). On the fixed OTGW32 `isPICEnabled()`
  is compile-time false, so behaviour there is unchanged.
- The telnet `p` manual PIC reset is blocked in OTDirect mode
  (`handleDebug.ino:322-331`): `PIC_RST` is GPIO12, which is the SPI SCK on an
  OTGW32, so a manual PIC reset there would glitch the Ethernet bus.

### 7. Banner-recovery re-detect net

A combo boot that ends degraded but later recovers via the PIC banner
(banner-recovery path, TASK-861) persists `iBoardMode = 1` at that point
(`OTGW-Core.ino:4562`). A flaky first probe therefore converges to the correct
cached mode instead of re-probing forever.

### 8. Manual override

`boardmode` is a settable device setting: REST (in `knownSettings` and
`sendDeviceSettings`, combo-only; `restAPI.ino:2836, 3085`,
`settingStuff.ino:265, 788`) and a web-UI dropdown in the System settings
group with a reboot-confirm (`index.js:6089, 6214, 6952, 7099`). Setting it
back to auto (0) re-detects on the next boot. This is the recovery path for
any persisted misdetection.

### 9. Runtime `hardwareTypeName()` on the combo (amends ADR-113 §1)

`hardwareTypeName()` (`OTGW-firmware.h:591-597`) returns `otgw-classic` when
`state.hw.eMode == HW_MODE_PIC` and `otgw32` otherwise, but only under
`HAS_RUNTIME_HW_DETECT`; fixed boards keep the compile-time `HW_TYPE_NAME`.
HA discovery and the REST API publish the resolved identity, so a combo in PIC
mode advertises Classic identity (ADR-124 topology keys off it).

### 10. Deferred: compile-time HA OtCore suffix (TASK-847 carry-over)

`HA_OTCORE_NAME`/`HA_OTCORE_SUFFIX` remain compile-time, so a combo running in
OTDirect mode advertises its OtCore device as "pic". Cosmetic only (entity
values and topics are correct), documented in `MQTTstuff.h`, tracked as a
follow-up task. Accepted as a known gap rather than blocking the revival.

## Alternatives Considered

1. **Keep ADR-126's fixed builds permanently.** Safe, but reinstates the
   build-to-hardware matching burden ADR-125 set out to remove, and the
   objections that justified ADR-126 are demonstrably resolved (see Context
   table). Rejected as the end state; retained as the transition state
   (Decision 2).
2. **Revive the combo and immediately retire `esp32` / `esp32-classic`.**
   Rejected: the combo has not been field-validated yet; collapsing the fixed
   targets before validation would remove the known-good fallback images.
3. **Re-litigate the detection mechanism (strap GPIO, portal board pick).**
   Rejected for the same reasons as in ADR-125: no strap exists on the PCBs,
   and the portal pick survives as the manual override (Decision 8), not the
   primary path. The probe failure was a pin-plumbing bug, not a concept flaw.

## Consequences

**Positive**

- One ESP32-S3 image runs on both the Classic PCB and the OTGW32; detection is
  paid once and cached in `iBoardMode`.
- The Classic-on-S3 configuration keeps its 0x26 hardware watchdog feed (the
  gap that helped sink ADR-125 is closed).
- Fixed-board binaries are unchanged: every runtime helper folds to a
  compile-time constant when `HAS_RUNTIME_HW_DETECT=0`.
- Misdetections are observable (`/bootdetect.log`, `log_e`) and recoverable
  (boardmode override, banner-recovery net) without a reflash.

**Negative / risks**

- **Flash budget**: the combo image measures 98.4% of the 1.875 MB app slot,
  roughly 30 KB headroom. Code growth on the combo target is tightly
  constrained; any sizeable feature must be measured against this target
  first. *Mitigation*: `-flto` is already on; if the slot overflows, the
  partition table or per-feature gating must be revisited in a follow-up ADR.
- **Persistence trade-off (ADR-125 semantics)**: persisting `iBoardMode = 2`
  as soon as `initOTDirect()` succeeds means a Classic board with a dead or
  absent PIC at first boot can be cached as OTDirect. *Mitigation*: the
  boardmode override (Decision 8) and the banner-recovery net (Decision 7)
  both correct it; degraded boots persist nothing.
- Four build targets instead of three until field validation completes:
  more CI time, more release assets.

**Gate before collapse**

The fixed `esp32` and `esp32-classic` targets are only collapsed into the
combo after field validation on both physical boards (PIC detection on the
Classic PCB, OTDirect bring-up incl. Ethernet on the OTGW32, override and
recovery paths). That collapse is a separate decision and gets its own ADR or
an explicit amendment commit.

## Related Decisions

- **ADR-125** (superseded by ADR-126; its design is revived here with fixes).
- **ADR-126** (superseded by this ADR; its `esp32-classic` board class,
  verified pin map, and portal-first boot order are all retained).
- **ADR-113** (hardware-type codepath-selection contract; §1 amended for the
  combo class only, runtime slug; everything else stands).
- **ADR-080** (gate policy; this ADR is guideline-level, see Status).
- **ADR-051** (settings/state architecture; `iBoardMode` is persisted
  config, `state.hw.eMode` stays transient).
- TASK-864 (implementation), TASK-845/848/853/854/861/862 (combo and
  esp32-classic field-debugging history).

## References

- Commit db976c22 (TASK-853, WiFi-portal-first boot order).
- Commit 77f084b0 (TASK-862/bug-119, PIC UART pins passed to the `OTGWSerial`
  constructor; the probe's electrical root cause).
- `src/libraries/Platform/src/boards.h:290-333` (combo board class, flags,
  `PIN_CLASSIC_*` map, `HAS_RUNTIME_HW_DETECT` default).
- `platformio.ini:230` (`[env:esp32-combo]`), `build.py:128-136`
  (combo target + slug).
- `src/OTGW-firmware/OTGW-firmware.ino:143-144, 209, 314-335, 459`
  (bootdetect log, `applyResolvedComboPins()`, persistence,
  `reinitLedDriver()`).
- `src/OTGW-firmware/OTGW-Core.ino:842-986` (three-way watchdog `#if`),
  `:3014, 4641` (OTDirect gates), `:4562` (banner-recovery persist).
- `src/OTGW-firmware/OTGW-firmware.h:591-597` (`hardwareTypeName()`),
  `:658, 687-697` (`iBoardMode` and its consumers).
- `src/OTGW-firmware/restAPI.ino:2836, 3085`, `settingStuff.ino:265, 788`,
  `data/index.js:6089-7099` (boardmode override surfaces).
- `docs/hardware/PINOUT.md` (field-verified D1-mini footprint to S3 Mini GPIO
  map).
