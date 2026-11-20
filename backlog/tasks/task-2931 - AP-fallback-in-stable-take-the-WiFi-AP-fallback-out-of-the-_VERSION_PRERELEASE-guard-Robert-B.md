---
id: TASK-2931
title: >-
  AP fallback in stable: take the WiFi AP fallback out of the
  _VERSION_PRERELEASE guard (Robert B)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T20:19:00Z'
updated_date: '2026-11-19T21:55:00Z'
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
- [x] #3 Log texts neutral (no 'BETA:' / 'BETA AP:'); otgw-docs told for manual 6.1.3 and ch09 (apfallback field)
- [x] #4 Production dry run of the build (version.h commented out, not committed) benched on .61 by otgw-bench-tester: WiFi outage -> AP after 2 attempts, no restart loop; plus a prerelease regression of the same commit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev: every AP-fallback site is unguarded (networkStuff.ino boot path, startAPFallback/stopAPFallback, WIFI_AP_FALLBACK/WIFI_AP_RETRY, wifiApFallbackHeapOk, AP timers, AP states, stopAPFallback on reconnect, banner line; Networktypes.h NET_AP_FALLBACK + bAPFallback/sAPSSID; OTGW-firmware.h isNetworkUp; OTGW-firmware.ino setup line, NTP and MQTT skips; OLED.ino drawPageConfig, keep-awake, forced page; restAPI.ino apfallback fields). Remaining _VERSION_PRERELEASE guards are bench/debug only (handleDebug heap hold + help, B key in restAPI, heapHoldTick, enableDebugForPrerelease, the prototype). The runtime decision moved into wifiNextStateAfterFailedAttempt(): < 2 attempts or wifinoportal -> keep retrying (TASK-1645, no restart); healthy heap -> AP fallback; starved heap -> skip line + restart after 10 (TASK-2923). Log texts neutral ('WiFi:' / 'WiFi AP:'). Tests: tests/test_ap_fallback_starved_heap.py now compiles the real decision twice (prerelease and stable): red on d6544e15e in the stable configuration (3 of 4: no AP, no skip, wifinoportal restarts), green 8/8. Behaviour change in stable reflected in test_wifi_deadlink (new scenario dead_no_dhcp_lowfree: the WIFI_FAILED + restart-cap path is now reached when the AP is skipped on a low heap, both builds; release build goes to AP like prerelease) 32/32, and test_wifi_skip_portal (an unmarked boot with saved credentials starts the AP fallback in both builds; WIFI_FAILED restart-site tests on a low heap) 38/38. Sweep of all 99 test files that read the touched sources: 99/99 (scripts/run_tests.py -j 3). evaluate.py --quick 89/0. Answers for docs (sent to otgw-docs): low heap -> restart after 10; wifinoportal -> retry every 30 s, no restart (also stable now); boot AP comes along; BETA texts renamed.

2026-11-19 build 401: the first compile failed on the device toolchain: "'WifiState_t' does not name a type" (OTGW-firmware.ino.cpp:1304), because the Arduino prototype generator places a prototype of every function above the enum; the host test compiles the block in isolation and could not see that. Fix: wifiNextStateAfterFailedAttempt() returns uint8_t, the call site casts to WifiState_t; folded into the still-local commit (new hash 47d08b117). Then: 2.0.0-beta.401+47d08b1 build.bat exit 0, 0 src warnings; production dry run 2.0.0+47d08b1 (version.h:10 commented out, not committed) exit 0, 0 src warnings, .ino.bin: '_VERSION_PRERELEASE' 0, bench strings 0, 'BETA' 0, 'WiFi: retries exhausted, entering AP fallback mode' 1, 'WiFi AP: fallback started' 1, 'FW %d.%d.%d' 1. Artifacts in RvdB/OTGW-worktree-leftovers/b401-production and b401-prerelease; bench requested for .61.

2026-11-19 bench (otgw-bench-tester, .61, evidence OTGW-field-data/bench-otgw32/20261119-task2931-401/): PRODUCTION 2.0.0+47d08b1: version without -beta, H/B absent and inactive, 4-min WiFi outage: softAP OTGW-21B4F8 visible t+100..199 s, no restart, back 7 s after the unblock. PRERELEASE 2.0.0-beta.401+47d08b1: same outage, no restart, telnet shows the AP fallback was active ('WiFi AP: 5-min timer — attempting WiFi reconnect' 22:44:30, 'WiFi reconnected from AP mode', 'fallback stopped, WiFi connected'; the laptop scans missed the softAP); 10-min regression green, settings diff none, .61 stays on 401. NOT verified: the OLED page and the literal log lines on the production image (no telnet capture of the production run quoted), and joining 192.168.4.1 (not done). Observation (existing behaviour, not a regression): the STA was back on the router at 22:42:34 but the softAP only stopped at the 5-min AP timer (22:44:30), so ~2 min softAP + STA side by side; follow-up task filed.

2026-11-19 'no AP in the prerelease run' resolved: code-side the AP path is identical in both configurations (47d08b117: no #if left in startWiFi/loopWifi/the decision function; only H/B, heapHoldTick, enableDebugForPrerelease and their prototype stay prerelease-only; tests/test_prerelease_macro_guarded.py). My guess that the STA was not disassociated in that run was WRONG: 09's router log shows the same access-list reject in both runs, wifi_outages 1 and wifi_drop_down_sec 243 on both images, heap far above the guard (prerelease free 47628, maxBlk 31732), and telnet shows the prerelease in the AP fallback. Cause: the laptop scan missed the softAP.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AP fallback in stable builds (Robert 'B', DM 1573067183825621095): every AP-fallback site left the _VERSION_PRERELEASE guard (boot fallback instead of the 240 s portal, runtime fallback after 2 failed attempts, AP states and timers, OLED page, REST apfallback fields, setup/banner lines); the decision is wifiNextStateAfterFailedAttempt() (returns uint8_t because the Arduino prototype generator places prototypes above the WifiState_t enum): < 2 attempts or wifinoportal -> keep retrying, healthy heap -> AP, starved heap -> skip + restart after 10 (TASK-2923). Log texts lose the BETA prefix; H/B bench keys stay prerelease-only. AC1/AC2: tests/test_ap_fallback_starved_heap.py compiles the real decision in prerelease and stable configurations: red on d6544e15e in stable (3 of 4), green 8/8; test_wifi_deadlink 32/32 and test_wifi_skip_portal 38/38 updated to the new stable behaviour; 99/99 test files that read the touched sources; evaluate 89/0; builds 2.0.0-beta.401+47d08b1 and production dry run 2.0.0+47d08b1 both exit 0 with 0 src warnings. AC3: texts neutral, otgw-docs got the behaviour and the exact lines (docs commit follows). AC4: bench on .61 by otgw-bench-tester: production image no -beta, H/B gone, 4-min outage -> softAP seen, no restart, back 7 s after unblock; prerelease same (AP confirmed from telnet), 10-min regression green. Not verified: OLED page and literal log lines on the production image, joining 192.168.4.1. Follow-up: softAP stays up next to a reconnected STA until the 5-min timer (TASK filed, 2.1.0).
<!-- SECTION:FINAL_SUMMARY:END -->
