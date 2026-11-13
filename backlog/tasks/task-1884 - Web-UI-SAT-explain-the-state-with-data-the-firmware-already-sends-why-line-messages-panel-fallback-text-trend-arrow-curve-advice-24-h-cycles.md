---
id: TASK-1884
title: >-
  Web UI SAT: explain the state with data the firmware already sends (why-line,
  messages panel, fallback text, trend arrow, curve advice, 24 h cycles)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:23:00Z'
updated_date: '2026-11-13T06:55:00Z'
labels:
  - webui
  - sat
dependencies:
  - TASK-1879
priority: medium
ordinal: 957000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
UI proposals U1-U6 from the VTherm UI review (docs/sat/vtherm-comparison-2026-11-13.md, section Web UI). UI only (v2.js, v2.html, v2.css, i18n packs); no firmware change, no extra API call: everything reads the existing GET /api/v2/sat/status poll. U1 fallback_reason as text instead of a number (v2.js:7258, enum SATtypes.h:61). U2 a why-line under #satStatusBig from existing fields in a fixed priority: fallback_active/fallback_reason, pwm_veto_reason (1 no valid reply yet, 2 MsgID 3 bit 4 boiler forbids low-off), mod_suppressed, dhw_active, valves_open; texts describe what the port does and are checked against sergeant-d-corrections.md before merge. U3 an info/warning icon with an overlay listing all active messages, absorbing the four hint lines (v2.js:7067-7071). U4 a trend arrow next to the room temperature from indoor_rise_rate. U5 curve_recommendation (insufficient/increase/decrease/hold) on the heating-curve card. U6 a 24 h cycles card in the Technical view (24h_cycles, 24h_duty_ratio, 24h_overshoot_fraction, 24h_underheat_fraction, 24h_long_cycle_fraction). Constraints: ADR-091 class drift gate, t() for every string (nl/de/el via translate_webui_otgw-firmware), max 2 calls in flight, LittleFS budget.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 tests/webui test(s) cover U1-U6 rendering from a fixture sat/status payload; red before the change, green after
- [ ] #2 python evaluate.py --quick shows no new FAIL (ADR-091 drift gate included); i18n-no-hardcoded and the other webui tests green
- [ ] #3 No new fetch: the SAT page still issues only sat/status + sat/weather per poll (network capture or test)
- [ ] #4 nl/de/el packs carry the new keys (translate skill sync mode)
- [ ] #5 Bench screenshot of the SAT page on a unit (otgw-bench-tester) showing the why-line and the messages panel
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
UI only (v2.html, v2.js, v2.css, i18n/*.json, one new test). No firmware, no new fetch. U1 satFbReasonText(d): fallback_reason 1/2 -> t('tools.sat_fb_reason_thermostat_lost'/'mqtt_lost') (enum SATtypes.h:61), used in Simulation kv and messages. U2 #satWhy under #satStatusBig, only while enabled, first hit of: fallback_active (reason text); PWM refused now (pwm_veto_reason>0 and live !slave_cfg_valid (1) or slave_low_off_denied (2); veto reason is sticky, SATcycles.ino:1323, so the live flags gate it); mod_suppressed (entry 267); dhw_active; valves_open false (entry 32: CS=10, no heating). U3 info/warn button #satMsgBtn with count next to the status; panel #satMsgs holds the four existing hint divs (ids kept) plus the U2 reasons as items; opens by itself when a warning is active, the button toggles it. U4 #satRoomTrend arrow next to the Room tile from indoor_rise_rate (degC/h), |rate|>=0.2 -> up/down, title shows the rate; rate is 0 when solar-gain detection is off (SATcontrol.ino:6733), so then no arrow. U5 #satCurveRec under the curve legend: curve_recommendation insufficient/increase/decrease/hold as text (SATcontrol.ino:5484-5512, mean error vs 2x deadband over >=6 hourly samples). U6 new Technical-view card #satCycles24Kv: 24h_cycles, duty, overshoot, underheat, long-cycle fractions as %. Test tests/webui/sat-explain-state.test.mjs on the real page with fixture payloads, red on old data dir (OTGW_DATA_DIR), green on new; it also records every /api path the page hits after opening SAT (AC3). evaluate --quick + i18n-no-hardcoded + sat-* webui tests one file at a time. nl/de/el via translate_webui skill sync. FS build slot via coordinator, then OTA .61 with SAT sim for screenshot (synthetic data, no IP/hostname).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (wt-09-1884, lane-09-1884 from origin/dev fe-era HEAD): v2.html (#satWhy, #satMsgBtn/#satMsgs wrapping the four existing hint divs with ids kept, #satRoomTrend, #satCurveRec, Technical card #satCycles24Kv), v2.js (satFbReasonText, satWhyReasons, satRenderMessages, satRenderRoomTrend, satRenderCurveRec, satRenderCycles24; button wired in wireSat), v2.css (new classes with selectors, ADR-091), i18n nl/de/el +22 keys each.

Text provenance (UI describes what the port does): takeover = MQTT lost >5 min, SAT takes over (SATcontrol.ino:6876-6890; fallback_reason 1 THERMOSTAT_LOST is never set, only 2); PWM refusal per entries 213-215 (MsgID 3 bit 4, no PWM before a valid reply); pwm_veto_reason is sticky (only assignment SATcycles.ino:1323), so the why-line also requires the live flag (slave_cfg_valid false / slave_low_off_denied true); mod_suppressed per entry 267; valves closed per entry 32 (CS=10, no heating start); indoor_rise_rate is 0 while solar-gain detection is off (SATcontrol.ino:6733) so the arrow uses a +-0.2 degC/h dead band; curve_recommendation = hourly mean error vs 2x deadband, >=6 samples (SATcontrol.ino:5484-5512). heating_curve_recommendation (multi-day medians, SATcycles.ino:1686) is not shown; out of scope of U5.

Evidence AC1/AC3: tests/webui/sat-explain-state.test.mjs (real page, headless Chrome, fixture payloads, 17 checks). Old data dir (git archive HEAD before edits): 13 failures of 17 (the 4 passes are absence checks + the AC3 baseline). New: 0 failures of 17. AC3: API_ALLOWED = path set measured on the old page (device/info, device/time, filesystem/hash-check, health, otgw/messages/48+49, otgw/otmonitor, sat/ble/discovery, sat/markers, sat/status, sat/weather, sensors, sensors/labels, settings); new page: extra none. Logs in scratchpad t1884/red-old2.txt, green-new.txt; screenshot after-sat-msgs.png (synthetic). AC2: evaluate --quick 92 checks, 0 FAIL 0 WARN; i18n-no-hardcoded 3/3, i18n-followups 4/4, no-sample-values 2/2, i18n-lang 6/6, lang-menu 7/7, sat-push-refused-hint 0/2 fail, sat-status-pill 0/14 fail, sat-page-order no exception; run one file at a time, Chrome count 0 after.

AC4: nl/de/el keys written inline by the main session from i18n/glossary.md terms (stooklijn/Heizkurve/καμπύλη θέρμανσης, inschakelduur/Einschaltdauer/ποσοστό λειτουργίας, cyclus/Takt/κύκλος, warm tapwater/Warmwasser/ζεστό νερό χρήσης) and the existing pack wording; not through the multi-agent translate workflow (no opt-in for a workflow run, Robert asked for the fastest result). A native-review refresh can follow. Build/bump: waits for the 220 post (coordinator), then rebase, bump to 221, commit, FS build slot.
<!-- SECTION:NOTES:END -->
