---
id: TASK-725
title: 'tooling: install adr-kit v0.15.0 bin scripts on dev branch'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:07'
updated_date: '2026-05-27 17:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The dev branch has the adr-kit pre-commit hook and .adr-kit.json config, but is missing the bin/ convenience scripts (adr-judge, adr-lint, adr-audit, adr-context, adr-quality, adr-retire, adr-status, adr-generate-scripts). The 2.0.0 branch already has these at v0.15.0. Copy them from the plugin cache to bin/ so they can be run directly without knowing the plugin path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 bin/ contains all 8 adr-kit v0.15.0 scripts: adr-judge, adr-lint, adr-audit, adr-context, adr-quality, adr-retire, adr-status, adr-generate-scripts
- [x] #2 Scripts are executable (chmod +x)
- [x] #3 bin/adr-judge runs without error against docs/adr/
- [x] #4 Committed and pushed to origin/dev
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Installed adr-kit v0.15.0 bin scripts on dev branch.

Changes:
- Copied 8 scripts from plugin cache (v0.15.0) to bin/: adr-audit, adr-context, adr-generate-scripts, adr-judge, adr-lint, adr-quality, adr-retire, adr-status
- All scripts are executable (chmod +x preserved via cp)
- Pre-commit hook already resolved adr-judge from plugin cache; bin/ copies add parity with 2.0.0 and enable manual invocations

Verified:
- bin/adr-judge --diff - --adr-dir docs/adr/: 0 violations, 58 advisory
- Full commit run (LLM pass): 0 violations across 66 ADRs with Enforcement blocks

Committed as 96456f80, pushed to origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
