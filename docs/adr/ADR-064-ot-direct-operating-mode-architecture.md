# ADR-064: OT-Direct Operating Mode Architecture

**Status:** Accepted
**Date:** 2026-04-04

## Context

The OTGW32 (ESP32-S3) uses direct GPIO OpenTherm instead of a PIC co-processor. Unlike the PIC-based OTGW, which is always a gateway sitting between thermostat and boiler, the OT-direct hardware can operate in several distinct modes depending on deployment scenario:

- **Gateway**: Intercept and optionally modify frames between thermostat and boiler (classic OTGW behavior).
- **Monitor**: Passively listen to thermostat-boiler traffic without injecting frames.
- **Bypass**: Relay all frames unmodified between thermostat and boiler (transparent pass-through).
- **Master**: Act as the sole OpenTherm master, polling the boiler directly with no thermostat connected.
- **Loopback**: Internal test mode where master TX is looped to master RX for diagnostics.

Each mode requires different hardware configuration: the relay position, step-up converter enable state, and slave interface activity all change depending on the active mode. Without an explicit mode enum and centralized transition function, mode changes would scatter hardware manipulation across multiple call sites, risking partial state transitions and hardware conflicts.

### Alternatives considered

1. **Boolean flags only** (`isGateway`, `isMonitor`, etc.) -- Rejected: mutually exclusive modes represented as independent booleans invite invalid combinations (e.g., Gateway + Master simultaneously). An enum enforces mutual exclusivity at the type level.
2. **Runtime string-based mode selection** -- Rejected per project guidelines: internal discriminator strings are fragile on ESP8266/ESP32 and can hide RAM-vs-flash pointer bugs. Typed enum is safer and cheaper.
3. **No explicit mode; infer from settings** -- Rejected: hardware state transitions need a single source of truth. Deriving mode from scattered settings booleans makes it hard to reason about what hardware is actually doing.

## Decision

### `OTDirectMode` enum

```cpp
enum OTDirectMode : uint8_t {
  OT_MODE_GATEWAY  = 0,  // Intercept thermostat <-> boiler traffic
  OT_MODE_MONITOR  = 1,  // Passive listen only
  OT_MODE_BYPASS   = 2,  // Transparent relay, no modification
  OT_MODE_MASTER   = 3,  // Standalone master, no thermostat
  OT_MODE_LOOPBACK = 4   // Internal diagnostic loopback
};
```

### Centralized mode transition: `setOTDirectMode()`

A single function `setOTDirectMode(OTDirectMode newMode)` handles all hardware reconfiguration atomically:

- Sets relay position (bypass relay on/off depending on mode)
- Enables/disables step-up converter for the OT bus
- Starts or stops the slave interface (thermostat-side listener)
- Updates the runtime state variable

No other code path may directly manipulate these hardware lines. All mode changes go through `setOTDirectMode()`.

### Persistence

The active mode is stored in the `OTDirectSettingsSection` of the LittleFS settings structure and restored at boot. This ensures the device returns to its configured mode after power loss.

### Convenience flags

For readability in hot-path code (the cooperative loop), convenience booleans are derived from the enum:

- `otBypassActive` -- true when mode is Bypass
- `otMonitorMode` -- true when mode is Monitor
- `otMasterMode` -- true when mode is Master

These are set exclusively by `setOTDirectMode()` and are read-only everywhere else. They exist to avoid repeated enum comparisons in timing-sensitive code and to make conditionals self-documenting.

### Compile-time guard

All mode-related code is wrapped in `#if HAS_DIRECT_OT`. PIC-based builds never see the enum or transition logic.

## Consequences

### Benefits:
- Mutual exclusivity enforced at the type level -- impossible to be in Gateway and Master simultaneously
- Single function owns all hardware transitions -- no partial state from scattered pin manipulation
- Settings survive reboot -- user does not need to reconfigure after power cycle
- Convenience flags keep hot-path code readable without runtime overhead
- Clean separation from PIC path via compile-time guard

### Trade-offs:
- Adding a new mode requires updating the enum, `setOTDirectMode()`, settings serialization, and the Web UI mode selector
- Convenience flags must be kept in sync with the enum (single writer pattern mitigates this)

### Risks:
- Hardware transition timing (relay settling, step-up ramp) may need per-mode tuning on production hardware
- Loopback mode is diagnostic-only; exposing it in the UI without clear labeling could confuse users

## Related

- **ADR-063**: OTGW32 hardware support -- establishes the dual build target and `HAS_DIRECT_OT` flag
- **ADR-061**: Unified ESP8266/ESP32 platform abstraction
- **ADR-051**: Dual encapsulating structs -- settings/state architecture used for mode persistence
- **Code**: `OTDirect.ino` (setOTDirectMode, mode enum), `OTGW-firmware.h` (OTDirectMode enum, OTDirectSettingsSection), `settingStuff.ino` (persistence)
