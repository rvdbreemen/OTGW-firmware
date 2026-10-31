---
id: TASK-1534
title: 'Flash utility page (/update): no way to cancel and go back to the web UI'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T20:34:00Z'
updated_date: '2026-10-31T20:47:00Z'
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
- [ ] #1 The flash utility page has a visible 'Back to the web UI' link/button that returns to / without starting anything
- [ ] #2 Browser test or screenshot before/after (synthetic data)
- [ ] #3 Bench unit serves the new page
<!-- AC:END -->
