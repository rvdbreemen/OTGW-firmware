---
id: TASK-1694
title: OT-Direct auto-detect must not persist the master mode it picks at boot
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T13:12:00Z'
updated_date: '2026-11-07T14:14:00Z'
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
- [x] #1 A boot without a thermostat runs in master mode but settings.ini keeps OTDmode=1 (gateway); the next boot with a thermostat stays in gateway mode (host test red on old code, green on fix)
- [x] #2 A user mode change (REST/GW= command) still persists
- [x] #3 Release-note/BREAKING line for units that already saved OTDmode=2
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 bench .61 beta.123+b73ed43: otdmode set to 1 (gateway) via REST, two reboots without a thermostat: boot 1 runtime master, saved otdmode 1; boot 2 runtime master, saved otdmode 1. Before the fix the unit had otdmode 3 saved by the auto-detect. A user mode change persists (REST otdmode=1 and later otdmode=3 both read back from /api/v2/settings). Source test tests/test_otd_autodetect_no_persist.py: old 2 failures + 1 error, fix OK. Release-note line is in BREAKING_CHANGES.md and CHANGELOG [Unreleased] Fixed.
<!-- SECTION:NOTES:END -->
