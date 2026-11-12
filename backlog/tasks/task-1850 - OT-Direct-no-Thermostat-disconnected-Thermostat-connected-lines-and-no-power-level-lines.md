---
id: TASK-1850
title: >-
  OT-Direct: no "Thermostat disconnected"/"Thermostat connected" lines and no
  power-level lines
status: To Do
assignee: []
created_date: '2026-11-12T00:05:00Z'
updated_date: '2026-11-12T00:12:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 929000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N16), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC prints these on the serial line (G:1761-1764, :1776-1778, :5490-5491; power changes G:1532-1551; tests/standalone-1.stc, thermostat-3.stc).
- OT-Direct sends only a WebSocket status and a debug line (O:2807-2843). The firmware's event table knows the PIC tokens (OTGW-Core.ino:5528-5532) but never gets them on OT-Direct.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 on thermostat loss and return the PIC lines go to port 25238 and processOT
- [ ] #2 host test on the real checkThermostatTimeout
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): timing depends on TASK-1728 and TASK-1802.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
