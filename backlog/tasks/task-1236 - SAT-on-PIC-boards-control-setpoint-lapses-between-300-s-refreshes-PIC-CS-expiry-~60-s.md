---
id: TASK-1236
title: >-
  SAT on PIC boards: control setpoint lapses between 300 s refreshes (PIC CS
  expiry ~60 s)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-15 15:35'
updated_date: '2026-10-15 17:06'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-15: expiry window computed from gateway.asm. Timer1 = Fosc/4 (4 MHz internal osc, OSCCON b'01101010') with 1:8 prescale, 16-bit overflow = 0.524 s; StartTimer loads minutetimer=120 and clears the heartbeat flags; Expired = bit 7 after 121 ticks = ~63 s. A CS= starts the timer; at the first expiry the heartbeat is set, so the timer restarts with cleared flags; at the second expiry (~126 s) without a new CS= the PIC sets SysCtrlSetpoint. So SAT's 300 s cadence leaves the override off for roughly 174-237 s of every 300 s. Refresh must be < 63 s to be safe in all phases (AC#2 says <= 50 s). Added scripts/pic_cs_expiry_probe.py: reads the PIC stream on port 25238, follows MsgID 1 T/R frames and 'CS: x' acks, reports override ON/OFF transitions with time since the last ack. Smoke-tested on synthetic lines; bench run waits for the Classic-S3 (not connected today). Bench recipe: (1) --send-cs 45 --duration 240 with SAT off: expect override OFF ~63-126 s after the ack; (2) SAT on, --duration 900: expect OFF phases between SAT's 300 s acks.
<!-- SECTION:NOTES:END -->
