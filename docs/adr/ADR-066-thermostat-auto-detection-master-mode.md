# ADR-066: Thermostat Auto-Detection and Master Mode

**Status:** Accepted
**Date:** 2026-04-04

## Context

The OTGW32 may be deployed in environments without a thermostat -- for example, as a standalone boiler controller driven entirely by Home Assistant or other automation. In these deployments, the firmware must act as the sole OpenTherm master, polling the boiler directly using a built-in scheduler.

However, many users will deploy with a thermostat present. Rather than requiring manual mode configuration, the firmware should detect whether a thermostat is connected and automatically select the appropriate operating mode.

### Key constraints:

- **OpenTherm protocol timing**: A compliant thermostat sends MsgID 0 (status) approximately every 1 second. Waiting 5 seconds provides high confidence (5 expected frames) for detection.
- **Boot-time detection is acceptable**: The 5-second wait occurs during `initOTDirect()` in the setup phase, before the cooperative loop starts. No watchdog or network timing issues.
- **Dual role possibility**: Some users want the OTGW32 to respond to a thermostat as if it were a boiler (virtual boiler mode), while simultaneously polling the real boiler as master. This requires a slave interface that serves cached boiler data.

### Alternatives considered

1. **Always require manual mode selection** -- Rejected: poor out-of-box experience. Users without thermostats would need to know about Master mode and configure it before the device does anything useful.
2. **Continuous thermostat detection in the main loop** -- Rejected: adds complexity to the hot path. Mode should be stable after boot; runtime thermostat disconnect is a separate concern (handled by existing timeout logic).
3. **Detect by checking if slave bus has pull-up** -- Rejected: electrical detection is unreliable and hardware-dependent. Protocol-level detection (looking for actual OT frames) is definitive.
4. **Boot-time frame detection with auto-switch** -- Chosen: simple, reliable, zero-config for the common case.

## Decision

### Auto-detection at boot

When `bAutoDetect` is enabled (default: true) and the configured mode is Gateway:

1. During `initOTDirect()`, enable the slave interface (thermostat-side listener).
2. Wait up to 5 seconds, checking for incoming thermostat frames on the slave bus.
3. If at least one valid OT frame is received within the window, remain in Gateway mode -- a thermostat is present.
4. If no frames are received, automatically switch to Master mode via `setOTDirectMode(OT_MODE_MASTER)`.

The 5-second window is a blocking wait during the setup phase. This is acceptable because:
- The cooperative loop has not started yet
- Network services are not yet active
- The hardware watchdog timeout is longer than 5 seconds

### Master mode scheduler

In Master mode, the firmware runs a full OpenTherm polling scheduler that:
- Sends MsgID 0 (status) every ~1 second as required by the protocol
- Cycles through configured data IDs (setpoint, DHW, modulation, temperatures, etc.)
- Processes boiler responses via `bridgeFrameToParser()` (ADR-065) so MQTT, REST, and WebSocket all work normally

### Boiler response cache: `otBoilerCache[128]`

A 128-entry cache stores the most recent boiler response for each MsgID. This serves two purposes:

1. **Master mode**: Tracks boiler state for the scheduler (e.g., current modulation level, flame status).
2. **Virtual boiler mode**: When `bEnableSlave` is true in Master mode, the slave interface responds to thermostat requests using cached boiler data. The thermostat sees "real" boiler values without direct bus access.

### `bEnableSlave` toggle

Controls whether the slave interface is active in Master mode:

- **`bEnableSlave = false`** (default): Pure standalone master. No slave bus activity. Simplest deployment.
- **`bEnableSlave = true`**: Virtual boiler mode. The slave interface listens for thermostat requests and responds with cached boiler data via `handleMasterModeSlaveFrame()`. Enables hybrid deployments where both a thermostat and Home Assistant control the boiler.

### Settings

Both `bAutoDetect` and `bEnableSlave` are persisted in the `OTDirectSettingsSection` of LittleFS settings and exposed in the Web UI.

## Consequences

### Benefits:
- Zero-config for users without thermostats -- device auto-detects and enters Master mode
- Zero-config for users with thermostats -- device stays in Gateway mode as expected
- Virtual boiler mode enables hybrid thermostat + automation deployments
- Boiler cache is lightweight (128 x 4 bytes = 512 bytes) and provides instant responses to thermostat queries
- All master mode data flows through the existing frame bridge, so MQTT/REST/WebSocket work without modification

### Trade-offs:
- 5-second blocking wait at boot adds to startup time (acceptable in setup phase)
- `bAutoDetect` only runs at boot -- if a thermostat is connected after boot, a reboot or manual mode switch is required
- Virtual boiler responses use cached data that may be slightly stale (typically < 2 seconds old given the ~1s polling cycle)

### Risks:
- A thermostat that is powered but not sending frames during the 5-second window would be missed (unlikely given 1s protocol cadence, but possible with non-compliant devices)
- Virtual boiler mode with `bEnableSlave` could confuse thermostats if the boiler cache is empty at startup (first few seconds before boiler responds). Mitigation: respond with DataInvalid flag until cache is populated.

## Related

- **ADR-064**: OT-Direct operating mode architecture -- defines `OTDirectMode` enum and `setOTDirectMode()` used for the auto-switch
- **ADR-065**: Frame bridge pattern -- master mode scheduler feeds boiler responses through the bridge
- **ADR-063**: OTGW32 hardware support -- establishes the OT-direct build target
- **Code**: `OTDirect.ino` (`initOTDirect` auto-detect logic, `handleMasterModeSlaveFrame`, `otBoilerCache`), `OTGW-firmware.h` (`bAutoDetect`, `bEnableSlave` settings)
