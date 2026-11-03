---
id: TASK-1398
title: 'v2 Settings: explain the orange REBOOT badge (looks clickable, no explanation)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T19:49:00Z'
updated_date: '2026-11-03T05:44:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 537000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563987220115890257): on Settings > Network an orange 'REBOOT' badge shows without explanation and looks like a button but is not clickable. It marks reboot-sensitive categories (v2.js:2446, :2469). Option: a tooltip/aria-label 'Changes here take effect after a reboot' and a non-button style.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Badge has a tooltip and does not look clickable (screenshot)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: badge without title/aria (v2.js:2486/2509). Doable.

Released by Robert 2026-11-03 (interactive, maintainer): low priority. Only the badge changes (tooltip/aria-label + non-button style); Settings rail and card panels stay as designed.
<!-- SECTION:NOTES:END -->
