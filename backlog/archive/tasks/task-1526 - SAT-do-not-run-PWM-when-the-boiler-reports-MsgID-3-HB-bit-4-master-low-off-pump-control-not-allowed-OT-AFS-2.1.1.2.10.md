---
id: TASK-1526
title: >-
  SAT: do not run PWM when the boiler reports MsgID 3 HB bit 4 (master low-off &
  pump control not allowed), OT AFS 2.1.1.2.10
status: To Do
assignee: []
created_date: '2026-10-31T13:47:00Z'
labels:
  - sat
dependencies: []
ordinal: 682000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entry 179 (Robert 2026-10-31): build it after Sergeant D's OK (Q-1519j). OT AFS v1.1f 2.1.1.2.10: 'It's mandatory for the master to read ID3 at start-up and check bit 4'; bit 4 = 1 means the slave does not allow the master's low-load control (protocol spec v4.2 docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:1610, '[allowed, not allowed]'). Today the firmware decodes the bit only for MQTT (OTGW-Core.ino:3168-3186, topic supports_pump_control / master_low_off_pump_control_function, whose name reads as the opposite: ON = not allowed); SAT ignores it; thermo-nova only shows it as a binary sensor. Found in docs/sat/sim-1519/evaluation-2026-10-31.md section 3. Firmware addition, not in thermo-nova: needs Sergeant D's OK first (ADR-196).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer to Q-1519j recorded in docs/sat/sergeant-d-corrections.md before any code
- [ ] #2 Host test: with MsgID 3 HB bit 4 = 1 the PWM entry (satCycleCheckOvershootSwitch) never switches to PWM; with bit 4 = 0 or MsgID 3 unknown it behaves as before; red before the change, green after
- [ ] #3 SAT log and telnet say once why PWM is disabled (boiler does its own low-load control)
- [ ] #4 Bench or replay: a capture with bit 4 = 1 shows SAT staying in continuous mode
- [ ] #5 python evaluate.py --quick green; combo build green
<!-- AC:END -->
