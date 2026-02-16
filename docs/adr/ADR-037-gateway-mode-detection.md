# ADR-037: Gateway Mode Detection via PR=M Polling

**Status:** Accepted  
**Date:** 2026-02-16  
**Updated:** 2026-02-16 (Initial documentation of existing pattern)

## Context

The OpenTherm Gateway PIC firmware supports two operational modes:
- **Gateway mode (GW=1):** The OTGW intercepts and can modify OpenTherm messages between thermostat and boiler. This is the primary mode for Home Assistant control.
- **Monitor mode (GW=0):** The OTGW passively observes OpenTherm traffic without modifying messages. Used for diagnostics or legacy Domoticz compatibility.

The ESP8266 firmware needs to **reliably detect** the current mode for several reasons:
1. **Time synchronization:** When `PS=1` (Print Summary mode) is active, time sync commands (`SC=...`) must be suppressed to avoid interfering with the summary output.
2. **MQTT state reporting:** Home Assistant needs to know the gateway mode to display correct status.
3. **Health monitoring:** The `/api/v1/health` endpoint reports gateway mode as a health indicator.
4. **User visibility:** The Web UI shows the current operational mode.

**Problem:** The PIC firmware does not push mode changes to the ESP8266. The ESP must actively query the PIC to discover the current mode.

**Constraints:**
- PIC serial communication is at 9600 baud (slow)
- Too-frequent queries waste serial bandwidth and can delay OT message processing
- The PIC firmware command `PR=M` returns the mode but has no push notification equivalent
- The mode can change at any time (via OTmonitor on port 25238, or via MQTT/REST command)

## Decision

**Poll the PIC firmware every 30 seconds using `PR=M` with a 60-second hard throttle cache, and publish mode changes to MQTT.**

### Detection Strategy

```cpp
// Every 30 seconds in doTaskEvery30s():
if (bPICavailable && bOTGWonline) {
  bool newGatewayState = queryOTGWgatewaymode();
  bOTGWgatewaystate = newGatewayState;
  
  // Only publish MQTT on state change (or first run)
  if (bOTGWgatewaystate != previousState || firstRun) {
    sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(bOTGWgatewaystate));
  }
}
```

### Caching and Throttling

```cpp
bool queryOTGWgatewaymode() {
  static uint32_t lastQueryMs = 0;
  static bool cachedMode = false;
  static bool hasCachedMode = false;
  constexpr uint32_t MIN_INTERVAL_MS = 60000; // Max 1 query per minute
  
  if (!bPICavailable) return false;
  
  // Return cached value if queried too recently
  if (hasCachedMode && (millis() - lastQueryMs < MIN_INTERVAL_MS)) {
    return cachedMode;
  }
  
  String response = executeCommand("PR=M");
  // "G" = Gateway mode, "M" = Monitor mode
  cachedMode = (response.charAt(0) == 'G' || response.charAt(0) == 'g');
  hasCachedMode = true;
  lastQueryMs = millis();
  return cachedMode;
}
```

### PS=1 Mode Impact

When `PS=1` (Print Summary) is active on the serial port (set via OTmonitor on port 25238), the firmware suppresses time synchronization:

```cpp
void sendtimecommand() {
  if (bPSmode) return;  // Skip time sync in PS=1 mode
  // ... send SC= command
}
```

This prevents time commands from corrupting the Print Summary output that OTmonitor/Domoticz expects.

## Alternatives Considered

### Alternative 1: Infer Mode from Message Traffic
**Pros:**
- No additional serial commands needed
- Zero overhead
- Real-time detection

**Cons:**
- Unreliable: modified messages (gateway mode) vs. pass-through (monitor mode) are hard to distinguish
- Edge cases: if no thermostat commands are modified, gateway mode looks like monitor mode
- Requires deep OpenTherm protocol analysis

**Why not chosen:** Inference is unreliable. The `PR=M` command provides a definitive answer directly from the PIC firmware.

### Alternative 2: Event-Driven Notification from PIC
**Pros:**
- Immediate notification on mode change
- No polling overhead
- Always up-to-date

**Cons:**
- PIC firmware does not support push notifications for mode changes
- Would require PIC firmware modification (out of scope — maintained by Schelte Bron)
- Custom firmware forks create maintenance burden

**Why not chosen:** The PIC firmware is maintained by a third party. We cannot add push notification support.

### Alternative 3: Higher-Frequency Polling (Every 5s)
**Pros:**
- Faster detection of mode changes
- Near-real-time status

**Cons:**
- 6x more serial traffic
- Can interfere with OT message processing at 9600 baud
- Unnecessary for a setting that changes rarely (minutes/hours/never)

**Why not chosen:** Gateway mode changes are rare (user-initiated). 30-second detection latency is acceptable. The 60-second hard throttle prevents excessive serial traffic even if external code calls the function more frequently.

### Alternative 4: Query Only on Startup
**Pros:**
- Minimal overhead (one query ever)
- No polling

**Cons:**
- Misses runtime mode changes via OTmonitor or MQTT commands
- MQTT reports stale mode information indefinitely
- Users confused by incorrect mode display

**Why not chosen:** Mode can change at runtime via multiple paths (OTmonitor port 25238, MQTT `GW` command, REST API). Must detect these changes.

## Consequences

### Positive
- **Reliable detection:** `PR=M` returns definitive mode from PIC firmware
- **Low overhead:** 1 serial round-trip every 30-60 seconds
- **Change detection:** MQTT only publishes on actual state changes
- **Cached response:** Multiple callers within 60 seconds share cached result
- **PS=1 compatibility:** Time sync suppression prevents Domoticz/OTmonitor conflicts

### Negative
- **Detection latency:** Up to 30 seconds to detect mode change
  - Accepted: Mode changes are rare and user-initiated
- **Serial bandwidth:** PR=M command uses serial time
  - Mitigation: Hard throttle at 60 seconds prevents excessive queries
  - Mitigation: Skipped when PIC unavailable or OTGW offline
- **Cached stale data:** 60-second cache can serve outdated mode
  - Accepted: Mode changes are infrequent enough that 60s staleness is acceptable

### Risks & Mitigation
- **PIC not responding:** executeCommand() times out
  - **Mitigation:** Returns `false` (monitor mode) as conservative default
  - **Mitigation:** Only queries when `bPICavailable && bOTGWonline`
- **Unexpected PR=M response:** PIC firmware version doesn't support PR=M
  - **Mitigation:** Empty response handled gracefully (defaults to false)
  - **Mitigation:** Unexpected characters logged to telnet debug

## Related Decisions
- ADR-031: Two-Microcontroller Coordination Architecture (ESP8266 ↔ PIC communication)
- ADR-016: OpenTherm Command Queue (GW=1/GW=0 commands routed through queue)
- ADR-006: MQTT Integration Pattern (gateway_mode published to MQTT)
- ADR-015: NTP and AceTime for Time Management (time sync suppressed in PS=1 mode)

## References
- Implementation: `OTGW-Core.ino` queryOTGWgatewaymode()
- Polling caller: `OTGW-firmware.ino` doTaskEvery30s()
- PS=1 impact: `OTGW-firmware.ino` sendtimecommand()
- PIC firmware commands: https://otgw.tclcode.com/firmware.html
- PR command documentation: `PR=M` returns "G" (Gateway) or "M" (Monitor)
