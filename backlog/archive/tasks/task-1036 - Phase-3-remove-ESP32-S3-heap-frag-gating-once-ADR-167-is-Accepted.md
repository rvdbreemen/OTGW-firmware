---
id: TASK-1036
title: 'Phase-3: remove ESP32-S3 heap-frag gating once ADR-167 is Accepted'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-09 21:17'
updated_date: '2026-10-05 22:55'
labels: []
dependencies: []
ordinal: 245000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-956 (heap-frag soak investigation, complete). The soak evidence (10h + 30-June, 0 tier escalations) is captured and ADR-167 (Retire the ESP8266-Era Heap Tier Machine and Per-Consumer Gating on the ESP32-S3-Only Dev Branch) is drafted as Proposed. This task is the actual Phase-3 removal, GATED on the maintainer accepting ADR-167. When accepted: remove dev's preventive drip/tier gating + delay(1) loop pacing; update the evaluate.py gates that REQUIRE the removed code (check_heap_fragmentation_promotion / check_per_consumer_heap_gate / check_heap_tier_entry_counters / check_heap_tier_thresholds_ordered under ADR-089/121) together with the ADR status flips; rebuild + re-soak clean to confirm no regression. Do NOT start until ADR-167 is Accepted. Recommend one clean re-soak (no concurrent hardware testing on the same unit) either before or after removal to settle the loop-gap sub-criterion TASK-956 flagged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-167 is Accepted (precondition — do not start otherwise)
- [x] #2 Preventive drip/tier gating + delay(1) pacing removed from dev
- [x] #3 evaluate.py gates check_heap_fragmentation_promotion/check_per_consumer_heap_gate/check_heap_tier_entry_counters/check_heap_tier_thresholds_ordered updated to match, ADR-089/121 status flipped, evaluator green
- [x] #4 Rebuilt (esp32-combo, current dev) and re-soaked >= 10 h on the OTGW32 with scripts/heap_soak_driver.py. The driver exits 0: no reboot, unreachable, sim_inactive, republish_failed or republish_not_drained anomaly. Its SUMMARY shows hd_enter_low/warning/critical_max = 0, hd_ws_drops_max = hd_mqtt_drops_max = 0, hd_min_max_block_min >= 8192 (report the floor), and hd_max_loop_gap_ms_max with no multi-second stall.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-167 Accepted 2026-07-31 on the maintainer's direct instruction (adr-kit quality A / 0.92). AC#1 satisfied.

Removal landed as follows. Thresholds in helperStuff.ino reverted to ADR-030's original ladder (3072/5120/8192); ADR-089's ESP8266-tuned 1536/3072/5120 is gone. HEAP_FRAG_PROMOTE_MAXBLOCK and the fragmentation-aware promotion branch in getHeapHealth() deleted. ADR-121's per-consumer split deleted entirely: getHeapHealthForWebSocket(), getHeapHealthForMQTT(), heapTierWithThresholds() and the WS_HEAP_*/MQTT_HEAP_* macro ladders, plus the now-unused WEBSOCKET_/MQTT_THROTTLE_MS_* constants and the lastWebSocketSendMs/lastMQTTPublishMs statics. canSendWebSocket() and canPublishMQTT() collapsed to a CRITICAL-only block; all ~20 call sites unchanged.

MAINTAINER DECISION during implementation: ADR-167 Decision item 1 listed the tier-entry counters for removal, but they are a published contract (otgw-firmware/stats/enter_{low,warning,critical}, three HA diagnostic entities at faux id 247, state.heap.entered_* over REST, telnet + web UI rows) and ADR-167 item 4 simultaneously requires preserving raw heap observability. Maintainer chose 'keep as pure telemetry': the counters keep counting, they are no longer a gate input, and check_heap_tier_entry_counters is retired anyway. No published topic or HA entity was broken.

evaluate.py: all four gates removed (check_heap_tier_thresholds_ordered, check_heap_fragmentation_promotion, check_heap_tier_entry_counters, check_per_consumer_heap_gate), ~11.7KB of gate code, replaced by a comment block explaining the ADR-080 reasoning. ADR-089 and ADR-121 flipped to Superseded by ADR-167 with status_history entries; both note they remain in force on otgw-1.x.x. docs/adr/README.md index rows and the heap-threshold table updated; CLAUDE.md's binding-ADR list now names ADR-167 instead of ADR-089.

Verification: python evaluate.py 90 checks / 0 failures / exit 0. python tests/test_evaluate.py 47 tests OK. build.bat --target esp32 SUCCESS for firmware AND filesystem, artifacts confirmed fresh by mtime (not trusting exit code alone).

INCIDENTAL FIX: the first build failed on 'src/OTGW-firmware/version.h:8:1: error: version control conflict marker in file', cascading into ~40 bogus AceTime 'acetime_t does not name a type' errors. Pre-existing, unrelated to this task: an unresolved stash-pop conflict left in the working tree, whose two sides were byte-identical apart from CRLF vs LF. Resolved by keeping the upstream side and stripping the markers. Logged as bug-150 in .wolf/buglog.json.

AC#4 (re-soak) is NOT done and is not self-verifiable: it needs a clean multi-hour soak on dedicated hardware with no concurrent testing on the same unit. Left unchecked deliberately.

2026-07-31 status: ACs 1-3 complete and pushed as 9c0a7e78a. AC#4 is the only thing outstanding and is BLOCKED on hardware: it needs a clean multi-hour soak on a dedicated unit with no concurrent testing on the same board (the TASK-956 10h run was already contaminated by parallel PIC-flash testing, which is what produced its 1027ms loop-gap blemish). Left In Progress rather than Done so the open field-validation is visible. UNBLOCKS WHEN: a bench unit is free for an uninterrupted soak window.

2026-09-30 soak driver ready for the AC#4 re-soak (scripts/, no prerelease bump).
scripts/heap_soak_driver.py: snapshots carry bootcount, lastreset, uptime and fwversion; a change in bootcount/lastreset/fwversion or an uptime regression is a 'reboot' anomaly (bootcount alone is not enough: updateRebootCount() returns N+1 even when the write fails). 'unreachable' = no complete snapshot for longer than --max-unreachable-sec (default 120). Telnet 's' toggles are replaced by idempotent POST /api/v2/simulate/start and /stop, and simulation.active is checked in every snapshot (sim_inactive anomaly). --republish-every-min N POSTs /api/v2/discovery/republish, logs each status and tracks disc_pending_ids back to 0 (republish_failed / republish_not_drained); disc_republish_triggered is not used because a REST republish never increments it. One SUMMARY line; exit 0 clean, 1 anomaly, 2 not started. JSON requests are spaced 0.3 s so a trailing REST-slot release cannot cause a self-inflicted 503 at cap 1 (it would inflate hd_rest_503). The docstring lists the preconditions: upload /otgw_simulation.log (not in the LittleFS image) and switch the nightly restart off.
Evidence: python -m py_compile OK; scripts/tests/test_heap_soak_driver.py 19 tests OK (102.8 s, under build load). Workflow contrast run: the OLD driver on a stub that reboots exits 0 with no anomaly and leaves the replay ON; the new driver exits 1 with ['reboot','sim_inactive'] and stops the replay.
Proposed sharper AC#4 wording (for the maintainer): 'Rebuilt (esp32-combo, current dev) and re-soaked >= 10 h on the OTGW32 with scripts/heap_soak_driver.py. The driver exits 0: no reboot, unreachable, sim_inactive, republish_failed or republish_not_drained anomaly. Its SUMMARY shows hd_enter_low/warning/critical_max = 0, hd_ws_drops_max = hd_mqtt_drops_max = 0, hd_min_max_block_min >= 8192 (report the floor), and hd_max_loop_gap_ms_max with no multi-second stall.'
Known test gap: the 'no POST when fewer than N minutes remain' guard has no deterministic test yet. OPEN: AC#4 needs the bench (overnight).

2026-10-01: the maintainer adopted the sharper AC#4 wording proposed in the 2026-07-31 notes (it replaces 'Rebuilt + re-soaked clean (no tier escalations) to confirm no regression'). Still needs the OTGW32 back on the network and an overnight run on a dedicated unit.

2026-10-04 12:54: AC#4 re-soak STARTED on the OTGW32 bench (192.168.88.61) with esp32-combo 2.0.0-alpha.412+8ad028c, which is current dev firmware (later dev commits are docs/backlog only). Command: scripts/heap_soak_driver.py --host 192.168.88.61 --duration-hours 10.25 --republish-every-min 60, detached (pid 9592). First snapshot: freeheap 79596, maxblock 34804, hd_min_max_block 31732, enter_low/warn/crit 0/0/0, drops ws/mqtt 0/0, sim on, bootcount 41. No other testing on this unit until the run ends (about 23:10). Output: %LOCALAPPDATA%/OTGW-capture/task1036/soak-alpha412-20261004-1254/ (driver.out, snapshots.jsonl).

Mid-soak 2026-10-04 ~17:00 (t+2h05): no anomaly, bootcount 41, enter_low/warn/crit 0, drops 0, hd_min_max_block 31732. Watch item for AC#4 ('no multi-second stall'): hd_max_loop_gap_ms rose 313 to 3215 ms at 13:44:01 (t+49 min, nothing else changed in that snapshot) and 3215 to 4181 ms at 15:59:36 (t+185 min, during the drain of the hourly discovery republish, disc_pending_ids 5 to 0). The firmware logs every loop gap over 200 ms as '[loop-stall] N ms gap', after the debug line that names the section. A passive telnet logger was attached at 16:57 (the driver allows a passive reader after its 'z'; it adds telnet output load on the device) to capture the next stall with context: telnet.txt in the soak folder. Host memory sat at 98% from 14:34 (llama-server processes, not ours); the MQTT broker runs in Docker/WSL on that host, so broker latency is one candidate for the stalls.

Soak t+~4h45 (about 19:10): still no anomaly (bootcount 41, crit 0, drops 0). hd_max_loop_gap_ms is now 5724; hd_min_max_block dropped to 14324 (floor 8192 still met). The telnet capture holds 126 '[loop-stall]' lines. The two largest (5724 ms at 17:55:13, 4361 ms at 18:55:34) start right after '[drip] OT ID 0 published OK': the loop task is silent for 4-6 s while async_tcp keeps serving REST. OT ID 0 is the heaviest discovery set (climate, SAT switches/select, full device block). The 15:59 jump (before the logger was attached) also fell in a republish drain, so the logger is not the cause. Hypothesis, NOT proven: after the drip queues ID 0's ~20 large configs, the MQTT client blocks the loop task while it writes that backlog to the broker. If AC#4's 'no multi-second stall' fails at the end, this becomes its own task.

AC#4 RESULT 2026-10-04 23:09: the soak FAILED AC#4. heap_soak_driver SUMMARY verdict=ANOMALY anomalies=republish_failed:1 (one POST /api/v2/discovery/republish timed out with no response; 8 of 9 answered 200 and drained, max drain 283 s), duration 10.25 h, snapshots ok 1717, failed 2 (device/info timeouts), longest gap 51.7 s, devinfo_503 0, 2470 load requests, bootcount 41 throughout (no reboot).
PASS: enter_low/warning/critical max 0/0/0, ws/mqtt drops 0/0, rest/webfile 503 0, drip_slowmode 0, sim never inactive.
FAIL:
- hd_min_max_block_min 7668 (floor 8192).
- hd_min_free_heap_min 532 B (since-boot native minimum).
- hd_max_loop_gap_ms_max 6125 (multi-second stalls).
The telnet capture (16:57-23:09) holds 100+ [loop-stall] lines; the largest begin right after '[drip] OT ID 0 published OK' (see the earlier notes).
Caveats: the passive telnet logger added debug-output load from 16:57, but the 3215 and 4181 ms stalls came before it. Host memory reached 98% (llama-server), and the MQTT broker runs in Docker/WSL on that host.
The follow-up investigation is filed as a separate task. Evidence: %LOCALAPPDATA%/OTGW-capture/task1036/soak-alpha412-20261004-1254/ (driver.out, driver.err, snapshots.jsonl, telnet.txt).

2026-10-05 14:38: AC#4 re-soak #2 STARTED on alpha.416+9f64919. It includes the TASK-1213 fix (ADR-186: non-blocking MQTT socket write) and TASK-1214 (vendored espMqttClient at upstream main, ADR-187). Command: heap_soak_driver.py --duration-hours 10.25 --republish-every-min 60, detached (pid 5008). First snapshot: freeheap 80124, maxblock 34804, hd_min_max_block 31732, tiers 0/0/0, drops 0/0, sim on. No other testing on this unit until it ends (about 00:55). Output: %LOCALAPPDATA%/OTGW-capture/task1036/soak-alpha416-20261005-1438/. The rig broker still runs in Docker/WSL on the laptop; with ADR-186 a slow broker should no longer stall the loop.

2026-10-06 00:55: AC#4 re-soak #2 PASSED on alpha.416+9f64919 (OTGW32, esp32-combo, current dev firmware; later dev commits are docs only). heap_soak_driver.py SUMMARY: verdict=CLEAN anomalies=none duration_h=10.25, snapshots_ok=1714 failed=3, load_requests=2513, bootcount 17 throughout (no reboot), sim_inactive_snapshots=0, hd_enter_low/warning/critical_max=0/0/0, hd_ws_drops_max=0, hd_mqtt_drops_max=0, hd_min_max_block_min=19444 (floor; >= 8192 required), hd_min_free_heap_min=16800, hd_max_loop_gap_ms_max=550 (no multi-second stall; previous soak on alpha.412: 6125 ms and 532 B), republish 9 sent / 9 drained, max drain 281 s. longest_gap_s=53.7 is the driver's own snapshot gap (3 failed polls), not a device loop stall: the device loop-gap watermark stayed 550 ms. The difference from re-soak #1 is the TASK-1213 fix (ADR-186, non-blocking MQTT socket write) plus TASK-1214 (ADR-187). Output: %LOCALAPPDATA%/OTGW-capture/task1036/soak-alpha416-20261005-1438/ (driver.out, snapshots.jsonl, final_device_info.json).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Phase-3 heap-frag gating removal (ADR-167) is done and soak-proven. The preventive drip/tier gating and delay(1) pacing are gone; canSendWebSocket()/canPublishMQTT() block on CRITICAL only; the evaluate.py gates and ADR-089/121 status were updated. Evidence for AC#4: a 10.25 h soak on alpha.416 (OTGW32 combo) ended CLEAN with no reboot, no tier entries, no WS or MQTT drops, a largest-free-block floor of 19444 B (>= 8192) and a max loop gap of 550 ms. The first re-soak (alpha.412) failed on a 6.1 s loop stall caused by blocking MQTT socket writes against a slow broker; TASK-1213 (ADR-186) fixed that, and TASK-1214 (ADR-187) moved the vendored espMqttClient to upstream.
<!-- SECTION:FINAL_SUMMARY:END -->
