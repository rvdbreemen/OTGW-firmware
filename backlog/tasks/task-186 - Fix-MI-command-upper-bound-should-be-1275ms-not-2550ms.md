---
id: TASK-186
title: 'Fix: MI= command upper bound should be 1275ms not 2550ms'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:35'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The MI= command in handleOTDirectCommand() (OTDirect.ino ~line 2297) validates the range as 100-2550ms. However the PIC firmware spec says 100-1275ms (limited by OT protocol requirement that a message is sent at least every second, and valid range is 100-1275). Values 1280-2550 are currently accepted but should return OR (out of range). Fix: change the upper bound check from 2550 to 1275.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MI= rejects values above 1275 with OR response
- [x] #2 MI= still accepts valid values 100-1275
- [x] #3 PR=N response still reflects stored value in centiseconds
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed MI= upper bound from 2550ms to 1275ms per PIC firmware spec.

Changed val > 2550 to val > 1275 in handleOTDirectCommand(). Also updated the comment in OTGW-firmware.h, the REST API range constraint in restAPI.ino, and the settings constrain() in settingStuff.ino to consistently enforce the correct bound everywhere.
<!-- SECTION:FINAL_SUMMARY:END -->
