---
id: TASK-1344
title: >-
  SAT: release the MM override when SAT stops (disable, trip, fallback end,
  reboot)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T10:10:00Z'
updated_date: '2026-10-25T21:40:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 467000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #1. satDisable() (SATcontrol.ino:2328) only sends CS=0; no SAT path sends MM release, so MM=0 from PWM stays on the PIC (gateway.asm:4752-4765, single source) and the thermostat gets the boiler back at max modulation 0 %. thermo-nova serial coordinator sets MM on every update (coordinator/serial/__init__.py:228-233). Not a behaviour choice for SAT control itself (it is the hand-back), but name it to Sergeant D in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every SAT stop path (disable, trip, MQTT-fallback end) releases MM (host test red on HEAD, green on fix; bench capture shows MM= release on PIC). Boot with SAT off is out of scope (Robert 2026-10-25)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24: fix in satDisable(): MM=T after CS=0/CH=1 (non-numeric MM= clears the override: PIC gateway.asm:4752 SetMaxModLevel -> ClrMaxModLevel when GetDecimalArg fails; pyotgw.py:542-543 'Clear the setting by specifying a non-numeric value'; OT-Direct MM= handler clearWriteOverride(14) for non-digit/non-'-'). Every stop path calls satDisable(): settings disable (settingStuff.ino:989), early bail (:4765), fallback end (:4710), both safety trips (:4875, :5209). Boot with SAT off dropped from scope: an ESP-only reboot with SAT enabled resumes SAT; a power cut also resets the PIC; a user-disabled SAT already released at disable time. Evidence: tests/test_otd_ch_override.py test_sat_disable_releases_mm OK on fix, FAIL on HEAD; SAT tests OK; evaluate 0 failed; build.bat SUCCESS. BLOCKED AC: PIC bench capture (no PIC board connected); OT-Direct has no PR report for MM, so no runtime capture there either.

2026-10-24: shipped in alpha.493+97b1e5b; asked gruijter (PIC Classic) to check MsgID 14 after switching SAT off in PWM.

2026-10-25 gruijter capture alpha.504 (continuous mode, Intergas, PIC 6.8; analysed + verified by a second reader): user switch-off at 13:00:01 sends CS=0, CH=1, MM=T and the PIC answers 'CS: 0.00', 'CH: 1', 'MM: -' (ClrMaxModLevel -> ValueCleared, gateway.asm 6.6 :4752/:4764/:4233; unit runs 6.8, single source); TSet from the thermostat passes unchanged from 13:00:04. The Intergas rejects MsgID 14 writes (B700E0000), so the MM release cannot be seen at the boiler. Proven: the disable command path. Not proven: PWM MM=0 release on the bus, other stop paths. Better test: while the PIC rewrites the thermostat's 0 % to 100 % (seen from 13:03:17), switch SAT off and check the R..0E.. rewrite disappears.

2026-10-25 Robert approved the AC amendment: 'boot with SAT off' dropped from scope (an ESP-only reboot with SAT enabled resumes SAT, a power cut resets the PIC, a user switch-off already released MM; a boot-time MM=T could clobber a user's own MM=). Audit evidence this session: tests/test_otd_ch_override.py 8/8 OK on HEAD incl. test_sat_disable_releases_mm; against 97b1e5b70^ SATcontrol.ino FAILED (failures=1). satDisable() (SATcontrol.ino:2391-2445) is the single path for settings disable (settingStuff.ino:989), early bail (:4917), MQTT-fallback lift (:4862), safety trip (:5369-5371) and OT-loss hand-back (:2506). gruijter's PIC capture shows the MM= release on the disable path.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satDisable() now queues MM=T after CS=0/CH=1, so every SAT stop path (disable, trip, MQTT-fallback end, OT-loss hand-back) releases the modulation override (commit 97b1e5b70). Evidence: tests/test_otd_ch_override.py OK on HEAD, FAILED on 97b1e5b70^ (source-pattern test); all stop paths route through satDisable() (call sites listed in notes); gruijter's PIC capture shows MM= released on disable. Not verified: separate captures of the trip and fallback-end paths (they share satDisable()); the MM=0 bus write is rejected by the Intergas (MsgID 14). Boot with SAT off removed from scope by Robert 2026-10-25.
<!-- SECTION:FINAL_SUMMARY:END -->
