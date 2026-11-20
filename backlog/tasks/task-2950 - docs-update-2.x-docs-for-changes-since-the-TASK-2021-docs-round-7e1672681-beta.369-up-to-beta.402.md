---
id: TASK-2950
title: >-
  docs: update 2.x docs for changes since the TASK-2021 docs round (7e1672681,
  beta.369) up to beta.402
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T11:12:00Z'
updated_date: '2026-11-20T13:48:00Z'
labels:
  - docs
  - update-docs
milestone: 2.0.0
dependencies: []
ordinal: 1078000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Standalone /update-docs-2x run (coordinator assignment for TASK-1740, 2026-11-20). Base 7e1672681 (TASK-2021 docs round, beta.369), head 53f9f94ee, 36 code commits, 12 areas: all chapters affected. Unmapped: scripts/README.md, scripts/build_manual.py (ch08), fsWriter.h/.ino (ch08 + C4), settingsHelpGz.h (generated, no doc impact). Manual findings: OTGW32=0 in every chapter; ESP8266 mentions ch01=4, ch02=11, ch07=3, ch08=1, ch10=7 (same in NL), Robert's call. Known lag: ch03 graph history says 24 h on PSRAM (7 days since TASK-1963).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Manual ch03/h03 (web UI, settings) updated
- [x] #2 Manual ch05/h05 (SAT) updated
- [x] #3 Manual ch04/h04 and docs/api/MQTT.md updated
- [x] #4 Manual ch02/h02, ch06/h06, ch07/h07 updated
- [x] #5 Manual ch09/h09, docs/api/openapi.yaml and docs/api/README.md updated
- [x] #6 Manual ch08/h08 and C4 docs updated
- [x] #7 docs/MANUAL.md regenerated, tests/test_manual_generated.py green
- [x] #8 Cleanup complete
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 ch03/h03 (subagent, verified by 95): Home room temp follows SAT (TASK-2946, v2.js:2965-2972), injected badge kept on a failed poll (TASK-2914), graph history 1 h/30 s without PSRAM and 7 days/2 min with PSRAM plus the 7 days chip thinned to 10 min (graphHistoryStuff.h:42-48, ADR-245; was '24 hours'), SAT target series (TASK-2018), SAT paused hints (TASK-2036), weather card sun elevation (TASK-2015), SAT category 'Smart Autotune Thermostat' (TASK-2928), BLE needs PSRAM switch (TASK-2025), update-check question timing (UItypes.h:23-28). One subagent error reverted: it changed the NL labels to English ('the web UI is English-only'); the UI is translated, NL labels are i18n/nl.json:872 'Gedrag' and :1003-1004. Finding for later: the legacy 'Navigation' half of ch03 still describes the classic UI (ECharts, Screenshot/Export buttons, FSexplorer System Actions); a rewrite decision, not done in this run.

AC2 ch05/h05 (subagent, second read by 95 against code): Kd 0.05 x 8400 x Kp (SATpid.ino:26-28, 232-236; entries 425/427), self-tune 12 counted hours, step 0.05 (SATcurveTune.h:11-28; ADR-260 Proposed), integral halved once outside the deadband and held, main + zone PIDs (SATpid.ino:258-270; ADR-259 Proposed), no switch in a band pause narrowed to the tolBandOff gate, simulation refused while a boiler answers + HA switch unavailable (TASK-2014, ADR-255), the stale +/-20 C cap sentence corrected (entry 75). Thermo-nova differences checked by 95 at the pinned commit: pid.py:93 (0.07) and pid.py:185-189 (integral 0 outside the deadband); 'thermo-nova has no such tuner' replaced by a cited statement (const.py:97, entry_data.py:213: the coefficient is a fixed option). NL fixes by 95: satflameoffoffset row (18.0, 0-30 C, SATtypes.h:554, settingStuff.ino:1229; NL said 0.0 / 0-5) and two '(zie 5.21)' -> 5.20 (HA section in NL numbering). Finding: the PID tuning table is header-only in EN and NL.

AC3 ch04/h04 + docs/api/MQTT.md (subagent, checked by 95): sat/curve_self_tune description now 12-hour window and 0.05 step (SATcurveTune.h:10-29), 'counted hours'; HA number entity outside -> OT= counts for SAT on PIC boards (TASK-2040, MQTTHaDiscovery.cpp:3258); SAT switch table now lists the 15 switches of MQTTHaDiscovery.cpp cases 0-14 (removed SAT_OVP_Enabled, 0 hits in src/; added SAT_Curve_Self_Tune, SAT_Fireplace_Mode, SAT_DHW_Enable, SAT_Enabled); sat/sun_elevation command row; 10 old em dashes in MQTT.md removed. Breaking found: TASK-1889 (beta.379) removed sat/curve_tune/days and .../clean_hours_today (now window_hours) and the REST curve_tune fields days/days_needed/clean_hours_*/hours_today (now window_hours/_needed); added to the stable notes BREAKING list (95).

AC4 ch02/h02, ch06/h06, ch07/h07 (subagent, spot-checked by 95): flash_otgw.sh --update paragraph in ch02 (TASK-2028; flash_otgw.sh 141-148, 395-447); OLED section corrected (5 pages, fixed 30 s auto-off, address 0x3C, no OLED setting; stable build shows FW 2.0.0; OLED.ino:28-35, 57-60, 356-362; TASK-2932); ch06 6.1.2 dead-link check paragraph (TASK-2026; networkStuff.ino:631-642, 830-848, 926-938); ch07 re-association telnet line (networkStuff.ino:935) and the capture-host gap in gaps.log (TASK-2922; otgw_capture.py:2345-2375). Finding confirmed by 95: the starved-heap AP-fallback skip runs only in the runtime path (networkStuff.ino:658 wifiNextStateAfterFailedAttempt); the boot path calls startAPFallback() without a heap check (networkStuff.ino:426-430). 6.1.3 EN/NL and the stable notes now say 'at runtime'. Reported to the coordinator as a possible firmware gap (not a docs decision).

AC5 ch09/h09, docs/api/openapi.yaml, docs/api/README.md (subagent, spot-checked by 95): curve_tune REST fields + BREAKING paragraph (TASK-1889), update-check settings and defaults (TASK-2023; restAPI.ino:4201-4203, UItypes.h), BLE 409 body on POST /api/v2/settings (restAPI.ino:4600-4603), simulation 409 body (restAPI.ino:1697-1701), busy answer: 503 + Retry-After: 1, or a closed connection without a response below a 4096 B largest block (restAPI.ino:208-217, checked by 95), web UI fetchWithRetry 3 attempts at 400/800 ms (v2.js:100-110, checked by 95), graph history ?hours= 1-168, max 1008 slots, satTarget (ADR-245; openapi count max 720 -> 1008), health no longer writes /.health (TASK-1865/TASK-2020, helperStuff.ino:263). YAML safe_load ok. Findings: /api/v2/diag/recording routes (TASK-1666, before the base) missing from all four files; duplicate /sat/settings/{name} lists in ch09; old em dashes in openapi.yaml (15) and README (9) untouched.

AC6 ch08/h08 + C4 (subagent): MANUAL.md generated by scripts/build_manual.py (TASK-2030), flash zip packs the otgw_capture.py modules (TASK-2917, build.py:1224-1235), flash_otgw.sh --update (TASK-2028), prerelease telnet keys H and B (handleDebug.ino, restAPI.ino:48-62, guarded by _VERSION_PRERELEASE), health check read-only (TASK-2020, helperStuff.ino:279-289); C4: AP fallback no longer 'prerelease only' in c4-code-network/component-network/configuration-state/settings/sensors-hardware, boot path without heap guard (TASK-2951), PID integral halve-and-hold in c4-code-sat (ADR-259 Proposed), OLED pages in c4-code-sensors, fsWriter file:line in c4-code-utilities. AC7: python scripts/build_manual.py -> 'wrote docs\MANUAL.md (5877 lines from 10 chapters)'; python tests/test_manual_generated.py -> 'Ran 4 tests ... OK'; 0 em dashes in MANUAL.md; 0 em dashes in all added lines of this run. AC8 cleanup: 4B no release files at the repo root. 4A (archive 22 old release notes) and 4C (group 6 old reviews) deliberately NOT done: the beta.250 notes are linked with absolute blob/dev URLs from the published notes of beta.294 to beta.401 (and the draft links beta.170), and the 2026-02 reviews are linked from ADR-035 and ADR-040; moving them would break published links. Decision for Robert/coordinator if the archive move should still happen with link updates.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Standalone /update-docs-2x run, base 7e1672681 (TASK-2021 round, beta.369) to beta.402, 36 code commits, 12 areas. AC1 ch03/h03, AC2 ch05/h05 (SAT claims second-read by 95 against the code and thermo-nova pid.py:93, pid.py:185-189, const.py:97), AC3 ch04/h04 + MQTT.md, AC4 ch02/h02 + ch06/h06 + ch07/h07, AC5 ch09/h09 + openapi.yaml (YAML ok) + api README, AC6 ch08/h08 + 11 C4 files, AC7 MANUAL.md regenerated, test_manual_generated 4 tests OK, AC8 cleanup checked (archive moves skipped on purpose, links). Breaking found and added to the stable notes: TASK-1889 curve_tune MQTT topics and REST fields. Firmware finding filed: TASK-2951 (boot-path AP fallback without the heap guard). Findings for later: legacy classic-UI half of ch03; header-only PID tuning table in ch05; /api/v2/diag/recording routes undocumented (TASK-1666, before the base); duplicate /sat/settings lists in ch09; old em dashes in openapi.yaml (15) and api README (9); ESP8266 mention counts ch01=4, ch02=11, ch07=3, ch08=1, ch10=7 (Robert's call). Not verified: rendering of the markdown; no device or bench run (docs only).
<!-- SECTION:FINAL_SUMMARY:END -->
