---
id: TASK-1534
title: 'Flash utility page (/update): no way to cancel and go back to the web UI'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T20:34:00Z'
updated_date: '2026-10-31T21:17:00Z'
labels:
  - webui
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 690000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-10-31 19:45 UTC (msg 1566176271086522579), screenshot of /update on alpha.569 (phone): 'from this screen there is no way to cancel and go back to the main menu'. The page (src/OTGW-firmware/updateServerHtml.h) has no back/cancel link to the web UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The flash utility page has a visible 'Back to the web UI' link/button that returns to / without starting anything
- [x] #2 Browser test or screenshot before/after (synthetic data)
- [x] #3 Bench unit serves the new page
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The /update form page (updateServerHtml.h) starts with '<- Back to the web UI' (href ./); hidden while an update runs, shown again by the Back button after a failed run.
Evidence: AC1 tests/test_update_page_back_link.py: new 2/2 OK, old header 2 FAIL; update-orchestrator.test.mjs 58/58 pass. AC2 screenshots of the rendered page old vs new (scratchpad shots1534/update-old.png, update-new.png; static render, no device data). AC3 bench .61 alpha.582+443a3c3: GET /update contains the link (bench_check PASS).
Not verified: a phone browser. Picked up after Robert's 'gadoor' (label needs-maintainer-ok, reported by gruijter).
<!-- SECTION:FINAL_SUMMARY:END -->
