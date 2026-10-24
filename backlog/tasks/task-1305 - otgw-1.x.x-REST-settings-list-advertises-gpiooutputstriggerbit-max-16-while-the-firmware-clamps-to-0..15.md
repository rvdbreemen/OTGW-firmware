---
id: TASK-1305
title: >-
  otgw-1.x.x: REST settings list advertises gpiooutputstriggerbit max 16 while
  the firmware clamps to 0..15
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T20:28:00Z'
updated_date: '2026-10-24T18:05:00Z'
labels:
  - 1.x
  - rest
dependencies: []
priority: low
ordinal: 428000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1295 review (2026-10-22). On otgw-1.x.x restAPI.ino:1678 advertises 0..16 for gpiooutputstriggerbit while settingStuff.ino:685 clamps to 0..15 (webhooktriggerbit already says 15). The dev line fixed its copy in TASK-1295. One-line port in the 1.x worktree; the 1.x UI renders the field as a plain number input with min/max from this range.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 restAPI.ino on otgw-1.x.x advertises 0..15 for gpiooutputstriggerbit
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
otgw-1.x.x restAPI.ino advertises 0..15 for gpiooutputstriggerbit, matching settingStuff.ino's clamp. Commit 0f9c3c56b on origin/otgw-1.x.x. Evidence: 1.x build (esp8266) 'Build completed successfully', fresh OTGW-firmware-1.7.8-beta.6 .ino.bin; evaluate --quick 0 failed; adr-judge 0 violations.
<!-- SECTION:FINAL_SUMMARY:END -->
