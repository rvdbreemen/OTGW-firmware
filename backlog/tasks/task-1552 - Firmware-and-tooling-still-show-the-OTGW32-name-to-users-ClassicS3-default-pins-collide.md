---
id: TASK-1552
title: >-
  Firmware and tooling still show the OTGW32 name to users; Classic+S3 default
  pins collide
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T19:34:00Z'
updated_date: '2026-11-01T20:18:00Z'
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
- [ ] #1 Robert's decision per item recorded in the notes
- [ ] #2 Default-pin collision checked on the Classic+S3 bench unit (S0 and outputs enabled with defaults: PIC reset / LED1 behaviour) and fixed or closed with evidence
- [ ] #3 User-visible strings without the board name, each change under its own prerelease tag
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-01 (AskUserQuestion): (1) default pins: guard on reserved pins, defaults unchanged; (2) otgw32/ MQTT topics and hardware_type 'otgw32': leave until the board is announced; user-visible texts (web UI message, 503 text, flash-script banners) become neutral.

Full suite before commit: 87/90 passed. Fixed in tests: (a) test_manual_sat_commands end markers named the 'OTGW32 Command Topics' heading that TASK-1549 removed (my regression, pushed in 467160acc) -> now 'Gateway Command Topics' / 'Gateway-commando's'; (b) test_sat_mqtt_contract_1485 read only [Unreleased], the entry moved to [2.0.0-beta.1] at the beta.1 rollover -> whole 2.x part up to the first 1.x section; (c) test_sat_preset_persist harness lacked SAT_TARGET_CHANGE_MIN_C (TASK-1517 moved it to SATtypes.h) -> the real line is read from SATtypes.h. (b) and (c) were red on clean origin/dev too (wt-base1547 at 358c1cf6b). Versioning: lane-1547 (wt-9c, unpushed, another session) already uses 2.0.1-alpha.1/.2 after beta.1 and was flashed to .63; this task takes 2.0.1-alpha.3 (pin guard) and 2.0.1-alpha.4 (strings).
<!-- SECTION:NOTES:END -->
