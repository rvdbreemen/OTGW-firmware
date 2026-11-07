---
id: TASK-1694
title: OT-Direct auto-detect must not persist the master mode it picks at boot
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T13:12:00Z'
updated_date: '2026-11-07T13:19:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 809000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 (session, AskUserQuestion: 'Alleen het opslaan fixen'): auto-detect (OTDirect.ino:1027-1052, gateway mode only, 5 s thermostat listen) calls setOTDirectMode(OTD_MODE_MASTER), which always persists the mode via updateSetting("OTDmode") (OTDirect.ino:2380-2385). Next boot the mode is master, auto-detect is skipped, and the unit never returns to gateway when a thermostat is connected later. Fix: the boot auto-detect switches without persisting; user mode changes still persist. Existing units with OTDmode=2 saved by auto-detect stay master until the user sets gateway once (cannot tell user vs auto origin): release-note item. Detection itself stays as is (boot only, thermostat only). Behaviour in master mode is passive after TASK-1684.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A boot without a thermostat runs in master mode but settings.ini keeps OTDmode=1 (gateway); the next boot with a thermostat stays in gateway mode (host test red on old code, green on fix)
- [ ] #2 A user mode change (REST/GW= command) still persists
- [ ] #3 Release-note/BREAKING line for units that already saved OTDmode=2
<!-- AC:END -->
