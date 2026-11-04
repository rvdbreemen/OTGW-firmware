---
id: TASK-324
title: '[A11Y-L3] Add accessible names to network SVG icons (role=img aria-hidden)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:26'
updated_date: '2026-04-18 21:02'
labels:
  - accessibility
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index.html:32-37 <span class=net-status> has title attribute but the SVGs have no <title>/aria-label/role=img. Textual status in #netStatusText already conveys the info.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SVGs carry role=img aria-hidden=true (decorative)
- [x] #2 Parent span has aria-label=Network status: ... kept in sync with JS in updateNetStatus
- [x] #3 Screen reader announces a single meaningful label per network widget, not two
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SVG netIconWifi and netIconEth carry role=img aria-hidden=true (decorative). Parent span #netStatus has a static aria-label in HTML and updateNetworkIndicator in index.js now sets container.setAttribute('aria-label', ...) alongside title, so SR users get a meaningful announcement that stays in sync with the WiFi/Ethernet/AP state.
<!-- SECTION:FINAL_SUMMARY:END -->
