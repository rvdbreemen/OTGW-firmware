---
id: TASK-1274
title: 'Check: MQTT publish gate logs prev=0x0000 first=true on every slave frame'
status: To Do
assignee: []
created_date: '2026-10-20 18:18'
labels:
  - mqtt
  - investigate
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 404000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. In the telnet log the MQTT gate prints 'src=S slot=1 prev=0x0000 first=true' on every A/B frame, as if the previous value is never stored. Not verified; may cause redundant publishes or just a misleading log line.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Cause explained with file:line; fixed if it is a real defect (old-vs-fix publish count)
<!-- AC:END -->
