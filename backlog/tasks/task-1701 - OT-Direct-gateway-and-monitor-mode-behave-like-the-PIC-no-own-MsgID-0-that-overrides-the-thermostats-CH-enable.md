---
id: TASK-1701
title: >-
  OT-Direct gateway and monitor mode behave like the PIC: no own MsgID 0 that
  overrides the thermostat's CH enable
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T17:04:00Z'
updated_date: '2026-11-07T17:52:00Z'
labels:
  - otdirect
  - bug
  - field-report
dependencies: []
priority: high
ordinal: 814000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs on #alpha-testing (2026-11-07 15:10 UTC, Discord msg 1568643857133080729, 177 KB log). In OT-Direct gateway mode the gateway polls its own MsgID 0 every 800 ms with master status 0x01 (CH enable on, OTDirect.ino otMasterStatusFlags) while the relayed thermostat sends CH off (T80000200). The boiler follows the last frame and toggles between CH demand with flame and standby although the room (20.6 C) is above the setpoint (17 C). buildStatusRequest() only clears bit 0 in master mode (TASK-1684). Robert 2026-11-07: monitor and gateway mode in OT-Direct must work exactly as on the PIC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In gateway mode the boiler never receives a CH enable bit the thermostat did not send, except under an active CS=/CH= override (ADR-208, PIC gateway.asm semantics cited)
- [x] #2 In monitor mode the gateway sends no frame of its own to the boiler
- [ ] #3 Gateway mode traffic towards the boiler matches the PIC model (which frames the PIC injects, substitutes or never sends), with gateway.asm citations in the task notes
- [x] #4 Host test reproduces tjfs's log pattern: red on the old code, green on the fix
- [ ] #5 Bench: OT-Direct unit in gateway mode with a thermostat sending CH off: boiler sees no CH enable from the gateway
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
From otgw-firmware-6b (TASK-1704, closed as duplicate): tjfs's log counts thermostat T80000200 [-D---W--] 137x vs gateway R80000100 [C----W--] 299x, slave toggling [--------]/[-C------]; scheduler gate OTDirect.ino:1972-1976 runs in every mode except monitor, MsgID 0 entry -> buildStatusRequest(). I told tjfs (msg 1568673633960992904) that monitor mode stops the gateway's own frames as a stopgap.

Robert 2026-11-07: hotfix first (gateway mode with a connected thermostat sends no own MsgID 0), full PIC alternative-slot model as follow-up TASK-1707. PIC model: one boiler frame per thermostat frame, own reads only in slots of unsupported IDs (gateway.asm:2384-2414, 3390-3442), own frames only without a thermostat (gateway.asm:1770-1816), monitor mode transmits nothing (gateway.asm:1488-1489, 1793).

Fix: OTDirect.ino otdOwnStatusRequestAllowed(gatewayMode, thermostatConnected) between markers; scheduleMasterRequest() skips the MsgID 0 entry when it returns false. Monitor mode already skips the scheduler (loopOTDirect, DUE(timerOTSchedule) && !IS_MONITOR_MODE()) and forwards the thermostat frame unmodified: no code change, now covered by source-pattern tests.

Evidence: tests/test_otd_gateway_own_status.py compiles the real scheduleMasterRequest/buildStatusRequest/schedule table, 6 min with a thermostat sending 0x02 each second. OLD (origin/dev c928bcaf7): G1 FAIL own_status=347 ch_changes=568 (tjfs pattern). FIX: 6/6 OK, G1 own_status=0 ch_changes=0, G2 (no thermostat) 347 own MsgID 0 kept, G3 master mode CH off kept. test_otd_ch_override 9/9 OK; test/host/test_override_reply.py 4 suites OK; evaluate --quick 84 passed 0 failed. test_otd_status_relay: 9 errors WinError 225 (Defender blocks the compiled exe) on OLD and FIX alike, environment, not the fix.
<!-- SECTION:NOTES:END -->
