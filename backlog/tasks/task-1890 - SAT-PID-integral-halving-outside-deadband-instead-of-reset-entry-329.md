---
id: TASK-1890
title: SAT PID integral halving outside deadband instead of reset (entry 329)
status: Done
assignee:
  - '@otgw-dev'
created_date: '2026-11-13T08:09:00Z'
updated_date: '2026-11-19T22:31:00Z'
labels:
  - sat
  - adr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 963000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570710410427109397) accepted Robert's proposal (https://discord.com/channels/812969634638725140/1105556725714649128/1570705537383800905, https://discord.com/channels/812969634638725140/1105556725714649128/1570707806607245357): outside the deadband, halve the integral instead of resetting it to 0. Entry 329. Today thermo-nova sets the integral to 0 when |error| > DEADBAND 0.1 (pid.py:185-189, const.py:16) and the firmware does the same (_pidUpdateIntegral, SATpid.ino:247-250); halving is a deliberate difference from thermo-nova. Interpretation to confirm: multiply by 0.5 on each update while outside the band (decay), not once per crossing; the question goes with the PID research report the coordinator posts. Q-VT1 dropped (overtaken by entry 329). ADR-197 / ADR-204 (PID and its reset triggers) affected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan with thermo-nova citations (pid.py:185-189, const.py:16) and SATpid.ino:247-250, plus Sergeant D's confirmation (decay per update vs once per crossing), before code
- [x] #2 ADR that supersedes the integral-reset rule of ADR-197 (and checks ADR-204)
- [x] #3 Host test on the real PID: red on reset-to-0, green on halving
- [x] #4 Bench test by the bench-tester
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Ground truth checked this session: thermo-nova 7beaaf5 resets the integral to 0 outside the deadband: pid.py:185-189 (_update_integral: if abs(error) > DEADBAND: _integral = 0.0), DEADBAND = 0.1 at const.py:16; the zones use the same PID class (area.py:50). Firmware the same: main PID _pidUpdateIntegral() SATpid.ino:247-256; zone PIDs SATcontrol.ino:3621-3628. Decided: entry 329 (Sergeant D: halve instead of reset) and entry 333 (Q-329a B, Robert, not objected by Sergeant D: halve ONCE when the error leaves the deadband, hold while outside, accumulate from the held value when back inside; a target change still resets fully, SATcontrol.ino:2124 -> satPidResetTerms() SATpid.ino:150-158). Open before code (entry 333 open points 2 and 4): does the rule also apply to the zone PIDs, and does Sergeant D confirm B? One question to him (2.1.0 scope). Implementation once confirmed: (1) SATpid.ino _pidUpdateIntegral(): a static _pid_wasInBand; on the transition inside->outside multiply _pid_integral by 0.5 once, while outside hold it, inside accumulate as today; the solar-gain freeze (TASK-1323) still skips the whole update; satPidResetTerms() resets _pid_wasInBand to inside so a target change starts clean; the halved value is what the 5-min save persists (no file format change). (2) zones the same if he says so (SATzoneState gets a wasInBand flag). (3) ADR superseding the integral-reset rule of ADR-197 (checks ADR-204). (4) Host test on the real _pidUpdateIntegral(): a sequence inside (accumulate) -> outside (one halving, then held over several updates) -> inside (accumulates from the half); red on reset-to-0, green on halving; plus the target-change reset and the solar freeze. Not on otgw-1.x.x (SAT is 2.x only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: after the I/D simulation (1570758325740507237) Sergeant D said 'The integral works pretty good. I am also happy about the derivative' (1570765446636900415), recorded as entry 332 (assessment, no change decided). It does not revoke entry 329; whether the halving is still wanted goes into the coordinator's PID report.

2026-11-13: Q-329a answered B by Robert (13:00:35 UTC, 1570785451613225121), recorded as entry 333 (maintainer answer, reading posted 1570786623585521665, no correction): halve the integral once when the error leaves the deadband, then hold it while outside; accumulate from the held value when back inside; target change keeps the full reset. Open: zone PIDs, exit right after a reset, the faster self-tune part (PID study).

2026-11-16: moved to milestone 2.1.0. Release scope per entry 386 (Robert 08:47:36 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1571808950330986547: 'Mark anything new to the next release'): the initial SAT release covers the decisions of entries 375-385; this item is outside that scope. Not built now (coordinator 2026-11-16).

2026-11-18 Q-329b queued for Sergeant D (docs/sat/open-questions.md; comms 92): halving for the main PID only, or for the zones too (entry 333 open point 2; thermo-nova pid.py:186-188 resets to 0, area.py:50 zones use the same PID). Until he answers: option B (main PID and zones) decided by directive (Robert, 15 Nov: decide and proceed, he may overrule), so the work can proceed.
AC1 status (coordinator 2026-11-18, under Robert's 15 Nov directive: do not wait for Sergeant D, take the recommended option, record it as decided by directive; he may overrule): option B of Q-329b is implemented: halve once on leaving the deadband and hold, for the main PID AND the zone PIDs (thermo-nova zones use the same PID class, area.py:50). Q-329b is queued/posted by 92 and stays open; AC1 reads 'decided by directive, Q-329b open'.

Implemented (lane-1890, uncommitted, not built): main PID _pidUpdateIntegral() (SATpid.ino) with _pid_inBand: on leaving the deadband the integral is halved once, held while outside, accumulates again inside; _pid_inBand set to inside at every reset/restore (satPidReset, satPidResetTerms, satPidRestoreState, the lost-reading reset in satPidCurveOnly). Zones: SATZoneState::bPidInBand, same rule in satZonePidAdvance() (SATcontrol.ino). ADR-204 reset triggers, the solar freeze, the clamp and the persisted file format are unchanged. ADR-259 (Proposed, adr-kit; related to ADR-197 and ADR-204 via adr relate; partly supersedes ADR-197 decision 3). Docs: firmware-sat-spec.md (3 lines), sat-comparison.md item 16. Host tests: test_sat_pid_semantics.py (thermo-nova oracle with the deviation as a parameter) 24/24 on the fix, 5 FAIL on the old SATpid.ino (incl. the new held/re-halve case); test_sat_pid_cadence.py new g3 zone case 32/32 on the fix, FAIL on the old SATcontrol.ino (zone I 0.0387 -> 0.0 at 360 s); also green: published_curve, zone_aggregation, pid_resets, pid_integral_cap (stub struct gets bPidInBand); evaluate 88/0. Known, not from this change: adr-lint --strict reports a one-directional supersession ADR-204 -> ADR-197 on origin too. Not done: the SAT simulation model and the 'Totals per area' count in sat-comparison.md.

2026-11-18 ~19:20 scope and priority (Robert van den Breemen, relayed by the coordinator): 'Geef prioriteit aan taak 1890' (priority high); 'Voor release 2.0.0' in the coordinator session, confirmed by DM 'A' (1572675307033596055, reply to the coordinator's 1890 scope question). Milestone 2.1.0 -> 2.0.0; the earlier hold until release/2.x no longer applies. ADR-259 accepted via adr-kit (Agent: Claude) on 'Accepteer ADR-259' (coordinator session) and DM 'Ja' (1572637584843079733). Lane rebased onto origin/dev 13c0a4e41: fix commit rebumped to beta.375 (banner conflicts resolved to origin, SATtypes.h bPidInBand re-applied and checked hunk by hunk against 49bb78f9a). adr-lint --strict: ADR-259 PASS strictly; repo FAIL count 63 = origin/dev baseline 63 (no new FAIL). Accept rewrote the Status line; the ADR-080 guideline label and the amends text were restored. README: ADR-259 entry Accepted, ADR-197 entry names decision 3 amended by ADR-259 (ADR-197 body untouched).

Field-evidence signature (for 92/09 on every tester capture with SAT on, firmware >= the 1890 build): in telnet SATTICK lines (field i=, one PID step per 60 s, SATpid.ino satPidService) or MQTT sat/pid_i, when |room error| = |target - room| goes from <= 0.1 C to > 0.1 C, i drops to exactly half its previous value in that one step and then stays constant (unchanged i= on every following tick) while |error| > 0.1; once |error| <= 0.1 again, i resumes changing from the held value. Old code signature: i= 0.00 on every tick outside the band. Not a halving: an ADR-204 reset (target/preset change, window, SAT off->on, reset_integral) sets i=0. Bench (2026-11-18, agreed with coordinator): regression only on .63 (no boiler = no OT link = satDisable/PID reset, so the halving cannot be shown without sim, excluded by Robert's 15 Nov rule): (1) githash + settings unchanged, (2) 30 min no crash/restart, heap flat, (3) SAT on/off without boiler as on 374, (4) /sat_pid_state.json from 374 restores. Compile check: beta.375+a9bc77b built OK (4x 'Successfully created ESP32S3 image', 'Build completed successfully', 19:43); bench build will be the 376 rebump after ba's 375 lands.

2026-11-18 Q-329b answered by Sergeant D: 'My answer is B' (#dev-sat-mqtt msg 1572686857228124251, verified by the coordinator via REST) = halving for the main PID and the zone PIDs, as implemented. Recorded by 92 as entry 414; it replaces 'zones decided by Robert's directive' as the basis for the zone part. No code change. ADR-259 is already Accepted and therefore immutable (adr-kit): its Context still names the directive; the confirmation lives here and in entry 414. If the ADR text must name it, that is a follow-up amending ADR, not an edit.

Bench (otgw-bench-tester 09, 2026-11-18, .63 Classic+S3 PSRAM, no boiler) on 2.0.0-beta.377+412f888 (d3 stack 376 TASK-1890 + 377 TASK-1887/entries 410-417), REGRESSION ONLY, not feature evidence: (1) OTA app+fs 21:38-21:39, githash 412f888, 'settings: 152 keys, changed: none', bootcount 211->213. (2) 30 min SAT off: bootcount 213 throughout, 37/37 device/info 200, freeheap 37.3k flat after the first 9 min, hd_min 21,328 flat from 21:49, 0 crash/panic, 0 loop-stall. (3) SAT on without boiler: exactly one hand-back at +30 s (SATcontrol.ino:4377), no repeat in 4 min, 0 'Control mode:' lines in the whole capture, SAT off clean ('hand-back already sent'). (4) PID state restore: hand-made sat_pid_state.json i=1.2345 ts=now-60, SAT off, /ReBoot -> /api/v2/sat/status pid_i 1.235 (restored); the telnet line was not captured (client attached to the old boot). Backup file and settings restored, settings.ini diff empty. Evidence: 09 scratchpad b377\. The integral-halving behaviour itself is host-proven (AC3), not exercised on hardware (no boiler, no SAT scenario on bench boards).
2026-11-18 bench (09, 377+412f888 on .63, regression only, evidence 09 scratchpad b377\): 4/4 OK. Item 4: hand-made /sat_pid_state.json with I=1.2345, SAT off, reboot -> GET /api/v2/sat/status pid_i 1.235 (restored); the telnet 'SAT: PID state restored' line was not captured (client on the old boot). Side note, not reproduced: the first settings-restore POST right after the filesystem upload returned curl 000 once, the retry gave 200. Pushed 412f88843 (beta.376 + 377), zip post 1572723857679450212. Feature AC stays open for the field signature.

2026-11-19 follow-up (found by ba's full suite before the 08:00 beta): the simulation port tools/research/sat/sat_sim_port.py still set the integral to 0 outside the deadband, so tests/test_sat_sim_port_equivalence.py::test_pid_and_curve_match_the_firmware failed on origin b72c6d6c5 (PID output differs, max 0.402). Bisect by file: passes with SATpid.ino from 95bcfc78e^, fails with 95bcfc78e (this task). Not a firmware regression: the port did not follow ADR-259. Fix: the port halves once and holds (in_band flag, set by reset()), as _pidUpdateIntegral; a class switch Pid.HALVE_ON_EXIT (default True) keeps the old rule for reproducing studies whose committed results were made with it (tests/test_sat_kd_005_sim.py 1655 x09 rows). Evidence: port equivalence 4/4 incl. the mutant check; against the pre-1890 SATpid.ino it now fails (the port follows 1890); the 4 port-user files 18 passed. My miss: the 1890 sweep ran the SAT PID files, not the sim-port tests.

2026-11-19 full-suite follow-up: tests/test_sat_pwm_override.py PidIntegralFreeze failed to compile on origin b72c6d6c5 (_pid_inBand undeclared in the harness stubs) and then held the old expectation (integral 0 outside the deadband). Harness stub added; the two tests now expect the halved value 1.5 (renamed ...halves_outside...). 83 passed.

2026-11-19 ACs checked by otgw-dev against the notes and the sources: AC1: thermo-nova 7beaaf5 pid.py:185-189 (_update_integral: abs(error) > DEADBAND -> _integral = 0.0) and const.py:16 (DEADBAND = 0.1) opened this session in other-projects/SAT-releases-thermo-nova; Sergeant D's answer recorded as entry 414 ('My answer is B': main and zone PIDs) on top of Robert's entry 333. AC2: ADR-259 Accepted 2026-11-18 (partly supersedes ADR-197 decision 3, checks ADR-204). AC3: host tests in the notes: thermo-nova oracle 24/24 on the fix, 5 FAIL on the old SATpid.ino; zone case 32/32 on the fix, FAIL on the old SATcontrol.ino; commit 95bcfc78e (beta.376).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT halves the PID integral once when the room error leaves the 0.1 C deadband and holds it while outside, for the main PID and the zone PIDs; inside the band it accumulates again from the held value (entries 329/333/414, ADR-259 Accepted, partly superseding ADR-197 decision 3). Shipped in beta.376 (95bcfc78e). AC1: thermo-nova citations pid.py:185-189 / const.py:16 (reset to 0, the difference this task makes on purpose) and Sergeant D's 'B' (entry 414). AC2: ADR-259. AC3: host oracle 5 FAIL on the old code -> 24/24, zone case FAIL -> 32/32; follow-ups in the sim port and test_sat_pwm_override fixed on 2026-11-19. AC4: bench regression by the bench-tester on .63 (377+412f888, 4/4 OK, PID state restore pid_i 1.235). Not verified: the halving in a field run (signature in the notes for the field analyses).
<!-- SECTION:FINAL_SUMMARY:END -->
