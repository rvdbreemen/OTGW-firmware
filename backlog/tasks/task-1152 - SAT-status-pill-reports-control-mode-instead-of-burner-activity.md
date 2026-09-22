---
id: TASK-1152
title: SAT status pill reports control mode instead of burner activity
status: Done
assignee:
  - '@claude'
created_date: '2026-09-22 06:38'
updated_date: '2026-09-22 09:36'
labels: []
dependencies: []
ordinal: 287000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on Discord #dev-sat-mqtt 2026-06-21 by sergeantd (with screenshot): 'since SAT is not heating at the moment, why it says continuous instead of idle?'

Verified against dev HEAD 2a2e5fba3 (2026-09-22). Still applicable; an adversarial pass confirmed every load-bearing citation verbatim.

THE CONFUSION IS REAL AND IT IS A UI ONE. 'Continuous' is a CONTROL MODE (how SAT modulates: Off / Continuous / PWM, sat.js:54 MODE_LABELS). 'Idle' is a BURNER ACTIVITY state. The status pill mixes the two vocabularies, so a perfectly idle boiler under continuous control reads as 'Continuous' where the user expects 'Idle'.

PARTIALLY ADDRESSED, NOT ENOUGH TO CLOSE. The v2 SAT page (TASK-986, commit f863e4e7e, 2026-07-02, after the report) gave the control mode its own 'Burner cycle mode' card (v2.js:3247-3254) and moved the status pill to activity words (v2.js:3084-3087). But v2.js:3082 still computes heating = enabled && active, which is control-loop engagement, not burner state. And the CLASSIC SAT dashboard pill (#sat-status-badge, sat.js:161-178) is untouched and still prints the control-mode label.

NO FIRMWARE CHANGE NEEDED. boiler_status is already emitted by /api/v2/sat/status (SATcontrol.ino:2012). This is a data-assets-only fix.

CONTRACT TO WRITE DOWN: 'active' means the SAT control loop is engaged (set at SATcontrol.ino:4219); 'boiler_status' means what the burner is actually doing. The two are not interchangeable and the UI must stop treating them as such.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The classic SAT pill (#sat-status-badge, sat.js:161-178) no longer prints a control-mode label; its heating/idle text derives from the status JSON boiler_status
- [x] #2 Pill precedence is explicit and matches the simple-view summary (sat.js:294-305): Disabled -> Safety Tripped -> Window open -> Summer mode -> activity
- [x] #3 Control mode (Off / Continuous / PWM) stays visible in its own Control Mode row (sat.js:209, index.html:590); no information is lost, only relocated
- [x] #4 v2.js:3082 stops computing heating = enabled && active; the v2 pill (3084-3087) and satStatusBig (3098-3100) use the same boiler_status-derived activity state
- [x] #5 No firmware change is made; verified as a data-assets-only change with a filesystem build
- [x] #6 Verified with SAT enabled and the burner off (on device or against a captured /api/v2/sat/status payload): the pill reads Idle and the Control Mode row still reads Continuous
- [x] #7 docs/c4/c4-code-web-assets.md records the active-versus-boiler_status contract in one line
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence: tests/webui/sat-status-pill.test.mjs (new) drives the shipped assets in headless system Chrome over CDP, no new dependency. It serves the real data dir plus a synthetic /api/v2/sat/status body, lets the page's own poll parse it, and reads the rendered DOM.

- 16/16 checks pass on 2047b92d.
- The same harness reports 9 failures against the parent commit, including the reported symptom (pill reads 'Continuous' where 'Idle' is expected), so it fails for the right reason and is not a fixture that confirms its own assumption.
- Screenshot of the decisive case (SAT enabled, control mode Continuous, boiler_status idle -> pill reads IDLE) captured via Page.captureScreenshot, opt-in with OTGW_SHOT_DIR.
- node --check clean on sat.js and v2.js.
- build.bat green on all three targets (esp32, esp32-classic, esp32-combo), artifacts at 2.0.0-alpha.367+2047b92; evaluate.py --quick 0 failures, health 98.7%.

AC6 was satisfied against a synthetic /api/v2/sat/status payload rather than a live device, which the AC explicitly allows.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT status pill reported the control mode where burner activity was expected, so an idle boiler under continuous control read 'Continuous'. Root cause was vocabulary mixing, not a data gap: boiler_status is already published by /api/v2/sat/status.

Both UIs now derive activity through a single small helper (burnerIsFiring / satBurnerIsFiring) that treats off, idle, anti_cycling, post_cycle and overshoot_cooling as not firing and falls back to the control-loop flag only when the firmware omits boiler_status. The classic pill also gained the window-open and summer-mode precedence steps that the simple-view summary already had. The control mode keeps its own row; nothing was removed, only relocated.

Data-assets only, no firmware change. Guarded by a new dependency-free browser harness that fails on the parent commit and passes here.
<!-- SECTION:FINAL_SUMMARY:END -->
