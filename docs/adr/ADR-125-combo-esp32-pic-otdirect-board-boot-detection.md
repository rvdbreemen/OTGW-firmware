# ADR-125 Combo ESP32-S3 board: one binary, runtime PIC/OTDirect selection

## Status

Superseded by ADR-126, 2026-06-10 (was: Accepted, 2026-06-08).
Forward note: the design recorded here was revived by ADR-127 (2026-06-12)
after the boot-probe failures were root-caused to a pin-plumbing bug
(TASK-862/bug-119), not to the detection concept.

Guideline-level (per ADR-080): this ADR defines a board class and a boot
discipline. Enforcement is the existing ESP-abstraction boundary gate
(`evaluate.py::check_esp_abstraction_boundary`) plus field validation; no new
dedicated gate is introduced. If runtime-mode-selection regressions appear, a
`check_combo_runtime_selection` gate should be added and this ADR promoted to
binding.

Amends ADR-113 (Hardware-type as codepath-selection contract) for the combo
board class only. ADR-113's separation of machine-identity from runtime
liveness stands; ADR-125 changes *when* the identity is resolved (boot
detection, not compile time) for one new board class and lifts its
"OTGW32 ... there never can be a PIC" scope statement to "PIC-less by default,
unless the combo board detects one".

## Context

The 2.0.0 line ships two mutually-exclusive firmware builds, selected at compile
time in `boards.h`:

| Board (`boards.h`)       | `HAS_PIC` | `HAS_DIRECT_OT` | OT handling |
|--------------------------|-----------|-----------------|-------------|
| `BOARD_NODOSHOP_ESP8266` | 1         | 0               | PIC co-processor |
| `BOARD_NODOSHOP_ESP32`   | 0         | 1               | OTDirect (native GPIO) |

`HAS_PIC` and `HAS_DIRECT_OT` are mutually exclusive by construction. ADR-113
codifies a compile-time `HW_TYPE_NAME` slug from this choice and states OTGW32
"is a PIC-less product ... there never can be one".

A new hardware reality breaks that assumption: an **ESP32-S3 that can be wired to
an OTGW Classic PCB (PIC present)** *or* run as an **OTGW32 (no PIC, OTDirect)**.
The user wants **one binary** that runs on either board and decides which OT
path to drive at boot, instead of two separate builds the installer must match
to the hardware by hand.

The codebase is already ~70% prepared for this:

- `OTGWSerial.cpp` (the PIC serial driver) already compiles on ESP32: it uses
  `Serial1` and reads `PIN_PIC_RX`/`PIN_PIC_TX`/reset pin
  (`OTGWSerial.cpp:838-847`). Only `lib_ignore = OTGWSerial` on the esp32 env
  keeps it out of that build.
- `state.hw.eMode` (`HW_MODE_PIC` / `HW_MODE_OT_DIRECT` / `HW_MODE_DEGRADED` /
  `HW_MODE_UNKNOWN`, `Hardwaretypes.h`), the `detectPIC()` ETX handshake
  (`OTGW-Core.ino:507-520`), and the runtime helpers `isPICEnabled()` /
  `isOTDirectEnabled()` (`OTGW-firmware.h:468-483`) already exist.
- Two hot paths — `addCommandToQueue()` and `sendPICSerial()` — already gate on
  `isPICEnabled()` at runtime and fall through to OTDirect; the intended pattern
  is half-built.

### The keystone constraint: two pin maps, overlapping GPIOs

The two physical boards **reuse the same ESP32-S3 GPIO numbers for different
jobs** (illustrative; exact Classic-on-S3 numbers are a hardware fact recorded
in `boards.h`):

| GPIO | Classic-on-S3 (PIC) | OTGW32 (OTDirect) |
|------|---------------------|-------------------|
| 14   | (PIC reset, candidate) | `PIN_SPI_CS` (W5500) |
| 16   | PIC UART TX (candidate) | `PIN_SPI_RST` (W5500) |
| 17   | PIC UART RX (candidate) | `PIN_I2C_SCL` (OLED) |

A single compiled binary holds **both** pin sets. The conflict is *runtime-only*:
each physical PCB wires a given GPIO exactly one way, and boot detection
initializes **only** the subsystem that matches the detected board, so each
shared GPIO has exactly one live owner per boot. This is the core design
invariant.

### Why peripheral-presence detection is rejected

W5500 Ethernet and the SSD1306 OLED are **optional** on OTGW32. Their absence
does not prove "not an OTGW32", so probing for them cannot discriminate the
boards. The only reliable, always-present discriminator is the PIC itself.

## Decision

### 1. New board class `BOARD_NODOSHOP_ESP32_COMBO`

A third `boards.h` section with `HAS_PIC=1` **and** `HAS_DIRECT_OT=1`, defining
both pin sets (the OTGW32 peripheral map *and* the Classic-on-S3 PIC
reset/UART pins) and the OTGW32 capability flags (`HAS_ETH_CAPABLE`,
`HAS_OLED_CAPABLE`, `HAS_SAT*`, ...). The mutual-exclusion of `HAS_PIC` /
`HAS_DIRECT_OT` assumed elsewhere is replaced by runtime selection (Decision 4).

### 2. Watchdog capability split: `HAS_PIC_WATCHDOG`

The external I2C watchdog at address `0x26` is OTGW-Classic-ESP8266 hardware. It
is currently gated by `#if HAS_PIC` (`OTGW-Core.ino:60,71`). Introduce a
distinct `HAS_PIC_WATCHDOG` flag (ESP8266 Classic = 1, combo = 0, OTGW32 = 0) so
the combo build compiles the PIC serial path **without** feeding a watchdog that
does not exist on its boards. `HAS_PIC` reverts to meaning "PIC serial driver is
compiled in", not "the Classic watchdog board is present".

### 3. Build target `env:esp32-combo`

A new PlatformIO env that `lib_ignore`s **neither** `OTGWSerial` **nor**
`OpenTherm`, so both OT engines link into one image. The per-env ctags
type-stub workarounds (`-DOTGWFirmware=int` on esp32, `-DOTDirectMode=int` on
esp8266) are **not** applied to this env: with both libraries compiled, the real
enums are visible and the stubs would shadow them. `build.py` gains a combo
build target.

### 4. PIC-probe-first boot detection, persisted to LittleFS

Hardware mode is resolved once and cached:

- The **persisted selector** is config: `settings.hw.iBoardMode`
  (`0=auto, 1=pic, 2=otdirect`), living in `OTGWSettings` per ADR-051. The
  **live operational mode** stays transient in `state.hw.eMode`.
- Boot sequence (after `LittleFS.begin()` + `readSettings()`, **not** before —
  the current `setup()` ordering runs detection before settings load and must be
  reordered):
  1. `iBoardMode != auto` → apply directly to `state.hw.eMode`, initialize only
     that subsystem, **skip probing**.
  2. `iBoardMode == auto` → run `detectPIC()` (reset PIC, wait ~1 s for ETX):
     - ETX seen → `HW_MODE_PIC`; persist `iBoardMode = pic`.
     - no ETX → `HW_MODE_OT_DIRECT`, `initOTDirect()`; persist
       `iBoardMode = otdirect`.
     - bus dead / inconclusive → `HW_MODE_DEGRADED`; **persist nothing**
       (re-detect next boot).
- `initOTDirect()` must set `eMode = HW_MODE_OT_DIRECT` **only when
  `!isPICEnabled()`** — today (`OTDirect.ino:842`) it sets it unconditionally and
  would clobber a PIC-detected mode in a combo build.

**Safety of the probe.** `detectPIC()` drives only the PIC pins
(`PIN_PIC_RST/RX/TX`). On an OTGW32 those GPIOs are W5500/OLED lines that
`initOTDirect()` reconfigures immediately after a failed probe; a momentary
pre-init pulse on lines that are re-initialized anyway is benign. The OT bus
pins, step-up enable, and bypass relay are never touched by the probe.

### 5. Manual override

A REST endpoint + web-UI control writes `settings.hw.iBoardMode` and reboots,
so a misdetection is correctable without a reflash. Setting it back to `auto`
re-detects on next boot.

### 6. Runtime `hardware_type` (amends ADR-113 point 1)

On the combo board class only, `hardwareTypeName()` resolves at runtime from
`state.hw.eMode` (`otgw-classic` in PIC mode, `otgw32` in OTDirect mode) instead
of returning a compile-time `HW_TYPE_NAME`. For the two fixed board classes
`hardware_type` stays compile-time as ADR-113 specifies. The capability-vs-
liveness invariant of ADR-113 point 3 is preserved: identity follows the
detected board; `isPICEnabled()` / `isOTDirectEnabled()` remain the liveness
signals. HA discovery must publish the resolved identity so a combo board in PIC
mode advertises Classic identity and in OTDirect mode advertises OTGW32 identity
(see ADR-124 seven-device topology; wrong identity silently breaks HA).

### Scope

2.0.0 only. The combo board exists solely on the 2.0.0 line; `dev` (1.5.x) is
Classic-ESP8266-only where this is moot. Not a cross-worktree change.

## Alternatives Considered

1. **Two separate builds, installer matches build to board** (status quo).
   Rejected: the user's goal is explicitly one binary that self-configures; a
   build/hardware mismatch flashes a non-working image with no runtime recovery.
2. **Peripheral-presence auto-detect** (probe OLED `0x3C` + W5500). Rejected:
   both are optional on OTGW32, so absence is not evidence of board class.
3. **First-boot manual board pick in the captive portal** (no probing).
   Viable and 100% safe, but loses the "just works" first boot the user asked
   for. Kept as the *manual override* (Decision 5), i.e. the safety net rather
   than the primary path.
4. **Hardware strap/ID GPIO** read at boot for a deterministic board id.
   Cleanest auto-detect, but requires the boards to carry a dedicated strap
   (a hardware change). Not available on the existing PCBs.
5. **PIC-probe-first + persist + manual override** (chosen): reliable
   discriminator (the PIC), one-time cost, bounded probe side-effects,
   recoverable from a wrong guess.

## Consequences

**Positive**

- One image runs on both boards; the installer no longer has to match a build to
  the hardware.
- Detection cost is paid once; later boots read the cached mode and skip the
  ~1 s PIC probe.
- Reuses the existing `eMode` state machine, `detectPIC()`, and the
  `isPICEnabled()` / `isOTDirectEnabled()` helpers; the remaining work is mostly
  converting compile-time `#if` mutual-exclusion branches to runtime `if`.

**Negative / costs**

- Both OT engines link into one image: larger flash + RAM footprint than either
  single-target build. Acceptable on the 4 MB ESP32-S3.
- `~6` compile-time `#if HAS_PIC/#else` exclusivity branches must become runtime
  `if (isPICEnabled())` (tracked as Phase 2). Until converted, a combo build
  would mis-route on the unconverted branch.
- `hardware_type` becomes runtime on the combo class, adding a small branch to a
  previously compile-time-folded helper and a re-publish of HA identity once the
  mode is known.

**Neutral**

- The two fixed board classes are unchanged; `HAS_PIC_WATCHDOG` makes the
  previously-implicit "PIC implies the 0x26 watchdog board" explicit.

## Related Decisions

- **Amends ADR-113** (hardware-type codepath-selection contract): runtime-
  resolved `hardware_type` and lifted "never a PIC on ESP32" scope for the combo
  class; the rest of ADR-113 (picavailable deprecation, frontend migration,
  liveness invariant) stands.
- ADR-051 (settings/state architecture): `iBoardMode` is persisted config in
  `settings.hw`; `eMode` stays transient in `state.hw`.
- ADR-079 (per-component type headers): `OTGWHardwareMode` / `HardwareSection`
  live in `Hardwaretypes.h`.
- ADR-124 (HA seven-device topology): combo HA identity must follow the resolved
  mode.
- ESP-abstraction boundary (CLAUDE.md / TASK-739): all platform conditionals
  stay in `boards.h` / `platform_*.h`; application code uses `HAS_*` flags and
  the runtime helpers.

## References

- TASK-845 (Phase 1 implementation: ADR + combo board + persisted boot
  detection).
- `OTGWSerial.cpp:838-847` (ESP32 `Serial1` PIC path, already present).
- `OTGW-Core.ino:507-520` (`detectPIC()` ETX handshake).
- `OTGW-firmware.h:468-483` (`isPICEnabled()` / `isOTDirectEnabled()`).
