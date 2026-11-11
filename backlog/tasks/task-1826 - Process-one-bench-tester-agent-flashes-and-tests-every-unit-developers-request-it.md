---
id: TASK-1826
title: >-
  Process: one bench-tester agent flashes and tests every unit; developers
  request it
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T21:55:00Z'
updated_date: '2026-11-11T22:01:00Z'
labels:
  - process
  - docs
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 908000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer directive, Robert, 2026-11-11 (in session otgw-firmware-95): "otgw-cycle mag je otgw-bench-tester noemen. Deze agent managed alle hardware units aan de laptop. Hij mag flashen USB en OTA, en hij bepaald welke testen en validatie er draaien. Hij krijgt taken via de coordinatoren. Geen van de andere agents draaien zelf een flash / update van de firmware, maar vragen dat aan de otgw-bench-tester plus dat dan vervolgens testen worden gedaan via deze agent." And: "Het idee is dat we de developers lekker kunnen laten ontwikkelen, en dat er een andere agent is die flasht en test."

Decisions by Robert (AskUserQuestion, same session): the developer builds esp32-combo (under the one build lock) and hands the bench-tester the artifact and githash; the bench-tester records the bench evidence and ticks the bench ACs itself; the developer ticks host tests and build.

Today the rules tell every agent to OTA-flash and bench-test its own work: CLAUDE.md Build-flash-bench section, AGENTS.md per-task cadence, docs/guides/agent-build-and-bench.md, the implement-next-task, beta-release-2x and release-2x skills. This task rewrites those to the new flow and records the directive in docs/process/directive-changelog.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The AGENT-CORE block in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md has one identical bullet: only the bench-tester agent flashes (USB or OTA) and runs bench tests; other agents request it through the coordinator; developer builds, bench-tester ticks bench ACs
- [x] #2 CLAUDE.md, AGENTS.md and docs/guides/agent-build-and-bench.md describe the per-task flow as: developer tests, bumps, commits and builds; bench-tester flashes that githash, checks the device reports it, runs the tests it chooses, records evidence and ticks the bench ACs; developer pushes after the bench-tester's OK
- [x] #3 The implement-next-task, beta-release-2x and release-2x skills route their flash and bench steps to the bench-tester instead of the running agent
- [x] #4 docs/process/directive-changelog.md has a dated row for the directive and names what it replaces
- [x] #5 tests/test_adr_governance.py (AGENT-CORE parity) and python evaluate.py --quick show no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edits (one script, every replacement asserted to match exactly once): AGENT-CORE block in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md (Done-needs-evidence now says 'the session that produced it', plus a new 'Flashing and bench tests' bullet); CLAUDE.md Build-flash-bench; per-task cadence in AGENTS.md and docs/guides/agent-build-and-bench.md (incl. the rebase case: the bench-tester flashes the rebuilt zip before the post; developer does not rebuild in that worktree until the flash is confirmed); guide flash section; implement-next-task step 4 and per-task cadence; beta-release-2x Phase 3 step 2; release-2x Phase 4 step 2; docs/guides/bench-check.md; .claude/agents/otgw-device-verify.md description (bench-tester only); docs/process/directive-changelog.md (new 2026-11-11 row; 2026-10-30 cadence row amended, not replaced, since the cadence itself stays).
Evidence: python tests/test_agent_core_block.py OK; red check: dropping the agent name from the copilot copy only -> FAILED (failures=1), restored -> OK. tests/test_claude_md_core.py OK (CLAUDE.md 186 lines, cap 230). tests/test_adr_governance.py OK. python evaluate.py --quick: exit 0, 92 checks, 85 passed, 0 warnings, 0 failed, 7 info. AC 5 named test_adr_governance for AGENT-CORE parity; the parity test is tests/test_agent_core_block.py, both were run.
Not verified: no agent has run a flow under the new rule yet; the first real bench request through the coordinator is the field check.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rule in force: only otgw-bench-tester flashes (USB/OTA) and bench-tests; developers build and request a bench run via the coordinator with githash, absolute artifact path and behaviour; bench-tester ticks the bench ACs; developer pushes after its OK. AC1-3: edits listed in the notes, 10 files. AC4: directive-changelog row 2026-11-11. AC5: test_agent_core_block (red on a one-file drift, green restored), test_claude_md_core, test_adr_governance OK; evaluate --quick 0 failed.
<!-- SECTION:FINAL_SUMMARY:END -->
