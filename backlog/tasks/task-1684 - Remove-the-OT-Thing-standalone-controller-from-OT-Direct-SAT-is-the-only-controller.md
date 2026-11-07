---
id: TASK-1684
title: >-
  Remove the OT-Thing standalone controller from OT-Direct (SAT is the only
  controller)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T09:22:00Z'
updated_date: '2026-11-07T13:15:00Z'
labels:
  - otdirect
  - breaking
  - cleanup
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 805000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's decision 2026-11-07 (session, AskUserQuestion): remove, not keep as fallback. The OT-Thing PI room compensation + heating curve + fixed flow + CH hysteresis block (TASK-183, TASK-582, exposed by TASK-933; OTDirect.ino getFlowTemp/loopPiCtrl/loopCHHysteresis, settings otdchmode/otdflowtemp/otdflowmax/otdroomsetpoint/otdgradient/otdexponent/otdoffset/otdkp/otdki/otdkboost/otdhysteresis/otdroomcomp/otdhysteresisenable) only acts in OT-Direct master mode with SAT off. Found via tjfs's 0x2D00 report (TASK-1681): its fixed-flow default writes MsgID 1 = 45.0 C. Side finding: loopCHHysteresis yields to state.sat.bActive instead of satOwnsControlSetpoint() (same gap TASK-1150 closed for the curve). BREAKING: master mode without SAT gets no TSet unless HA/MQTT sends CS=.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The controller code, its settings, REST keys, UI fields and MQTT/HA entities are removed; old settings files load without error (unknown keys ignored)
- [ ] #2 Master mode without SAT behaviour is defined and documented (no TSet write unless CS= arrives), entry in docs/BREAKING_CHANGES.md and CHANGELOG
- [ ] #3 Host tests and evaluate --quick green, combo build, bench .61 in master mode: no MsgID 1 write without SAT/CS, SAT still controls with SAT on
- [ ] #4 App-slot size before/after recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN TASK-1684 (draft for Robert, 2026-11-07; analysis: 5 read-only mappers + critic, scratchpad/map1684.md)

A. Remove (firmware, under HAS_DIRECT_OT)
1. OTDirect.ino: PI state block (otPi*, otNextPiCtrl, OT_PI_INTERVAL_MS), getFlowTemp(), loopPiCtrl(), loopCHHysteresis(), the 60 s curve block and the 10 s hysteresis timer in loopOTDirect(), the otPi* reset lines in resetTransientState(). Prototype getFlowTemp in OTGW-firmware.h.
2. OTDirecttypes.h: 13 settings (iCHMode, fFlowTemp, fFlowMax, fRoomSetpoint, fGradient, fExponent, fOffset, bRoomCompEnabled, fKp, fKi, fKboost, bHysteresisEnable, fHysteresis) and state.otd.bCHSuspended.
3. settingStuff.ino: write + updateSetting lines for the 13 OTD* keys. Old settings.ini loads fine: unknown keys fall through updateSetting (settingStuff.ino:588-593, 1356-1370); they drop out on the next save.
4. restAPI.ino: GET/POST /api/v2/otdirect/settings fields, /api/v2/settings list + knownSettings[] (old POST then gets 400, explicit), /api/v2/debug dump lines, otdirect/status ch_suspended. handleDebug.ino telnet dump lines.
5. UI: v2.js SET_META 13 keys + ENUM_OPTS otdchmode + comment; settingsHelp.h 13 help texts (same commit: tests/webui/settings-help.test.mjs checks both ways); otdmode help text says Master needs SAT.
6. MQTT: one-time empty retained publish of otgw32/ch_suspended (retired-topic pattern), so brokers drop a stale value.
B. Keep (shared, SAT depends on it): floatToF88, setOverride/clearOverride, enqueueWriteCommand, updateWriteCache, W_ENTRY(1) 15 s refresh, otMasterStatusFlags/otChModeOff/otdEffectiveMasterFlags (SAT CH=), satOwnsControlSetpoint() and its 3 other gates (CS expiry, thermostat write in master, external CS refusal), handleMasterModeSlaveFrame, flame ratio diagnostics, webhook {chmode} (= slave CH-active bit, not the setting). SATcontrol.ino: only comments that name the curve change; CH=1 in satSendRelease stays.
C. Tests
1. New guard tests/test_otd_controller_removed.py (pattern of test_sat_autogains_removed.py), anchored names only (the literal "heating-curve" label, getFlowTemp, loopPiCtrl, loopCHHysteresis, OTDchmode..OTDhysteresis, ch_suspended); RED on HEAD, GREEN after.
2. test/host override_reply harness: drop the PI anchor, rewrite TASK-1184 U1/U3/U5 to assert otBoilerCache[27] directly, add probe case master mode + SAT off: old 1 enqueue of MsgID 1 = 0x2D00, new 0. Add a test_override_reply.py wrapper so run_tests.py runs it (today it does not: critic).
3. Full regression: python scripts/run_tests.py (all), SAT/OT-Direct suites (test_sat_command_delivery, test_sat_requested_vs_transmitted, test_otd_ch_override, test_sat_ot_loss, test_sat_restart_quiet, test_sat_disable_handback), webui tests, evaluate --quick.
4. flash_otgw.sh/.bat --migrate compare: tolerate a RETIRED_KEYS list + case in tests/test_flash_migrate_settings_compare.py.
D. SAT validation test (bench .61, OT-Direct, no boiler/thermostat)
1. Before (beta.120) and after: master mode SAT off 10 min: count MsgID 1 writes on the wire (port 25238 / OT log) and enqueues (telnet OTD debug). Expect old about 10 x 0x2D00, new 0.
2. Master mode SAT on (satsimulation off, loopback/replay for a boiler): SAT CS values are the only MsgID 1 writes, CS refresh 5 s, MsgID 0 bit0 follows SAT CH=, MM sent on change; compare with the before run per field.
3. SAT disable handback (CS=0, CH=1, MM=T) and re-enable; MQTT fallback takeover (bFallbackActive) still counts as owner.
4. Gateway mode: setback path unchanged (thermostat timeout is host-tested only; bench has no thermostat).
5. Settings: save before flash, old settings.ini with OTD keys loads, settings diff only the 13 keys.
E. Docs: BREAKING_CHANGES.md + CHANGELOG Removed entry; docs/api openapi.yaml + README (also fixes a wrong 0/1/2 mapping), manuals en/nl ch09 + ch07, C4 otdirect/settings/sat/component files, OT_THING_FEATURE_PARITY.md (intentionally removed), firmware-sat-spec.md:36/3637, RELEASE_NOTES_2.0.0 drafts. ADR: Proposed ADR via adr-kit 'SAT is the only temperature controller on OT-Direct' (narrows ADR-179's writer set, records the REST/settings contract change); offered to Robert, never self-accepted.
F. Sizes: app-slot bytes before/after from the build log.
G. Order: one commit (firmware+UI+tests+docs), bump, build, OTA .61, D1-D5, reviewer team, push, zip.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert's decisions on the plan (AskUserQuestion): (1) master mode with SAT off: CH-enable (MsgID 0 bit 0) = 0 while no CS override is active; SAT and external CS= enable it as usual; DHW untouched; host test required. (2) auto-detect to master stays as is. (3) the 4 ventilation booleans stay. (4) fail-safe setback and MQTT otgw32/room_temp + room_setpoint stay. My defaults stated to Robert: POST of a removed key -> 400; one-time clear of retained otgw32/ch_suspended; flash_otgw --migrate tolerates retired keys; Proposed ADR via adr-kit offered to Robert. Plan approved in that form; implementation next, after the TASK-1679 beta.120 A/B finishes on .61.

Correction to the previous note: Robert answered the four open decisions; an explicit go on the full plan is asked separately, not yet given.

2026-11-07 sub-agent implementation (uncommitted, unbuilt): controller removed from OTDirect.ino/OTDirecttypes.h/settingStuff/restAPI/handleDebug/v2.js/settingsHelp.h; master mode without a MsgID 1 override clears MsgID 0 bit 0 in buildStatusRequest(); retired topic row for otgw32/ch_suspended in kRetiredSatEntities; tests/test_otd_controller_removed.py (red on HEAD, 120 hits; green now); test/host/test_override_reply.py runs suites 1178/1177/1184/1684; suite 1684 old-vs-fix against HEAD: OLD 12 MsgID 1 enqueues and status HB 0x03, NEW 0 and 0x02; flash_otgw RETIRED_KEYS; goldens, docs, C4, CHANGELOG, BREAKING_CHANGES updated.

2026-11-07 review (opus correctness + sonnet rules/tests/docs): no blocker; SAT's own on/off unchanged (SAT off states send CS=10 + CH=0, SATcontrol.ino:7104-7122). Major 1: SAT hand-back (CS=0) while SAT stays enabled (no outside temp > 1 h, OT loss, restart) gave CH off in master mode with nobody to heat. Robert decided (AskUserQuestion): keep CH enable on while SAT is enabled; the rule becomes CH off in master mode only when no CS override AND satOwnsControlSetpoint() is false. Major 2: external CS= expires after 60 s (OT_CSC2_EXPIRY_MS) and then CH goes off in master mode: document that CS= must be repeated within 60 s. Minor fixes queued: 2.0.0 release-note drafts still list PI room compensation, em dashes in OT_THING_FEATURE_PARITY.md, retired-topic wording (every discovery pass), OTDirect.ino:64 comment, BREAKING auto-detect wording (a later thermostat MsgID 1 write enables CH; auto-detect persists master mode), float threshold > 42, thermostat-write comment (TSet=0 is ignored, CH stays on once enabled).

2026-11-07 review fixes applied (uncommitted, unbuilt): buildStatusRequest() clears CH enable only when IS_MASTER_MODE() && !otdCsOverrideActive() && !satOwnsControlSetpoint(); host case P8 (SAT owns, no CS override => 0x03) added, suite 1684 OLD fails P1,P2 only, FIX 8/8; docs state the 60 s CS= repeat, persisted auto-detect master mode, thermostat MsgID 1 > 0 enables CH, ch_suspended emptied every discovery pass; release drafts PI claims removed; float key threshold > 42 (43 keys); comments reworded. Tests green, evaluate --quick 85 pass 0 fail.
<!-- SECTION:NOTES:END -->
