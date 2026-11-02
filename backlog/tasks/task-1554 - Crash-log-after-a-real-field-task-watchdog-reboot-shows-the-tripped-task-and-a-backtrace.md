---
id: TASK-1554
title: >-
  Crash log after a real field task-watchdog reboot shows the tripped task and a
  backtrace
status: To Do
assignee: []
created_date: '2026-11-01T22:28:00Z'
updated_date: '2026-11-02T04:39:00Z'
labels:
  - needs-field-data
dependencies: []
priority: low
ordinal: 707000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1479 AC#1 (lead triage 2026-11-01): the field half needs a tester whose unit hits a real TWDT. TASK-1479 proves the path on the bench (async_tcp hang injected on .61, addr2line matched). This task waits for one field crashlog + /api/v2/device/crashlog from a tester after a real TWDT.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A field TWDT crashlog (tester capture or crashlog JSON) names the tripped task and decodes to a backtrace with addr2line against the matching .elf
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: bench half proven in TASK-1479 (42571b0ea). Waits on a field TWDT capture.
<!-- SECTION:NOTES:END -->
