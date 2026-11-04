---
id: TASK-1443
title: >-
  Host test test_ha_discovery_json.py does not compile: discovery slice uses
  NET_ETHERNET/Ethernet/IPAddress[] (TASK-1377.1) unknown to the host shim
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T05:37:00Z'
updated_date: '2026-10-29T06:16:00Z'
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
- [x] #1 test/host/test_ha_discovery_json.py compiles and passes on dev
- [x] #2 A deliberately broken discovery row makes it fail (red), restored makes it pass (green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 confirmed by the TASK-1282 reviewer: the same 8 MSVC errors on HEAD and on the working tree (gen_mqtt.inc:60 WiFi undeclared, :62 state.net / Ethernet / NET_ETHERNET undeclared, :64 IPAddress operator[] x4), from buildDiscoveryContext() configuration_url code in commit 6da168984 (TASK-1377.1). When fixed, add an empty-retained check for the TASK-1282 retired entity (sat_comfort_adjust_enable) next to the TASK-1332/1341 checks (~570-580).

2026-10-29 fix (test/host only, src untouched). Red before on b07e3194a: python test/host/test_ha_discovery_json.py -> exit 2, COMPILATION FAILED (FIX), 6 MSVC errors at gen_mqtt.inc:62 (NET_ETHERNET, Ethernet undeclared) and :64 (IPAddress has no operator[] x4). Shim fix in test_ha_discovery_json.cpp: IPAddress with 4 bytes and operator[], WiFi and Ethernet doubles with localIP() (RFC 5737 addresses 192.0.2.10 / 198.51.100.20), state.net.eMode = NET_WIFI; OTGWNetworkMode sliced from the revision's Networktypes.h into gen_types.inc (not copied by hand). After it compiled, 3 checks were still red (FIX gate, TASK-1204 topic pattern, M1) on the empty retained publishes of sat_comfort_adjust_enable: the harness kept RETIRED_SAT_TOPIC_TAILS by hand and TASK-1282 (e0b89d25f) added a kRetiredSatEntities row without updating it. Now derived from the RETIRED-SAT-ENTITY rows in MQTTHaDiscovery.cpp. Added the TASK-1282 empty-retained check next to TASK-1332/1341. Green: exit 0, RESULT: PASS, 12 checks yes, 0 NO. AC2 red/green: temporary mutation of ha_name_boiler_flow_water_temperature to contain an unescaped quote -> exit 1, RESULT: FAIL, invalid JSON on the MsgID 25 payloads (doAutoConfigureMsgid id=25, expandAndStreamSensorSources row=53); git checkout restored src (git diff src empty); rerun exit 0, PASS. Not fixed (pre-existing, out of scope): --old-rev 8795bacc0 fails to compile the OLD build on SAT_HA_SWITCH_COUNT (test_ha_discovery_json.cpp:209-211, added in 53db6f342), unrelated to this change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
test/host/test_ha_discovery_json.py compiles and passes again. Shim (test_ha_discovery_json.cpp): IPAddress with operator[], WiFi/Ethernet localIP() doubles, state.net; OTGWNetworkMode sliced from Networktypes.h. The retired-SAT topic list is now read from the firmware's RETIRED-SAT-ENTITY rows instead of a hand-kept tuple that missed TASK-1282. New TASK-1282 empty-retained check. Evidence: AC1 red before (exit 2, 6 MSVC errors at gen_mqtt.inc:62/64), green after (exit 0, PASS, 12/12). AC2: a row string with an unescaped quote -> exit 1 FAIL on the id 25 payloads; restored -> exit 0 PASS. src/ unchanged. Not verified / open: --old-rev 8795bacc0 does not compile its OLD build (SAT_HA_SWITCH_COUNT, from 53db6f342), pre-existing, worth its own task.
<!-- SECTION:FINAL_SUMMARY:END -->
