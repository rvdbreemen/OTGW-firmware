---
id: TASK-1625
title: Agent instructions from one shared core, with a drift test
status: To Do
assignee: []
created_date: '2026-11-04T22:39:00Z'
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
- [ ] #1 One shared core (roles, hard prohibitions, gates, backlog-via-CLI, Discord rules) is the single source; CLAUDE.md, AGENTS.md and .github/copilot-instructions.md contain it verbatim or are generated from it
- [ ] #2 AGENTS.md and copilot-instructions.md agree with CLAUDE.md on: backlog CLI only, ESP32-S3-only dev line, no ArduinoJson (ADR-146), branch model
- [ ] #3 .claude/docs/discord-backlog-bridge.md matches the current setup (saseq Java server, bot identity, channels, CLI) or is removed with its references
- [ ] #4 A test in tests/ fails when the shared core differs between the files; shown red on a deliberately drifted copy and green on the fix
- [ ] #5 python scripts/run_tests.py --no-host passes
<!-- AC:END -->
