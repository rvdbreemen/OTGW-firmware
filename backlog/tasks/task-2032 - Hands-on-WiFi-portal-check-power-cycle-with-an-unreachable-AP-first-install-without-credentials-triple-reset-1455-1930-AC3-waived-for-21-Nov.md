---
id: TASK-2032
title: >-
  Hands-on WiFi portal check: power cycle with an unreachable AP, first install
  without credentials, triple reset (1455/1930 AC3, waived for 21 Nov)
status: To Do
assignee: []
created_date: '2026-11-18T15:40:00Z'
labels:
  - needs-maintainer-ok
  - wifi
  - bench
milestone: 2.1.0
dependencies:
  - TASK-1455
  - TASK-1930
priority: low
ordinal: 1043000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1455 AC3 and TASK-1930 AC3, waived by Robert for the 21 Nov stable only ("B", Discord DM 1572636126211080274). Not bench-testable by the bench-tester: a USB reset reports ESP_RST_USB and no USB route presses the reset button (TASK-1930), and a REST /ReBoot inside a WiFi outage cannot be placed (TASK-1455). Robert's own hands-on session, about 10 minutes, on a Classic with an S3 Mini.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A power cycle with the saved AP unreachable opens the WiFi config portal (TASK-1455 AC3)
- [ ] #2 A first install without saved credentials opens the config portal (TASK-1455 AC3)
- [ ] #3 Three reset-button presses or plug pulls, each within 10 s of boot, open the config portal; a single power cut does not (TASK-1930 AC3, ADR-238)
<!-- AC:END -->
