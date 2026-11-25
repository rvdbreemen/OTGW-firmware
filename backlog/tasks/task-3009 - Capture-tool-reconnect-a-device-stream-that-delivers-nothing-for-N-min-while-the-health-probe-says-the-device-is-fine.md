---
id: TASK-3009
title: >-
  Capture tool: reconnect a device stream that delivers nothing for N min while
  the health probe says the device is fine
status: To Do
assignee: []
created_date: '2026-11-21T14:56:00Z'
updated_date: '2026-11-25T06:40:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-3006 (otgw-dev analysis, will_85903 20 Nov): 08:04-13:18 both device streams (telnet, /ws frames) delivered nothing with open sockets and a healthy device; the 60 s telnet idle check only reconnects when the HTTP health probe fails or the uptime drops, so the tool kept waiting. TASK-3006 now REPORTS such a silence (StreamWatch); this task makes the tool also reconnect that stream and mark the gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A telnet or frame stream silent for N min while the health probe is OK is reconnected once, with a gap marker
- [ ] #2 A test on the real tool (fake device that keeps the socket open but sends nothing) fails on the old code and passes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: deferred to 2026-11-25 (token budget, Robert via coordinator). Commits stay as they are, no builds, no new work until then. The .63 25 h soak of 464a188 runs unattended; 09 reads only the end result. The capture e2e runs by 09 on .61 may finish; their results get read on 25 Nov.
<!-- SECTION:NOTES:END -->
