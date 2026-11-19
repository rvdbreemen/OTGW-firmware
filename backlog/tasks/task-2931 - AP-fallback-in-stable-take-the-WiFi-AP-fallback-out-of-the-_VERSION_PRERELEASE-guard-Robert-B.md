---
id: TASK-2931
title: >-
  AP fallback in stable: take the WiFi AP fallback out of the
  _VERSION_PRERELEASE guard (Robert B)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T20:19:00Z'
updated_date: '2026-11-19T21:11:00Z'
labels:
  - wifi
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1064000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, DM 1573067183825621095 (2026-11-19 20:07 UTC, relayed by the coordinator): 'B' = the AP fallback goes into the stable release. Today every AP-fallback site is under _VERSION_PRERELEASE, so a production build has no AP at all (boot: config portal; runtime: restart after 10 failed attempts). Scope: networkStuff.ino boot path (:426-441), startAPFallback/stopAPFallback block (:498-531), WIFI_AP_FALLBACK/WIFI_AP_RETRY states (:540-543), wifiApFallbackHeapOk (:652-661), AP timers (:967-970), runtime switch (:1002-1023), stopAPFallback on reconnect (:1037-1039), AP states (:1064-1090), banner line (:1178-1182); Networktypes.h NET_AP_FALLBACK + bAPFallback/sAPSSID; OTGW-firmware.h isNetworkUp; OTGW-firmware.ino :398, :541 (NTP), :568 (MQTT); OLED.ino drawPageConfig, keep-awake and forced page; restAPI.ino apfallback fields (:3332, :4081). Keep: the TASK-2923 heap guard, the TASK-1645 wifinoportal exception (no AP, keep retrying), and the bench keys H/B prerelease-only. Rename the 'BETA:' / 'BETA AP:' log texts to neutral ones.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every AP-fallback site above compiles and runs in prerelease AND production builds; the TASK-2923 heap guard and the wifinoportal exception stay; H/B bench keys stay prerelease-only
- [x] #2 Host test on the real networkStuff.ino decision compiled twice (with and without _VERSION_PRERELEASE): AP after 2 failed attempts on a healthy heap, skip + restart after 10 on a starved heap, wifinoportal keeps retrying; red on today's code in the production configuration
- [ ] #3 Log texts neutral (no 'BETA:' / 'BETA AP:'); otgw-docs told for manual 6.1.3 and ch09 (apfallback field)
- [ ] #4 Production dry run of the build (version.h commented out, not committed) benched on .61 by otgw-bench-tester: WiFi outage -> AP after 2 attempts, no restart loop; plus a prerelease regression of the same commit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev: every AP-fallback site is unguarded (networkStuff.ino boot path, startAPFallback/stopAPFallback, WIFI_AP_FALLBACK/WIFI_AP_RETRY, wifiApFallbackHeapOk, AP timers, AP states, stopAPFallback on reconnect, banner line; Networktypes.h NET_AP_FALLBACK + bAPFallback/sAPSSID; OTGW-firmware.h isNetworkUp; OTGW-firmware.ino setup line, NTP and MQTT skips; OLED.ino drawPageConfig, keep-awake, forced page; restAPI.ino apfallback fields). Remaining _VERSION_PRERELEASE guards are bench/debug only (handleDebug heap hold + help, B key in restAPI, heapHoldTick, enableDebugForPrerelease, the prototype). The runtime decision moved into wifiNextStateAfterFailedAttempt(): < 2 attempts or wifinoportal -> keep retrying (TASK-1645, no restart); healthy heap -> AP fallback; starved heap -> skip line + restart after 10 (TASK-2923). Log texts neutral ('WiFi:' / 'WiFi AP:'). Tests: tests/test_ap_fallback_starved_heap.py now compiles the real decision twice (prerelease and stable): red on d6544e15e in the stable configuration (3 of 4: no AP, no skip, wifinoportal restarts), green 8/8. Behaviour change in stable reflected in test_wifi_deadlink (new scenario dead_no_dhcp_lowfree: the WIFI_FAILED + restart-cap path is now reached when the AP is skipped on a low heap, both builds; release build goes to AP like prerelease) 32/32, and test_wifi_skip_portal (an unmarked boot with saved credentials starts the AP fallback in both builds; WIFI_FAILED restart-site tests on a low heap) 38/38. Sweep of all 99 test files that read the touched sources: 99/99 (scripts/run_tests.py -j 3). evaluate.py --quick 89/0. Answers for docs (sent to otgw-docs): low heap -> restart after 10; wifinoportal -> retry every 30 s, no restart (also stable now); boot AP comes along; BETA texts renamed.
<!-- SECTION:NOTES:END -->
