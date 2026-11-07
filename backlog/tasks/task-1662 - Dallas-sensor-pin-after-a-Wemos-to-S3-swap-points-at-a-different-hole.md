---
id: TASK-1662
title: Dallas sensor pin after a Wemos-to-S3 swap points at a different hole
status: To Do
assignee: []
created_date: '2026-11-05T19:30:00Z'
labels:
  - docs
  - sensors
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 786000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the Classic the S3 Mini is footprint-compatible, but settings.sensors.iPin (gpiosensorspin) is a raw GPIO number: a 1.x value (e.g. 13 = D7 on the D1 mini) lands on another hole on the S3 (GPIO13 = D6; D7 = GPIO11, PINOUT.md 3a). A user who restores 1.x settings or keeps the old number gets no sensor or the wrong pin. Raised by a field-tester question in #nederlandse-ondersteuning 2026-11-05 (msg 1567961775649853561). Options: document the mapping in the S3 migration guide and web UI help; or translate D1-mini GPIO numbers on settings import when the board is Classic-on-S3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision by Robert: document only, or translate on import
- [ ] #2 The S3 migration docs list the D1-mini hole to S3 GPIO mapping for the sensor pin
<!-- AC:END -->
