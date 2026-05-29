---
id: TASK-752
title: 'docs(reset): document triple-reset WiFi-wipe procedure (count + timing)'
status: To Do
assignee: []
created_date: '2026-05-29 08:35'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field feedback (tjfs, alpha.84): holding the reset button does NOT wipe WiFi config; tjfs expected hold-5s (OT-Thing / common device behaviour). Decision (Robert): keep triple-reset wipe — deliberate, a recognised ESP/IoT-community standard, prevents accidental wipes. Action is docs-only (no behaviour change): document the exact procedure — how many resets, the timing window between them, and what gets wiped — in the wiki/manual, and surface it where users look. Find the triple-reset implementation to document the real counts/timing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Triple-reset procedure documented (reset count + timing window + scope of wipe) in the user-facing docs
- [ ] #2 Procedure matches the actual firmware implementation (counts/timing verified in code)
<!-- AC:END -->
