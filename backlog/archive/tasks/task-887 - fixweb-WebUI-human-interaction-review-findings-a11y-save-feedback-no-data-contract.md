---
id: TASK-887
title: >-
  fix(web): WebUI human-interaction review findings (a11y, save-feedback,
  no-data contract)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-19 10:17'
updated_date: '2026-06-29 21:52'
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
- [x] #1 WebUI fixes applied across data/ assets; M1 firmware emits null for no-source; OpenAPI updated nullable; build esp32 + littlefs green; evaluate --quick green (ADR-091 drift pass); on-device verified (nav contrast, Advanced caret/keyboard, M1 '--', focus); openapi_compliance 18/18 live
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
alpha.220: re-review of the alpha.219 fixes (3 firmware-aware dimensions, adversarially verified) found 0 Critical/High, 2 self-inflicted a11y gaps in the new code (both fixed): re-review-M1 Escape focus-restore targeted the hidden main-page trigger clone (now '.page-section.active .adminSettings') and re-review-L1 aria-sort was click-only (now set on initial Statistics render). Plus header-version retry-on-failure (maintainer request). firmware-m1 dimension found nothing (SAT null contract clean). All 3 on-device verified on OTGW32 alpha.220 (focusRestoredToVisibleTrigger=true, Dec aria-sort='ascending' on initial render, version fills on hard reload). Remaining for Done: maintainer field/a11y sign-off (keyboard-tab focus rings, screen-reader pass, Firefox/Safari, real phone).

CLOSE 2026-06-29: maintainer field/a11y sign-off PASSED (user-verified keyboard focus rings, screen-reader, Firefox/Safari, real phone, M1 '--', Advanced caret). openapi 17/17 confirmed live earlier this session. AC#1 done.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WebUI a11y + null-source data contract done; maintainer field/a11y sign-off passed.
<!-- SECTION:FINAL_SUMMARY:END -->
