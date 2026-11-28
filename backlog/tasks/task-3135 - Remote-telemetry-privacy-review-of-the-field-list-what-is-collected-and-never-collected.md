---
id: TASK-3135
title: >-
  Remote telemetry: privacy review of the field list (what is collected and
  never collected)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T19:01:00Z'
updated_date: '2026-11-28T20:20:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1186000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owners: otgw-poc-dev writes the field list, otgw-architect reviews it against the new ADR. Lane task TASK-3010 has the strict-privacy rules.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 One field list file: every recorded field or event, its source, its redaction; reviewed by otgw-architect with findings resolved
- [x] #2 Never collected: IP, MAC, SSID, BSSID, host names, user names, e-mail, passwords, tokens, API keys, MQTT credentials, location beyond what the ADR allows; a host test greps a recorded sample for each and fails on a mutant that leaks one
- [ ] #3 The pseudonymous ID cannot be derived back to the MAC by a reader of the stored data
- [ ] #4 R1/R2 (coordinator 2026-11-28 21:20, owner otgw-poc-dev on the rebased telemetry lane): settings.mqtt.sHaprefix and settings.sat.sPresetSyncTopic are in remoteTelemetryScrubWords() and refreshed with the other setting words; tests/test_remote_telemetry_privacy_sample.py carries both values in the sample and the no-word-list mutant leaks both; R3 field-list row Debug text lists the full word set; R4 (an MQTT user of 1-2 characters is not masked) is named as a known limit in the privacy text
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-28 21:3x otgw-architect re-review (O1) of the field list after c98f2a94e and dfed46681, read in a detached worktree of lane/telemetry-298 3d127095f (the code is not on origin/dev). F1 fixed: RemoteTelemetryDevice.h remoteTelemetryScrubWords() adds the MQTT broker, user, topic prefix (factory OTGW kept readable), unique id and NTP server to the scrub, refreshed at init and in remoteTelemetryLoop(); MQTT in/out and REST bodies go through diagPrepareMqtt/diagPrepareBody (localRecording.h). F2 fixed: EV_FW sends diagFwClass() = the extension only. F3 decided: EV_S0 and EV_DATE never written (c98f2a94e). F5: tests/test_remote_telemetry_privacy_sample.py exists. F4 accepted as known (field list open items).

Residual findings of the re-review (none blocks the beta in my view; the coordinator decides): R1 should: settings.mqtt.sHaprefix (MQTTstuff.h:60, user-editable, default homeassistant) is not in the word list, my F1 proposal named it. R2 should: settings.sat.sPresetSyncTopic is printed in a debug line (SATcontrol.ino:2307 on the lane) and is not in the word list; a user-chosen topic. R3 nit: field-list.md row Debug text (line 17) still lists the old scrub set; add the five new words. R4 nit: diagScrubWord leaves a word shorter than 3 characters alone, so a 1-2 character MQTT user is not masked; name it as known. Webhook URLs are not logged with their value (webhook.ino:37-228 print fixed strings only), so no finding there.

O2 evidence (this session, otgw-architect, one process, CPU 30 % and 7.2 GB free before the run): GREEN on lane/telemetry-298 3d127095f: python tests/test_remote_telemetry_privacy_sample.py -v -> Ran 2 tests in 48.8s OK (test_the_real_pipeline_leaves_no_hit: 0 hits with at least 5 control values seen; test_each_mutant_leaks_exactly_what_it_breaks: 8 mutants of localRecording.h, each leaks exactly its set). RED on the old code: the same test with localRecording.h from dfed46681^ -> FAILED (failures=9): the REAL build and the mutant builds do not compile (diagScrubAll, diagPrepareMqtt, diagFwClass do not exist there) and 3 mutant patterns do not match. That red is structural, not a leak shown on the old pipeline; the leak-level red is the mutants: no-word-list leaks ssid, ap-pass, host, mqtt-user, broker, ntp, topic-prefix (the F1 regression) and no-fw-class leaks fw-name (the F2 regression). Gap: the test calls the shared pipeline functions, not the platform-bound remoteTelemetryEv* functions; their wiring is checked by tests/test_remote_telemetry_privacy_wiring.py (not run by me) and on the bench (decoded download, ADR-302 Verification).

2026-11-28 21:20 coordinator decision: R1 and R2 are fixed before telemetry ships (AC4, otgw-poc-dev); R3 is a field-list doc fix; R4 is named as a known limit in the privacy text. AC1 stays open until R1/R2 are fixed. AC3 (ID derivation) is re-checked by otgw-architect in the next light slot.
<!-- SECTION:NOTES:END -->
