---
id: TASK-976
title: Reusable on-device verify subagent + helper script (cut session token cost)
status: Done
assignee: []
created_date: '2026-07-01 16:56'
updated_date: '2026-07-01 17:01'
labels: []
dependencies: []
ordinal: 188000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Token-cost analysis (last 24h): cache-read dominated (848M tokens, ~$1.3-2.5k) because one 1310-turn session carried ~594K context every turn; 93% of turns >200K (premium tier). Individual tool outputs were negligible (35K tokens total). Biggest lever after /clear: delegate the on-device verify loop (buildfs -> flash -> Playwright -> summarise) to a SUBAGENT so the verbose build log + multi-KB esptool progress dump + Playwright output stay in the subagent's context, not the main thread. Deliverables: (1) scripts/device-verify.sh — build LittleFS (+ optional firmware, collision-safe), flash to the bench device over USB, confirm boot; prints a COMPACT summary only (suppresses build log + esptool progress dump). (2) .claude/agents/otgw-device-verify.md — a Sonnet subagent that runs the script + a task-specific Playwright check and returns only a structured PASS/FAIL + key values + console-error count.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/device-verify.sh builds+flashes+confirms boot with compact output (no esptool progress dump, no full build log)
- [x] #2 .claude/agents/otgw-device-verify.md defines a device-verify subagent (model sonnet) that returns a compact summary
- [x] #3 Verified: running the script against .39 prints <=8 lines and the device comes up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
scripts/device-verify.sh: tested against .39 -> 4 lines output (v2.js OK / buildfs SUCCESS / flash OK 1 img / device UP alpha.308), exit 0. Suppresses the PlatformIO build log + esptool progress dump (the ~9KB-per-flash noise). Collision-safe --firmware path (buildfs -> stash littlefs.bin -> build firmware -> flash both). Fixed a Windows charmap crash by exporting PYTHONIOENCODING=utf-8 (esptool's Unicode progress bar). .claude/agents/otgw-device-verify.md: Sonnet subagent, tools Bash/Read/Write/Glob/Grep, output contract = compact RESULT/build-flash/checks/console-errors/restored, explicitly forbids pasting raw logs. NOTE: Claude Code discovers agents at session START, so the agent is NOT invocable in the session that created it — available next session. Script is usable immediately.
<!-- SECTION:NOTES:END -->
