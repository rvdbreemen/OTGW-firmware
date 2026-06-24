---
id: TASK-923
title: >-
  feat(webui): store UI preference in settings.ini (revert localStorage)
  (TASK-908 P7)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 10:09'
updated_date: '2026-06-24 10:12'
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
- [x] #1 Toggle persists choice in settings.ini via ui_usev2; no localStorage
- [x] #2 localStorage redirects removed from index.html + v2.html
- [x] #3 Default classic; serving via sendIndex/bUseV2
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reverted TASK-922 localStorage: toggle now POSTs ui_usev2 -> settings.ui.bUseV2 -> /settings.ini (TASK-910 plumbing); localStorage redirects removed. Device-wide, survives reboot/reflash. Default classic via sendIndex. Code-complete + pushed (alpha.253); on-device activation needs the bUseV2 firmware flashed (old fw ignores ui_usev2). Amends ADR-152.
<!-- SECTION:FINAL_SUMMARY:END -->
