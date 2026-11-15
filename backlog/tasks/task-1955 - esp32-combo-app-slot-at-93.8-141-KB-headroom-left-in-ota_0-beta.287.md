---
id: TASK-1955
title: 'esp32-combo app slot at 93.8 %: 141 KB headroom left in ota_0 (beta.287)'
status: To Do
assignee: []
created_date: '2026-11-15T03:20:00Z'
updated_date: '2026-11-15T17:07:00Z'
labels:
  - build
  - flash-size
dependencies: []
priority: low
ordinal: 997000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measured on beta.287+9d1510b (build log, 2026-11-15): Flash 2152623 of 2293760 bytes (93.8 %), 141137 B free in the single app slot ota_0 (0x100000, 2240K, ADR-191 safeboot layout). TASK-1639 (GitHub #694) did the size breakdown at 93.5 % (beta.89) and Robert chose only the siscanf reduction then. OTA of 287 on .61 worked (09, changed: none). Safeboot updater writes ota_0 from the factory partition, so the limit is the ota_0 size, not twice the image. Growth since beta.89: +0.3 points. Purpose: keep the headroom visible per beta so a feature that needs more than the remaining ~141 KB is planned (reduction candidates of TASK-1639) instead of found at build time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A table of app size per beta from the build logs since beta.89 (at least 5 points) with the free bytes in ota_0
- [x] #2 A warning threshold proposed to Robert (e.g. build.py WARN above 95 %), with the consequence of each option
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Coordinator decision (2026-11-15): do options 1 (built-in replay fixture stored deflated, inflated with the ROM tinfl) and 2 (settings-help.json served gzipped), about 22 KB. Both break ADR-194's 'Must Not: gzip or otherwise compress-encode an asset, in firmware or on LittleFS', so ADR-245 (Proposed, amends ADR-194 for these two assets only) is written; the accept goes to Robert through the coordinator; implementation waits for it (an Accepted ADR binds). Trade-off named in ADR-245: a built-in replay then needs ~23 KB of heap while it runs (11.5 KB output + the tinfl decompressor struct; streaming would need a 32 KB dictionary). Options 3-5 parked with thresholds: (3) pack mqttHaSensors (5-8 KB, HA contract) when the headroom falls below 96 KB; (4) replace WiFiManager + Arduino WebServer by an async portal (30-40 KB, onboarding) when the headroom falls below 64 KB or a feature of more than 40 KB is planned; (5) shorten our debug strings (5-10 KB) only alongside other work in the same files.

ADR number: the amendment is ADR-247, not ADR-245 (245 is reserved for 92's 7-day graph ADR, 246 for ba's TASK-1931; renumbered before the push at the coordinator's request).

ADR-247 Accepted: Robert chose A (Discord DM 1571521326060146782, answer to question 1571520520153735261, relayed by the coordinator). Implementation of options 1+2 follows after the TASK-1966 rebuild (number 323).

Analysis (session 95, 2026-11-15, beta.320+5b9efcc, re-added after a rebase): image 2 157 711 B of 2 293 760 B (94.1 %, headroom 133 KB). Compiler already -Os (platform _bare.py) + -flto + function/data sections + --gc-sections. Sections: .flash.text 1 465 652, .flash.rodata 559 556, .iram0.text 100 047, .dram0.data 32 020. By origin (nm -S -l): WiFi/BT binary blobs ~470 KB (net80211 161, btdm 85, pp 61, phy 35), lwip 106, newlib 72, mbedtls 62 (WPA), wpa_supplicant 56, NimBLE 50, our app 233 KB (SATcontrol 43, restAPI 31, OTGW-firmware 30, OTGW-Core 26). rodata strings ~318 KB: our debug formats ~50 KB, IDF ESP_LOG ~40 KB (precompiled). Largest blobs, all in flash: SETTINGS_HELP_JSON 17.8 KB, UpdateServerIndex 16.1 KB (gzip already), OTGW_SIM_FIXTURE 11.6 KB, mqttHaSensors 11.3 KB. Growth beta.250 -> beta.320: +8.5 KB over 70 tags (~120 B per tag).
Harness seams: tests/test_replay_timing.py and test_sat_bench_scenarios_1412_20.py compile the real replay code, now with the inflate: fixture_source() gives the host a tinfl shim that returns the canonical log bytes (the inflate itself is proven by test_sim_fixture_builtin on the generated stream, the ROM call on the bench); both OK. Sweep (57 tests touching OTGW-Core/FSexplorer/webui_pages/help/fixture) green except test_sat_lowload_contract (needs ~500 s, run separately); node tests/webui/settings-help.test.mjs 29/29; host harnesses banner_board_mode, boiler_unsupported_origin, cmd_queue_drop, ha_discovery_json, ot_reserved_range green.

App size per beta (esp32-combo .ino.bin; published GitHub release assets, the last two local builds), ota_0 slot 2 293 760 B: | beta.1 2026-11-01: 2128432 B, 92.8 %, free 165328 B | beta.38 2026-11-03: 2138704 B, 93.2 %, free 155056 B | beta.77 2026-11-04: 2149456 B, 93.7 %, free 144304 B | beta.105 2026-11-06: 2151184 B, 93.8 %, free 142576 B | beta.118 2026-11-07: 2123984 B, 92.6 %, free 169776 B | beta.130 2026-11-07: 2117200 B, 92.3 %, free 176560 B | beta.174 2026-11-11: 2127856 B, 92.8 %, free 165904 B | beta.224 2026-11-13: 2139328 B, 93.3 %, free 154432 B | beta.250 2026-11-14: 2149200 B, 93.7 %, free 144560 B | beta.294 2026-11-15: 2155936 B, 94.0 %, free 137824 B | beta.320+b8dd97f (local) 2026-11-15: 2159120 B, 94.1 %, free 134640 B | beta.323+c9dd325 (local, TASK-1955) 2026-11-15: 2136816 B, 93.2 %, free 156944 B | Trend: beta.130 (11-07) to beta.320 (11-15) +41 920 B in 8 days, about 5 KB per day; the drop at beta.118/130 is an earlier size fix. After TASK-1955 (beta.323) 93.2 %, 156 944 B free.

Implementation (options 1+2, ADR-247) pushed as 2bd950c7c (beta.323): app 2 136 816 B vs 2 159 120 B = 22 304 B less. Bench 09 on .61 (beta.323+c9dd325, app sha256 1165305443dbe3bb...): settings-help.json Content-Encoding gzip, If-None-Match 304, 166 keys = settingsHelp.h, built-in replay OK, heap back to baseline (~55.5 KB) after the replay, an uploaded /otgw_simulation.log still wins. Zip posted, Discord 1571570226049450017. Host: test_progmem_assets_deflated 4/4 (red when settingsHelpGz.h is stale: regenerated after 1785ac494), test_sim_fixture_builtin, replay harnesses green; evaluate --quick 0 FAIL; lowload_contract fails only ll018h, identical on origin (fff3604f4 TASK-1954, d3).

AC2: threshold proposal (A WARN 95 % / FAIL 98 %, B WARN only, C nothing, each with its consequence) sent to the coordinator for Robert; the coordinator decided A under Robert's delegation (2026-11-15) and asked for its own task: TASK-1981 (implementation after 20:00 and after TASK-1980).
<!-- SECTION:NOTES:END -->
