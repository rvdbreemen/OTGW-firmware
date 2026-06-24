---
id: TASK-910
title: 'feat(webui): v2 bundle skeleton + device-wide toggle setting (TASK-908 P0b)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-24 04:32'
updated_date: '2026-06-24 04:40'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 126000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
settings.ui.bUseV2 + sendIndex switch + /v2.html,/v2.css,/v2.js routes + toggle in both UIs. Skeleton v2 shell loads, themes, toggles back. Reuses settings POST path (no new REST route).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bUseV2 persists; root serves chosen UI; toggle round-trips; both builds green; ADR proposed
<!-- AC:END -->
