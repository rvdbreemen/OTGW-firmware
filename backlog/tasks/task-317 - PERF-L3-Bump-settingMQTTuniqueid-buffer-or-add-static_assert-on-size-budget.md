---
id: TASK-317
title: '[PERF-L3] Bump settingMQTTuniqueid buffer or add static_assert on size budget'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:24'
updated_date: '2026-04-19 06:21'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
settingMQTTuniqueid[26] currently uses ~14 chars. New streaming composer builds long composite keys; a future change appending hostname into a 32-byte buffer would silently overflow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Buffer size documented with a short comment explaining the format budget
- [x] #2 Either bump to [33] at next settings schema bump, or add static_assert(sizeof(...) >= 20)
- [x] #3 No change in saved settings format
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Buffer was already 41 bytes (not 26 as the review quoted from an older memory). Added format-budget comment plus a static_assert(sizeof(sUniqueid) >= 20) inside MQTTSection so any future shrink below 20 bytes fails to compile.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTGW-firmware.h MQTTSection: added a 3-line format budget comment plus static_assert(sizeof(sUniqueid) >= 20) inside the struct. No size change (buffer is already 41). Saved settings format unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
