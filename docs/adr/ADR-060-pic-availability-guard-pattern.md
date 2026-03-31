# ADR-060: PIC Availability Guard Pattern

**Status:** Accepted
**Date:** 2026-03-31

## Context

The OTGW hardware traditionally requires a PIC microcontroller for OpenTherm protocol handling (ADR-031). However, future hardware variants may operate without a PIC — for example, ESP32-based boards with direct OpenTherm support, or diagnostic/development setups where no PIC is present.

Before this decision, the firmware assumed a PIC was always present. When no PIC was detected at boot (`detectPIC()` failed), the system would still attempt PIC communication: sending commands to serial, publishing PIC-related MQTT topics, exposing PIC endpoints in the REST API, and showing PIC UI elements. This caused:

- Silent command failures (commands sent to empty serial port)
- Misleading MQTT topics and Home Assistant entities for non-existent PIC state
- REST API endpoints returning stale/default values instead of indicating unavailability
- UI elements (firmware flash, gateway mode, command bar) visible but non-functional

**Key constraint:** PIC detection at boot is not always reliable. The `detectPIC()` function relies on a single ETX character check after reset, which can fail due to transient timing issues. The system needs a recovery path when the initial probe misses a real PIC.

## Decision

**Introduce a central PIC availability guard via `isPICEnabled()` that all PIC-dependent code paths check before proceeding.**

**Guard function:**

```cpp
// OTGW-firmware.h
inline bool isPICEnabled() { return state.pic.bAvailable; }
inline bool isGatewayFirmware() { return strcmp_P(state.pic.sType, PSTR("gateway")) == 0; }
```

**Guarded code paths:**

1. **Serial communication:** `sendOTGW()`, `executeCommand()`, `addOTWGcmdtoqueue()` — early return with log message when no PIC
2. **MQTT publishing:** `sendMQTTversioninfo()`, `sendMQTTstateinformation()`, `processOT()` state topics — skip `otgw-pic/*` topics
3. **MQTT commands:** `handleMQTTcallback()` — reject set commands
4. **HA discovery:** `doAutoConfigure()` — skip entries with `otgw-pic/` topic prefix
5. **REST API:** `handleCommandSubmit()`, `handlePic()` — return HTTP 503; `sendDeviceInfoV2()`, `sendFlashStatus()` — omit PIC fields
6. **Boot commands:** `sendOTGWbootcmd()`, `resetOTGW()` — skip
7. **PIC settings:** `triggerPICsettingsReadout()`, `queryNextPICsetting()`, `publishAllPICsettings()` — skip
8. **PIC firmware upgrade:** `upgradepicnow()`, `upgradepic()` — reject with error
9. **Frontend:** CSS class `pic-only` hides PIC-related UI elements; `applyPICAvailability()` toggles visibility based on `picavailable` from device info API

**Auto-recovery mechanism:**

- `state.pic.bAvailable` is set by `detectPIC()` at boot
- A 60-second retry probe writes `PR=A\r\n` directly to serial (bypasses the guarded command queue) when PIC identity is unknown
- If `processOT()` receives a PIC banner while `state.pic.bAvailable` is false, it sets it to true — re-enabling all PIC functions without a reboot
- `state.otgw.bOnline` defaults to `false` (was `true`) to prevent false-positive online status before any OT traffic is seen

## Alternatives Considered

### Alternative 1: Compile-time PIC exclusion (#ifdef NO_PIC)

**Pros:**
- Zero runtime overhead
- Smaller binary for PIC-less builds
- No guard checks needed

**Cons:**
- Requires separate firmware builds for PIC and non-PIC hardware
- Users must choose the correct binary
- Cannot recover if PIC appears after boot (e.g., hot-swap, delayed startup)
- Doubles maintenance and testing burden

**Why not chosen:** Runtime detection is more flexible and user-friendly. A single firmware binary works on both hardware variants. The overhead of inline guard checks is negligible.

### Alternative 2: Disable PIC functions permanently after boot failure

**Pros:**
- Simpler — no recovery path needed
- Deterministic behavior

**Cons:**
- Transient boot-probe failures permanently disable PIC until manual reboot
- Poor user experience for the common case where PIC is present but slow to respond

**Why not chosen:** The 60-second retry and banner-based recovery provide a reliable fallback for transient failures without complexity.

### Alternative 3: Centralized PIC proxy object

**Pros:**
- All PIC communication through a single object
- Could queue commands and replay when PIC becomes available

**Cons:**
- Major refactor of the existing codebase
- Command replay semantics are complex (stale setpoints, ordering)
- Over-engineered for the current need

**Why not chosen:** The guard pattern achieves the goal with minimal code changes and no architectural disruption.

## Consequences

### Positive

1. **Single firmware binary** — works on both PIC and non-PIC hardware without recompilation
2. **Clean degradation** — no misleading state, no silent failures; REST API returns 503, MQTT topics are simply absent, UI hides irrelevant elements
3. **Auto-recovery** — transient boot-probe failures self-heal within 60 seconds via banner detection
4. **Minimal invasiveness** — guard checks are inline one-liners added at function entry points; no architectural restructuring required
5. **Prepares for ESP32** — ESP32 variants with direct OpenTherm support can run the same firmware without PIC

### Negative

1. **Distributed guard checks** — `isPICEnabled()` appears in ~15 locations; a missed guard could leak PIC commands to serial
   - **Mitigation:** All three low-level serial entry points (`sendOTGW`, `executeCommand`, `addOTWGcmdtoqueue`) are guarded, providing a safety net even if higher-level callers miss the check
2. **Topic-prefix coupling** — `doAutoConfigure()` filters HA discovery by string-matching `otgw-pic/` prefix; renaming topics requires updating the filter
   - **Mitigation:** Comment in code documents this coupling

### Risks & Mitigation

**Risk:** Guard bypass — new code sends PIC commands without checking `isPICEnabled()`
- **Mitigation:** The three low-level serial functions all have guards, so even unguarded callers won't reach the serial port
- **Mitigation:** Code review checklist item: "Does this touch PIC serial? Check isPICEnabled()"

**Risk:** False recovery — non-PIC serial noise triggers banner detection
- **Mitigation:** Banner match requires the exact `OTGW_BANNER` string from the PIC firmware; random noise is extremely unlikely to produce it

## Related Decisions

- **ADR-031:** Two-Microcontroller Coordination Architecture — establishes the PIC/ESP8266 relationship; this ADR extends it to handle the "no PIC" case
- **ADR-012:** PIC Firmware Upgrade via Web UI — upgrade paths are now guarded by `isPICEnabled()`
- **ADR-016:** OpenTherm Command Queue — `addOTWGcmdtoqueue()` is a guarded entry point
- **ADR-037:** Gateway Mode Detection — `queryOTGWgatewaymode()` now requires `isPICEnabled() && isGatewayFirmware()`
- **ADR-038:** OpenTherm Data Flow Pipeline — `processOT()` conditionally publishes PIC state topics

## References

- Implementation: PR #522 — "Disable all PIC-related functions when no PIC is detected at boot"
- Guard function: `src/OTGW-firmware/OTGW-firmware.h` (`isPICEnabled()`, `isGatewayFirmware()`)
- Boot detection: `src/OTGW-firmware/OTGW-Core.ino` (`detectPIC()`)
- Auto-recovery: `src/OTGW-firmware/OTGW-Core.ino` (`processOT()` banner handler)
- Frontend: `src/OTGW-firmware/data/index.js` (`applyPICAvailability()`)
