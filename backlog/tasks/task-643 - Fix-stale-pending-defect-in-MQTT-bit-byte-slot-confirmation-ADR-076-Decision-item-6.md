---
id: TASK-643
title: >-
  Fix stale-pending defect in MQTT bit/byte slot confirmation (ADR-076 Decision
  item 6)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 07:37'
updated_date: '2026-05-21 07:46'
labels:
  - mqtt
  - adr-076
  - beta
  - silent-failure
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement ADR-076 Decision item 6: a heap-throttled sendMQTTData() currently leaves mqttPendingBitSlot/ByteSlot.pending=true even though the publish never reached the broker; the next successful sendMQTTData() for any topic anywhere in the firmware then calls confirmMQTTPublishBitSlot/ByteSlot() and silently commits the stale pending into the per-slot tracked-time array. The slot is marked as published-at-time-T even though HA never received the value, suppressing the next 60 s heartbeat. Fix: move bit/byte pending-confirmation ownership out of sendMQTTData() and into the publish helpers (publishStatusBitMQTT, publishStatusVHBitMQTT, publishGatedBitMQTT, publishStatusByteMQTT, publishStatusVHByteMQTT, publishGatedByteMQTT). sendMQTTData() returns bool; helpers commit on true and discard pending on false. The confirmMQTTPublishSlot() call for normal msgIDs stays in sendMQTTData() — fixing that one is out of scope for ADR-076.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sendMQTTData(const char*, const char*, const bool) signature changes from void to bool — true on successful end-to-end publish, false on any guard or transport failure
- [x] #2 sendMQTTData(const __FlashStringHelper*, const char*, const bool) signature changes from void to bool — same semantics
- [x] #3 sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool) signature changes from void to bool — same semantics
- [x] #4 All three sendMQTTData declarations in OTGW-firmware.h are updated to return bool
- [x] #5 confirmMQTTPublishBitSlot() and confirmMQTTPublishByteSlot() calls are removed from both sendMQTTData() overloads in MQTTstuff.ino (lines 980-982 and 1024-1026 area)
- [x] #6 confirmMQTTPublishSlot() (the non-bit/byte one) remains called from sendMQTTData() — out of scope for ADR-076
- [x] #7 publishStatusBitMQTT (OTGW-Core.ino) commits or discards mqttPendingBitSlot based on the bool return from the downstream sendMQTTData chain — pending=false on success after confirm, or .pending=false on failure
- [x] #8 publishStatusVHBitMQTT applies the same commit-or-discard logic to mqttPendingBitSlot
- [x] #9 publishGatedBitMQTT applies the same commit-or-discard logic to mqttPendingBitSlot
- [x] #10 publishStatusByteMQTT (or wherever shouldPublishStatusByte / shouldPublishTrackedStatusByte is called) applies the same commit-or-discard logic to mqttPendingByteSlot
- [x] #11 publishStatusVHByteMQTT applies the same commit-or-discard logic to mqttPendingByteSlot
- [x] #12 publishGatedByteMQTT applies the same commit-or-discard logic to mqttPendingByteSlot
- [x] #13 publishMQTTOnOff signature is updated as needed to propagate the bool return from sendMQTTData up to the publish helpers
- [x] #14 No publish helper that installs mqttPendingBitSlot/ByteSlot.pending=true returns control to its caller without either committing the pending (on success) or clearing it (on failure)
- [x] #15 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash are staged with the firmware change
- [x] #16 python build.py --firmware exits 0
- [x] #17 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [x] #18 ADR-076 is cited in at least one comment near the relocated confirmation logic
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect sendMQTTData() overloads and publish helpers to confirm exact call surface.\n2. Change all three sendMQTTData() overloads + their declarations in OTGW-firmware.h from void to bool. Return true on successful endPublish, false on any guard or early-return path.\n3. Change publishMQTTOnOff() overloads in MQTTstuff.ino from void to bool, propagating the sendMQTTData() return.\n4. Remove confirmMQTTPublishBitSlot() and confirmMQTTPublishByteSlot() calls from sendMQTTData() in MQTTstuff.ino. Keep confirmMQTTPublishSlot() (out of scope for ADR-076).\n5. In publishStatusBitMQTT, publishStatusVHBitMQTT, publishGatedBitMQTT, and the byte equivalents (publishStatusByteMQTT, publishStatusVHByteMQTT, publishGatedByteMQTT): capture publishMQTTOnOff()/sendMQTTData() return; commit pending via confirmMQTTPublishBitSlot/ByteSlot() on true, OR clear mqttPendingBitSlot/ByteSlot.pending=false on false.\n6. Audit every caller of shouldPublishTrackedStatusBit/Byte to verify each goes through one of the audited helpers.\n7. Bump prerelease beta.8 → beta.9.\n8. python build.py --firmware; verify exit 0.\n9. python evaluate.py --quick; verify no new failures.\n10. Commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed in one pass.

sendMQTTData() signature: all three overloads in MQTTstuff.ino now return bool — true after MQTTclient.endPublish() success, false on any guard or transport early-return. Header declarations in OTGW-firmware.h updated to match.

publishMQTTOnOff() overloads in MQTTstuff.ino now return bool, propagating sendMQTTData() result.

Confirmation relocations (MQTTstuff.ino sendMQTTData success paths): removed confirmMQTTPublishBitSlot() + confirmMQTTPublishByteSlot() calls. Kept confirmMQTTPublishSlot() (normal-msgId pending — out of scope for ADR-076 per the Decision text).

Nine publish helpers updated to commit-or-discard pending:
- publishStatusBitMQTT (OTGW-Core.ino)
- publishStatusVHBitMQTT
- publishGatedBitMQTT
- publishGatedByteMQTT (F() topic overload)
- publishGatedByteMQTT (char* topic overload)
- publishMasterStatusState — inline sendMQTTData("status_master", ...)
- publishSlaveStatusState — inline sendMQTTData("status_slave", ...)
- publishMasterStatusVHState — inline sendMQTTData(F("status_vh_master"), ...)
- publishSlaveStatusVHState — inline sendMQTTData(F("status_vh_slave"), ...)

Pattern at each site: `if (sendMQTTData(...)) confirmMQTTPublish{Bit,Byte}Slot(); else mqttPending{Bit,Byte}Slot.pending = false;`

- python build.py --firmware: exit 0, artifact OTGW-firmware-1.6.0-beta.9 produced.
- python evaluate.py --quick: 34/36 pass / 0 fail / 0 warn / 2 info / Health 100%.
- Prerelease beta.8 → beta.9.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Relocated MQTT bit/byte pending-slot confirmation ownership out of sendMQTTData() and into the per-helper publish path (ADR-076 Decision item 6).

The stale-pending defect: a heap-throttled sendMQTTData() left mqttPendingBitSlot/ByteSlot.pending=true even though the publish never reached the broker; the next successful sendMQTTData() for any topic anywhere in the firmware then called confirmMQTTPublishBitSlot/ByteSlot() and silently committed the stale pending into the per-slot tracked-time array. The slot was marked as published-at-time-T even though HA never received the value, suppressing the next 60 s heartbeat.

Changes:
- src/OTGW-firmware/OTGW-firmware.h: sendMQTTData() overloads now declared `bool`, with ADR-076 contract comment.
- src/OTGW-firmware/MQTTstuff.ino: all three sendMQTTData() definitions return bool; removed confirmMQTTPublishBitSlot/ByteSlot calls from both success paths. publishMQTTOnOff() overloads return bool.
- src/OTGW-firmware/OTGW-Core.ino: nine publish helpers (Status bit, Status VH bit, gated bit, gated byte ×2, plus the four inline sendMQTTData("status_*", …) sites in publishMasterStatusState / publishSlaveStatusState / publishMasterStatusVHState / publishSlaveStatusVHState) now commit pending on a true return from sendMQTTData / publishMQTTOnOff and clear pending on false.

The normal-msgId confirmation (confirmMQTTPublishSlot) remains called from sendMQTTData — out of scope for ADR-076 per the Decision text. A separate task can fix that pending lifecycle if/when it becomes visible.

Verification:
- python build.py --firmware: exit 0, artifact OTGW-firmware-1.6.0-beta.9.
- python evaluate.py --quick: 34 pass / 0 fail / 0 warn / 2 info / Health 100%.

Follow-ups:
- Field validation under beta.9 will confirm bits 2-5 publish on heartbeat AND that no silent drops persist under heap pressure.
- 2.0.0 worktree replication (option 1 of the master plan) handled separately.
<!-- SECTION:FINAL_SUMMARY:END -->
