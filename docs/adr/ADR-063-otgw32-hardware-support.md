# ADR-063: OTGW32 Hardware Support — Dual Build Targets with Runtime Feature Detection

**Status:** Accepted
**Date:** 2026-04-03

## Context

The Nodoshop OTGW product line now includes two hardware variants:

1. **PIC-based OTGW** (ESP8266): A PIC16F1847 co-processor handles all OpenTherm bus communication over UART at 9600 baud. The ESP8266 reads text lines like `T80401234` (direction char + 8 hex digits of a 32-bit OT frame).
2. **OT-direct OTGW32** (ESP32-S3): The ESP32-S3 drives the OpenTherm bus directly via GPIO using the `opentherm_library` (Phunkafizer fork of ihormelnyk/OpenTherm). Interrupt-driven Manchester encoding/decoding. No PIC chip present.

The firmware should support both hardware variants from a single codebase to avoid maintaining two divergent forks.

### Key constraints:
- **MCU is orthogonal to OTGW hardware** — ESP8266/ESP32 is the processor; PIC/OT-direct is the OpenTherm interface. These dimensions must not be conflated.
- **GPIO pin conflicts make a single ESP32 binary impossible** — The OTGW32 uses GPIO 21 for OT Master IN and GPIO 17 for I2C SCL, while a hypothetical ESP32+PIC board would use GPIO 21 for I2C SDA and GPIO 17 for PIC TX. Electrical conflicts prevent runtime switching.
- **No ESP32+PIC hardware exists** — The Nodoshop ESP32 board is always OT-direct. A hypothetical ESP32+PIC combination would add complexity for a non-existent product.
- **ESP8266 has ~40KB usable RAM** — All code must use PROGMEM, static buffers, and avoid String class in hot paths.
- **The PIC serial link must not be disturbed** — Never write to Serial after OTGW initialization on PIC boards.
- **OT-Thing (OTGW32/OT-Thing project) already validates the approach** — Its TCP server on port 25238 outputs PIC-compatible text lines (`T`, `B`, `R`, `A` prefix + hex frame), confirming the frame bridge pattern works.

## Alternatives Considered

1. **Separate firmware fork for OTGW32** — Rejected: duplicates MQTT, REST, WebSocket, settings, and UI code. Maintenance burden grows linearly with features.
2. **Single ESP32 binary supporting both PIC and OT-direct** — Rejected: GPIO pin conflicts between board variants make this physically impossible. Would require runtime pin remapping that conflicts with hardware electrical design.
3. **Three build targets (ESP8266, ESP32+PIC, ESP32+OT-direct)** — Rejected: no ESP32+PIC hardware exists. Adds complexity for a hypothetical product.
4. **Two build targets with compile-time feature flags** — Chosen: ESP8266 (PIC) and ESP32-S3 (OT-direct). Compile-time selects pin map and code inclusion; runtime detects what's actually connected.

## Decision

### Two build targets from a single codebase

| Build Target | MCU | OTGW Hardware | PlatformIO env |
|---|---|---|---|
| ESP8266 PIC | ESP8266 | PIC16F1847 co-processor | `[env:esp8266]` |
| OTGW32 | ESP32-S3 | Direct GPIO OpenTherm | `[env:otgw32]` |

`python build.py` (no arguments) builds **both** binaries by default.

### Compile-time feature flags

Two flags in `boards.h` control code inclusion:

| Flag | ESP8266 build | OTGW32 build | Purpose |
|---|---|---|---|
| `HAS_PIC` | 1 | 0 | Include OTGWSerial, PIC detection, PIC firmware upgrade, I2C watchdog |
| `HAS_DIRECT_OT` | 0 | 1 | Include OTDirect.ino, opentherm_library, step-up converter, GPIO OT pins |

Additional capability flags (`HAS_OLED_CAPABLE`, `HAS_ETH_CAPABLE`) control whether peripheral detection code is compiled in. The actual presence is determined at runtime.

### Runtime guard functions

```cpp
inline bool isPICEnabled();       // true only when HAS_PIC=1 AND PIC detected at boot
inline bool isOTDirectEnabled();  // true only when HAS_DIRECT_OT=1 AND OT bus probed OK
```

These replace the existing `isPICEnabled()` pattern with a two-level guard: compile-time inclusion + runtime detection.

### Frame bridge pattern

The OT-direct driver converts 32-bit GPIO frames to the same 9-character text format the PIC produces:

```cpp
// Boiler response:  snprintf(buf, sizeof(buf), "B%08lX", frame);
// Thermostat request: snprintf(buf, sizeof(buf), "T%08lX", frame);
// Then: processOT(buf, 9);
```

This reuses the entire existing parser, MQTT publisher, REST API, WebSocket streamer, and logging stack without modification.

### Hardware mode enum

```cpp
enum OTGWHardwareMode : uint8_t {
  HW_MODE_UNKNOWN = 0,
  HW_MODE_PIC,
  HW_MODE_OT_DIRECT,
  HW_MODE_DEGRADED    // hardware expected but not responding
};
```

Stored in `state.hw.eMode`, set during boot-time detection.

### Board definition: `BOARD_NODOSHOP_OTGW32`

New board section in `boards.h` with ESP32-S3 pin map from OT-Thing NODO variant. Pin numbers are based on the OT-Thing hardware definition and should be verified against production Nodoshop OTGW32 hardware.

### Build system changes

- **`platformio.ini`**: Replace `[env:esp32]` with `[env:otgw32]` targeting `esp32-s3-devkitm-1`
- **`build.py`**: Default target changed from `esp8266` to building both ESP8266 + OTGW32. Individual targets selectable via `--target esp8266` or `--target otgw32`.

## Consequences

### Benefits:
- Single codebase for both hardware variants — shared MQTT, REST, WebSocket, settings, UI code
- Frame bridge pattern requires zero changes to the existing OT message processing pipeline
- Runtime detection allows graceful degradation (e.g., boiler disconnected, OLED missing)
- Compile-time flags eliminate dead code from each binary — no RAM/flash overhead for unused features
- `python build.py` produces both firmware images in one command

### Trade-offs:
- `#if HAS_PIC` / `#if HAS_DIRECT_OT` guards add conditional compilation complexity
- New `OTDirect.ino` file (~200 lines) must be maintained alongside PIC code paths
- Pin numbers need verification against production OTGW32 hardware (currently based on OT-Thing reference design)
- `opentherm_library` dependency added for OTGW32 builds only

### Risks:
- ESP32-S3 interrupt timing for OpenTherm Manchester encoding is unverified on this firmware (proven on OT-Thing)
- Step-up converter enable sequence timing may need tuning for actual hardware
- OT slave interface (thermostat emulation) is more complex than master-only mode — may be phased

## Related Decisions

- **ADR-061**: Unified ESP8266/ESP32 platform abstraction layer — provides the `platform.h` shim layer this builds on
- **ADR-060**: PIC availability guard pattern — `isPICEnabled()` pattern extended here

## References

- **OT-Thing project**: https://github.com/OTGW32/OT-Thing — reference implementation for direct GPIO OpenTherm on ESP32
- **OpenTherm Library**: ihormelnyk/OpenTherm (stock library pinned in platformio.ini); Phunkafizer fork adds HW timer support
- **Plan document**: `docs/plan/OTGW32_INTEGRATION_PLAN.md`
