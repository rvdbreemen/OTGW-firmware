---
id: TASK-1881
title: 'SAT self-tune: expose per-reason exclusion counters in REST/MQTT'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T08:26:00Z'
labels:
  - sat
  - telemetry
dependencies:
  - TASK-1879
priority: low
ordinal: 954000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 2 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). The tuner already logs the first SatTuneReason per hour over telnet (SATcurveTune.h:37-40, SATcontrol.ino:6424-6428). Add counters per reason (hours excluded per reason over the current tuning window) to REST and MQTT so a long-run capture shows why days did not count without a telnet transcript. Telemetry only; the tuner decision does not change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: excluded hours increment the counter of their reason; the decision path is unchanged (tests/test_sat_curve_selftune.py green)
- [ ] #2 Counters visible in REST and as flat MQTT topics (ADR-101)
- [x] #3 Web UI (U9): the curve card shows tune progress (counted days x/3, last decision, per-reason excluded hours) from the new fields; webui test on a fixture payload
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (2026-11-13, otgw-firmware-95). Telemetry only: no tuner decision, threshold, exclusion order or file format changes, so no Sergeant D OK is needed (vtherm-comparison lesson A2: "No behaviour change"). ADR-228/ADR-205/ADR-231 unchanged; ADR-101 governs the MQTT topics. No new ADR (existing pattern, telemetry fields).

1. SATcurveTune.h: SATT_COUNT closes enum SatTuneReason. SatTune gets uint16_t exclHours[SATT_COUNT] (excluded hours per reason, saturating at 65535) and the last daily record (haveLast, lastDay, lastDecision, lastReason PSTR). New struct SatTuneProgress {running, days, daysNeeded, cleanToday, hoursToday, haveLast, lastDay, lastDecision, lastReason[16], exclHours[SATT_COUNT]}. Marker #define SAT_CT_EXCL_COUNTERS 1 (like SAT_CT_PHASE_SPLIT) so the old-vs-fix run compiles the old tuner.
2. Pure block: satTuneCloseHour() adds one to exclHours[out.reason] when the hour does not count (the reason it already reports, also the hour-close reasons coeff_changed/no_ticks/few_ticks). satTuneEmptyWindow() clears the counters, so they cover "since the window last started": a manual or a tuner coefficient change, a tuner (re)start, a reboot. The counters are not in /sat_tune.json: after a reboot they restart at 0 while a restored window keeps its days (stated in docs; persisting them would change the file format, not needed for a capture).
3. Glue: satTuneRollHour() stores the day record (decision, reason, day) after satTuneCloseDay(). New satCurveTuneGetProgress(SatTuneProgress&) copies the tuner state; satControlLoop() copies it into state.sat.tune after satCurveTuneService(), so the /sat/status snapshot (state copy) and MQTT read a frozen copy, never _sat_tune across tasks.
4. REST /api/v2/sat/status: nested object "curve_tune": {running, days, days_needed, clean_hours_today, hours_today, last_day, last_decision (raise/lower/hold/none), last_reason, excluded_hours:{not_sat, dhw, no_sun, sun, pwm, flame_off, min_mod, coeff_changed, no_ticks, not_controlling, few_ticks, no_room}}. openapi.yaml updated.
5. MQTT (flat, ADR-101, on-change + heartbeat, not retained, only while curve_self_tune is on): sat/curve_tune/running, /days, /clean_hours_today, /last_decision, /last_reason, sat/curve_tune/excluded_hours/<reason> (12). No HA discovery entity (like the other SAT diagnostics, MQTT.md list). MQTT.md updated.
6. Web UI (AC3, U9): the curve card shows "Self-tune: x/3 days, today y/6 clean hours, last decision <decision> (<reason>)" and the non-zero excluded-hour reasons; webui test on a fixture payload.
7. Tests first: tests/test_sat_curve_selftune.py class ExclusionCounters (red on HEAD via OTGW_SAT_INO/OTGW_SAT_HDR, green after): per-reason counts for each exclusion, the hour-close reasons, reset on manual change and on the tuner's own change, saturation; DecisionUnchanged: a mixed multi-day scenario gives byte-identical SATTUNEH/SATTUNED/SATTUNE log lines and final coefficient on the HEAD tuner and the new one. Plus a source check that REST and MQTT emit every reason name from satTuneReasonName().
8. Gates: full test file, run_tests.py, evaluate.py --quick; prerelease bump; esp32-combo build in a slot after TASK-1880; bench request (09: sat/status curve_tune present, MQTT topics seen on the broker; sim tune_hour shortens the hour so counters move within minutes).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: UI follow-up U9 from the VTherm UI review (docs/sat/vtherm-comparison-2026-11-13.md) added as an AC.

2026-11-13 implementation (wt-95w, on d34723882): SATcurveTune.h SATT_COUNT + SatTune.exclHours[]/last day record + SatTuneProgress; pure satTuneCloseHour() counts the reason of each hour that does not count, satTuneEmptyWindow() clears; glue satTuneRollHour() keeps the last day record, satCurveTuneGetProgress() copies; satControlLoop() -> state.sat.tuneProgress. REST sat/status object curve_tune (running, days, days_needed, clean_hours_today, clean_hours_needed, hours_today, last_decision, last_reason, excluded_hours{12}); MQTT sat/curve_tune/{running,days,clean_hours_today,last_decision,last_reason} + sat/curve_tune/excluded_hours/<reason> (12), on change + heartbeat, not retained, only with curve_self_tune on, no discovery. UI: curve card box #satTuneProgress (satRenderTune), 32 i18n keys in en + nl/de/el packs (glossary terms: zelf inregelen / Selbstoptimierung / αυτόματη ρύθμιση; warm tapwater; kamertemperatuur). Docs: MQTT.md section 'Heating Curve Self-Tune Progress', openapi.yaml curve_tune schema + example.

Evidence so far: python tests/test_sat_curve_selftune.py -> Ran 95 tests OK (skipped=1: DecisionUnchanged is opt-in). Red: OTGW_SAT_INO/OTGW_SAT_HDR = d34723882 copies -> ExclusionCounters 8 tests FAIL/ERROR (no PROGRESS record). Decision path: OTGW_TUNE_BASE_REV=d34723882 python -m unittest test_sat_curve_selftune.DecisionUnchanged -> OK: a 10-day scenario (dhw/sun/flame_off/no_sun exclusions, an uncounted day, a manual change, a tuner change, phase-split days, few_ticks hour) gives identical RESULT line and identical SATTUNEH/SATTUNED/SATTUNE/NARR records on the base tuner and the new one. Web UI: node --test tests/webui/sat-tune-progress.test.mjs -> 8/8 pass; red on d34723882 v2.js/v2.html -> 0/8. i18n-lang + i18n-no-hardcoded 9/9 pass. evaluate.py --quick: 93 checks, 0 FAIL, 0 WARN, 100%.

Gates 2026-11-13: affected host tests (run_tests.py --changed selection, 94 files, run one by one after the full parallel run was reaped for low memory): 94/94 pass. Fix on the way: tests/test_sat_cycle_classifier.py extract_types() now prepends SATcurveTune.h when SATRuntimeSection uses SatTuneProgress (it also feeds test_sat_pwm_start_limit). Web UI: 72 files one by one, 71 pass; tests/webui/pic-flash-status.test.mjs fails 1/14 (ReferenceError applyPicTabGate) on the d34723882 v2.js too, so not from this change (reported to the coordinator). AC2 stays open: REST/MQTT on a device needs the bench (09).

Follow-up: the 32 nl/de/el strings were translated by hand against i18n/glossary.md (terms 5, 15, 20, 28, 45), not through the translate_webui skill with its two reviews; a sync review of the sat_tune_* keys is still open (e.g. Greek 'ώ' as the hour unit). pic-flash-status finding sent to the coordinator 2026-11-13.
<!-- SECTION:NOTES:END -->
