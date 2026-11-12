---
id: TASK-1827
title: >-
  Agent rule: finish every lane (commit, push, update from remote, then remove
  own merged branches and worktrees), shared by CLAUDE.md, AGENTS.md and Copilot
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T22:47:00Z'
updated_date: '2026-11-12T10:40:00Z'
labels:
  - process
  - docs
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 913000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-11 ~23:40: 'Zorg je er wel voor dat alles netjes commit, push, en weer bijgewerkt wordt van remote terug. Verder moet je echt lokaal je worktrees ook opruimen en remote je lane branches, als je ze hebt laten samenvoegen.' and ~23:50: 'Zorg ervoor dat andere agents ook de regels weten rond opruimen en pushen van werk.' The cleanup that night found 16 dead wt-* directories, several merged local lane branches and one abandoned worktree. Put the rule in the shared AGENT-CORE block (CLAUDE.md, AGENTS.md, .github/copilot-instructions.md, changed together) and a 'Finish a lane' checklist in docs/guides/worktrees.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 AGENT-CORE block carries one line on finishing a lane, identical in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md; tests/test_adr_governance.py green
- [x] #2 docs/guides/worktrees.md has a Finish-a-lane checklist: fetch --prune, ff own tree, prove landed (git cherry), branch -d local and remote, worktree remove (submodule case), only own items, leftovers dir, never pull over another session's local commit
- [x] #3 Live sessions told via the coordinator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Rule line 'Finish every lane' added to the AGENT-CORE block, identical in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md (right after 'Versions and commits'); docs/guides/worktrees.md gets the 7-step 'Finish a lane' checklist with Robert's words. Gates: tests/test_agent_core_block.py 6 OK, tests/test_claude_md_core.py 5 OK, tests/test_adr_governance.py OK. Committed locally on lane-1815 during ba's push freeze; pushed after ba's post.

AC3: told the coordinator (with a request to relay to new sessions) and directly 09, 92, 95 and ba (2026-11-11 ~23:55), each with the 6-point short version; 95 and ba also got their own open items (lane-docs95 not yet on origin; ba's local main-tree commit labelled beta.183). Done after the push (waits for ba's post).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 'Finish every lane' line in the AGENT-CORE block of CLAUDE.md, AGENTS.md and .github/copilot-instructions.md (each 1 match on origin/dev, checked 2026-11-12 by git show origin/dev:<file> | grep -c); commit 987ba435c is an ancestor of origin/dev. tests/test_agent_core_block.py OK, tests/test_adr_governance.py OK (run on detached origin/dev e8af7d61a+, 2026-11-12). AC2: docs/guides/worktrees.md:15 '### Finish a lane (maintainer directive, 2026-11-11, TASK-1827)' with the 7-step checklist. AC3: coordinator plus 09, 92, 95, ba told 2026-11-11 ~23:55 (notes).
<!-- SECTION:FINAL_SUMMARY:END -->
