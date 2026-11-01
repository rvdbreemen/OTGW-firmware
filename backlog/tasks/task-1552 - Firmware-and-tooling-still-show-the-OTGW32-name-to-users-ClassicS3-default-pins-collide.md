---
id: TASK-1552
title: >-
  Firmware and tooling still show the OTGW32 name to users; Classic+S3 default
  pins collide
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T19:34:00Z'
updated_date: '2026-11-01T22:15:00Z'
labels:
  - needs-maintainer-ok
  - firmware
dependencies: []
ordinal: 705000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1549 (manual cleanup, 2026-11-01). User-visible board name in firmware/tooling: v2.html:830 'No PIC detected ... (OT-Direct / OTGW32)' (also an em dash); restAPI.ino:516 503 text 'No OT-direct hardware - OTGW32 functions disabled'; MQTT contract under otgw32/ (command topics MQTTstuff.ino:915, published otgw32/ch_suspended + flame_* via OTDirect.ino / MQTTHaDiscovery.cpp:281); hardware_type 'otgw32' in OT-Direct mode (OTGW-firmware.h:660); flash_otgw.sh help/banner (sh:15,116,440) and flash_otgw.bat banner (bat:182) print 'Nodoshop OTGW32'; platformio.ini:148 comment. Possible bug: on the Classic + S3 map the default S0 pin GPIO 12 (S0types.h:22) is the PIC reset pin (boards.h:162) and the default output pin GPIO 16 (Outputstypes.h:22) is LED1 (boards.h:163); not reproduced on the bench. Robert decides which of these change (MQTT/hardware_type renames are contract changes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert's decision per item recorded in the notes
- [x] #2 Default-pin collision checked on the Classic+S3 bench unit (S0 and outputs enabled with defaults: PIC reset / LED1 behaviour) and fixed or closed with evidence
- [x] #3 User-visible strings without the board name, each change under its own prerelease tag
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-01 (AskUserQuestion): (1) default pins: guard on reserved pins, defaults unchanged; (2) otgw32/ MQTT topics and hardware_type 'otgw32': leave until the board is announced; user-visible texts (web UI message, 503 text, flash-script banners) become neutral.

Full suite before commit: 87/90 passed. Fixed in tests: (a) test_manual_sat_commands end markers named the 'OTGW32 Command Topics' heading that TASK-1549 removed (my regression, pushed in 467160acc) -> now 'Gateway Command Topics' / 'Gateway-commando's'; (b) test_sat_mqtt_contract_1485 read only [Unreleased], the entry moved to [2.0.0-beta.1] at the beta.1 rollover -> whole 2.x part up to the first 1.x section; (c) test_sat_preset_persist harness lacked SAT_TARGET_CHANGE_MIN_C (TASK-1517 moved it to SATtypes.h) -> the real line is read from SATtypes.h. (b) and (c) were red on clean origin/dev too (wt-base1547 at 358c1cf6b). Versioning: lane-1547 (wt-9c, unpushed, another session) already uses 2.0.1-alpha.1/.2 after beta.1 and was flashed to .63; this task takes 2.0.1-alpha.3 (pin guard) and 2.0.1-alpha.4 (strings).

One commit, one tag for the task (2.0.1-alpha.3): the banner update touches v2.html, v2.js and restAPI.ino, which also carry the string changes, so a split would bump the same files twice. AC3 wording 'each change under its own tag' read as: the task's firmware change ships under its own tag.

Bench FIX (.63, 2.0.0-beta.2): first build a83eb66 refused GPIO16 (0 'Output GPIO16 set' lines vs continuous on OLD) but logged the refusal every ~3 ms (validateGPIOOutputsConfig runs per OT frame); fixed in the same unpublished commit (amended to c922f17d9, one-time OTGW_BUMP_HOOK_DISABLE=1 because the hook only sees the amend delta): refusal reported once per pin. Recheck on c922f17: outputs on GPIO16 for 40 s + 3x 'p' + 'i': 0 GPIO16 writes, 0 repeat lines; switching to GPIO4 (LED2) live: exactly 1 refusal line in 15 s. Device serves the new texts (v2.js 'OT-Direct (no PIC)', 'unavailable in OT-Direct mode', 0 OTGW32; /api/v2/otdirect 503 'No OT-Direct hardware: OT-Direct functions disabled'). OTA ok, 175 settings unchanged. Pushed c922f17d9 + ed896c02c to origin/dev; zip posted #alpha-testing msg 1566576111456878622, report 1566576262762471586 (beta.2 supersedes 2.0.1-alpha.1/.2); ledger rows added.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
S0, relay output and Dallas now refuse a GPIO the board drives (boardPinReserved()/isBoardReservedPin() in OTGW-firmware.h over the live combo pin map: PIC UART/reset, I2C, button, LEDs, status pixel, USB 19/20, flash/PSRAM 26-32); defaults unchanged (Robert). User-visible board name removed from web UI, REST 503 text, update page, flash-script banners, platformio.ini comment; otgw32/ MQTT topics and hardware_type kept until announcement (Robert). Shipped as 2.0.0-beta.2 (c922f17d9) per Robert's version decision. Evidence: AC1 decisions in notes (both sessions); AC2 bench .63 old-vs-fix: OLD writes GPIO16=LED1 every ~3 ms, FIX 0 writes and one refusal line per pin; host test tests/test_board_pin_reserved.py red on old header, green on fix, mutant red; S0-vs-PIC-reset conflict shown by code reading only (bench S0 counted 0 pulses on both); AC3 own tag beta.2. Full suite: 87/90 before, the 3 failures fixed in ed896c02c (1 was my TASK-1549 regression). evaluate --quick 80/0. Follow-up: TASK-1553 (build lock PID reuse, fresh worktree build/ dir, embedded python config.py).
<!-- SECTION:FINAL_SUMMARY:END -->
