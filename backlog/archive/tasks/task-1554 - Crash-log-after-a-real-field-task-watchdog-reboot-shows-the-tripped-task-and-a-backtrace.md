---
id: TASK-1554
title: >-
  Crash log after a real field task-watchdog reboot shows the tripped task and a
  backtrace
status: Done
assignee: []
created_date: '2026-11-01T22:28:00Z'
updated_date: '2026-11-04T05:33:00Z'
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

2026-11-04 Robert (interactive): closed as waiting-for-field; reopen when the field evidence arrives (tester capture / dump).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed by Robert 2026-11-04 as waiting-for-field: the remaining AC needs field evidence that cannot be produced on the bench (see notes). Reopen when that evidence arrives.
<!-- SECTION:FINAL_SUMMARY:END -->
