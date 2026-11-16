---
id: TASK-1999
title: >-
  OT-Direct: the boot-time 'Thermostat disconnected' line lands on port 25238 as
  a client connects, so pyotgw (HA) fails its connect with IndexError
status: To Do
assignee: []
created_date: '2026-11-16T10:48:00Z'
labels:
  - bug
  - ot-direct
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1021000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by bench tester 09 on 2026-11-16 (.61, OT-Direct board without a thermostat, 2.0.0-beta.341+3cb72c3, LegacyPort25238Enabled on; evidence 09 scratchpad pyotgw/). pyotgw 2.2.3 (the library behind HA's OpenTherm Gateway integration) connects to socket://<ip>:25238 right after /ReBoot; the connect fails in 2 of 5 tries (plus 1 of 1 earlier) with IndexError: list index out of range in pyotgw.py:778 process_statusfields_v4. Cause from its debug log: during its connect sequence (PR=A..W, PS=1, summary, PS=0) the bridge pushes the unsolicited 'Thermostat disconnected' (in the failure 19 s after the reboot, 3 ms after PS=0); pyotgw takes it as the PS answer and parses it as the summary. Source: OTDirect.ino checkThermostatTimeout() (origin/dev ~:3478-3497) reports the line once when no thermostat frame was ever seen, on its first run with millis() > 3000, about 19 s after boot; port 25238 starts accepting at 16.8-17.4 s (5 tries). A connect long after boot succeeds (1/1); a Classic with PIC 6.8 (.63, beta.347) succeeds too (an ESP reboot does not reset the PIC). The line itself is PIC parity (TASK-1850, gateway.asm TDisconnect); its timing is not. Fix options, not decided here: report the boot-time line before 25238 accepts clients; do not send it to the bridge at boot (the PIC reports it long before any client connects); send it only on a real connected-to-disconnected transition. Also check pyotgw upstream for robustness against unsolicited lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: a pyotgw connect to 25238 right after /ReBoot, on an OT-Direct unit without a thermostat, fails (IndexError) on the current code in at least 1 of 5 tries
- [ ] #2 Fix: the same reproduction succeeds 5/5 (or 10/10); 'Thermostat disconnected' and 'Thermostat connected' still reach the bridge on a real thermostat loss and return (TASK-1850 parity)
- [ ] #3 Host test on the real checkThermostatTimeout() / bridge path: no unsolicited boot-time line while a client handshake can be in progress; red on the old code
<!-- AC:END -->
