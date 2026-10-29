---
id: TASK-1443
title: >-
  Host test test_ha_discovery_json.py does not compile: discovery slice uses
  NET_ETHERNET/Ethernet/IPAddress[] (TASK-1377.1) unknown to the host shim
status: To Do
assignee: []
created_date: '2026-10-29T05:37:00Z'
updated_date: '2026-10-29T05:44:00Z'
labels:
  - test
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 603000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test/host/test_ha_discovery_json.py fails to compile on clean origin/dev acab024d7 (MSVC, reported by the 1.x session 2026-10-29): gen_mqtt.inc lines ~62/64, 'NET_ETHERNET' and 'Ethernet' undeclared, 'IPAddress' has no operator[]. Likely the configuration_url code from TASK-1377.1 in the discovery slice that the host shim does not stub. Fix the shim (or the slice extraction) so the test runs again; prove the test passes and catches a broken discovery JSON (red/green).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test/host/test_ha_discovery_json.py compiles and passes on dev
- [ ] #2 A deliberately broken discovery row makes it fail (red), restored makes it pass (green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 confirmed by the TASK-1282 reviewer: the same 8 MSVC errors on HEAD and on the working tree (gen_mqtt.inc:60 WiFi undeclared, :62 state.net / Ethernet / NET_ETHERNET undeclared, :64 IPAddress operator[] x4), from buildDiscoveryContext() configuration_url code in commit 6da168984 (TASK-1377.1). When fixed, add an empty-retained check for the TASK-1282 retired entity (sat_comfort_adjust_enable) next to the TASK-1332/1341 checks (~570-580).
<!-- SECTION:NOTES:END -->
