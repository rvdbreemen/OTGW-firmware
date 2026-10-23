---
id: TASK-1322
title: 'SAT page: remove the manual Continuous/PWM burner cycle buttons'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T15:43:00Z'
updated_date: '2026-10-23T16:45:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 445000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-23 15:08Z (#alpha-testing msg 1563207530275152037): 'the continuous and pwm buttons should never be present in the SAT dashboard. Because the software decides when every mode will be in charge.' TASK-1310 moved the Burner cycle mode card to the Control view; this removes the manual mode buttons. Check what they write today (POST /api/v2/sat/mode, satSetControlMode, settings.sat.bForcePWM?) and whether a read-only indicator of the active mode should stay (the mode is still useful to see). Decide with Sergeant D whether the force-PWM setting itself stays (setting vs dashboard button).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The SAT page has no buttons that set Continuous/PWM; the active mode is still shown read-only (screenshot)
- [x] #2 No other UI path changes the mode; the REST endpoint decision (keep for API users or remove) recorded with Sergeant D's answer
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
v2.html/v2.js: replace the Burner cycle mode button row (#satCycleRow .ds-seg data-mode) with a read-only mode indicator (text from sat/status control_mode, e.g. 'Continuous' / 'PWM', plus the existing sub text) on the Control view; remove the click handlers that POST /api/v2/sat/mode; keep the force-PWM setting in SAT settings (check it exists in SAT_L3; if not, note it). Tests: tests/webui/sat-page-order.test.mjs: no element posts sat/mode anymore (request log), the indicator shows the mode from a fake status (continuous then pwm), no .ds-seg buttons in the card; HEAD red. Screenshot Control view before/after. Firmware unchanged (REST endpoint kept; documented).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 16:16-16:17Z Sergeant D: 'Yes the active mode can stay visible'; 'Follow the thermo-nova about how it is done'. thermo-nova @7beaaf5: force PWM is a config option, not a dashboard control (const.py:80 CONF_FORCE_PULSE_WIDTH_MODULATION, default False const.py:149, config_flow.py:728 options schema, entry_data.py:58/223). Decision: remove the Continuous/PWM buttons; show the active mode read-only; keep the force-PWM setting (SAT settings), as thermo-nova does. POST /api/v2/sat/mode stays for API users unless it has no other caller (to check).

2026-10-23 18:50 evidence: Burner cycle card shows the mode read-only (#satCycleMode from control_mode, 'Continuous'/'PWM'/'Off'); buttons and their satPost('mode') handler removed; .ds-seg CSS removed (no other users). Force PWM stays as SAT setting (v2.js SET_META satforcepwm, 'PID / PWM' group). sat/mode has no UI caller left; REST endpoint kept for API users (firmware unchanged). tests/webui/sat-page-order.test.mjs: fix 24 PASS 0 FAIL incl. read-only Continuous/PWM, no buttons, 0 POST to sat/mode over the whole test; HEAD: mode-indicator/no-button checks FAIL (the no-POST check cannot go red on HEAD since the test no longer clicks). Regression v2-navigation 50, sat-status-pill 14, tret-pr-events 7, evaluate 0 fail. build.bat SUCCESS (littlefs 18:44), bench fs flash, settings 178/178 differing 0, device serves #satCycleMode and no data-mode buttons. Screenshots docs/evidence/task-1322/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT page no longer offers Continuous/PWM buttons (Sergeant D: SAT chooses the mode itself); the Control view shows the active mode read-only, and forcing PWM stays a setting under SAT tuning, as thermo-nova has it as a configuration option (const.py:80, config_flow.py:728). The REST endpoint POST /api/v2/sat/mode stays for API users. Evidence: AC1 browser test (read-only mode, no buttons) red on HEAD, green on the fix, screenshots; AC2 no POST to sat/mode anywhere in the UI test, grep shows no other caller. Not verified: a real phone.
<!-- SECTION:FINAL_SUMMARY:END -->
