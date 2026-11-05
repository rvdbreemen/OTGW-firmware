---
id: TASK-1631
title: Slim CLAUDE.md to a core with on-demand rules
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:42:00Z'
updated_date: '2026-11-05T13:03:00Z'
labels:
  - process
  - agentic
dependencies:
  - TASK-1625
priority: medium
ordinal: 758000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F4. CLAUDE.md has 673 lines with 14 dated maintainer directives and is loaded in full in every session (with AGENTS.md about 120 KB). Some rules replace earlier ones but the old text stays (build cadence 2026-10-29 -> 10-30, SAT question rhythm 10-24 -> 10-25). This costs context on every turn and invites conflicting readings.
Plan checkpoint: the split is a maintainer choice. Present the proposed core and the moved sections to Robert with AskUserQuestion before moving anything.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert approved the split (core vs on-demand) in the session
- [x] #2 CLAUDE.md core is at most about 200 lines: roles, hard prohibitions, gates, where to find the rest
- [x] #3 Domain rules (SAT ground truth, build/bench cadence, release, Discord) live in .claude/rules/ or skills and are referenced from the core
- [x] #4 Superseded directives are removed from the running text and listed in a dated directive changelog
- [x] #5 The TASK-1625 drift test and tests/test_adr_governance.py still pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05, Robert (AskUserQuestion): deferred until a quiet moment after the beta phase. Options offered: a light clean-up pass (superseded directives to a changelog, ~690 to ~550 lines) or a full split into path-scoped .claude/rules (~200-250 line core). Facts at the time: CLAUDE.md 690 lines, 70 KB; largest blocks test automation 61, navigation 47, versioning 41, build 39, ADR guidelines 37; the ADR list and SAT ground truth blocks must stay (drift tests), and the AGENT-CORE test (TASK-1625) already guards the shared rules. Before a split, verify that this Claude Code version loads .claude/rules/*.md with a paths: frontmatter only for matching files.

2026-11-05 Robert approved (AskUserQuestion) the split 'Kern + rules + guides': core ~200 lines (AGENT-CORE, tasks and evidence, prohibitions, gates, push policy, binding ADR list, index); .claude/rules/sat-ground-truth.md and discord.md without paths (always loaded); firmware-coding.md and navigation.md path-scoped to src/** and docs/c4/**; build/flash, test automation, worktrees and versioning to docs/guides with a 3-5 line summary in the core; superseded directives to docs/process/directive-changelog.md. AGENTS.md stays complete for Codex. Source for rule loading: code.claude.com/docs/en/memory (rules without paths load at launch; path rules load on Read/Write/Edit of a matching file).

Implementation 2026-11-05: split by script from verbatim slices of origin/dev CLAUDE.md (d08cd47a6). New core CLAUDE.md 184 lines, about 21 KB (was 690 lines, 69 KB). Always-loaded rules: .claude/rules/sat-ground-truth.md (the SAT-GROUND-TRUTH block verbatim) and .claude/rules/discord.md (MCP servers plus the alpha-tag protocol). Path-scoped rules: firmware-coding.md (src/**, test/**, safeboot/**) and navigation.md (src/**, docs/c4/**, other-projects/**). Guides: docs/guides/agent-build-and-bench.md, versioning-policy.md, worktrees.md. Changelog: docs/process/directive-changelog.md. AGENTS.md stays complete for Codex; its SAT block and the AGENT-CORE sentence changed in step with the rule and core files. Test changes: tests/test_sat_ground_truth_block.py now reads the SAT block from the rule file and the navigation row from navigation.md. New tests/test_claude_md_core.py covers the core budget (230 lines), pointer existence, the frontmatter of the always-loaded and scoped rules, and the guarded blocks. Red on the old CLAUDE.md (3 of 5 fail: 690 lines, missing pointers, SAT block still in the core), green on the new one. No-loss check: every non-blank line of the old CLAUDE.md was compared against the new files. The 90 lines without an exact match are the condensed Tasks/Overview/ADR-kit/graphify sections and the superseded clauses; review restored three items: the release-CI line, the adr.coding/adr.review instructions with the four gate names, and the build announcement plus freeze rule. Live references were repointed in the implement-next-task skill, the backlog_discord command, scripts/alpha_ledger.py and 4 guides.

Rule-loading evidence (headless claude -p --model haiku in the lane worktree). Without a tool call, the question whether the 'Firmware coding rules' section is loaded answers NO. After a Read of src/OTGW-firmware/version.h it answers YES, so path-scoped loading works on this Claude Code version. The always-loaded rule is quoted from its first sentence without any tool call ('This rule file is loaded in every session (no paths: frontmatter).'). The existing .claude/rules/openwolf.md (no paths) also appears in session context. Known gap: Bash-only access (grep, sed) does not trigger path-scoped rules, so the core tells agents to read firmware-coding.md themselves when they plan without opening a source file.

Tests: drift tests test_claude_md_core, test_agent_core_block, test_sat_ground_truth_block and test_adr_governance exit 0 after the final edit. Full suite --no-host on the lane: 139 files, 135 passed, 3 failed, 1 skipped. test_sat_lowload_contract fails LL-021 (classifier_parity skip) and LL-023 (no UPSTREAM_COMMIT marker); both are identical on a clean origin/dev worktree at d08cd47a6. test_capture_tool is flaky on both trees. test_refresh_storm fails only under the -j 4 load and passes solo.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CLAUDE.md is now a 184-line core (was 690 lines, 69 KB; now about 21 KB) with roles, tasks and evidence, prohibitions, gates, push policy, the binding-ADR list and an index. AC1: Robert chose 'Kern + rules + guides' (AskUserQuestion, 2026-11-05). AC2: 184 lines, guarded by the new tests/test_claude_md_core.py (red 3 of 5 on the old file, green on the new one). AC3: SAT ground truth and Discord/alpha-tag rules are in always-loaded .claude/rules; firmware coding and navigation are in path-scoped rules; build/bench, versioning and worktrees are in docs/guides. Loading was verified headless: the path rule is absent without a Read (NO) and present after a src/ Read (YES); the always-loaded rule is quoted without tools. AC4: superseded directives are removed from the running text and listed in docs/process/directive-changelog.md; AGENTS.md keeps its own full text for Codex, apart from the SAT block and the AGENT-CORE sentence. AC5: test_agent_core_block, test_adr_governance and test_sat_ground_truth_block (now reading the rule files) exit 0; full suite 135/139, with the remaining failures pre-existing or flaky on clean origin/dev. A no-loss line check was done against the old file. Not verified: how agents behave with the new structure over several sessions; Bash-only work does not load the path-scoped rules.
<!-- SECTION:FINAL_SUMMARY:END -->
