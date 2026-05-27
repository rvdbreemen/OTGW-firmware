---
id: TASK-725
title: 'tooling: install adr-kit v0.15.0 bin scripts on dev branch'
status: To Do
assignee: []
created_date: '2026-05-27 17:07'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The dev branch has the adr-kit pre-commit hook and .adr-kit.json config, but is missing the bin/ convenience scripts (adr-judge, adr-lint, adr-audit, adr-context, adr-quality, adr-retire, adr-status, adr-generate-scripts). The 2.0.0 branch already has these at v0.15.0. Copy them from the plugin cache to bin/ so they can be run directly without knowing the plugin path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/ contains all 8 adr-kit v0.15.0 scripts: adr-judge, adr-lint, adr-audit, adr-context, adr-quality, adr-retire, adr-status, adr-generate-scripts
- [ ] #2 Scripts are executable (chmod +x)
- [ ] #3 bin/adr-judge runs without error against docs/adr/
- [ ] #4 Committed and pushed to origin/dev
<!-- AC:END -->
