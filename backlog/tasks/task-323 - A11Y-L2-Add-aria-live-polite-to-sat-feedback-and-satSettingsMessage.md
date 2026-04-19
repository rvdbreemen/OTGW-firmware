---
id: TASK-323
title: '[A11Y-L2] Add aria-live polite to sat-feedback and satSettingsMessage'
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
index.html:405 #sat-feedback and index.html:592 #satSettingsMessage are updated after user actions but have no aria-live, so screen readers do not announce success/failure. Dashboard polling stays silent (by design).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sat-feedback has aria-live=polite and aria-atomic=true
- [x] #2 satSettingsMessage has aria-live=polite
- [x] #3 Continuous-update panels (cards, graphs) remain without aria-live
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.html:405 #sat-feedback gets aria-live=polite aria-atomic=true; index.html:592 #satSettingsMessage gets aria-live=polite. User-action outcomes will now be announced by screen readers; polling panels intentionally left silent to avoid chattiness.
<!-- SECTION:FINAL_SUMMARY:END -->
