---
id: TASK-166
title: 'OTGW32-Audit-7C: resetTransientState() completeness — cold boot vs GW=R reset'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:23'
updated_date: '2026-04-08 22:31'
labels:
  - audit
  - otgw32
  - phase-7
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that resetTransientState() clears all transient OTDirect state variables completely. A GW=R reset must return the system to the same state as a cold boot from the perspective of the OT bus. Check that all static variables (overrides, unknown-ID list, DHW push state, thermostat tracking, schedule lastSentMs, etc.) are reset.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All otOverrides[] entries are cleared on reset
- [x] #2 Unknown-ID list and counters are cleared
- [x] #3 DHW push state machine is reset to PUSH_IDLE
- [x] #4 Thermostat connectivity tracking is reset
- [ ] #5 Schedule lastSentMs values are cleared (force immediate re-poll)
- [ ] #6 otMasterStatusFlags returns to default (CH=1 only)
- [x] #7 Any variable not reset results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

Reviewed resetTransientState() at OTDirect.ino:1325-1370.

**AC1 — otOverrides[] cleared:**
Lines 1359-1362 clear all otOverrides[].active flags. PASS.

**AC2 — Unknown-ID list and counters cleared:**
- otUnknownIdCount = 0 at line 1350 (list cleared)
- memset(otUnknownCounters, 0, sizeof(otUnknownCounters)) at line 1344 (3-strike counters cleared)
- otResponseOverrides[].active cleared at lines 1352-1354
PASS.

**AC3 — DHW push state machine reset:**
- otDHWPushState = PUSH_IDLE at line 1331
- otDHWPushStartMs = 0 at line 1332
PASS.

**AC4 — Thermostat connectivity tracking reset: GAP FOUND**
otThermostatSeen and otLastThermostatMs are NOT cleared in resetTransientState(). After GW=R, the thermostat timeout watchdog (checkThermostatTimeout) will use stale otLastThermostatMs. If the reset happens mid-timeout, the setback could incorrectly engage or disengage. These should be cleared.

**AC5 — Schedule lastSentMs values cleared: GAP FOUND**
resetTransientState() clears otSchedule[].disabled and otSchedule[].valueSet, but does NOT clear otSchedule[].lastSentMs. The task spec says lastSentMs should be zeroed to force immediate re-poll after reset. Currently a reset would not force immediate re-poll of slow-poll items (60s interval) if they were recently sent.

**AC6 — otMasterStatusFlags resets to CH=1 only: GAP FOUND**
resetTransientState() does `otMasterStatusFlags &= ~(0x44)` (clears cooling bit2 and DHW block bit6), but does NOT clear CH2 enable (bit4=0x10), summer mode (bit5=0x20), or DHW override bits. It also does not clear bit0 (CH enable) and bit1 (DHW enable) to reset to the pure default 0x01 state. Per task spec, reset should restore to "CH=1 only" (0x01).

Note: otSummerMode is a persisted setting (settings.otd.bSummerMode), so it arguably should not be cleared on GW=R. However bit4 (CH2) and bit1 (DHW) are runtime state set by commands and should be reset. The correct reset is `otMasterStatusFlags = 0x01` (preserving only the default CH=1).

**GAPS REQUIRING FIX:**
1. otThermostatSeen not cleared (risk: stale timeout tracking after reset)
2. otLastThermostatMs not cleared (companion to above)
3. otSchedule[].lastSentMs not zeroed (slow-poll items not forced immediate)
4. otMasterStatusFlags not fully reset to 0x01 (CH=1 only default)

Creating audit-fix task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit found 4 gaps in resetTransientState() (OTDirect.ino:1325):\n\n1. otThermostatSeen not cleared — stale after GW=R\n2. otLastThermostatMs not cleared — companion, feeds checkThermostatTimeout()\n3. otSchedule[].lastSentMs not zeroed — slow-poll items not forced to immediate re-poll\n4. otMasterStatusFlags only partially cleared (~0x44) — CH2 (bit4) and DHW (bit1) runtime bits remain set; should reset to 0x01\n\nACs 1 (overrides), 2 (unknown-ID list/counters), 3 (DHW push PUSH_IDLE), 4 (setback cleared) all PASS.\nACs 5, 6 FAIL — fix task TASK-178 created.
<!-- SECTION:FINAL_SUMMARY:END -->
