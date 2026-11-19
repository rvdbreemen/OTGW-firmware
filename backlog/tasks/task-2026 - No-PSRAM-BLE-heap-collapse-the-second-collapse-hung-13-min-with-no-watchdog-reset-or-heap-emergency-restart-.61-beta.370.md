---
id: TASK-2026
title: >-
  No-PSRAM BLE heap starvation: the unit hung 13 min with no watchdog reset or
  heap-emergency restart (.61, beta.370)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T13:06:00Z'
updated_date: '2026-11-19T05:28:00Z'
labels:
  - heap
  - watchdog
  - bug
  - no-psram
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1040000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2025, from otgw-bench-tester on .61 (no PSRAM, beta.370, 2026-11-18). With BLE on, the heap was starved from ~13:41 (free 7-13 KB, max block 2-5 KB, minFree 520 B) and the unit hung completely from ~13:50 until a USB reset at 14:03:10. The reboot log shows NO reboot between the 13:38:51 OTA and the USB reset: neither the heap-emergency restart (ADR-030 ladder / ADR-167 CRITICAL) nor the task watchdog (TWDT) reset the unit in about 22 minutes of starvation and 13 minutes of a full hang. In the field such a unit needs a power cycle. Find why neither recovery path fired; TASK-2025 removes the BLE trigger but not this gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The hang is reproduced or explained from evidence (crash log, core dump, telnet trace up to the hang, emergency-recovery counters), not assumed
- [ ] #2 Named cause with file:line why neither the heap-emergency restart nor the TWDT reset the unit
- [ ] #3 Fix or recorded decision; the same reproduction shows a self-recovery (red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 otgw-dev: cause and fix (option A, Robert DM 1572611211378565161; ADR-258 accepted on his 'Ja', DM 1572613449282756649). Why nothing restarted the unit: the WiFi dead-link supervisor (networkStuff.ino) skipped every probe it could not run for lack of memory ('heap not healthy', 'no pcb', 'no echo buffer') as 'skipped', never as 'dead', so a starved heap never counted toward the restart; the heap-emergency ladder and the TWDT did not fire because the loop kept running. Fix: wifiProbeStarvedDead() (networkStuff.ino:816-830) counts such a skip as a dead probe when the check is armed and the largest free block is under WIFI_STARVED_MAXBLOCK 4096 (:649), used at :867, :890, :896; the restart reason stays 'WiFi: connected but no traffic'. Red/green: tests/test_wifi_deadlink.py::WifiDeadLinkStateMachine::test_a_starved_heap_counts_as_dead_and_recovers FAILS on origin networkStuff.ino (OTGW_NETWORK_INO), the file passes 35/35 with the heaphold test on the fix (heaphold in its own commit). evaluate --quick 88/0. adr-lint --strict: ADR-258 PASS strictly, repo FAIL count 63 unchanged. Not built, not benched yet (bench: 'H' heap-hold on .61, prerelease only).

2026-11-18 heaphold hook (separate commit, diagnostics, compiled only in prerelease builds: handleDebug.ino under #if defined(_VERSION_PRERELEASE)): telnet 'H' allocates and holds heap so the starved state of .61 can be reproduced on the bench without BLE, for the red/green bench of AC3 (old: hang, no restart; fix: 'WiFi: connected but no traffic' restart). tests/test_heaphold_hook.py + test_wifi_deadlink 35 passed.

2026-11-18 build 380+43298e2 (379 = fix 0a37ba2fb, 380 = 'H' hook 82703ecc3, on ba's 18bce9597): 4x 'Successfully created ESP32S3 image', app 2151424 B (93.8 %), RTC check OK. Evidence plan (coordinator decision): no device red, because beta.378 has no 'H' hook (the hook came in the commit after the fix). Red = tests/test_wifi_deadlink.py starved scenario FAILING on origin networkStuff.ino + the documented .61 hang on beta.370 (13:50-14:03, no restart, reboot log empty, TASK-2025/2026 description). Green = 'H' on 380 on .63 (09): reconnect line networkStuff.ino:804 after 8 dead probes, restart :811 after 8 more; with the witness caveat (MQTT/REST traffic = LAN works = correctly no restart; then 'does not reproduce', not a pass).

2026-11-19 interim .63 (09): 'H' at 23:54 (free 6764, maxblk 4084), no restart in 15 min; MQTT flapped on its own ~5-min reconnect cycle (online ~1.5-2 min), REST HTTP 000 throughout. Code reading (380, networkStuff.ino): each 8th dead probe resets the counter (:780) and then checks the witness wifiLanTrafficSeen() = MQTT connected at that instant (:704) or REST/25238 traffic within 60 s; witness -> no action (:784) and a new streak; a forced reconnect does not reset the streak. One decision point per 8 min, ~1/3 chance that it falls on an MQTT-online moment: one miss explains the 15 min. Designed behaviour of the witness rule (ADR-258 kept it), weak under a flapping MQTT. Coordinator decision (within ADR-258, no code change): A for now + a longer run (>= 3 decision points, branch lines grepped); push 379/380 if the restart fires when a decision point falls in an MQTT-offline window; otherwise file B (stricter MQTT witness, e.g. connected for the whole last 60 s) and put 2.0.0-or-2.1.0 to Robert.

2026-11-19 run 3 cause FOUND (09 on .63, 381 counters + device/info): starved_dead 14, witness 0, reconnect 0 = no dead streak ever reached 8; wifi_outages 7 / wifi_disconnect_events 8 (NOT_AUTHED) in ~30 min of hold. Every re-association resets the streak: wifiLinkProbeOnReconnect() sets wifiDeadProbes = 0 (networkStuff.ino:910-915), so with the AP dropping every ~4-5 min under the starved heap, 8 probes in a row (8 min) never happen. Red reproduction: tests/test_wifi_deadlink.py::test_starved_link_with_ap_drops_every_4_min_still_recovers (scenario starved_flap: starved + dead + an AP drop every 4 min for 40 min) -> 0 forced reconnects, no restart; marked expectedFailure until the chosen follow-up. Counterfactual: the same file with only that streak reset removed -> the scenario recovers (unexpected success) and the other 30 scenarios still pass. Separate known limit (coordinator's question): the probe is ICMP echo to the gateway via an lwIP raw pcb (platform_esp32.h:676-705): it proves link + lwIP, not a TCP/app round trip ('stack alive, app starved'). Options A/B/C with Robert (coordinator DM 2026-11-19); C = keep the streak across a reconnect while the heap is starved.

2026-11-19 option C prepared as a local commit (NOT built; waits for Robert's A/B/C answer): wifiLinkProbeOnReconnect() keeps wifiDeadProbes across a re-association while platformMaxFreeBlock() < WIFI_STARVED_MAXBLOCK (networkStuff.ino). ADR-261 Proposed (amends ADR-258; number from the coordinator, 260 is ba's), with the choice as its open question; README entry, index, ADR-258 related link (the invented status-history line adr relate wrote into the Accepted ADR-258 was removed). Red/green: test_starved_link_with_ap_drops_every_4_min_still_recovers FAILS on the pre-C networkStuff.ino (1 failed, 30 passed) and passes on C; test_wifi_deadlink + test_heaphold_hook 36 passed; evaluate --quick 89/0; adr-lint strict FAIL 63 unchanged (ADR-261 advisory: its open question).

2026-11-19 the supervisor counters (wifi_sv_* in device/info, branch lane-2026-ctr, b06eeff32, built once as 381+b06eeff for run 3) are PARKED: not part of the TASK-2026 push; re-apply on top of option C later if the bench needs them again. Option B (app-level progress, the 'stack alive, app starved' blind spot of the ICMP probe) is filed as TASK-2038 (2.1.0).

2026-11-19 Robert chose option C for 2.0.0 (DM 1572842729485111327, 05:15 UTC, reply to the coordinator's update 1572774626625589432, relayed by the coordinator); B goes to 2.1.0 as TASK-2038. ADR-261 records the choice and stays Proposed (acceptance is a separate yes). Lane rebased onto origin/dev e4eb2cdae and renumbered beta.383 (probe counts dead), beta.384 (H heap-hold hook), beta.385 (streak continues, option C); whole-lane non-banner patch identical to the pre-rebase lane (656 lines), 0 markers added. Gates on the rebased lane: tests/test_wifi_deadlink.py + tests/test_heaphold_hook.py 36 passed; red with origin's networkStuff.ino (OTGW_NETWORK_INO): 2 failed, test_a_starved_heap_counts_as_dead_and_recovers and test_starved_link_with_ap_drops_every_4_min_still_recovers; evaluate.py --quick 89 passed 0 failed; tests/test_adr_governance.py 17 passed (the ADR-261 README entry was lost in the rebase when README.md was taken from origin; restored from 5bf0ff471). Next: esp32-combo build in a slot after the 08:00 beta build, then a bench request.
<!-- SECTION:NOTES:END -->
