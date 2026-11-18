---
id: TASK-2025
title: >-
  No-PSRAM board: enabling BLE starves the heap within ~2 min (free 7-13 KB,
  maxblk 2-5 KB, minFree 520 B), web 503/000, MQTT drops, then the unit hangs
  (.61, beta.370)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T12:51:00Z'
updated_date: '2026-11-18T14:24:00Z'
labels:
  - ble
  - heap
  - bug
  - no-psram
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
- [ ] #1 Reproduction on .61 placed in time with the settings state quoted (satbleenable, satbleriskack, recording on/off, UI open), and whether beta.360 behaves the same
- [ ] #2 Root cause named with file:line and evidence (heap trace / allocation owner), not assumed from the 64 KB figure
- [ ] #3 Options with consequences and a recommendation put to Robert when they change ADR-222's decision; the chosen fix shows the collapse gone on the same reproduction (red/green)
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
<!-- SECTION:NOTES:END -->
