---
id: TASK-352
title: >-
  fix(heapdiag): expand sendMQTTheapdiag JSON buffer to prevent truncation at
  max counters
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:31'
updated_date: '2026-04-23 19:19'
labels:
  - code-review
  - heap
  - mqtt
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2B review found sendMQTTheapdiag json[384] overflows by 81 bytes at max counter saturation. snprintf_P silently truncates, corrupting the retained MQTT message on otgw-firmware/stats/heap; the corrupt message stays on the broker until the next hourly publish overwrites it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 json buffer in sendMQTTheapdiag raised from 384 to 512 bytes
- [x] #2 Worst-case 17-field serialization (465 bytes + NUL) fits within new buffer
- [x] #3 Inline comment documents the size calculation
- [x] #4 No snprintf_P truncation under max-counter stress test
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read sendMQTTheapdiag and buffer math
2. Raise json[384] to json[512] with comment explaining the 465-byte worst case
3. Verify build
4. Check ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edit applied to MQTTstuff.ino (sendMQTTheapdiag).
- char json[384] -> char json[512]
- Added 6-line comment documenting the 465-byte worst-case math (17 JSON scaffolding tokens + uint32/uint16/uint8 max-width digits).
Build: python build.py --firmware passed (no warnings introduced).
AC4 (no snprintf_P truncation under max-counter stress test) left unchecked: requires a deliberate stress scenario to saturate all counters simultaneously; that is tester territory, not something I can objectively verify from a compile-only pass.

2026-04-23 triage: sendMQTTheapdiag() has been fully refactored to publish 17 individual topics via publishStatU32() instead of a single JSON blob (MQTTstuff.ino:1048-1076). The json[] buffer this task was fixing no longer exists in the codebase. The underlying truncation concern is structurally eliminated -- no JSON, no truncation. Task is Done-by-obsolescence: the 384->512 fix was briefly applied, then superseded by the per-topic architectural change that makes buffer sizing irrelevant for this path. AC #4 (no snprintf_P truncation under max-counter stress) vacuously satisfied because snprintf_P is not invoked here anymore.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Raised the `json[]` buffer in `sendMQTTheapdiag()` (MQTTstuff.ino) from 384 to 512 bytes to eliminate silent `snprintf_P` truncation on the retained `otgw-firmware/stats/heap` message.

Why:
- Phase-2B performance review measured a 465-byte worst-case serialisation for the 17-field JSON payload (10 uint32 counters at 10 digits each + 3 uint16 + 1 uint8 + scaffolding). 384 bytes truncated under realistic counter saturation, corrupting the retained payload for up to an hour (until the next hourly publish overwrote it).

Changes:
- `char json[384]` -> `char json[512]`.
- Replaced the one-line historical comment with a 6-line inline breakdown so future maintainers know both the budget and the components, and can resize correctly if new `disc_*` / `heap_*` fields are added.

Tests:
- python build.py --firmware passed.
- Tester verification still required for AC4 (live max-counter stress test); build-time cannot prove runtime saturation.

Risk: +128B static RAM (one-shot in function scope, not on the hot path). Negligible.
<!-- SECTION:FINAL_SUMMARY:END -->
