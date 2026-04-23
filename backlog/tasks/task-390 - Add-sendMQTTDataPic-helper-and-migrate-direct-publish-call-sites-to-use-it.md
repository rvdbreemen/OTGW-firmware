---
id: TASK-390
title: Add sendMQTTDataPic() helper and migrate direct publish call-sites to use it
status: To Do
assignee: []
created_date: '2026-04-23 17:57'
labels:
  - refactor
  - mqtt
  - technical-debt
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:980-985'
  - 'src/OTGW-firmware/MQTTstuff.ino:1045-1050'
  - 'src/OTGW-firmware/OTGW-Core.ino:691'
  - 'src/OTGW-firmware/OTGW-Core.ino:3743'
  - 'src/OTGW-firmware/OTGW-Core.ino:3750'
  - 'src/OTGW-firmware/OTGW-Core.ino:3758'
  - 'src/OTGW-firmware/OTGW-Core.ino:707-749'
  - 'src/OTGW-firmware/OTGW-Core.ino:778-794'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-388 lands, kPicSubtreePrefix exists as a central constant and the discovery side uses it correctly. However, the publish-side call-sites in MQTTstuff.ino and OTGW-Core.ino still contain the literal string otgw-pic/ in each F() argument. This works and is currently correct, but spreads the subtree name across multiple locations. If the subtree ever needs renaming, 24 sites must be touched manually.

This task introduces a PROGMEM-correct helper sendMQTTDataPic(label, value) that composes the topic internally using kPicSubtreePrefix, and migrates the direct call-sites (11 locations) to use it. Indirect usage (13 switch-case literals in OTGW-Core.ino:707-749 that build mqttTopic for later use) is out of scope in this task - those require a larger dispatcher refactor.

Benefit: subtree name lives in exactly one place. Zero behavior change (topic paths remain byte-identical). This is pure technical-debt reduction; not required to fix the bug in TASK-388 but captures the future-proofing intent.

Blocked by: TASK-388 must be merged first (kPicSubtreePrefix is defined there).
Related: ADR-065 (TASK-389) documents the contract this helper operationalizes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 void sendMQTTDataPic(const __FlashStringHelper* label, const char* value) implemented in MQTTstuff.ino, PROGMEM-correct, using strlcpy_P/strlcat_P with char[128] stack buffer (conform ADR-004, no String class)
- [ ] #2 Function prototype added to OTGW-firmware.h forward-declarations section so OTGW-Core.ino can invoke it (Arduino auto-prototype does not lift cross-file)
- [ ] #3 Direct publish call-sites in MQTTstuff.ino migrated: lines 980, 981, 982, 983, 985, 1045, 1046, 1048, 1050 - 9 call-sites changed from F(otgw-pic/X) to sendMQTTDataPic(F(X))
- [ ] #4 Direct publish call-sites in OTGW-Core.ino migrated: line 691, 3743, 3750, 3758 - 4 call-sites changed
- [ ] #5 Indirect literals in OTGW-Core.ino:707-749 (switch-case mqttTopic assignment) and :778-794 (picSettings publish) are NOT touched in this task - documented as out of scope
- [ ] #6 grep -rn F.otgw-pic/ src/OTGW-firmware yields zero results except for the kPicSubtreePrefix definition itself and any intentionally-scoped-out literals
- [ ] #7 python build.py --firmware exits 0, binary size delta within +-200 bytes vs baseline
- [ ] #8 python evaluate.py --quick exits 0
- [ ] #9 OTA flash to test device: mosquitto_sub -v -t <pub>/otgw-pic/# shows all previously-present topics still publishing (boiler_connected, thermostat_connected, gateway_mode, otgw_connected, version, deviceid, firmwaretype, designer, picavailable)
<!-- AC:END -->
