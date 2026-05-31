---
id: TASK-788
title: >-
  Fix MQTT publish regression: drop forced 0-to-60 migration and redundant
  on-change flag
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 17:59'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Commit 15e7a207 (TASK-787) added a bOnChangePublishing flag and a settings-load migration that silently rewrites existing MQTTinterval=0 to 60. This throttles previously real-time MQTT publishing to once per minute on all existing devices, perceived as the firmware breaking. Revert to interval-only semantics (iInterval==0 = legacy always-publish, >0 = on-change+heartbeat) while keeping the new fresh-install default of 60s. Existing devices keep their stored value. The bOnChangePublishing bool is fully redundant with iInterval and is removed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Existing devices with stored MQTTinterval=0 keep real-time publishing after upgrade (no silent migration to 60)
- [ ] #2 Fresh installs default to MQTTinterval=60
- [ ] #3 bOnChangePublishing field and all references removed from firmware (struct, OTGW-Core gate, settingStuff load/update, restAPI knownSettings + debug dump)
- [ ] #4 writeSettings() is no longer called from within readSettings()
- [ ] #5 Accepted ADR-006 and ADR-052 content restored (improper edit undone); 60s default recorded in CHANGELOG/README only
- [ ] #6 python build.py exits 0 (firmware + filesystem)
- [ ] #7 python evaluate.py --quick shows no new failures
<!-- AC:END -->
