---
id: TASK-643
title: >-
  Fix stale-pending defect in MQTT bit/byte slot confirmation (ADR-076 Decision
  item 6)
status: To Do
assignee: []
created_date: '2026-05-21 07:37'
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
- [ ] #1 sendMQTTData(const char*, const char*, const bool) signature changes from void to bool — true on successful end-to-end publish, false on any guard or transport failure
- [ ] #2 sendMQTTData(const __FlashStringHelper*, const char*, const bool) signature changes from void to bool — same semantics
- [ ] #3 sendMQTTData(const __FlashStringHelper*, const __FlashStringHelper*, const bool) signature changes from void to bool — same semantics
- [ ] #4 All three sendMQTTData declarations in OTGW-firmware.h are updated to return bool
- [ ] #5 confirmMQTTPublishBitSlot() and confirmMQTTPublishByteSlot() calls are removed from both sendMQTTData() overloads in MQTTstuff.ino (lines 980-982 and 1024-1026 area)
- [ ] #6 confirmMQTTPublishSlot() (the non-bit/byte one) remains called from sendMQTTData() — out of scope for ADR-076
- [ ] #7 publishStatusBitMQTT (OTGW-Core.ino) commits or discards mqttPendingBitSlot based on the bool return from the downstream sendMQTTData chain — pending=false on success after confirm, or .pending=false on failure
- [ ] #8 publishStatusVHBitMQTT applies the same commit-or-discard logic to mqttPendingBitSlot
- [ ] #9 publishGatedBitMQTT applies the same commit-or-discard logic to mqttPendingBitSlot
- [ ] #10 publishStatusByteMQTT (or wherever shouldPublishStatusByte / shouldPublishTrackedStatusByte is called) applies the same commit-or-discard logic to mqttPendingByteSlot
- [ ] #11 publishStatusVHByteMQTT applies the same commit-or-discard logic to mqttPendingByteSlot
- [ ] #12 publishGatedByteMQTT applies the same commit-or-discard logic to mqttPendingByteSlot
- [ ] #13 publishMQTTOnOff signature is updated as needed to propagate the bool return from sendMQTTData up to the publish helpers
- [ ] #14 No publish helper that installs mqttPendingBitSlot/ByteSlot.pending=true returns control to its caller without either committing the pending (on success) or clearing it (on failure)
- [ ] #15 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash are staged with the firmware change
- [ ] #16 python build.py --firmware exits 0
- [ ] #17 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [ ] #18 ADR-076 is cited in at least one comment near the relocated confirmation logic
<!-- AC:END -->
