---
id: TASK-3002
title: 'analyze-capture: capture_plot.py --band fails on every label'
status: To Do
assignee: []
created_date: '2026-11-21T12:38:00Z'
labels:
  - tooling
dependencies: []
priority: low
ordinal: 1126000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
capture_plot.py:169 does label, span = b.split('=', 1) if b.count('=') >= 2 else ('band', b). A label without '=' (count 1) falls to the else branch and fromisoformat gets 'Label=ISO'; a label with '=' (the documented example 'HW=0=ISO/ISO') splits at the first '=' and the span becomes '0=ISO'. Both raise ValueError. Seen 2026-11-21 on marceld91d's capture (92).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 --band 'Label=ISO/ISO' and the documented 'HW=0=ISO/ISO' both draw the band (split on the last '=' before the ISO span)
- [ ] #2 A test on the real capture_plot.py argument parsing, red on the old code, green on the fix
<!-- AC:END -->
