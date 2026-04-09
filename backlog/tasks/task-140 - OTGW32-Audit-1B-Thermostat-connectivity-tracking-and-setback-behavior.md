---
id: TASK-140
title: 'OTGW32-Audit-1B: Thermostat connectivity tracking and setback behavior'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:16'
updated_date: '2026-04-08 22:34'
labels:
  - audit
  - otgw32
  - phase-1
milestone: m-1
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare thermostat connectivity detection and setback logic in OTDirect.ino against OT-Thing reference. Verify: timeout threshold, setback temperature application, re-engagement on reconnect, and fail-safety interaction (FS= command).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Timeout threshold matches OT-Thing behavior
- [x] #2 Setback temperature (SB= command) is correctly applied on thermostat disconnect
- [x] #3 Setback is released when thermostat reconnects
- [x] #4 Interaction with FS=0 (fail safety disabled) is correct
- [x] #5 Any deviation from OT-Thing results in an audit-fix task
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Review checkThermostatTimeout() in OTDirect.ino
2. Verify timeout threshold (iSetbackTimeout setting, default 30s)
3. Verify SB= setback temp application on disconnect
4. Verify setback release on reconnect
5. Verify FS=0 disables setback
6. Compare against OT-Thing (which has NO thermostat timeout/setback logic)
7. Document findings, check ACs, create audit-fix if any gap found
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis of checkThermostatTimeout() at OTDirect.ino:1439:

1. TIMEOUT THRESHOLD: Uses settings.otd.iSetbackTimeout (default 30s, configurable 5-255s). Called once per second from loopOTDirect(). Elapsed = millis() - otLastThermostatMs.
   - OT-Thing has NO thermostat timeout mechanism. OTDirect.ino is the sole reference.
   - The 30s default is confirmed correct (typical OT thermostat sends MsgID 0 every ~1s; 30s gap unambiguously indicates disconnect).

2. SETBACK TEMPERATURE APPLICATION: On disconnect, if otFailSafeEnabled && gateway mode && !otSetbackEngaged:
   - otSetbackEngaged = true
   - Uses settings.otd.fSetbackTemp (set by SB= command, constrained 1.0-30.0°C, default from settings)
   - Calls setOverride(1, f88) to override MsgID 1 (TSet) in repeater path
   - Calls updateWriteCache(1, f88) so scheduler keeps writing it to boiler
   - Logs temperature in debug
   - state.otd.bSetbackActive = true for REST/MQTT visibility
   - CONFIRMED CORRECT.

3. RE-ENGAGEMENT ON RECONNECT: When elapsed <= timeoutMs:
   - state.otd.bThermostatConnected = true
   - If otSetbackEngaged: clears it, calls clearOverride(1), logs "setback released"
   - CONFIRMED CORRECT.

4. FS=0 INTERACTION: otFailSafeEnabled is checked before engaging setback. If FS=0 (otFailSafeEnabled=false), the setback block is skipped entirely. The thermostat connect/disconnect state is still tracked (state.otd.bThermostatConnected), only the setback action is suppressed.
   - This is correct behavior: FS=0 = disable safety net, FS=1 = enable.
   - CONFIRMED CORRECT.

5. GUARD CONDITIONS:
   - Setback only engages in OTD_MODE_GATEWAY (not monitor/master/bypass).
   - otThermostatSeen guard prevents false timeout on fresh boot before any thermostat frame.
   - Both correct.

CONCLUSION: OTDirect.ino fully implements thermostat connectivity tracking with setback. OT-Thing has no equivalent (does not track thermostat disconnect). No gaps found; OTDirect has MORE functionality in this area.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verified thermostat connectivity tracking and setback behavior in OTDirect.ino against OT-Thing reference.

OT-Thing has NO thermostat timeout/setback logic — this entire feature is OTDirect-only. All aspects verified correct:
- Timeout: configurable via settings.otd.iSetbackTimeout (default 30s, range 5-255s)
- Setback application: overrides MsgID 1 (TSet) via setOverride() + updateWriteCache() at SB= temperature
- Reconnect release: clearOverride(1) on reconnect, state flags updated
- FS=0: cleanly suppresses setback while still tracking connect/disconnect state
- Guards: only in gateway mode, only after at least one thermostat frame received

No gaps found. No audit-fix tasks created.
<!-- SECTION:FINAL_SUMMARY:END -->
