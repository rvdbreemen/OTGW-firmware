---
id: TASK-162
title: 'OTGW32-Audit-6C: MI= inter-message gap enforcement coverage'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:22'
updated_date: '2026-04-08 22:32'
labels:
  - audit
  - otgw32
  - phase-6
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that the MI= minimum inter-message gap (otMinIntervalMs, default 100ms) is enforced consistently for all frame transmission paths: scheduled reads/writes, command queue drain, and any other direct sends. Check that otLastAnySendMs is updated on every send and that the guard check covers all code paths.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otLastAnySendMs is updated every time a frame is sent to the boiler
- [x] #2 Schedule send path checks MI= gap before sending
- [x] #3 Command queue drain path checks MI= gap before sending
- [x] #4 No direct frame send bypasses the MI= gap check
- [x] #5 MI= range validation (100-2550ms) enforced, OR response for out-of-range
- [x] #6 Any gap in enforcement results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

### otLastAnySendMs update coverage
- Updated in scheduleMasterRequest() at two points:
  - Line 1004: after successful command queue dequeue+send.
  - Line 1041: after successful schedule-based send.
- NOT updated in the thermostat forward paths (monitor mode line 1187, gateway mode line 1227).
- These paths call sendMasterRequestAsync() which itself does NOT update otLastAnySendMs.
- ISSUE: Thermostat-forwarded frames bypass the MI= gap tracking.

### MI= gap check coverage
- scheduleMasterRequest() checks `(now - otLastAnySendMs) < otMinIntervalMs` at line 996 — PASS.
- Thermostat forwarding paths in loopOTDirect() check `!otMasterRequestActive` (bus busy) but NOT the MI= gap.
- This means a thermostat frame can be forwarded immediately after a scheduled frame, violating the MI= gap.
- Real OT masters typically drive the bus at their own pace so this is low-risk in gateway mode,
  but in principle the inter-message gap guarantee is not upheld for thermostat-originated frames.

### MI= range validation (AC5)
- MI= command validates 100-2550ms (line 2299), returns OR (out-of-range) if violated — PASS.

### Verdict
- TWO frame send paths (monitor-mode forward and gateway-mode normal-path forward) do not update
  otLastAnySendMs after the send succeeds, and are not gated by the MI= gap check.
- The gap check in scheduleMasterRequest() only prevents the *scheduled* polls from firing too soon,
  but cannot prevent a thermostat frame from being forwarded immediately before or after.
- Severity: LOW — real thermostats drive MsgID 0 at ~1s; gap will naturally be respected.
  However it is an architectural gap worth closing for correctness.
- Audit-fix task to be created.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit of MI= inter-message gap enforcement — PARTIAL PASS, one architectural gap found.

Findings:
- scheduleMasterRequest() correctly checks and updates otLastAnySendMs for both the command-queue drain path and the schedule-poll path.
- MI= range validation (100-2550ms, OR response on violation) is correct.
- GAP: sendMasterRequestAsync() does not update otLastAnySendMs internally. The two thermostat-forwarding paths in loopOTDirect() (monitor-mode forward and gateway-mode normal-path forward) therefore bypass the MI= gap tracking. A fast thermostat can trigger a boiler send immediately after a scheduled poll.
- Severity: LOW in practice (real thermostats pace themselves at ~1s), but incorrect by design.

Audit-fix created: TASK-179 — move otLastAnySendMs update into sendMasterRequestAsync() for all real-bus sends.
<!-- SECTION:FINAL_SUMMARY:END -->
