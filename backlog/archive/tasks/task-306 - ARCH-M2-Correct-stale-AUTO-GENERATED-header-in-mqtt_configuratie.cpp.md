---
id: TASK-306
title: '[ARCH-M2] Correct stale AUTO-GENERATED header in mqtt_configuratie.cpp'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:20'
updated_date: '2026-04-18 21:02'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
File header claims generation by tools/generate_mqttha_data.py which no longer exists per ADR-077. A contributor following the instructions destroys hand-written streaming composers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Header rewritten: hand-written streaming HA discovery per ADR-077, edit directly
- [x] #2 Reference to ADR-077 included in the new comment
- [x] #3 No other file carries a similar stale auto-gen banner
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
mqtt_configuratie.cpp header rewritten: removed the AUTO-GENERATED / DO NOT EDIT banner that pointed at a tool removed per ADR-077, replaced with an honest hand-written note citing ADR-077. Content counts kept for quick reference.
<!-- SECTION:FINAL_SUMMARY:END -->
