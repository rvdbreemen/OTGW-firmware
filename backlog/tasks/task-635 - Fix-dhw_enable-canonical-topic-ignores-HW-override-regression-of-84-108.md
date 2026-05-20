---
id: TASK-635
title: 'Fix: dhw_enable canonical topic ignores HW override (regression of #84/#108)'
status: To Do
assignee: []
created_date: '2026-05-20 06:46'
labels:
  - bug
  - mqtt
  - opentherm
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MsgID 0 print_status() used the legacy is_value_valid() instead of the ADR-066/ADR-069 boiler-side worldview gate is_value_valid_for_master_topic(). Gateway-substituted T-frames and gateway-faked A-frames overwrote the canonical dhw_enable / status_master / Statusflags state with pre-override values. HW=0 left dhw_enable=ON in MQTT/REST. Verbatim regression of historical issues #84 and #108.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 is_value_valid_for_master_topic() guards the master-side publish branch in print_status (OTGW-Core.ino around line 2113)
- [ ] #2 is_value_valid_for_master_topic() guards the slave-side publish branch in print_status (around line 2141)
- [ ] #3 is_value_valid_for_master_topic() guards the final Statusflags state write-back (around line 2146)
- [ ] #4 build.py --firmware exits 0 (CI proxy)
- [ ] #5 evaluate.py --quick shows no new failures
- [ ] #6 Prerelease bumped via bin/bump-prerelease.sh
<!-- AC:END -->
