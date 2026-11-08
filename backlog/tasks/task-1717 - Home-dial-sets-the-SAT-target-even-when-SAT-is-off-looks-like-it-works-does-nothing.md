---
id: TASK-1717
title: >-
  Home dial +/- sets the SAT target even when SAT is off (looks like it works,
  does nothing)
status: To Do
assignee: []
created_date: '2026-11-08T07:54:00Z'
labels:
  - webui
  - field-report
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 826000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs 2026-11-08 07:51 #alpha-testing (msg 1568895744029499403): in OT-Direct gateway mode with a thermostat and SAT off, the phone view dial (+/-, design-b bUp/bDown) 'doesn't do anything'. v2.js stepTarget() always POSTs /api/v2/sat/target and reports 'target set', so with SAT off the boiler is unaffected and the UI suggests success. The thermostat owns the setpoint; the gateway way is TT= (temporary) / TC= (constant) room setpoint override (OTDirect.ino:2276-2311, TASK-466), refused while SAT is on (TASK-1695).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With SAT off the dial does not pretend to set anything: it either sends a TT= room-setpoint override (and says so) or is shown as SAT-only (decision Robert)
- [ ] #2 With SAT on the dial keeps setting the SAT target (test)
<!-- AC:END -->
