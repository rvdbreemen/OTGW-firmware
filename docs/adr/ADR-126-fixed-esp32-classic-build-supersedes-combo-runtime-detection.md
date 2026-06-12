# ADR-126 Fixed esp32-classic build supersedes combo runtime detection

## Status

Superseded by ADR-127, 2026-06-12 (was: Accepted, 2026-06-10)

Supersedes ADR-125. ADR-127 revives the ADR-125 combo design with the
objections below root-caused and fixed; the `esp32-classic` board class, its
verified pin map, and the portal-first boot order defined here are retained. Guideline-level (per ADR-080): this ADR removes a board
class and a boot discipline; the existing ESP-abstraction boundary gate
(`evaluate.py::check_esp_abstraction_boundary`) continues to enforce the
flag-based layering. No new dedicated gate is introduced.

## Context

ADR-125 introduced a "combo" ESP32-S3 binary that linked both OT engines
(PIC `OTGWSerial` + OTDirect) and selected one at boot by probing for the PIC,
persisting the result in `settings.iBoardMode`. Field testing on real hardware
(alpha.166 through alpha.172) showed the approach does not converge:

- The boot-time probe interacted badly with WiFi provisioning: the captive
  portal was starved or never reachable on a fresh flash (TASK-845, TASK-853
  mitigations: UART teardown before the portal, portal-first boot order).
- The detection result depended on pin-mapping assumptions for the LOLIN S3
  Mini in the Classic D1-mini socket that took several field iterations to
  pin down (PIN_PIC_RST 5 → 12), and a wrong probe result silently
  misclassified the board.
- The dual pin map forced runtime indirection (`activeI2cSda()`, runtime
  `hardwareTypeName()`, persisted board-mode override) through code that is
  otherwise fully compile-time, increasing surface area for exactly one
  hardware combination.
- The combo could not feed the Classic PCB's external 0x26 I2C watchdog
  (`HAS_PIC_WATCHDOG=0` was forced because the OTGW32 lacks the chip), leaving
  the Classic hardware watchdog unfed in PIC mode.

The maintainer decided on 2026-06-10 to drop runtime detection entirely.

## Decision

1. **Every board is a fixed compile-time class.** There is no runtime hardware
   detection, no persisted board-mode selector, and no dual pin map. The
   firmware ships as three builds:

   | Build | Board macro | Hardware | OT engine |
   |---|---|---|---|
   | `esp8266` | `BOARD_NODOSHOP_ESP8266` | OTGW Classic + Wemos D1 mini | PIC |
   | `esp32-classic` | `BOARD_NODOSHOP_ESP32_CLASSIC` | OTGW Classic + LOLIN S3 Mini | PIC |
   | `esp32` | `BOARD_NODOSHOP_ESP32` | OTGW32 (OT-Thing PCB) | OTDirect |

2. **`BOARD_NODOSHOP_ESP32_CLASSIC`** is a standalone section in `boards.h`
   with the Classic signals mapped to the S3 GPIOs at the matching D1-mini
   footprint holes (verified against the official LOLIN S3 Mini pin diagram +
   Arduino variant, see `docs/hardware/PINOUT.md`): PIC RST=12, UART=43/44,
   I2C=36/35, button=18, LED1=16, LED2=4. Flags: `HAS_PIC=1`,
   `HAS_PIC_WATCHDOG=1` (the Classic PCB carries the 0x26 watchdog),
   `HAS_DIRECT_OT=0`, `HAS_ETH_CAPABLE=0`, `HAS_OLED_CAPABLE=1`, SAT enabled
   with the ESP32 buffer sizing.

3. **`HAS_RUNTIME_HW_DETECT` is retired.** The flag remains `#define`d to 0 in
   `boards.h` so stale references fail soft; no application code may gate on
   it. The combo-only machinery is removed: boot detection block,
   `/bootdetect.log`, `settings.iBoardMode`, `activeI2cSda()/activeI2cScl()`,
   runtime `hardwareTypeName()`.

4. **`hardwareTypeName()` is compile-time again** (`HW_TYPE_NAME` per board
   class, ADR-113 semantics): `esp8266` and `esp32-classic` both advertise
   `otgw-classic`; `esp32` advertises `otgw32`.

5. The **WiFi-portal-first boot order** introduced for the combo (TASK-853)
   is kept for all boards: WiFi configuration runs before PIC/OTDirect
   bring-up in `setup()`. It is a good ordering independent of detection.

## Consequences

- Users must pick the correct firmware image for their hardware; flashing the
  wrong ESP32-S3 image on the wrong board yields a non-working gateway (no
  auto-correction). The flash zips are named per target.
- The S3-in-Classic configuration gains the external 0x26 watchdog feed that
  the combo could not provide, and loses ~no functionality: SAT, BLE room
  sensors and the weather forecast all run on the S3's RAM budget.
- `runtime` mutual-exclusion gates added in ADR-125 Phase 2
  (`isPICEnabled()` / `isOTDirectEnabled()` guards) remain in place: they are
  correct and cheap under compile-time flags (each returns a constant false
  on boards without the corresponding hardware).
- HA discovery identity is static per build again; the ADR-124 seven-device
  topology keys off the compile-time `HW_TYPE_NAME`.
- The `esp32-combo` PlatformIO env, build target and asset set are replaced
  by `esp32-classic` equivalents.

## Related

- ADR-125 (superseded): combo board, runtime boot detection.
- ADR-113 (board-class slug semantics; the `hardwareTypeName()` contract
  reverts to its original compile-time form).
- ADR-080 (gate policy; this ADR is guideline-level).
- TASK-854 (implementation), TASK-845/848/853 (combo field-debugging history).
- `docs/hardware/PINOUT.md` (verified D1-mini footprint → S3 Mini GPIO map).
