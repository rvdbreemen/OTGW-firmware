---
id: TASK-358
title: >-
  fix(mqtt): dedupe heap threshold 6000 between REST /verify and
  startDiscoveryVerification
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:33'
updated_date: '2026-04-21 17:31'
labels:
  - code-review
  - mqtt
  - refactor
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A HIGH#3 and Phase 2A LOW: restAPI.ino:499 hard-codes literal 6000 where VERIFICATION_MIN_HEAP_START constant exists at MQTTstuff.ino:192. Two sources of truth will silently drift on future tuning. REST also duplicates guards already enforced in startDiscoveryVerification.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 restAPI.ino uses VERIFICATION_MIN_HEAP_START instead of literal 6000
- [x] #2 Constant exposed via MQTTstuff.h or extern accessor to keep single source of truth
- [ ] #3 Optional: startDiscoveryVerification returns enum-class result for richer REST error mapping (bonus, not required)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace literal 6000 at restAPI.ino:499 with VERIFICATION_MIN_HEAP_START
2. Verify Arduino concat makes constexpr visible
3. Build firmware
4. Check ACs + mark Done
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Moved VERIFICATION_MIN_HEAP_START into MQTTstuff.h (constexpr, C++17 implicit inline so no ODR issue). MQTTstuff.ino now references via the header; restAPI.ino:499 uses the named constant. Other VERIFICATION_* constants kept local in MQTTstuff.ino with their ADR-062 tuning comments intact.

AC#3 (enum-class result from startDiscoveryVerification for richer REST error mapping) skipped: marked as Optional/bonus in the AC text; not pursued in this task. Separate follow-up candidate if REST error granularity becomes important.

Build: python build.py --firmware passed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Deduplicated the 6000-byte heap threshold between REST /verify and startDiscoveryVerification() by moving VERIFICATION_MIN_HEAP_START from MQTTstuff.ino into MQTTstuff.h.

Changes:
- MQTTstuff.h: exposed VERIFICATION_MIN_HEAP_START (constexpr uint32_t = 6000) with a cross-TU comment explaining why only this constant was promoted to the header.
- MQTTstuff.ino: removed the local definition, left a placeholder comment pointing to the header so the ADR-062 tuning block stays cohesive.
- restAPI.ino:499: literal 6000 replaced with VERIFICATION_MIN_HEAP_START.

Why:
- Single source of truth restored; a future re-tune of the heap floor only needs to touch one line.
- Header-based exposure is robust against future build-system changes (PlatformIO compiles .ino files as separate TUs, unlike the Arduino IDE concat).
- constexpr at namespace scope is implicitly inline since C++17, so no ODR violation across multiple TUs that include MQTTstuff.h.

Tests:
- python build.py --firmware passed (0.70 MB artifact, no new warnings).

Follow-ups / skipped scope:
- AC#3 (enum-class result from startDiscoveryVerification) marked Optional in the AC text and deferred. Could be revisited if REST error granularity becomes a priority.
<!-- SECTION:FINAL_SUMMARY:END -->
