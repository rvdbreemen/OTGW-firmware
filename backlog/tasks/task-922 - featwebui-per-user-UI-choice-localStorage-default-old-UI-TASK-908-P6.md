---
id: TASK-922
title: 'feat(webui): per-user UI choice (localStorage), default old UI (TASK-908 P6)'
status: Done
assignee: []
created_date: '2026-06-24 07:55'
updated_date: '2026-06-24 08:09'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 138000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Toggle remembers the v2-vs-classic choice PER BROWSER via localStorage otgw-ui, overriding the device-wide bUseV2 default. Default (no choice) = classic. index.html redirects to /v2.html when otgw-ui=v2; v2.html redirects to /index.html when otgw-ui=classic (explicit files, no loop). Toggle buttons set localStorage (not the device-wide setting). Amends ADR-152.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Per-user localStorage otgw-ui drives the choice; default old UI
- [ ] #2 Redirects use explicit files (no loop with bUseV2 default)
- [ ] #3 Both toggle buttons set localStorage; verified on device
<!-- AC:END -->
