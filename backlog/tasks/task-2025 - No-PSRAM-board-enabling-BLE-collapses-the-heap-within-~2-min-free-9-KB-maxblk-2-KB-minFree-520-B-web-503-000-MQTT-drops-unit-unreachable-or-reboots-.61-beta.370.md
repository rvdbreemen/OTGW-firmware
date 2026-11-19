---
id: TASK-2025
title: >-
  No-PSRAM board: enabling BLE starves the heap within ~2 min (free 7-13 KB,
  maxblk 2-5 KB, minFree 520 B), web 503/000, MQTT drops, then the unit hangs
  (.61, beta.370)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T12:51:00Z'
updated_date: '2026-11-19T22:38:00Z'
labels:
  - ble
  - heap
  - bug
  - no-psram
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-bench-tester on .61 (no PSRAM, beta.370) on 2026-11-18 while preparing the TASK-1470 AC2 read. SATbleriskack was already true (from an earlier session); one REST POST SATbleenable=true (accepted without a warning; the ADR-222 warning lives only in the web UI) started the scanner. From ~13:41 the heap was starved (free 7-13 KB, max free block 2-5 KB, minFree 520 B; telnet 13:42 'free 8992 frag 44% minFree 804 maxBlk 5108'), the web answered 503 then 000 with short REST windows, MQTT dropped 270 packets by 13:45, and from ~13:50 the unit hung completely, with no self-restart, until a USB reset at 14:03:10. The reboot log shows no reboot between the 13:38:51 OTA and that USB reset: no restart loop (an earlier report said so; corrected by 09). Code (origin/dev and v2.0.0-beta.360, identical): the gate was bleActive() = bBleEnable && (psramFound() || bBleRiskAck) (SATble.ino:624-626); REST and MQTT accept satbleenable and satbleriskack. Field risk in 360: every no-PSRAM user who accepts the warning (or any client that sets both keys). Workaround while the unit still answers now and then: switch BLE off over REST, then reset or power-cycle. The missing self-recovery is TASK-2026.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction on .61 placed in time with the settings state quoted (satbleenable, satbleriskack, recording on/off, UI open), and whether beta.360 behaves the same
- [ ] #2 Root cause named with file:line and evidence (heap trace / allocation owner), not assumed from the 64 KB figure
- [x] #3 Options with consequences and a recommendation put to Robert when they change ADR-222's decision; the chosen fix shows the collapse gone on the same reproduction (red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
09's facts (2026-11-18, .61 settings backup 13:40 before the POST): SATbleriskack was ALREADY true (settings-before.json and settings.ini), SATbleenable false. One POST SATbleenable=true started BLE (no warning in the REST answer). No diag recording ran (the start call hung, curl timeout 60 s) and the web UI tab was not open (headless runner failed before goto): BLE alone, passive scan, 4 sensors in range, ~400 ads per 30 s, plus light device/info polls and one telnet. Timeline: 13:39 POST -> ~13:41 first 503s -> 13:42 telnet 'free 8992 frag 44% minFree 804 maxBlk 5108' -> MQTT_drops 270 by 13:45 -> 13:46:48 unreachable -> 13:48:59 back with 'Software reset' and SATbleenable still true -> loop. The heap falls over ~2 min instead of at once, so beyond the ~64 KB static footprint something grows with the advertisements (root cause still open, AC2). Code reading: no second start path; bleActive() = bBleEnable && (psramFound() || bBleRiskAck) is the only gate (SATble.ino:624-626), identical in v2.0.0-beta.360.

09, later the same day (via coordinator): after the 13:48:59 software reset (it looked like the heap-emergency restart, ADR-030 ladder, not confirmed) the second collapse ended in a HANG of 13 min (13:50-14:03): no ping, no boot window, no watchdog reset, until a USB hard reset. Recovery was a REST POST satbleenable=false in the boot window plus a hard reset; the retained MQTT set ble_enable=false did not demonstrably help. So on a no-PSRAM board with BLE on the failure mode is also a hang without self-recovery. Why the emergency restart fired the first time and not the second, and why the TWDT did not catch the hang, is filed separately (see the follow-up task); the fix here removes the trigger.

Draft fix A (2026-11-18, wt-ba-2025, uncommitted, waiting for Robert's choice): bleActive() = bBleEnable && psramFound() (SATble.ino); bleEnableAllowed(psram, want) block in settingStuff.ino used by the SATbleenable setter, so the boot parse, REST and MQTT all refuse BLE on without PSRAM; a refused stored true is persisted as false once through the deferred flush; REST POST satbleenable=true without PSRAM answers 409 'BLE needs PSRAM: not available on this board'. Tests: tests/test_ble_nopsram_refuse.py 5/5 on the fix; on origin the gate case fails with 'no PSRAM, enable=1 ack=1 starts BLE' (the bug), all 4 code checks red; REST check red on origin restAPI.ino. All BLE + settings host tests 78/78. Not done yet: the web UI (the 'Enable anyway' path now ends in a 409 'save failed' toast; the warning dialog and paused hint should become a 'needs PSRAM' note), ADR superseding ADR-222, prerelease bump, build, bench.

Correction 2026-11-18 (09, reboot log): there was no restart loop. One boot from the 13:38:51 OTA, starved by BLE from ~13:41, hung from ~13:50, no self-restart until the USB reset at 14:03:10 (bootcount 1038 -> 1039 only then). 'Boot: Software reset' in the telnet banner was the last reset of that boot (the OTA). The POST false at 13:49:13 was accepted and flushed, which is why BLE was off after the USB reset. Fix A therefore does not break a loop; it makes the starvation impossible because the scanner never starts without PSRAM.

Maintainer decision (Robert, 2026-11-18 13:11:34, Discord DM 1572600155788546051), verbatim reply: 'A'. It answers the coordinator's question 1572596080632139908, which put my options A/B/C (notes above: A = BLE off on boards without PSRAM in the firmware, REST refuses, a stored true is cleared, the UI shows a note; B = keep Enable anyway plus a boot guard; C = cap BLE memory) with the recommendation A. The exact question text is in that DM; I quote only the reply verbatim.

Question 1572596080632139908, verbatim as the coordinator relayed it (Dutch, elisions as relayed): "Vraag TASK-2025 (HOOG, ook in publieke 360). Op een board zonder PSRAM vreet BLE de heap op (vrij 9 KB, grootste blok 2 KB) en komt de unit in een herstart-loop; via REST is BLE niet meer uit te zetten, alleen USB helpt. [...] A) Geen BLE op no-PSRAM: firmware weigert het (REST 400 met reden), een opgeslagen true wordt bij boot gewist, UI verbergt de schakelaar met een korte uitleg (Sergeant D stelde dit 7 nov voor). [...] Advies: A." The 'herstart-loop' wording was corrected later in DM 1572601402079973387 (no restart, a hang). The ADR is ADR-257 (ADR-256 is TASK-2018's, already with Robert). Implementation differs from the option text in two details, named in ADR-257 for his accept: REST 409 instead of 400, and the switch stays visible with the note instead of hidden.

Robert, 2026-11-18 14:05:06, DM 1572613627528085586, verbatim: 'Niet verbergen maar disablen. Geen PSRAM dan mag je de schakelaar niet aanraken.' (on the deviations note 1572607116852596767). Implemented in 375: bleLockSwitch() disables the SATbleenable switch without PSRAM (row greyed, the needs-PSRAM note as title), at row build and when the BLE poll first reports psram 0; the save refusal and the firmware 409 stay as defence. ble-nopsram-warning.test.mjs asserts the disabled switch (red on the first 375 build, 0f6055de3) and a usable switch with PSRAM. Test runs: 3/3 when the page starts in ~3 s; two of four repeats hit start-up timeouts (42 s and 70 s per case) while the machine ran at 70 % CPU from other sessions, one case each, not the same case; the assertions themselves never failed. ADR-257 (Proposed) updated with his words; accept ('Ja', DM 1572612342850658579) to run after the Discord post.

2026-11-18 bench PASS on .61 (OT-Direct, psram_found 0) by otgw-bench-tester on 2.0.0-beta.375+ff73985: stored SATbleenable=true cleared at boot and flushed to settings.ini within 10 s, no 'SAT BLE: initialized' over 4 min, runtime refuse line 'SATbleenable=true refused: BLE needs PSRAM on this board (TASK-2025)', REST POST 409 'BLE needs PSRAM: not available on this board', MQTT ble_enable refused, UI switch disabled (programmatic click no change) with the note, 0 JS exceptions, heap 47-51 k free, maxblock 31,732, MQTT_drops 0; settings restored byte-identical. Evidence: 09 scratchpad t2025. Pushed build: 375 rebased onto docs-only origin/dev (src diff ff73985cd..808291b6b empty), built 2.0.0-beta.375+808291b (sha256 ino e707ea99...3b64, littlefs e97938b8...21a4), archive OTGW-build-archive/beta375-808291b, posted Discord 1572704935647256636. ADR-257 Accepted (Robert, DM 1572612342850658579), ADR-222 superseded. Web UI suite: 60/84 files green, load stalls rerun green (poll-spread-hidden and otd-mode-selector stall the same on the 374 data); 24 files run after the push (coordinator's call). Bench ACs are ticked by 09 in its own lane after this push.

Bench ACs ticked by otgw-bench-tester (09), 2026-11-18. AC1: reproduction on .61 (OT-Direct, psram_found 0) on beta.370, settings quoted in the notes above (SATbleriskack true, SATbleenable false -> true by one REST POST, no diag recording, no web UI open), placed in time 13:38:51 OTA, starve from ~13:41, hang 13:50-14:03:10, reboot log bootcount 1038 -> 1039 only at the USB reset. AC3: red = that 370 reproduction; green = the same trigger (stored true and REST/MQTT true) on 375, PASS note above. Benched on ff73985; the pushed build 808291b differs only by the rebase: git diff ff73985cd 808291b6b -- src/ is empty (0 lines, checked by 09), 808291b6b is an ancestor of origin/dev 0a9696733, sha256 ino e707ea992ab68281...c3b64 and littlefs e97938b8f767c40f...fc21a4 match ba's report. AC2 (root cause with file:line and heap evidence) is not a bench AC and stays with the developer.

2026-11-19 triage (otgw-dev): AC2 (root cause with file:line) blocked: since beta.375 BLE cannot be switched on without PSRAM (ADR-222 guard), so the collapse cannot be reproduced on .61 any more without a debug path that bypasses the guard (prerelease-only) or a build with the guard removed. Needs an approach decision first; not a 2.0.0 blocker because the guard prevents the state.

2026-11-19 APPROACH PROPOSAL for AC2 (otgw-dev, design only, for Robert to choose; nothing built). Situation: since beta.375 the ADR-222 guard keeps BLE off on a board without PSRAM, so the collapse (free 9 KB, maxblk 2 KB, minFree 520 B within ~2 min on .61, beta.370) can no longer happen in the field and can no longer be reproduced without bypassing the guard. AC2 wants the allocation owner with file:line, not the 64 KB estimate. Options: (A) Prerelease-only bench override (a telnet key like H/B, compiled out of stable) that starts NimBLE on a no-PSRAM unit despite the guard, plus a heap trace: heap_caps_get_info(MALLOC_CAP_INTERNAL) before init, after the controller init, after the host init, after the first scan start, then every 10 s, and the largest-block owner via heap_caps_dump/heap tracing if the IDF build has it. Cost: ~1-2 h code + host test + 1 build + a 15-30 min bench by 09 on .61; risk: the unit hangs as on 370 (recovery by USB reset, settings kept), prerelease-only so no user risk. Gives the real file:line. (B) Static estimate from our sdkconfig (CONFIG_BT_NIMBLE_* buffers, controller memory, msys pools) and the IDF memory tables. Cost: ~1 h, no device risk; but it is an estimate, which AC2 explicitly does not accept, so it can only support A or C. (C) Close AC2 for 2.0.0 by waiver: the guard makes the state unreachable (bench-proven on 375), BLE without PSRAM is not a supported 2.0.0 configuration, and the root cause moves to a 2.1.0 task with option A (needed only if BLE without PSRAM should ever be supported). Cost: none now; risk: we never learn the owner if no one asks for no-PSRAM BLE. Recommendation: C for 2.0.0 (the guard removes the user risk; A spends a bench slot and a possible hang the weekend before the stable release) plus a 2.1.0 task for A. Decision: Robert.

2026-11-19 AC2 WAIVED for 2.0.0 by Robert: 'C' (DM 1573102947980542135, reply to the coordinator's TASK-2025 question, relayed by the coordinator): the ADR-222 guard makes the state unreachable (bench-proven on 375), BLE without PSRAM is not a supported 2.0.0 configuration. Root cause hunt moves to TASK-2934 (2.1.0, option A: prerelease bench key + heap trace per init phase). AC2 stays unchecked (waived, not evidenced).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
No-PSRAM boards can no longer enable BLE (ADR-222, beta.375): a stored 'on' is cleared at boot, REST/MQTT refuse with 'BLE needs PSRAM', the UI greys the switch out. AC1 + AC3 (bench-tester): reproduction on .61 beta.370 placed in time (hang 13:50-14:03:10) and the guard on 375 (no 'SAT BLE: initialized' in 4 min, refusals logged). AC2 (allocation owner with file:line) NOT evidenced: waived by Robert ('C', DM 1573102947980542135) because the guard makes the state unreachable; the hunt is TASK-2934 (2.1.0).
<!-- SECTION:FINAL_SUMMARY:END -->
