---
id: TASK-1698
title: >-
  Per-board defaults for the S0 and GPIO output pins (the 1.x defaults are board
  pins on the S3 Mini)
status: To Do
assignee: []
created_date: '2026-11-07T16:55:00Z'
labels:
  - firmware
  - settings
dependencies: []
priority: medium
ordinal: 811000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 (follow-up of TASK-1686): the S0 default GPIO 12 (PIC reset on the S3 Mini, I2C SDA on the Pro) and the output default GPIO 16 (LED1 on the S3 Mini) are board pins; initS0Count()/validateGPIOOutputsConfig() refuse them (TASK-1552), so enabling S0 or the output without changing the pin does nothing. Choose a free hole per board map (Classic S3 Mini, Classic S3 Mini Pro, OT-Direct) and use it only while the stored pin is still the old default, so a user's own choice is never overwritten.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Per board map a free default for S0 and the output, documented with the hole it lands on
- [ ] #2 Applied only when the stored pin equals the old 1.x default; a user's own pin stays; host test red on the old code, green on the fix
- [ ] #3 Bench: a unit with the old default gets the new pin, S0/output start without the board-pin refusal
<!-- AC:END -->
