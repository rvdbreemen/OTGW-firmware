---
id: TASK-437
title: fixsat-use-SW-command-for-DHW-setpoint-instead-of-unsupported-TW
status: To Do
assignee: []
created_date: '2026-04-27 09:47'
updated_date: '2026-04-27 09:51'
labels:
  - sat
  - otdirect
  - pic-parity
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/SATcontrol.ino
  - src/OTGW-firmware/OTDirect.ino
  - other-projects/otgw-6.6/gateway.asm
  - docs/MANUAL.md
  - docs/manuals/en/ch09-api-reference.md
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl
  - other-projects/OT-Thing-OTGW32/Firmware/src/mqtt.cpp
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
  - other-projects/OT-Thing-OTGW32/Firmware/include/otvalues.h
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

SAT sends `TW=<dhw setpoint>` when DHW becomes active. `TW=` is not a PIC command in `gateway.asm` and is not implemented by OTDirect. The supported DHW setpoint command is `SW=`. As written, this path produces `NG` on OTDirect and would also be rejected by the PIC command table if reached on PIC hardware.

Evidence:
- `src/OTGW-firmware/SATcontrol.ino`: DHW transition path builds `TW=%d` and logs that it sent `TW=`.
- `src/OTGW-firmware/OTDirect.ino`: command handler implements `SW=` for MsgID 56 and has no `TW=` branch; unknown commands return `NG`.
- `other-projects/otgw-6.6/gateway.asm`: command table lists `SW` under `SerialCmd04`; no `TW` command exists.
- `docs/MANUAL.md` and `docs/manuals/en/ch09-api-reference.md`: DHW setpoint command is documented as `SW=60`.
- OpenTherm v4.2 local reference: MsgID 56 is `TdhwSet` / DHW setpoint.

Keep this as a one-command fix unless testing reveals a second directly coupled issue.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT DHW transition code sends `SW=<setpoint>` instead of `TW=<setpoint>`.
- [ ] #2 The debug/log message is updated so it reports the actual command sent.
- [ ] #3 The command remains guarded by the existing DHW enabled/range/interface checks; no new control-loop behavior is introduced.
- [ ] #4 The valid DHW range check remains at the existing SAT level unless a separately justified bounds change is required.
- [ ] #5 OTDirect accepts the generated command and maps it to MsgID 56 (`TdhwSet`) with the expected f8.8 value.
- [ ] #6 PIC command parity is preserved: the generated command is accepted by `gateway.asm` command table semantics.
- [ ] #7 A focused verification documents the exact generated command for a DHW-active transition, either by test, loopback trace, or code-level assertion in the task notes.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit detail added 2026-04-27 by `codex` on branch `feature-dev-2.0.0-otgw32-esp32-sat-support`.

Cross-project context:
- `other-projects/otmonitor-6.6/otmonitor.vfs/gui.tcl` is the strongest external client reference for PIC command spelling: OTmonitor's DHW setpoint UI sends `SW=<value>`, not `TW=<value>`.
- `other-projects/OT-Thing-OTGW32/Firmware/src/mqtt.cpp` routes its DHW setpoint MQTT topic to direct DHW setpoint control rather than to an alternate `TW` command name. This confirms that direct-control projects still model the feature as the standard DHW setpoint value.
- `other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp` applies DHW setpoint override behavior at the OpenTherm value layer. Use it only to validate the intended target (`TdhwSet` / MsgID 56), not as a reason to refactor this repository's SAT command path.
- `other-projects/OT-Thing-OTGW32/Firmware/include/otvalues.h` names MsgID 56 as the DHW setpoint value, matching OpenTherm v4.2 and this repository's existing OTDirect `SW=` implementation.

Implementation guardrails:
- This should be a one-string command spelling fix plus the matching log text.
- Do not add a `TW=` alias unless a real compatibility need is proven; adding aliases can hide SAT bugs and expands the command surface unnecessarily.
- Verification should prove the SAT path emits `SW=` and that OTDirect handles it as MsgID 56.
<!-- SECTION:NOTES:END -->
