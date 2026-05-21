---
id: TASK-646
title: >-
  feat-2.0.0: port TASK-643 — Fix stale-pending in MQTT bit/byte slot
  confirmation (ADR-104)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 08:00'
updated_date: '2026-05-21 08:20'
labels:
  - mqtt
  - adr-104
  - beta-2.0.0
  - port-from-dev
dependencies: []
priority: high
ordinal: 46000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 sibling of dev's TASK-643. Relocate bit/byte pending-slot confirmation out of sendMQTTData() into the per-helper publish path. sendMQTTData() returns bool; helpers commit pending on true and clear on false. The confirmMQTTPublishSlot() (normal-msgId) is handled separately by the 2.0.0 sibling of TASK-644 (see ADR-104 Decision item 7).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sendMQTTData(const char*, const char*, const bool) returns bool
- [x] #2 sendMQTTData(const __FlashStringHelper*, const char*, const bool) returns bool
- [x] #3 sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool) returns bool
- [x] #4 All three sendMQTTData declarations in OTGW-firmware.h updated to return bool
- [x] #5 confirmMQTTPublishBitSlot/ByteSlot calls removed from both sendMQTTData success paths in MQTTstuff.ino
- [x] #6 publishStatusBitMQTT commits/discards mqttPendingBitSlot based on bool return from downstream send
- [x] #7 publishStatusVHBitMQTT applies the same commit-or-discard logic
- [x] #8 publishGatedBitMQTT applies the same commit-or-discard logic
- [x] #9 publishGatedByteMQTT (both F() and char* overloads) applies the same commit-or-discard logic
- [x] #10 publishMasterStatusState inline sendMQTTData(status_master) applies the same commit-or-discard logic
- [x] #11 publishSlaveStatusState inline sendMQTTData(status_slave) applies the same commit-or-discard logic
- [x] #12 publishMasterStatusVHState inline sendMQTTData(status_vh_master) applies the same commit-or-discard logic
- [x] #13 publishSlaveStatusVHState inline sendMQTTData(status_vh_slave) applies the same commit-or-discard logic
- [x] #14 publishMQTTOnOff returns bool to propagate sendMQTTData success
- [x] #15 Prerelease bumped via bin/bump-prerelease.sh
- [x] #16 python build.py --firmware exits 0
- [x] #17 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2.0.0 port complete.
- sendMQTTData() overloads return bool in both MQTTstuff.ino and OTGW-firmware.h header.
- publishMQTTOnOff() overloads return bool.
- confirmMQTTPublishBitSlot/ByteSlot removed from sendMQTTData success paths.
- 9 publish helpers updated to commit-or-discard pending based on bool return.
- Build (ESP8266 target) exit 0; ESP32 build deferred (container TLS limit).
- Evaluator: 60/0/0/1/7, Health 98.5%.
- Prerelease alpha.43 → alpha.44.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
2.0.0 port of TASK-643. Relocated MQTT bit/byte pending-slot confirmation ownership out of sendMQTTData() and into the per-helper publish path (ADR-104 Decision item 6).

Identical structural change as dev: sendMQTTData() returns bool; nine publish helpers (publishStatusBitMQTT, publishStatusVHBitMQTT, publishGatedBitMQTT, publishGatedByteMQTT ×2, plus the four inline sendMQTTData calls in publishMasterStatusState / publishSlaveStatusState / publishMasterStatusVHState / publishSlaveStatusVHState) now commit pending on true and clear on false. publishMQTTOnOff propagates the bool.

Verification:
- python build.py --firmware --target esp8266: exit 0
- ESP32 build deferred (container TLS sandbox blocks Espressif core)
- python evaluate.py --quick: 60 pass / 0 fail / 1 warn / 7 info / Health 98.5%

Note: this commit is unsigned (one-time --no-gpg-sign exception authorised by the maintainer; the container's commit-signing service does not recognise the mid-session-created 2.0.0 worktree path).
<!-- SECTION:FINAL_SUMMARY:END -->
