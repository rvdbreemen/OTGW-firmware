# ADR-114 OLED runtime detection decoupled from the board flag

## Status

Accepted, 2026-06-02.

Guideline-level (per ADR-080): this ADR records a compile-gate-removal
decision and the runtime-detection discipline that replaces it. There is no
dedicated `evaluate.py` gate; the rule is enforced by the absence of
`HAS_OLED_CAPABLE` conditionals around the OLED code path and by the existing
runtime probe. If a regression reintroduces a compile-time OLED gate, a
`check_oled_runtime_only` gate could be added and this ADR promoted to binding.

## Status History

status_history:
  - date: 2026-05-29
    status: Proposed
    changed_by: Claude (TASK-757)
    reason: Records the decision to drop the compile-time HAS_OLED_CAPABLE gate in favour of the existing runtime I2C probe; implementation landed under TASK-757 with the button-ISR follow-up tracked as TASK-758
    changed_via: adr-kit
  - date: 2026-06-02
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted; TASK-757 shipped and the TASK-758 button-ISR shim follow-up is complete, so the known exception noted at proposal time is resolved
    changed_via: adr-kit

## Context

The SSD1306 OLED display code (`OLED.ino`) was wrapped in an outer
`#if defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE` guard, with matching gates
in `OTGW-firmware.ino` (I2C bring-up + `initOLED()`, and the `loopOLED()` call),
`networkStuff.ino` (`oledShowConfigMode()` in the WiFiManager config-mode
callback), `restAPI.ino` (the `oledpresent` device-info field), and the
`state.hw.bOLEDPresent` field in `Hardwaretypes.h`. `HAS_OLED_CAPABLE` was set
only for the OTGW32 (ESP32-S3) board, so on the classic ESP8266 OTGW the entire
OLED subsystem was compiled out — even though a user can physically wire an
SSD1306 to the ESP8266's I2C bus.

Two facts make the compile-time gate the wrong abstraction:

1. **The OLED is a hot-pluggable I2C peripheral on a shared bus.** The firmware
   already detects it at runtime: `initOLED()` calls `probeOLED()` (an I2C probe
   at address `0x3C`). On a hit it sets `oledPresent = true` and
   `state.hw.bOLEDPresent = true`; on a miss every entry point
   (`loopOLED()`, `oledShowConfigMode()`, …) early-returns on `!oledPresent`.
   Whether a display exists is therefore a *runtime* property, not a static
   board property.
2. **The display library and the I2C bus are available on both platforms.**
   `SSD1306Ascii` links for ESP32 and ESP8266 (verified in `.pio`), and the I2C
   bus is already brought up on the ESP8266 (the external watchdog drives it).
   `PIN_I2C_SDA`, `PIN_I2C_SCL`, and `PIN_BUTTON` are all defined for both
   boards in `boards.h`. Nothing about the OLED code is genuinely
   board-specific except the button-ISR path (see Consequences).

Gating a runtime-detected, cross-platform-linkable peripheral at compile time
means an ESP8266 user with an OLED physically connected gets nothing, for no
hardware reason.

Contrast with Ethernet (W5500): its SPI wiring is genuinely board-specific and
only present on the OTGW32, so it correctly stays `HAS_ETH_CAPABLE`-gated. The
distinction is wiring-determinism: Ethernet's pins/SPI are a static board fact;
the OLED is a peripheral a user can add to any board's existing I2C bus.

## Decision

OLED presence is a **runtime** property, detected by the existing I2C probe
(`probeOLED()` → `oledPresent` / `state.hw.bOLEDPresent`), and is **no longer
gated at compile time** by `HAS_OLED_CAPABLE`.

Concretely:

- The OLED code in `OLED.ino` compiles unconditionally on all boards.
- The I2C bring-up + `initOLED()` block and the `loopOLED()` call in
  `OTGW-firmware.ino` are unconditional.
- `oledShowConfigMode()` (config-mode portal screen) is called unconditionally;
  it is a no-op when no OLED was detected.
- `state.hw.bOLEDPresent` always exists and the `oledpresent` device-info field
  is reported on all boards.
- `HAS_OLED_CAPABLE` remains defined in `boards.h` as an *informational* board
  capability flag, but no application code branches on it.

## Alternatives Considered

1. **Keep the compile-time `HAS_OLED_CAPABLE` gate** (status quo, OTGW32-only).
   Rejected: the OLED is a hot-pluggable I2C peripheral the firmware already
   detects at runtime (`probeOLED()` at `0x3C`), and the `SSD1306Ascii` library +
   I2C bus link on both platforms — so a compile gate denies an ESP8266 user with
   a physically-wired OLED any function, for no hardware reason.
2. **Make it a user-facing setting** (`oledEnabled` toggle). Rejected: redundant
   with the existing runtime probe — presence is already self-detecting at `0x3C`;
   a manual toggle adds config surface for a question the hardware answers itself.
3. **Treat it like Ethernet (`HAS_ETH_CAPABLE`, stays board-gated).** Rejected as
   the model here precisely because the cases differ: the W5500's SPI wiring is a
   static board fact, whereas the OLED rides any board's existing I2C bus. The
   wiring-determinism distinction is why Ethernet keeps its compile gate and the
   OLED drops it.
4. **Runtime detection, no compile gate** (chosen): the OLED compiles
   unconditionally and every entry point early-returns on `!oledPresent`. Cost is
   flash + modest RAM on ESP8266, accepted as the price of genuine auto-detection.

## Consequences

- An SSD1306 connected to any supported board's I2C bus now works, with zero
  configuration, on the strength of the runtime probe alone.
- The OLED code path (and the `SSD1306Ascii` library) is now always linked into
  the ESP8266 image, costing flash and a small amount of RAM. The display is a
  text-only `SSD1306Ascii` driver (no 1 KB framebuffer), so the RAM cost is
  modest; the flash cost is bounded by the OLED rendering code plus the library.
  This is the price of runtime detection and is accepted: a feature that only
  works when explicitly compiled in is not really "auto-detected".
- **Known exception (ESP-abstraction violation):** the page-cycle button still
  branches on a raw `#if defined(ESP32)` in `OLED.ino` — the ESP32 path uses a
  GPIO ISR + FreeRTOS queue, the `#else` path uses legacy polling. This
  pre-existing conditional is out of scope here (this ADR only removes the
  `HAS_OLED_CAPABLE` gates) and is tracked for migration to a platform shim in
  TASK-758. No new raw platform conditionals were introduced by this change.

## Related Decisions

- ADR-113 (hardware-type as codepath-selection contract — same static-vs-runtime
  distinction applied to PIC availability)
- ADR-080 (binding ADRs must have a CI gate; this one is guideline-level)

## References

- TASK-757 (this change: decouple OLED compilation from `HAS_OLED_CAPABLE`)
- TASK-758 (follow-up: migrate the button-ISR `#if defined(ESP32)` branch to a
  platform shim — since completed)
- ESP platform abstraction rule (CLAUDE.md): the button branch was a known
  pre-existing exception at the time of writing, not a new violation.
