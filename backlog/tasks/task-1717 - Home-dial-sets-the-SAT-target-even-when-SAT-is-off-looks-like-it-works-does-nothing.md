---
id: TASK-1717
title: >-
  Home dial +/- sets the SAT target even when SAT is off (looks like it works,
  does nothing)
status: Done
assignee: []
created_date: '2026-11-08T07:54:00Z'
updated_date: '2026-11-08T15:31:00Z'
labels:
  - webui
  - field-report
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 826000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs 2026-11-08 07:51 #alpha-testing (msg 1568895744029499403): in OT-Direct gateway mode with a thermostat and SAT off, the phone view dial (+/-, design-b bUp/bDown) 'doesn't do anything'. v2.js stepTarget() always POSTs /api/v2/sat/target and reports 'target set', so with SAT off the boiler is unaffected and the UI suggests success. The thermostat owns the setpoint; the gateway way is TT= (temporary) / TC= (constant) room setpoint override (OTDirect.ino:2276-2311, TASK-466), refused while SAT is on (TASK-1695).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With SAT off the dial does not pretend to set anything: it either sends a TT= room-setpoint override (and says so) or is shown as SAT-only (decision Robert)
- [x] #2 With SAT on the dial keeps setting the SAT target (test)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
tjfs 2026-11-08 07:57 (msg 1568897217505402944): 'definitely not obvious! I think if you're not in sat mode the temperature dial should alter the TT setting.' Depends on TASK-1718 (OT-Direct TT= does not reach the thermostat yet).

Robert 2026-11-08: with SAT off the dial sends a TT= temporary room-setpoint override and says so (e.g. 'thermostat override 20.5 C until its next program change'); with SAT on it keeps setting the SAT target. Depends on TASK-1718 (beta.142) for OT-Direct; PIC boards already handle TT=.

2026-11-08 implementation (per Robert's decision above). v2.js: sat.owns = sat/status enabled || fallback_active (the satOwnsControlSetpoint() rule); stepTarget() POSTs /api/v2/sat/target when SAT owns, else stepRoomOverride(): TT=<x> over POST /api/v2/otgw/commands, base = the last TT= this page sent, else the thermostat's room setpoint (MsgID 16), else 20; message 'thermostat override X° until its next program change'. A press before sat/status answered fetches it first (not covered by a test: timing-dependent). Test tests/webui/home-dial-tt-when-sat-off.test.mjs, 3 tests (SAT off -> TT=20.5 + message; SAT on -> sat/target 20.5; SAT off twice -> TT=20.5, TT=21.0): fix 3/3 pass; origin/dev v2.js: both SAT-off tests fail, the SAT-on guard passes. Occasional CDP Page.enable stalls under machine load are infrastructure. evaluate --quick 0 failed. Real device .61 (beta.143 firmware + this v2.js via ota_flash --webui): SAT off, design b +: POST {"command":"TT=20.5"}, UI 'thermostat override 20.5° until its next program change', firmware overrides write 16 = 5248 (20.5 C) and 100 = 2, sat_owns false; restored TT=0 and SAT on, settings diff none. Not tested on hardware: a thermostat following the TT= (none on .61); the SAT-on press (host-tested; skipped to keep the stored SAT target).

Robert 2026-11-08 ~16:30 reversed this: the UI may only control temperatures with SAT on; with SAT off it only shows the thermostat's values. The TT= dial path from beta.146 is removed by the follow-up task (see TASK-1735).
<!-- SECTION:NOTES:END -->
