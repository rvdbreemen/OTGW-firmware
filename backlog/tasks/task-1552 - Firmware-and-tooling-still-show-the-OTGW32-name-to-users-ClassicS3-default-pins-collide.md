---
id: TASK-1552
title: >-
  Firmware and tooling still show the OTGW32 name to users; Classic+S3 default
  pins collide
status: To Do
assignee: []
created_date: '2026-11-01T19:34:00Z'
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
