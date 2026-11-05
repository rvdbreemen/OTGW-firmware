---
id: TASK-1665
title: >-
  Diagnostic recorder core: ring, record format and capture points (ADR-213 step
  1)
status: To Do
assignee: []
created_date: '2026-11-05T20:34:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 788000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement ADR-213 decisions 1-4: versioned 4 KiB-block format (16-byte block header, type u8 + LEB128 dt + payload, repeat cache reset per block, topic id table), the 1.5 MB PSRAM ring via platformPsramAllocOnce() and the 4 KB no-PSRAM ring allocated on start / freed on stop with the refuse-below-5120 B rule, overwrite-oldest with gap records, and the capture points listed in ADR-213 (not HA discovery, not port 25238 bytes). Resume after reboot with the Reboot record first. Redaction of secrets, location (0.1 deg) and BLE MAC slots.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host tests: format round-trip, block-independent decode, refuse-below-5120 B red on an unguarded start and green with the rule, resume record first
- [ ] #2 Bench .61 (no PSRAM) and a PSRAM board: measured RAM/flash cost and append cost recorded in the task
<!-- AC:END -->
