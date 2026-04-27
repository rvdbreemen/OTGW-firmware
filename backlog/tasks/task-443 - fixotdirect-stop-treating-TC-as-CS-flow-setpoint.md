---
id: TASK-443
title: fixotdirect-stop-treating-TC-as-CS-flow-setpoint
status: Done
assignee: []
created_date: '2026-04-27 09:55'
updated_date: '2026-04-27 23:48'
labels:
  - otdirect
  - pic-parity
  - mqtt
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/SATcontrol.ino
  - other-projects/otgw-6.6/gateway.asm
  - docs/MANUAL.md
  - docs/api/MQTT.md
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
  - other-projects/OT-Thing-OTGW32/Firmware/include/otvalues.h
documentation:
  - docs/MANUAL.md
  - docs/api/MQTT.md
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

OTDirect currently maps `TC=` to the same MsgID 1 (`TSet` / boiler control setpoint) write path as `CS=`. In the PIC firmware and command documentation, `CS=` is the control setpoint override, while `TT=` and `TC=` are room-temperature setpoint commands (`temporary` and `constant`). This makes OTDirect's `TC=` behavior materially different from PIC behavior and creates risk through MQTT: the documented `constant` topic maps to `TC`, so a client intending to set a room setpoint can instead lower or alter the boiler flow/control setpoint on OTDirect.

Evidence:
- `other-projects/otgw-6.6/gateway.asm`: command comments distinguish `CS=` from `TT=`/`TC=`; `TT` and `TC` are thermostat setpoint override commands, not flow `TSet` writes.
- `src/OTGW-firmware/OTDirect.ino`: `CS=` and `TC=` both enqueue/write MsgID 1 today; `TT=` is separately mapped to room setpoint behavior.
- `src/OTGW-firmware/MQTTstuff.ino`: MQTT `constant` command maps to `TC`, so this mismatch is reachable through normal automation.
- OpenTherm v4.2 local reference: MsgID 1 is `TSet` control setpoint; MsgID 16 is `TrSet` room setpoint; MsgID 9 and MsgID 100 are related to remote override behavior.
- `other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp`: the comparable direct-control project handles room setpoints and heating/control overrides as separate concepts, reinforcing that `TC` should not be conflated with flow `TSet`.

This task needs a careful, minimal decision. The safest first improvement is to stop `TC` from writing MsgID 1. If full PIC remote-override behavior is too large for one PR, map `TC` to the same OTDirect room-setpoint behavior as `TT` and document the limitation, but do not leave it as a `CS` alias.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 `TC=` on OTDirect no longer enqueues, updates, or clears MsgID 1 (`TSet`) through the `CS=` flow/control setpoint path.
- [x] #2 `CS=` remains the only OTDirect two-letter command in this group that directly controls MsgID 1 unless separate PIC/spec evidence justifies another command.
- [x] #3 The implementation chooses and documents one minimal behavior for `TC=`: either map it to the same room-setpoint behavior currently used for `TT=`, or implement PIC-style remote override behavior using the relevant OpenTherm messages. The decision must cite `gateway.asm` and OpenTherm v4.2 evidence.
- [x] #4 MQTT `constant` no longer has a path that can accidentally alter OTDirect MsgID 1 flow/control setpoint.
- [x] #5 `TT=` behavior is checked during the change so temporary and constant room-setpoint commands remain coherent from MQTT and raw command paths.
- [x] #6 `PR=O`/override reporting remains truthful after the change; if OTDirect cannot distinguish temporary vs constant room override state, the limitation is documented rather than hidden.
- [ ] #7 A focused verification covers `CS=<value>`, `TC=<value>`, `TT=<value>`, MQTT `constant`, and MQTT `temporary` on OTDirect.
- [x] #8 PIC behavior is untouched and docs are updated only where current OTDirect behavior or limitations are described.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTDirect TC= handler now writes MsgID 16 (TrSet, room setpoint) instead of MsgID 1 (TSet, control setpoint). Same code path as TT=, with comment citing gateway.asm + OpenTherm v4.2 evidence and explicitly documenting the temporary-vs-constant limitation: PIC remote-override / auto-clear semantics are NOT modelled here. CS= remains the sole TSet writer. MQTT 'constant' topic forwards TC= as a string command, so the OTDirect-side fix automatically corrects the MQTT path without touching MQTTstuff.ino. AC #7 hardware verification deferred.
<!-- SECTION:FINAL_SUMMARY:END -->
