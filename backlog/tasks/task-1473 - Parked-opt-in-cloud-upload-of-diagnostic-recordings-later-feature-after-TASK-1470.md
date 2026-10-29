---
id: TASK-1473
title: >-
  Parked: opt-in cloud upload of diagnostic recordings (later feature, after
  TASK-1470)
status: To Do
assignee: []
created_date: '2026-10-29T12:45:00Z'
labels:
  - parked
  - needs-maintainer-ok
  - tooling
dependencies: []
priority: low
ordinal: 633000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: local first; the cloud upload from gruijter's proposal (TASK-1469: encrypted batches over plain HTTP to a Cloudflare store, offline key) is parked as a later feature. Not to be picked up until Robert releases it, and not before the local recording (TASK-1470) works. Constraints already recorded: must fail quietly on a LAN with internet blocked (martreides, TASK-1471); no TLS on the device (encrypt the payload instead); account and key ownership undecided.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert releases this task (removes the parked label) before any design work
<!-- AC:END -->
