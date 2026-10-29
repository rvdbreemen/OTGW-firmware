---
id: TASK-1357
title: Remove the dead SAT settings satautogains (fAutoGainsValue) and bAutoGains
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T19:11:00Z'
updated_date: '2026-10-29T14:20:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 492000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1354 (2026-10-24): no firmware code reads satautogains / settings.sat.fAutoGainsValue (only stored settingStuff.ino:387/1105, served restAPI.ino:4006-4007, published SATcontrol.ino:2746/3200 + MQTTstuff.ino:730, help settingsHelp.h:139 'the PID does not read it today'), and settings.sat.bAutoGains is read at SATpid.ino:122 but never set (default true, SATtypes.h:540), so the PID always computes gains from the heating curve. Robert approved the removal 2026-10-24 (contract change: REST/MQTT/HA). Remove end to end like TASK-1332/1341: field(s), settings file (old keys load and are ignored), REST GET/list/whitelist/POST, MQTT command + state topic, HA entity if any (retire via kRetiredSatEntities), v2.js wizard writes satautogains -> stop writing it, settingsHelp, docs/manuals/MQTT.md; keep the PID behaviour identical (gains from the curve).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No code, setting, REST/MQTT key, HA entity or UI field for satautogains/bAutoGains remains; an old settings file with the keys loads; PID gains unchanged (host test: same gains before/after); retired HA config cleared
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed satautogains (settings.sat.fAutoGainsValue), bAutoGains and the dead manual-gain branch with fKpManual/fKiManual/fKdManual end to end: SATtypes.h fields, SATpid.ino manual branch, settings file write/read-in, REST POST alias/settings list/knownSettings/settings.sat.auto_gains, SAT status JSON (auto_gains_value, auto_gains, kp/ki/kd_manual), MQTT command row and sat/auto_gains_value publish, debug line, help text, v2.js label/group/wizard gains state and write; docs (MQTT.md, openapi, manuals EN/NL, c4, firmware-sat-spec, sat-comparison, json-golden, gen_sat_props), CHANGELOG. There was no HA discovery entity; the retained value topic sat/auto_gains_value is emptied through a state-only kRetiredSatEntities row (review finding).

Evidence (2026-10-29): tests/test_sat_autogains_removed.py Ran 3 OK; on a HEAD export the names and no-manual-branch tests FAIL and the gains test (real _pidCalculateGains compiled from SATpid.ino vs the formula, 24 inputs) is OK on both, so gains are identical. test_sat_pid_cadence 27 OK (also compiles and passes against the HEAD SATpid.ino copy after the review fix), test_sat_room_loss 12 OK, verify_sat_low_load --only suites 162 tests 0 xfail, test/host/test_ha_discovery_json.py PASS (lists the retired sat/auto_gains_value topic), node --check v2.js OK, evaluate.py --quick 0 FAIL / 100%. Old settings file: updateSetting has no catch-all, so an unknown SATautogains key is ignored (read in code, not run). Review: 4 lenses + verifiers; 2 findings fixed (retained topic row; old-copy stub fields), 2 rejected with reasons in the workflow result (REST 400 for the removed key needs an unsupported app/fs mismatch; stale numbers in a historical comment).
Not verified: wizard in a browser on a device, live HA/broker clearing. Pre-existing red tests found on HEAD filed as TASK-1477.
<!-- SECTION:FINAL_SUMMARY:END -->
