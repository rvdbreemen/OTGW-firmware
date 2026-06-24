---
id: TASK-923
title: >-
  feat(webui): store UI preference in settings.ini (revert localStorage)
  (TASK-908 P7)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-24 10:09'
updated_date: '2026-06-24 10:10'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 139000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer: preference must persist device-side in settings.ini, not per-browser localStorage. Revert TASK-922: toggle buttons POST ui_usev2 (settings.ui.bUseV2 -> settings.ini) and reload; remove the localStorage otgw-ui redirects. Serving driven by sendIndex/bUseV2 (default false=classic). Device-wide (shared across clients); needs firmware flash to take effect. Amends ADR-152.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Toggle persists choice in settings.ini via ui_usev2; no localStorage
- [ ] #2 localStorage redirects removed from index.html + v2.html
- [ ] #3 Default classic; serving via sendIndex/bUseV2
<!-- AC:END -->
