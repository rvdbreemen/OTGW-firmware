---
id: TASK-1548
title: 'Docs: /update-docs-2x skill for the 2.x line'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T17:34:00Z'
updated_date: '2026-11-01T17:48:00Z'
labels:
  - docs
  - skills
dependencies: []
ordinal: 702000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01: make a 2.x skill based on the 1.x /update-docs. 2.x differences: previous tag must be the last v2.* tag (releases/latest is v1.7.7), dev source file map (SAT*.ino, MQTTHaDiscovery.cpp, v2.* assets, Platform lib, safeboot), release docs in docs/releases/, backlog CLI not MCP, beta vs stable mode, release mode runs inside /release-2x on release/2.x, no OTGW32 in release-facing text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SKILL.md at .claude/skills/update-docs-2x/ with scope detection, backlog-task plan, sequential subagents, beta and stable release mode, cleanup, commit rules
- [x] #2 Scope-detection commands dry-run on dev against v2.0.0-beta.1 with output recorded
- [x] #3 1.x /update-docs marked 1.x only; /release-2x, /beta-release-2x and CLAUDE.md Release flows point to /update-docs-2x
- [x] #4 No em dashes in the new skill outside grep patterns; governance test green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Dry-runs of scripts/docs-scope-2x.sh on dev (HEAD 8a8b52095): default base resolves v2.0.0-beta.1 (6 commits, 0 areas, exit 0); --base v1.7.7 refuses with exit 2 (gh release view returns v1.7.7, the 1.x skill's base); --mode stable reports first-2.x-stable fallback to v2.0.0-beta.1; --base HEAD~200: 12 areas, 36 banner-only files skipped, 8 unmapped dev-tooling scripts listed. First run had a bug: IFS='|' split the regex alternations (grep: Unmatched '('), fixed by using @ as table separator. Manual findings: OTGW32 mentions 1-22 per EN chapter, ESP8266/Wemos 1-39 (ch08 39, ch02 27, ch10 23); left for Robert. Em-dash scan of new/changed text: only inside grep patterns. tests/test_adr_governance.py: 15 tests OK.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
New /update-docs-2x skill (.claude/skills/update-docs-2x/SKILL.md) plus scripts/docs-scope-2x.sh for Phase 1. Differences from the 1.x skill: base = last v2.* tag merged into HEAD (refuses v1.*), dev source map with banner-only filter and unmapped list, backlog CLI, release docs only in docs/releases/, beta vs stable mode, release mode runs inside /beta-release-2x or /release-2x without commit or cleanup, selective staging, OTGW32/ESP8266 manual findings reported to Robert. 1.x /update-docs marked 1.x only; /release-2x Phase 2 step 4 and /beta-release-2x Phase 1 step 5 and the CLAUDE.md Release flows block point to it. Evidence: dry-runs and governance test in the notes. Not verified: a full run with subagents writing docs.
<!-- SECTION:FINAL_SUMMARY:END -->
