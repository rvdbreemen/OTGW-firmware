---
id: TASK-1829
title: >-
  Agent rule: questions to Robert only by Discord DM, never as an interactive
  CLI question (asynchronous in the session when DM is unavailable)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:32:00Z'
updated_date: '2026-11-11T23:42:00Z'
labels:
  - process
  - docs
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 914000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12 ~00:10 (session d3): 'Vragen die je wilt stellen aan mij mogen niet meer interactief via de CLI, maar mag je via DM aan mij voorleggen. Communiceer deze nieuwe regel over hoe je mij kan benaderen met al je agents, in het algemeen geldt deze regel. Want als je stopt in een CLI sessie leg je de voortgang vast, voortaan mag je zulke vragen dus alleen via Discord DM communiceren of als dat kanaal er niet meer is, dan asynchroon in de CLI sessie.' Replaces the 2026-10-29 directive 'Open questions go to Robert, interactively' (CLAUDE.md, AGENTS.md) and the AskUserQuestion checkpoints in the release and implement-next-task skills.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 AGENT-CORE Roles line (CLAUDE.md, AGENTS.md, Copilot) says: ask by Discord DM, never an interactive CLI question; tests/test_agent_core_block.py and test_claude_md_core.py green
- [x] #2 The 'Open questions go to Robert' principle in CLAUDE.md and AGENTS.md is rewritten to DM-first, asynchronous in the session as the fallback; the AskUserQuestion checkpoints in release-2x, beta-release-2x and implement-next-task point to the DM route
- [x] #3 All live sessions told; the hard-stop reading put to Robert asynchronously
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AGENT-CORE Roles line in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md: decisions by Discord DM via the coordinator, never an interactive CLI question, asynchronous in the session without a DM channel. 'Open questions go to Robert' rewritten in CLAUDE.md and AGENTS.md with Robert's quote. Skills: implement-next-task (needs-maintainer-ok release question by DM), beta-release-2x (new waivers etc. by DM), release-2x CHECKPOINT 1 (a release is a hard stop: asked asynchronously in the session, not by DM, not interactive). docs/process/agentic-setup.md diagram and channel table updated. ADRs and dated handoff docs that record past AskUserQuestion answers stay (history). Gates: test_agent_core_block 6 OK, test_claude_md_core 5 OK, test_adr_governance 17 OK.

Hard stops: Robert answered B by DM (msg 1570221009511125012, 2026-11-11 23:37:41 UTC, author number3nl 384411356616720384; read first-hand by d3 via the Discord MCP) to the coordinator's question 1570220793160532081: hard stops may be confirmed by DM too. Safeguard confirmed back to him (bot msg 1570221589985763459): only an explicit yes for that one action, as a reply to a DM that names the action and its target; a general 'go on' does not count. Added to the AGENT-CORE Roles line (3 files), the 'Open questions' principle (CLAUDE.md, AGENTS.md) and release-2x CHECKPOINT 1. All live sessions were told the base rule (coordinator, 09, 92, 95, ba). Gates green.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: AGENT-CORE Roles line identical in CLAUDE.md, AGENTS.md, Copilot: questions by Discord DM via the coordinator, never interactive; async in the session without DM; hard stops only with an explicit per-action yes to a DM naming action and target. test_agent_core_block 6 OK, test_claude_md_core 5 OK. AC2: 'Open questions go to Robert' rewritten in CLAUDE.md/AGENTS.md (Robert's quote, DM 1570221009511125012 for hard stops); release-2x, beta-release-2x, implement-next-task checkpoints point to the DM route; docs/process/agentic-setup.md updated; governance 17 OK. AC3: coordinator + 09, 92, 95, ba told; hard-stop reading put to Robert by DM, answered B.
<!-- SECTION:FINAL_SUMMARY:END -->
