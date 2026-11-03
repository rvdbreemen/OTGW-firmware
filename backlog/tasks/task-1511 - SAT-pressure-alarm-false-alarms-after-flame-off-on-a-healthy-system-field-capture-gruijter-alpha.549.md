---
id: TASK-1511
title: >-
  SAT pressure alarm: false alarms after flame-off on a healthy system (field
  capture gruijter, alpha.549)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T14:12:00Z'
updated_date: '2026-11-03T03:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 667000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence: gruijter's capture 20261030-092718 (alpha.549, Classic PIC, Intergas; msg 1565703062507495454). pressure_alarm toggled on/off 8 times (10:05:49-10:08:19, 10:31:19-10:33:49, 10:54:19-10:55:19, 11:13:49-11:16:49, 11:33:49-11:38:19, 11:57:49-12:11:49, 12:29:49-12:58:19) while CH pressure stayed 1.7-2.3 bar; pressure falls about 2.0 -> 1.71 bar after each flame-off (thermal contraction), e.g. 10:27 2.00 -> 10:32 1.72. Possible cause (inference): the drop-rate detector is not suspended across the flame/CH-active transition the way thermo-nova does (binary_sensor.py:25, :335-345: 600 s suspension and sample clear when coordinator.active changes). Directly tied to open question Q70 (which signal is 'CH active'), queued in docs/sat/open-questions.md. Do not change behaviour before Q70 is answered; first reproduce with a replay of this capture's pressure/flame series in a host test.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test replays the capture's pressure + flame series and shows the false alarms on the current code
- [ ] #2 After Q70 is answered and recorded, the alarm no longer fires on that series; a real slow leak series still alarms
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: fix landed 6f44f9da9 (TASK-1480, entry 201, SATcontrol.ino:4767); alpha.549 capture replay missing. Replay then likely close.
<!-- SECTION:NOTES:END -->
