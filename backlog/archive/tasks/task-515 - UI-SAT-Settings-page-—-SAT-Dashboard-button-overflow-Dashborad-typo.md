---
id: TASK-515
title: 'UI: SAT Settings page — ''SAT Dashboard'' button overflow + ''Dashborad'' typo'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-02 18:32'
updated_date: '2026-05-03 16:40'
labels:
  - ui
  - sat
  - bugfix
  - typo
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeantd reports (Discord #dev-sat-mqtt 2026-04-28 06:05 UTC, msg 1498565740801036410) two bugs on one button on the SAT Settings page:

1. **Text overflow** — the button label runs past the button's right edge.
2. **Typo** — the label reads `SAT Dashborad` instead of `SAT Dashboard`.

Likely a single-line fix (string + width/padding tweak) in either `src/OTGW-firmware/data/index.html` or one of the SAT settings partials, plus a CSS check on the containing button class.

**Reference screenshot:** Discord attachment on the source message.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 String reads 'SAT Dashboard' (correct spelling) wherever 'Dashborad' currently appears in the SAT settings UI
- [ ] #2 Button label fits within the button width on 1366x768 and on a 360px mobile viewport
- [x] #3 No other SAT-area copy regressions (grep the project for 'Dashborad' to catch other occurrences)
- [ ] #4 Verified in Chrome + Firefox latest
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-515 — closed as already-fixed (no code changes)

**Investigation (2026-05-03):**

Project-wide case-insensitive grep for `Dashborad` returns **zero matches in source files** — only the TASK-515 description itself contains the typo string.

```
$ grep -ri "Dashborad" .
backlog/tasks/task-515 ...md (4 lines, all describing the typo)
```

The original `SAT Dashborad` back-button that Sergeant D screenshotted on 2026-04-28 06:05 UTC (Discord msg `1498565740801036410`) **no longer exists in the source**. Most likely cause: the `page-nav-shell` template migration in commit `32e7d907 fix(webui): density tweaks + Save-button scope + nav shell migration` replaced the per-page navigation buttons with a shared `<template id="pageNavTemplate">` that has only `Home / SAT / Settings / Advanced` (`index.html:63-80`). After the migration, users navigate back to the SAT dashboard via the `SAT` tab in the shared nav, not via a dedicated "SAT Dashboard" back-button on the settings page.

**AC verdict:**

- **AC #1** ✅ — `grep -ri "Dashborad"` returns zero hits in source. Typo is gone.
- **AC #2** — N/A. The button that was overflowing no longer exists. Current SAT Settings page navigation uses the shared `nav-container` (`index.html:64-79`) whose buttons are short single words.
- **AC #3** ✅ — same grep covers entire project; no other Dashborad regressions.
- **AC #4** — visual verification not blocked by code; user can confirm in Chrome / Firefox at next browser session, but there's no defective state to verify against.

**No code changes required.** No commit needed; the underlying cause was already fixed by the nav-shell migration (`32e7d907`). This task is closed as a verification-only task.

**Next time** a typo report comes in, an early `grep -ri` check before scoping a task would catch the "already fixed by adjacent refactor" case faster — pattern worth keeping in mind for the SAT UI area which has been churning rapidly in recent weeks.
<!-- SECTION:FINAL_SUMMARY:END -->
