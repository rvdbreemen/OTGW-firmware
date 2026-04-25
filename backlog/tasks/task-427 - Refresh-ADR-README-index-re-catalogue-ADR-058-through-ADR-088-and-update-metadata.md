---
id: TASK-427
title: >-
  Refresh ADR README index: re-catalogue ADR-058 through ADR-088 and update
  metadata
status: To Do
assignee: []
created_date: '2026-04-25 22:14'
labels:
  - docs
  - adr-hygiene
dependencies: []
references:
  - docs/adr/README.md
  - .adr-kit.json
  - docs/adr/ADR-080-binding-adr-rules-must-have-a-ci-gate.md
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR index in `docs/adr/README.md` is significantly out of date. It claims 11 categories totalling 57 ADRs (counts: 2+5+6+8+10+2+3+6+9+4+2 = 57), but the actual repository contains 87 accepted ADRs (88 once TASK-426 lands). Roughly 30 ADRs from ADR-058 through ADR-087 have no index entry, the Decision Timeline stops at ADR-060, and the "Most Referenced ADRs" reference counts have not been recomputed since.

Additionally the "ADR Skill" section still points to the GitHub Copilot skill at `.github/skills/adr/SKILL.md` and `USAGE_GUIDE.md`, while the project has migrated to adr-kit via plugin (per `.adr-kit.json`, plugin cache at `~/.claude/plugins/cache/rvdbreemen-adr-kit/...`). This is a second stale-pointer layer in the same file.

This refresh is documentation hygiene only: no architectural decisions are being made, no ADR contents are being changed. The task closes the gap between the ADR corpus and its navigation hub so future readers (humans and AI agents) can use the README the way the adr-kit skill description prescribes ("maintain README.md as the navigation hub").

Discovered while writing TASK-426 (ADR-088 status-burst windowing) on 2026-04-26.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 README.md ADR Index section includes one-line summary entries for ADR-058 through ADR-088 placed in their appropriate categories; categories are added or split where the existing 11 no longer fit (e.g. SAT, OTGW32/ESP32, OT-Direct may need their own sub-sections)
- [ ] #2 All Quick Navigation category counts match the actual number of ADR entries in the corresponding section after the refresh
- [ ] #3 Decision Timeline extended from ADR-060 through ADR-088 with year buckets matching the ADR Date fields
- [ ] #4 ADR Skill section rewritten to reference adr-kit via plugin (skill: /adr-kit:adr; subagent: adr-generator) and removes references to the deprecated GitHub Copilot skill at .github/skills/adr/
- [ ] #5 Architectural Dependencies and 'Most Referenced ADRs' counts re-verified against current ADR cross-references, or replaced with a note that they are advisory rather than hand-counted
- [ ] #6 No ADR file content changes — this task is index hygiene only
- [ ] #7 grep -c '\*\*\[ADR-' docs/adr/README.md equals 88 (or matches the count of accepted ADR files), proving every ADR is indexed exactly once
<!-- AC:END -->
