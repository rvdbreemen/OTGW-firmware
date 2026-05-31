---
id: TASK-788
title: >-
  Fix MQTT publish regression: drop forced 0-to-60 migration and redundant
  on-change flag
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 17:59'
updated_date: '2026-05-31 18:53'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Commit 15e7a207 (TASK-787) added a bOnChangePublishing flag and a settings-load migration that silently rewrites existing MQTTinterval=0 to 60. This throttles previously real-time MQTT publishing to once per minute on all existing devices, perceived as the firmware breaking. Revert to interval-only semantics (iInterval==0 = legacy always-publish, >0 = on-change+heartbeat) while keeping the new fresh-install default of 60s. Existing devices keep their stored value. The bOnChangePublishing bool is fully redundant with iInterval and is removed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Existing devices with stored MQTTinterval=0 keep real-time publishing after upgrade (no silent migration to 60)
- [x] #2 Fresh installs default to MQTTinterval=60
- [x] #3 bOnChangePublishing field and all references removed from firmware (struct, OTGW-Core gate, settingStuff load/update, restAPI knownSettings + debug dump)
- [x] #4 writeSettings() is no longer called from within readSettings()
- [x] #5 Accepted ADR-006 and ADR-052 content restored (improper edit undone); 60s default recorded in CHANGELOG/README only
- [x] #6 python build.py exits 0 (firmware + filesystem)
- [x] #7 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reverted the regression from commit 15e7a207 (TASK-787). Root cause: a settings-load migration silently rewrote existing MQTTinterval=0 to 60 on every device, throttling real-time MQTT publishing to 1/min, perceived as the firmware breaking in Home Assistant.

Changes:
- Removed bOnChangePublishing flag (fully redundant with iInterval) from struct, OTGW-Core gate, settingStuff load/update, restAPI knownSettings + debug dump.
- Removed the 0-to-60 migration block; this also removed the writeSettings() call that ran inside readSettings(), restoring the documented load-must-not-rewrite invariant.
- Reverted to interval-only semantics: 0 = legacy publish-every-message, >0 = on-change + heartbeat. Fresh-install default kept at 60s (MQTT_DEFAULT_PUBLISH_INTERVAL_SEC); existing devices keep their stored value so upgrade is behaviour-neutral.
- Restored Accepted ADR-006 and ADR-052 to byte-identical pre-commit text (verified via git diff 15e7a207~1 = empty). 60s default recorded in CHANGELOG + README only. Removed the v1.6.1 BREAKING_CHANGES section (no breaking change remains). Reverted MQTT.md table, api/README.md known-settings + debug example, openapi.yaml debug schema.
- index.js synthetic publish-on-change checkbox left untouched: it drives mqttinterval 0/60 and matches the reverted backend semantics.

Validation:
- python build.py (no flags): exit 0, firmware (0.71 MB) + LittleFS (1.98 MB) artifacts produced.
- python evaluate.py --quick: 0 failures, 34/34 passed, 100%.
- Committed 44ca8fdc and pushed to origin/dev.

Note: fix addresses throttling-perceived-as-breakage. Any actual crash/reboot symptoms would be a separate issue; field-confirm on device after flashing.
<!-- SECTION:FINAL_SUMMARY:END -->
