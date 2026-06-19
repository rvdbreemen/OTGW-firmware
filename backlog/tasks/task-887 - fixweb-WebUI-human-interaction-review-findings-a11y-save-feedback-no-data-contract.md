---
id: TASK-887
title: >-
  fix(web): WebUI human-interaction review findings (a11y, save-feedback,
  no-data contract)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-19 10:17'
labels: []
dependencies: []
ordinal: 103000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Multi-dimensional human-interaction/UX review of the OTGW WebUI (live device + 4-dimension source review, adversarially verified) found 14 issues. Fix the actionable set: H1 settings-save silent network-failure; M1 0.00-vs-'--' no-data ambiguity (firmware SAT-status contract: emit null for no-source outside_temp/final_setpoint); M2 nav contrast; M3 invisible focus rings; M4 keyboard-inert Advanced dropdown; M5/M6 footer/caption contrast; M7 SAT toggle accessible name; L1-L6 polish (caret/Escape, friendly FS banner, sortable headers a11y, heap label, theme tooltip, mobile table); plus live supplements (sticky-footer overlap, console JSON spam).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 WebUI fixes applied across data/ assets; M1 firmware emits null for no-source; OpenAPI updated nullable; build esp32 + littlefs green; evaluate --quick green (ADR-091 drift pass); on-device verified (nav contrast, Advanced caret/keyboard, M1 '--', focus); openapi_compliance 18/18 live
<!-- AC:END -->
