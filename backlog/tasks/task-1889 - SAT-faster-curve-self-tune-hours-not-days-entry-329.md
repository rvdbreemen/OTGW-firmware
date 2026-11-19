---
id: TASK-1889
title: 'SAT faster curve self-tune: hours not days (entry 329)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T08:07:00Z'
updated_date: '2026-11-19T04:40:00Z'
labels:
  - sat
  - adr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 962000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570710410427109397) accepted Robert's proposal (https://discord.com/channels/812969634638725140/1105556725714649128/1570705537383800905, https://discord.com/channels/812969634638725140/1105556725714649128/1570707806607245357): keep the current control and self-tune structure but let the coefficient tune react in hours instead of days. Entry 329 in docs/sat/sergeant-d-corrections.md (replaces entry 325 / TASK-1886). Today: at most 0.1 per step, at least 3 counted days between steps (ADR-205, ADR-228, ADR-230, ADR-231). Interpretation to confirm: the ADR-228 two-phase signal stays, with a much shorter window and step interval. Open: window, step size, minimum time between steps, which exclusions stay; Q-1882a and the TASK-1883 tuner observation apply. The confirmation question goes with the PID research report the coordinator posts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan with thermo-nova citations (pid.py:185-189, const.py:16) and the ADR-228/ADR-230 timing, plus Sergeant D's confirmation of the interpretation, before code
- [x] #2 ADR that supersedes the ADR-205/ADR-228/ADR-230 timing
- [x] #3 Host test on the real tuner: red on the old timing, green on the new (tests/test_sat_tuner_convergence.py as base)
- [x] #4 Bench test by the bench-tester
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan TASK-1889 (ba, 2026-11-18). Scope 2.0.0 by Robert (DM 18:28 via the coordinator): "Die snellere self tune (1889) moet wel mee". Behaviour is Sergeant D's: the numbers go to him as Q-1889a (via 92); until he answers, the recommended option is taken by directive and recorded in docs/sat/open-questions.md as "decided by directive" (not a corrections entry). AC1 stays open until his answer.

1. Entries relied on (docs/sat/sergeant-d-corrections.md, read from origin/dev 2026-11-18 ~22:00):
- 329 (2026-11-13): a faster self-tune, "hours, not days", keep the current control. AGREES; this task implements the tune part.
- 333 and 414: halving the integral, main and zone PIDs. NOT in this task: TASK-1890 (otgw-dev) owns it. The AC1 citations pid.py:185-189 and const.py:16 belong to that part.
- 367 (Q-TUNE1 A): "Keep the exclusions; only a faster window and step." AGREES; no exclusion changes.
- 340/341/369/422 (ADR-234): a 60 min hour counts with >= 90 % of its expected ticks clean (108 of 120 at 30 s, any interval 10-300 s). AGREES; the hour itself does not change.
- 358 (Q-1882a A): hours with heating failure or no ignition stay out. AGREES, unchanged.
- 302-307, 313/315, 316-323 (ADR-228/230/231): two phases, both must agree; a phase without data does not veto; floating votes with >= 1 h of data; PWM tick rules. AGREE, unchanged; only the window they are summed over changes.
- 386 (new items to the next release): overridden for this item by Robert's scope decision above.

2. thermo-nova 7beaaf5 (opened this session in other-projects/SAT-releases-thermo-nova/custom_components/sat): no coefficient tuner. The coefficient is set once from the options (entry_data.py:213, default const.py:182 = 2.0), stored in the curve (heating_curve.py:15) and only read (heating_curve.py:39, pid.py:71); no other writer exists. Firmware-only behaviour, as entries 72, 329 and 358 say.

3. Today's timeline (firmware origin/dev, SATcontrol.ino satTuneAddTick ~6743, satTuneCloseHour ~6779, satTuneCloseDay ~6836; SATcurveTune.h):
 t+30 s each tick: first exclusion reason, else the signal goes to the warm-up or floating phase.
 every 3600 s tuner clock: the hour closes; counted with >= 108 clean ticks and no whole-hour marker.
 every 24 tuner hours: the day closes; counted with >= 6 counted hours (SAT_CT_MIN_CLEAN_HOURS); a counted day enters a 3-day window (SAT_CT_WINDOW_DAYS).
 with 3 counted days: each phase votes on its tick-weighted mean against +-1.0 C; both agree -> coefficient +-0.1 (SAT_CT_STEP), window emptied.
 => the fastest step is 72 tuner hours; in the field longer (gruijter 13 Nov: 0 of 17 hours counted).

4. Q-1889a for Sergeant D (two parameters split, per the advice: cadence = how often it may step, size = how much per step):
 A (recommended) rolling window of counted hours: decide at every counted hour once the window holds 12 counted hours; step 0.05; a change empties the window (>= 12 counted hours between steps). Moves 0.1 in >= 24 counted hours.
 B keep days but a "day" is 6 tuner hours with 2 counted hours, 3-block window, step 0.1. Moves 0.1 in >= 18 tuner hours.
 C rolling window of 24 counted hours, decide every counted hour, step 0.1. Moves 0.1 in >= 24 counted hours.
 Why A: same top speed as C, but half-size steps react in hours and correct a wrong step sooner; the two-phase agreement and the 1.0 C band filter noise over 12 hours. Risk: a 12-hour window sees one warm-up per day, so the warm-up phase can be thin; the floating phase still needs 1 h of data (ADR-230).

5. Work (after the answer, or with A under directive):
- ADR via adr-kit: supersedes the timing parts of ADR-205 (daily decision, 3-day window), ADR-228 and ADR-230 (window in days); keeps ADR-231/234. Consequence named in the ADR: the window file /sat_tune.json goes to version 3 (per-hour slots); a v2 file is dropped on restore, so every unit in the field starts an empty window once at the upgrade.
- Telemetry contract, explicit in the ADR: REST curve_tune.days/days_needed and MQTT sat/curve_tune/days become window_hours/window_hours_needed (old names removed, not reused with another unit); clean_hours_today and hours_today stay. The web UI card (v2.js ~7061-7080, key tools.sat_tune_days) and tests/webui/sat-tune-progress.test.mjs, openapi.yaml and the HA discovery (if it lists the topic) follow.
- Code: SATcurveTune.h constants and satTuneCloseHour/satTuneCloseDay -> an hour-slot window; SATTUNED log keeps its line but names hours.
- Host tests on the real block (tests/test_sat_curve_selftune.py, tests/test_sat_tuner_convergence.py): red on the old timing (no step within 24 counted hours of a clear signal), green on the new one; window file v2 dropped, v3 round trip.
- evaluate --quick, bump, build, bench by 09 (no SAT simulation on bench hardware: the step itself rests on the host test).

6. Not verifiable before the stable (~21 Nov), stated plainly: the new tuner needs >= 12 counted hours of a clear signal to step; a field house that gives counted hours could show one step in a day, but gruijter's house gives 0 counted hours under the kept exclusions. Field verification of a step before 21 Nov is unlikely. Robert and the coordinator own that trade-off.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 hard constraint from gruijter beta.218 capture (12 Nov 21:39 - 13 Nov 16:07; Sergeant D asked 1570830517580992553, answered 1570832004922998918): the coefficient stayed 1.80 (121 published heating_curve_coeff values, all 1.8) and the self-tune counted 0 of 17 evaluated hours (SATTUNEH counted=0 every hour): 7x min_mod (gas boiler outside PWM at or below minimum modulation, SATcontrol.ino:6031), 6x sun (sun elevation >= 12 deg, :6027, a position rule not measured sunshine), 4x not_sat (capture start and the first hour after each of the 3 restarts 04:00/08:38/11:59, SAT not active, :6024). Each restart also reset the hour count. Consequence: a faster self-tune (entry 329) gets no signal on this house unless the exclusions change; changing the exclusions is a Sergeant D question. Coordinator puts this into the Fable PID report as a hard constraint.

2026-11-16: moved to milestone 2.1.0. Release scope per entry 386 (Robert 08:47:36 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1571808950330986547: 'Mark anything new to the next release'): the initial SAT release covers the decisions of entries 375-385; this item is outside that scope. Not built now (coordinator 2026-11-16).

2026-11-18 picked up by otgw-webui (ba). Maintainer scope decision (Robert, DM 18:28 via the coordinator), verbatim: 'Die snellere self tune (1889) moet wel mee' -> milestone 2.0.0, priority high. This overrides entry 386 ('new items go to the next release') for this item; scope is the maintainer's call, behaviour stays Sergeant D's. Plan with timeline follows before code.

2026-11-18 implemented option A by directive (coordinator's nod; Q-1889a with Sergeant D via 92). ADR-260 Proposed (adr-kit). Code: SATcurveTune.h SAT_CT_HOURS_PER_DAY 24->1, SAT_CT_MIN_CLEAN_HOURS 6->1, SAT_CT_WINDOW_DAYS 3->12, SAT_CT_STEP 0.1->0.05, SAT_CT_FILE_VERSION 2->3 (#ifndef so the host test can compile the old timing); window file written only on a counted hour, SAT_CT_FILE_BUF 384->1024 (static, off the loop stack; FS-OTA keep list 1024, total 12172 <= 12288); REST curve_tune.window_hours/window_hours_needed replace days/days_needed/clean_hours_today/clean_hours_needed/hours_today; MQTT sat/curve_tune/window_hours replaces days and clean_hours_today (no HA discovery entity); UI card and settings help in hours (en/nl/de/el); openapi.yaml and MQTT.md. Evidence (this session, rebased onto 18bce9597, beta.379): tests/test_sat_curve_selftune.py + test_selftune_help_cadence.py + test_sat_tuner_convergence.py 120 passed 1 skipped; HourWindow 9/9 green, 9/9 red with OTGW_SAT_INO/OTGW_SAT_HDR at origin/dev; convergence on the new constants converges in all scenarios (lower 2.6->1.4: final 1.55, 21 steps, last day 10, tol 0.20; 13 decisions, 12 with a floating vote, 1 on warm-up alone), both mutants still fail; web UI sat-tune-progress 8/8 (5/3 on the origin v2.js), settings-help 29/29, i18n-followups 4/4, i18n-lang 6/6, v2-undefined-names 2/2; test_fs_update_keep 38 OK; test_adr_governance + open_questions 23 passed; evaluate --quick 0 FAIL 0 WARN. Not verified: a step on real hardware (no SAT simulation on bench units; gruijter gives 0 counted hours).

Bench (otgw-bench-tester 09, 2026-11-19, .63 Classic+S3 PSRAM, no boiler, no simulation) on 2.0.0-beta.379+d106f55 (src-identical to the pushed 0f9a906 per ba: only the committed generated settingsHelpGz.h differs). OTA app+fs 'changed: none'. (1) REST curve_tune: window_hours 0 / window_hours_needed 12; days, days_needed, clean_hours_today, clean_hours_needed, hours_today gone. (3) synthetic fixtures (removed afterwards): a v2 /sat_tune.json present before the OTA was dropped at the first restore (file 404, window_hours 0); a v3 file restored to window_hours 1 after a reboot and survived an FS-only OTA byte-identical (keep list). (4) tune card on the SAT page in en/nl/de/el: '1/12 counted hours in the window', 'No hour closed yet', 'No hours left out' (and the translations); settings-help.json satcurveselftune: 0.05 step after 12 counted hours. 'Last hour' needs a decision (not reachable). (5) 0 JS exceptions on Home/Monitor/Settings/Advanced/SAT in 4 languages; Monitor legend still has 'SAT target'. NOT reachable on this bench: (2) MQTT sat/curve_tune/window_hours, because satPublishMQTT runs from the SAT control tick and SAT is handed back without a boiler (no sat/curve_recommendation, sat/power or sat/state either in 220 s, while flag topics came); an actual 0.05 step (host tests). Screenshots: 09 scratchpad b379\ui\tune-card-*.png.

2026-11-19 beta.379 pushed: origin/dev ee51724f0..0f9a90699 (1816512ac code + 0f9a90699 docs). Zip 2.0.0-beta.379+0f9a906 posted to Discord, msg 1572790895839940630; archive OTGW-build-archive\2.0.0-beta.379+0f9a906.zip. Rebuild cause: the generated settingsHelpGz.h was not committed with the settingsHelp.h change; tests/test_progmem_assets_deflated.py red on 12792bc98 (2 failed), green on 0f9a90699 (4 passed).

Bench 09 on .63 (d106f55, src-identical to 0f9a906, differing only in the committed copy of the generated gz): REST window_hours/window_hours_needed PASS; v2 file dropped and v3 restore PASS (fixtures); tune card en/nl/de/el + help 12/0.05 PASS; 0 JS errors, keep list, SAT target line PASS. MQTT sat/curve_tune/window_hours NOT OBSERVED on the bench, confirm with SAT active: satcurveselftune=true, but SAT was handed back without a boiler, so the tick publisher satPublishMQTT() was idle (sat/# 220 s: 41 state/flag topics, no sat/curve_recommendation, sat/power, sat/state or curve_tune). 379 does not change any MQTT gate (diff: only the topic rename in the same bCurveSelfTune block). The curve-advice text 'at least 6 hours' is a separate rule (SATcontrol.ino:6134, >=6 hourly samples), not a leftover.

2026-11-19 Q-1889a answered A by Sergeant D (entry 423, 04:07:49 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1572825703299293265, unedited; recorded by 92, origin 25df1fdb6): the 12-counted-hour rolling window, decided every counted hour, step 0.05, window empties after a step = beta.379 as built, no code change. AC1 ticked on the plan (thermo-nova 7beaaf5 citations, ADR-228/230 timing; pid.py:185-189/const.py:16 belong to TASK-1890's integral halving, as the plan says) plus entry 423. ADR-260 updated (still Proposed; Q-1889a closed, entry 423 cited). Q-1889b (warm-up-only decisions) asked 04:24 UTC; an answer other than A becomes a new task.

2026-11-19 Q-1889b answered A (entry 424, origin aa6dd58df): warm-up-only decisions in the 12-hour window stay, no change. ADR-260 open questions both closed (still Proposed).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT heating curve self-tune decides every counted hour on a 12-counted-hour window with a 0.05 step (ADR-260 Proposed; acceptance is Robert's). Shipped in beta.379 (origin 0f9a90699). AC1: plan in this task (thermo-nova 7beaaf5 has no tuner: entry_data.py:213, const.py:182, heating_curve.py:15/39, pid.py:71; ADR-228/230 timing) + Sergeant D entry 423 (Q-1889a = A). AC2: ADR-260 supersedes the timing of ADR-205/228/230. AC3: tests/test_sat_curve_selftune.py HourWindow red 9/9 on the origin tuner, green 9/9; tuner tests 120 passed 1 skipped; tests/test_sat_tuner_convergence.py converges on the new constants in every scenario, both mutants fail; web UI sat-tune-progress 8/8 (5/3 on the old v2.js). AC4: bench 09 on .63 (REST, window file v2 drop/v3, card in 4 languages, 0 JS errors). Not verified: MQTT sat/curve_tune/window_hours on a unit with SAT active (bench had SAT handed back; field check); an actual 0.05 step in the field. Open: Q-1889b.
<!-- SECTION:FINAL_SUMMARY:END -->
