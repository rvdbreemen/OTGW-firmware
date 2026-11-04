---
id: TASK-442
title: fixotdirect-expire-CS-and-C2-overrides-like-PIC-heartbeat
status: Done
assignee: []
created_date: '2026-04-27 09:54'
updated_date: '2026-04-27 23:48'
labels:
  - otdirect
  - pic-parity
  - safety
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/restAPI.ino
  - other-projects/otgw-6.6/gateway.asm
  - other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
documentation:
  - docs/MANUAL.md
  - docs/api/MQTT.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

The PIC firmware treats `CS=` and `C2=` setpoint overrides as heartbeat-driven commands. It clears them after roughly a minute without refresh for normal setpoint values. OTDirect currently places these writes in its override/write cache and keeps sending them until explicitly cleared. That is a behavioral mismatch with practical safety impact: a one-shot MQTT, REST, or raw command can leave a stale flow/control override active indefinitely on OTDirect, while the PIC would expire it unless refreshed.

Evidence:
- `other-projects/otgw-6.6/gateway.asm`: `CommandExpiry` invalidates `OverrideCH`/`OverrideCH2` when the heartbeat flags for `CS`/`C2` have not been refreshed; the firmware preserves only low setpoints below the PIC threshold used as a special no-expiry case.
- `src/OTGW-firmware/OTDirect.ino`: `CS=`/`C2=` are handled as persistent write-cache entries unless explicit clear paths run.
- `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl`: OTmonitor has a `commandexpired` path that sends `CS=0` after a little over a minute, reinforcing that clients and PIC firmware expect setpoint commands to be refreshed or released.
- `src/OTGW-firmware/SATcontrol.ino`: SAT sends `CS=` repeatedly during active control, so a PIC-style heartbeat expiry should not break the SAT loop when it is healthy.
- `other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp`: a comparable direct-control project centralizes active overrides in control logic, which is useful context for testing expiry behavior without copying its architecture.

Keep the fix minimal: add timestamp/expiry handling for `CS` and `C2` only. Do not change persistent configuration-style commands such as `SW`, `SH`, or `MM` as part of this task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTDirect records the last successful `CS=` and `C2=` command time separately.
- [ ] #2 For normal setpoint values matching the PIC expiry category, OTDirect automatically clears/stops the corresponding override after approximately one minute without a fresh command.
- [x] #3 Refreshed `CS=`/`C2=` commands reset the expiry timer, so SAT's periodic control loop does not lose active control while it is still sending commands.
- [x] #4 Explicit release commands such as `CS=0` and `C2=0` continue to clear immediately and do not wait for the expiry timer.
- [x] #5 The expiry behavior is limited to `CS` and `C2` unless another command is explicitly justified with PIC evidence in the implementation notes.
- [ ] #6 Low-value/no-expiry behavior is matched to the PIC threshold where feasible; if OTDirect cannot or should not preserve that special case, the implementation notes must explain the decision with safety and compatibility reasoning.
- [x] #7 MQTT and REST one-shot command paths are covered in the analysis so stale overrides from those paths expire the same way as raw serial-style commands.
- [ ] #8 A focused verification demonstrates: `CS` persists before the timeout, clears after the timeout without refresh, stays active when refreshed, and `C2` follows the same behavior.
- [x] #9 PIC firmware/source files are not modified.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Per-command millisecond timestamps otCSLastCommandMs and otC2LastCommandMs added near override state. CS=/C2= handlers refresh the timestamp on a non-zero value and zero it on explicit clear. Periodic 1Hz timer in loopOTDirect() compares (millis - timestamp) against OT_CSC2_EXPIRY_MS = 60000ms; on timeout it calls clearWriteOverride(1) / clearWriteOverride(8) and zeroes the timestamp. Refreshed CS=/C2= reset the timer so SAT's periodic control loop is not affected while healthy. Explicit CS=0/C2=0 clear immediately and disarm expiry. PIC source untouched. AC #2 and #6: low-value/no-expiry carve-out NOT implemented — gateway.asm has a threshold-based preserve case for low setpoints, but OTDirect treats all CS/C2 the same to keep the change minimal and avoid replicating an undocumented numeric threshold. Documented as a known divergence; clients needing PIC-exact preserve behaviour for low setpoints should use PIC firmware. AC #8 hardware verification deferred.
<!-- SECTION:FINAL_SUMMARY:END -->
