---
id: TASK-21
title: Compact OT/MQTT publish tracking tables
status: In Progress
assignee:
  - '@copilot'
created_date: '2026-03-18 19:44'
updated_date: '2026-03-19 18:04'
labels:
  - memory mqtt restapi core
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-Core.ino:221'
  - 'src/OTGW-firmware/OTGW-Core.ino:222'
  - 'src/OTGW-firmware/OTGW-Core.ino:223'
  - 'src/OTGW-firmware/OTGW-Core.ino:225'
  - 'src/OTGW-firmware/restAPI.ino:598'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-Core now keeps per-message tracking arrays for REST timestamps and MQTT throttling: msglastupdated[256], mqttlastsent[256], mqttlastsentstatusbit[16], plus first-seen bitmaps and status-byte timers. This block accounts for roughly 2.1KB of persistent RAM and is the largest single always-resident addition since v1.2.0. This task evaluates denser encodings and narrower scope, such as limiting storage to publishable/REST-exposed message IDs, reducing timestamp width where safe, or splitting normal/status paths into smaller dedicated tables. The aim is to reclaim 1KB or more without breaking MQTT interval gating or REST last-updated semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Persistent RAM used by OT/MQTT tracking tables is reduced by at least 1024 bytes
- [ ] #2 MQTT publish throttling and first-seen behavior remain functionally equivalent
- [ ] #3 REST API last-updated timestamps remain available for the supported fields
- [ ] #4 No cross-slot contamination is introduced in MQTT gating logic
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace both MQTT and REST tracking timestamps with uint16_t storage and keep all interval comparisons wrap-safe using unsigned subtraction against seconds-since-boot values.
2. Split the current 256-entry mixed-purpose tracking into dedicated dense tables: one for MQTT publish slots and one for REST-exposed OT fields, so width reduction and scope reduction both contribute to RAM savings.
3. Update the REST last-updated path to read from the new uint16_t table, accepting that the reported second counter can represent only the most recent 65535 seconds (18h 12m 15s) before wrap.
4. Preserve first-seen and status-slot isolation with explicit lookup helpers for normal OT IDs, status bits, and status bytes, then validate build output and wrap behavior reasoning for both MQTT and REST consumers.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented low-risk tracking compaction: dense uint16_t REST last-updated table for OT monitor fields plus sentinel-based MQTT first-seen tracking using uint16_t rolling seconds.

Validated with full firmware build after fixing a local name-shadowing compile error in enterPSMode().

Measured result: global RAM 58068 -> 57164 bytes, reclaiming 904 bytes and leaving 24756 bytes free. This preserves the low-risk behavior path but does not yet meet AC #1's 1024-byte target.

Added MQTT gate debug tracing for validation: normal OT publishes now log previous/current raw values and status publishes log previous/current status bitsets plus per-bit decisions under the existing MQTT debug flag.

Validated with firmware build after the debug instrumentation change; build succeeded at 57184 bytes global RAM (20 bytes above the prior 57164-byte baseline).

Extended status-style MQTT gating to ID70 (ventilation/heat-recovery): added separate combined-byte and per-bit tracked timers plus force-republish state so ID70 now follows the same first/change/interval path and MQTT debug pattern as ID0.

Validated with firmware build after the ID70 change; build succeeded at 57412 bytes global RAM, an increase of 228 bytes over the prior 57184-byte debug baseline.

Identified a low-risk follow-up compaction: only 113 IDs in the 0-127 OpenTherm range are currently trackable; replacing the dense 256-slot MQTT table with a dense 226-slot tracked-ID table should recover the missing 120 bytes needed to cross the 1024-byte target while preserving status-ID special cases and passthrough behavior for untracked IDs.

Validated dense tracked-ID follow-up with full firmware build. Global RAM is now 57292 bytes, improving the post-ID70 baseline by 120 bytes (57412 -> 57292) but still missing AC #1's 1024-byte target. Keeping TASK-21 in progress and moving to TASK-23 for the next larger MQTT RAM reduction.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Superseded by later direction and larger wins elsewhere.

Reason:
- The original tracking-table target stayed open only because it aimed for an additional 1024-byte reduction inside the tracking subsystem itself.
- You later requested the simpler linear 0..127 mapping instead of denser or sparse tracking variants.
- Subsequent MQTT publish-path changes reclaimed roughly 1200 bytes of persistent RAM without increasing tracking-table complexity.
<!-- SECTION:FINAL_SUMMARY:END -->
