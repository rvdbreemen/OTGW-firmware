---
id: TASK-1236
title: >-
  SAT on PIC boards: control setpoint lapses between 300 s refreshes (PIC CS
  expiry ~60 s)
status: To Do
assignee: []
created_date: '2026-10-15 15:35'
labels:
  - sat
  - pic
  - bug
dependencies: []
priority: high
ordinal: 374000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the ADR-179 acceptance check (2026-10-15). other-projects/otgw-6.6/gateway.asm:1619-1630 (CommandExpiry): when no CS command arrived in the past minute (HeartbeatCS clear) and controlsetpt1 & 0xF8 != 0 (setpoint >= 8 C), the PIC sets SysCtrlSetpoint and drops the override; the thermostat drives TSet again. SAT re-sends unchanged commands only every SAT_CMD_REFRESH_MS = 300000 ms (SATcontrol.ino:317). On PIC boards (esp32-classic, combo in PIC mode) SAT's steady setpoint therefore likely lapses about 60-120 s after each send. The OTDirect path skips its own CS expiry while SAT owns MsgID 1 (OTDirect.ino:2148), so this is PIC-only. Single-source reading of the assembly, not yet reproduced on hardware. Also: external CS= is not gated by satOwnsControlSetpoint() on PIC boards (addCommandToQueue reaches handleOTDirectCommand only without PIC, OTGW-Core.ino:3616-3623).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench reproduction on the Classic-S3 (PIC): capture shows TSet returning to the thermostat value within ~2 min after a SAT CS= while SAT holds a steady setpoint (old code)
- [ ] #2 Fix: SAT re-sends its CS= on PIC boards within the PIC expiry window (<= 50 s), without raising the OTDirect cadence
- [ ] #3 Same capture on the fix shows TSet held at the SAT value for >= 10 min
- [ ] #4 Decide and record whether an external CS= is refused while SAT owns MsgID 1 on PIC boards (ADR-179 scope)
<!-- AC:END -->
