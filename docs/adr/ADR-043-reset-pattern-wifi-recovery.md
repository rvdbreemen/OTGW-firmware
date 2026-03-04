# ADR-043: Reset-Pattern WiFi Recovery Trigger

**Status:** Accepted  
**Date:** 2026-03-01

## Context

Users need a deterministic way to recover WiFi access when stored credentials are invalid or the network changed.

The original request was to force-start the WiFi configuration portal when reset is used in a special way. On ESP8266 (NodeMCU/Wemos D1 mini), firmware is not executing while reset is held, so a "hold reset during reboot" gesture cannot be detected in software.

Additional constraints:
- Must work with existing hardware (no extra button required)
- Must preserve normal boot path for most resets
- Must remain compatible with current WiFiManager-based configuration flow
- Must be robust on memory-constrained ESP8266 (ADR-001, ADR-004, ADR-009)
- Must fit deterministic boot ordering (ADR-036)

## Decision

Implement a **triple-reset trigger within a 10-second window** to force WiFi recovery mode:

1. Count consecutive external resets in RTC user memory.
2. If 3 resets occur within 10 seconds, mark recovery trigger as active.
3. During setup (before normal WiFi connect), when trigger is active:
   - Clear saved WiFi credentials
   - Force-start WiFiManager config portal
4. If reset timing window expires without reaching 3 resets, clear the counter.

This provides reset-only recovery behavior without requiring boot-time button reads.

## Alternatives Considered

### Alternative 1: Hold reset button during boot
**Pros:**
- Simple user mental model
- No extra state tracking

**Cons:**
- Not implementable on ESP8266: CPU is held in reset, firmware cannot sample button state
- Behavior would be unreliable by hardware definition

**Why not chosen:** Technically infeasible on target hardware.

### Alternative 2: BOOT/GPIO0 hold trigger
**Pros:**
- Could be read by firmware if device boots normally
- Common ESP recovery pattern on some projects

**Cons:**
- GPIO0 is a boot strapping pin; holding it low changes boot mode and may prevent normal firmware startup
- Not consistent across board handling and user workflows

**Why not chosen:** High risk of entering programming/invalid boot mode; poor UX reliability.

### Alternative 3: Double-reset trigger
**Pros:**
- Faster to execute
- Common in embedded recovery flows

**Cons:**
- Higher accidental activation probability during troubleshooting or unstable power cycles

**Why not chosen:** Triple-reset chosen to reduce false positives while remaining easy to perform.

## Consequences

### Positive
- Reliable reset-only recovery path on existing hardware
- No added external components or wiring
- Keeps default boot behavior unchanged for normal use
- Integrates with existing WiFiManager portal flow and credential reset routine

### Negative
- Slightly more complex boot logic (RTC state + reset window)
- Recovery gesture is less obvious than a dedicated button

### Risks & Mitigation
- **Risk:** False trigger on rapid unintended resets  
  **Mitigation:** Require 3 resets in a short window (10 seconds)
- **Risk:** RTC state corruption or unexpected values  
  **Mitigation:** Magic value validation and safe state reset
- **Risk:** User confusion between reset and recovery flows  
  **Mitigation:** Documented in README and wiki guide with explicit steps

## Related Decisions
- ADR-001: ESP8266 Platform Selection
- ADR-004: Static Buffer Allocation Strategy
- ADR-009: PROGMEM Usage for String Literals
- ADR-017: WiFiManager for Initial Configuration
- ADR-036: Boot Sequence Initialization Ordering

## References
- Implementation: `src/OTGW-firmware/OTGW-firmware.ino`
- Implementation: `src/OTGW-firmware/networkStuff.h`
- User docs: `README.md`
- User guide: `docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md`