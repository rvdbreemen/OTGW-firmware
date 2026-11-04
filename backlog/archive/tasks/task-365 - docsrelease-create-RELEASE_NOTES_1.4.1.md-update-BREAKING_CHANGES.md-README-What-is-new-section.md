---
id: TASK-365
title: >-
  docs(release): create RELEASE_NOTES_1.4.1.md + update BREAKING_CHANGES.md +
  README What is new section
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:46'
updated_date: '2026-04-21 17:05'
labels:
  - code-review
  - docs
  - release
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3B HIGH: branch ships 4 user-visible changes (heap-diag MQTT topic, auto-verify setting, 3 new REST endpoints, behavioural shifts in drip/slow-mode/nightly-restart) that are undocumented outside ADR-062. Users browsing the repo cold see the v1.4.0 section as the newest — misleading.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 RELEASE_NOTES_1.4.1.md created at repo root, structure mirrors RELEASE_NOTES_1.3.5.md
- [x] #2 Sections: Heap pressure reduction (TASK-338..347), Discovery verification and republish (TASK-349/351), Time-boundary dispatcher refactor (TASK-350)
- [x] #3 docs/BREAKING_CHANGES.md gets v1.4.0 and v1.4.1 blocks (no breaking changes, additive-only)
- [x] #4 README.md gets What's new in v1.4.1 section ahead of v1.4.0
- [x] #5 All referenced ADRs and TASKs linked correctly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read review artefacts + v1.3.x templates
2. Draft RELEASE_NOTES_1.4.1.md (structure mirrors v1.3.4)
3. Update BREAKING_CHANGES.md (add v1.4.0 + v1.4.1 blocks)
4. Add README What's new in v1.4.1 section immediately before 1.4.0 block
5. Check ACs, verify all referenced ADRs/TASKs resolve
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created RELEASE_NOTES_1.4.1.md (137 lines) at repo root mirroring the v1.3.4/v1.3.5 template structure: Overview, What's new with 4 subsections (heap pressure, discovery verify, hourly heap-diag, time-boundary dispatcher), Behavioural notes, Known limits/trade-offs, Upgrade notes, Breaking changes reference, Links.

Held document to 137 lines instead of the brief-suggested 300-500 because the house style (v1.3.4 = 29 lines, v1.3.5 = 30 lines) is concise; padding to triple-digit would violate project prose norms. All 12 task links and both ADR links verified against filesystem.

Added v1.4.0 and v1.4.1 blocks to docs/BREAKING_CHANGES.md at the top (both no-breaking). v1.4.1 block documents MQTTdiscoveryAutoVerify default and additive-only nature; v1.4.0 block documents SAT + ESP32 additive stance (no link to non-existent v1.4.0 release notes file).

Added "What's New in v1.4.1" section to README.md immediately above the v1.4.0 block, 4 bullet points + link to full notes.

Stripped all em dashes from my additions per user feedback; pre-existing em dashes in v1.2.0 block of BREAKING_CHANGES and in unmodified README sections left alone (not in my ownership scope).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Release-facing documentation for v1.4.1 is in place.

Changes:
- New: RELEASE_NOTES_1.4.1.md at repo root (137 lines). Mirrors the v1.3.4 template: Overview, What's new (heap pressure reduction, retained discovery verification + republish, hourly heap-diag MQTT topic, unified time-boundary dispatcher), Behavioural notes for users, Known limits and trade-offs, Upgrade notes, Breaking changes cross-reference, and Links (ADR-062, ADR-086 [originally ADR-064], TASK-338..351).
- Updated: docs/BREAKING_CHANGES.md. Added no-breaking blocks for v1.4.0 (SAT + ESP32 additive) and v1.4.1 (heap + discovery-verify additive, MQTTdiscoveryAutoVerify default note). Old blocks untouched.
- Updated: README.md. Added "What's New in v1.4.1" section immediately before the v1.4.0 block with 4 summary bullets and a link to the full release notes.

Verification:
- All 12 TASK-NNN links resolve to real files in backlog/tasks/.
- Both ADR links (ADR-062, ADR-086 [originally ADR-064]) resolve to real files in docs/adr/.
- No em dashes in any of my additions (per user global preference).
- English throughout (per release-notes policy).

Scope discipline: did not touch openapi.yaml / MQTT.md (owned by Agent E under TASK-366) and did not touch backlog task Final Summaries (owned by Agent F under TASK-367). No commits, no merges.

One conscious deviation: brief suggested 300-500 lines, released at 137. House style (v1.3.4 = 29 lines, v1.3.5 = 30 lines) is concise; 137 is already 4-5x house baseline and covers all four themes, behavioural notes, limits and upgrade notes without padding.
<!-- SECTION:FINAL_SUMMARY:END -->
