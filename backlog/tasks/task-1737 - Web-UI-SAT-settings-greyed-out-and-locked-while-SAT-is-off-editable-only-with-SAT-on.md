---
id: TASK-1737
title: >-
  Web UI: SAT settings greyed out and locked while SAT is off; editable only
  with SAT on
status: Done
assignee: []
created_date: '2026-11-08T16:49:00Z'
updated_date: '2026-11-08T22:55:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 844000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, #dev-sat-mqtt 2026-11-08 17:48 (msg 1569030889692987405): 'No setting should be editable when SAT is turned off. It should be grayed out. When you turn off SAT UI should be locked. Only when when it's enabled it should be editable.' Follows his 17:23-17:27 rules (see TASK-1735: dial only sets the SAT target; with SAT off the UI only shows the thermostat's values). Interpretation, posted back to Robert for correction: the Enable SAT switch stays usable; non-SAT settings (WiFi, MQTT, gateway, sensors) stay editable; all other SAT settings are disabled/greyed in the v2 settings UI while SAT is off, and saving them is not possible from the UI. Companion of TASK-1735 (same file v2.js; coordinate with toegang).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT off: every SAT setting except Enable SAT is greyed out and cannot be changed or saved from the web UI (webui test red on old code, green on fix)
- [x] #2 SAT on: SAT settings are editable as before (test)
- [x] #3 Bench .61 with SAT off and on, screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 17:50 Robert confirmed the interpretation (msg 1569031315653791996): 'Yep. You got it right. The SAT switch should always be usable.' So: Enable SAT always usable; non-SAT settings stay editable; all other SAT settings greyed out and locked while SAT is off.

2026-11-08 implementation (session toegang, same lane as TASK-1735). Where SAT settings live: since TASK-1211 the Settings page shows only a pointer card for the SAT category; the SAT fields render on the SAT page (#satSetL2/#satSetL3) through the same settingRow()/saveSettings(); the SAT page also has its own controls that POST at once (target +/-, DHW, max flow, max modulation, presets, curve click, detect location, simulation). Lock (v2.js): satIsOn() = sat/status enabled once known, else the stored satenabled; with SAT off settingRow() disables every catFor(k)==='sat' field except satenabled (greyed, title), renderSatSettings() shows 'SAT is off: the SAT settings are locked. Switch SAT on (top right) to change them.', saveSettings() drops locked SAT keys (also after a scripted change), applySatLock() at the end of renderSatPage() disables the page controls (and the preset buttons) and re-renders the forms when the state flips, satCurveClick() returns. Stay usable: #satEnable, Re-run setup (the wizard), view buttons, the beta notice close; non-SAT categories (BLE sensors are 'sensors') unchanged. Interpretation to confirm with Robert: there is no Enable switch inside the forms; the only switch is #satEnable on the SAT page, which saves at once. So switching it on unlocks the fields as soon as that save is answered (enable then configure, without a separate Save); switching it off locks them at once. The lock follows SAT's own enabled setting, not the MQTT-loss fallback (TASK-1735's dial uses enabled || fallback_active). Tests: tests/webui/sat-settings-locked-when-off.test.mjs 3/3 (SAT off: fields + 6 page controls disabled, switch and Re-run setup usable, note shown, a scripted change + Save posts nothing, mqttbroker editable; SAT on: all editable; SAT off and not onboarded: #satEnable still starts the wizard with a usable first step); on origin/dev v2.js the SAT-off test fails, the others pass. Regression: sat-*.test.mjs all pass after giving sat-area-sensor-mapping SAT on in its fixture (it edits and saves a SAT setting, which is locked with SAT off now); settings-error-toast (2 fail) and settings-help (20 fail) fail identically on origin/dev v2.js, so not from this change. evaluate --quick 0 failed.

2026-11-08 review (OTGW agent 3) and Robert's takeover rule (via the coordinator): during a SAT fallback takeover (fallback_active, Enable switch off) the home DIAL stays on and sets the SAT target (SAT really controls: sat.owns = enabled || fallback_active), while the SAT SETTINGS stay locked (they follow settings.sat.bEnabled and the live switch). Fixes from the review: (1) lock source is live: satEnabledLive is set from every sat/status answer (home fetchSatStatus and SAT-page fetchSatPage, same endpoint, enabled = settings.sat.bEnabled), else the stored satenabled; (2) markDirty() ignores a locked SAT key, and saveSettings() no longer drops silently: an edit made while SAT was on and locked since is dropped with a red toast naming it; (3) the dial asks sat/status once while sat.owns is unknown (dialAsk pending guard; presses add up), so rapid presses stay within the 2-in-flight rule. New tests: takeover (settings locked + dial sets the SAT target), unlock after the switch without a reload, dropped-edit toast, one sat/status ask for four presses.

2026-11-08 otgw-firmware-80 red/green on int-157 (HEAD e4d0280f4) vs origin/dev 07b38ee49 + the new test file: sat-settings-locked-when-off fix 6/6, old 2/6. Red on old: locked with SAT off, unlock on switch-on, takeover keeps locked, edit made while on not saved after off. Green on both (AC2, unchanged behaviour): 'with SAT on, SAT settings are editable' and 'SAT off and not set up starts the wizard'. settings-help needed the satLockedKey stub (e4d0280f4). AC3 (bench .61) pending.

2026-11-08 23:46 otgw-firmware-80 bench .61 on beta.157+8b67ad0 (OTA app+fs): SAT page Control view in headless Chrome against the real device. SAT on: 41 SAT rows, 41 editable, 0 locked (61-1737-sat-on-control.png). POST satenabled:false: 41/41 locked, note 'SAT is off: the SAT settings are locked', switch shows Disabled and stays usable (61-1737-sat-off-control.png). SAT restored on. Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\evidence\int157-beta157
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: sat-settings-locked-when-off 6/6 on the fix, 4 of 6 red on origin/dev 07b38ee49. AC2: 'with SAT on, SAT settings are editable' green on both trees (behaviour unchanged). AC3: .61 beta.157 screenshots, SAT on 41/41 editable, SAT off 41/41 locked. settings-help needed the satLockedKey stub (e4d0280f4). Pushed in 8b67ad039, posted as beta.157.
<!-- SECTION:FINAL_SUMMARY:END -->
