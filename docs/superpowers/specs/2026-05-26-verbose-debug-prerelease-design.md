# Verbose Debug Auto-Enable for Alpha/Beta Prerelease Builds

**Date:** 2026-05-26
**Status:** Approved

## Goal

When the firmware prerelease tag contains "alpha" or "beta", all pure-logging
debug toggles in `state.debug` are automatically enabled at boot. This ensures
alpha/beta testers always produce full telnet traces without manual toggle steps.

## Trigger condition

Checked once at boot in `setup()`, after `startTelnet()`:

```cpp
#if defined(_VERSION_PRERELEASE)
  if (strstr(_SEMVER_FULL, "alpha") || strstr(_SEMVER_FULL, "beta"))
    enableDebugForPrerelease();
#endif
```

`_SEMVER_FULL` is a quoted string literal defined by `autoinc-semver.py`
(e.g. `"2.0.0-alpha.71+c9f1a84"`). `strstr` on a string literal is safe on
ESP32. The `#if defined(_VERSION_PRERELEASE)` outer guard strips the entire
block in release builds — zero overhead in production.

## Toggles enabled

Only pure-logging toggles. Simulation flags are excluded by design.

| Toggle | Default | Set to |
|---|---|---|
| `bRestAPI` | false | **true** |
| `bMQTT` | false | **true** |
| `bMQTTGate` | false | **true** |
| `bSensors` | false | **true** |
| `bSATBLE` | false | **true** |
| `bOTmsg` | true | (unchanged) |
| `bNTP` | true | (unchanged) |
| `bSAT` | true | (unchanged) |
| `bOTDirect` | true | (unchanged) |
| `bOTGWSimulation` | false | (excluded — simulation) |
| `bSensorSim` | false | (excluded — simulation) |

## Files changed

| File | Change |
|---|---|
| `src/OTGW-firmware/debugStuff.ino` | Add `enableDebugForPrerelease()` |
| `src/OTGW-firmware/debugStuff.h` | Add forward declaration |
| `src/OTGW-firmware/OTGW-firmware.ino` | Call `enableDebugForPrerelease()` in `setup()` after `startTelnet()` |

## Out of scope

- `bOTGWSimulation` and `bSensorSim`: deliberately excluded (behaviour, not logging).
- No build script changes: runtime `strstr` on `_SEMVER_FULL` is sufficient.
- No REST API or settings persistence: toggles are transient runtime state.
- No "rc" or other prerelease tag variants: only "alpha" and "beta" trigger this.
