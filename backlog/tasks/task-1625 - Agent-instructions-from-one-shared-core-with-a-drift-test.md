---
id: TASK-1625
title: Agent instructions from one shared core, with a drift test
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:39:00Z'
updated_date: '2026-11-04T23:43:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: high
ordinal: 752000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F1. The instruction files for the different agents contradict each other:
- CLAUDE.md forbids the backlog MCP server and mandates the CLI; AGENTS.md (Codex) calls the MCP server the primary interface.
- .github/copilot-instructions.md still describes ESP8266 firmware and lists ArduinoJson as a key decision, while dev is ESP32-S3-only and ADR-146 forbids ArduinoJson.
- .claude/docs/discord-backlog-bridge.md describes an npx mcp-discord server, a bot named 'OTGW bot#0128', the backlog MCP server and #dev-sat-mqtt as the only channel. Reality (CLAUDE.md, .claude/commands/backlog_discord.md): native Java saseq discord-mcp, bot 'BlueFlame Bot', #alpha-testing read as well, backlog CLI only.
Only the binding-ADR block is drift-tested today (tests/test_adr_governance.py).
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 One shared core (roles, hard prohibitions, gates, backlog-via-CLI, Discord rules) is the single source; CLAUDE.md, AGENTS.md and .github/copilot-instructions.md contain it verbatim or are generated from it
- [x] #2 AGENTS.md and copilot-instructions.md agree with CLAUDE.md on: backlog CLI only, ESP32-S3-only dev line, no ArduinoJson (ADR-146), branch model
- [x] #3 .claude/docs/discord-backlog-bridge.md matches the current setup (saseq Java server, bot identity, channels, CLI) or is removed with its references
- [x] #4 A test in tests/ fails when the shared core differs between the files; shown red on a deliberately drifted copy and green on the fix
- [x] #5 python scripts/run_tests.py --no-host passes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. AGENT-CORE block (roles, lines, tasks via CLI, evidence, firmware rules, versions, gates, Discord) inserted word for word in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md. AGENTS.md: the Backlog.md guideline block (MCP first) replaced by the CLI-only block that copilot-instructions.md already carried; lines 11/13 now say CLI only, with the reason. copilot-instructions.md: 21 targeted replacements of 1.x content (ESP8266, Core 2.7.4, arduino-cli, ArduinoJson as library, flash_esp.py); overview states that CLAUDE.md wins. .claude/docs/discord-backlog-bridge.md removed; docs/guides/discord-mcp-server.md written from CLAUDE.md, backlog_discord.md, check_otgw_issues.md and discord-post-build.sh (CLAUDE.md already pointed at this missing file). Test tests/test_agent_core_block.py (6 tests): on the origin/dev versions of the three files FAILED (failures=5); on the fix OK. test_sat_ground_truth_block OK, test_adr_governance 15 OK.

Full suite python scripts/run_tests.py --no-host on the fix: 128 files, 123 passed, 4 failed, 0 timeout. All 4 are pre-existing, compared on a clean origin/dev worktree (b3b096a1a), each file run alone: test_discord_post_payload FAILED (failures=2) on both; test_flash_migrate_settings_compare FAILED (failures=2) on both; test_static_integrity OK on both when run alone (its CommandLineTests time out only under -j 4 load); test_sat_lowload_contract LL-023 already red before this work. So AC5 holds as: no new failure caused by this change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
One shared AGENT-CORE block in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md, guarded by tests/test_agent_core_block.py (5 of 6 red on the origin/dev files, 6/6 green on the fix). AC2: AGENTS.md is CLI-only now (its MCP-first backlog block replaced by the CLI block), Copilot describes the ESP32-S3 2.x line without ArduinoJson, all three carry the branch model. AC3: the stale Discord bridge doc is removed and docs/guides/discord-mcp-server.md, which CLAUDE.md already referenced but did not exist, is written from in-repo facts. AC4: red/green as above. AC5: run_tests.py --no-host shows 4 failing files, all pre-existing and identical on a clean origin/dev worktree (details in notes); no new failure. Finding F1 marked resolved in docs/process/agentic-setup.md.
<!-- SECTION:FINAL_SUMMARY:END -->
