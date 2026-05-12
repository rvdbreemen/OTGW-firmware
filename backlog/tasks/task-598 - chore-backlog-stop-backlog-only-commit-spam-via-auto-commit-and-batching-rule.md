---
id: TASK-598
title: 'chore(backlog): stop backlog-only commit spam (auto_commit off + batching rule + hook warn)'
status: To Do
assignee: []
created_date: '2026-05-11 17:05'
updated_date: '2026-05-11 17:05'
labels:
  - tech-debt
  - tooling
  - automation
  - backlog
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
60% of the last 100 commits on `dev` are pure backlog admin (47 × `Update task TASK-N` + 13 × `Create task TASK-N`), each a 1-file/1-8-line edit in `backlog/tasks/`. Evidence: `docs/reviews/2026-05-11_last-100-commits-multi-perspective/REVIEW.md` section C2.

Root cause: `backlog/config.yml` has `auto_commit: true`, so every `backlog task edit` CLI invocation produces a commit. With autonomous task progression (assign → plan → AC checks → notes → final summary → Done) a single feature can produce 5+ admin commits surrounding the one real code commit. `git log --oneline` becomes unreadable and bisect is wasteful.

Automation requirement: don't rely on operator discipline ("remember to batch"). The fix must be enforced by config + hook + documented policy so the spam cannot recur once enabled.

Three layers:
1. **Config** — flip `backlog/config.yml` `auto_commit: false` so backlog CLI edits stay unstaged until the operator explicitly commits.
2. **Hook** — `.githooks/pre-commit` adds a non-blocking warning when the staged set is *exclusively* `backlog/tasks/*`. Surfaces the smell without blocking legitimate end-of-day rollups.
3. **Policy** — `CLAUDE.md` "Task Management" gets a batching rule: stage the code change AND the matching task-file edits in one commit; backlog-only commits are reserved for end-of-day rollups and must use the `chore(backlog):` prefix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria

<!-- AC:BEGIN -->
- [ ] #1 `backlog/config.yml` `auto_commit:` is set to `false`. Verify with `grep '^auto_commit' backlog/config.yml`.
- [ ] #2 `.githooks/pre-commit` is extended with a non-blocking warning when *all* staged paths match `^backlog/tasks/`. The warning must:
  - print to stderr (does not affect exit code, never blocks a commit)
  - include the list of triggering paths
  - mention the `chore(backlog):` rollup pattern as the acceptable form
  - skip silently when `OTGW_BACKLOG_WARN_DISABLE=1` is set (escape hatch for explicit rollups)
- [ ] #3 `CLAUDE.md` `## Task Management (MANDATORY)` section adds a "Commit batching" subsection that states: backlog edits must ride in the same commit as the code change they describe; standalone backlog-only commits are limited to end-of-day rollups prefixed `chore(backlog):`; the pre-commit warning is the enforcement surface.
- [ ] #4 A representative end-to-end flow (create task → plan → code edits → AC checks → final summary → Done) produces **one** combined commit, verified by `git log --oneline -5` showing one code commit, not five.
- [ ] #5 Optional helper `bin/backlog-rollup.sh`: stages every modified `backlog/tasks/*.md` and produces a single `chore(backlog): roll up TASK-* updates` commit with the touched task IDs in the body. Skip if not needed; preferred path is in-place batching per AC3.
- [ ] #6 Cross-worktree mirror task opened on 2.0.0 if `backlog/config.yml` exists there too.

<!-- AC:END -->

## Definition of Done

<!-- DOD:BEGIN -->
- [ ] #1 ACs above all checked
- [ ] #2 The hook warning has been exercised at least once (commit only `backlog/tasks/foo.md` and capture the stderr in Final Summary)
- [ ] #3 `CLAUDE.md` change committed in the same PR
- [ ] #4 No code-path changes (only config + hook + docs) — `python build.py --firmware` should be unaffected; run it once to confirm no accidental regression
- [ ] #5 Final Summary captures the before/after admin-commit ratio target (was 60/100; target ≤15/100 over the next 100 commits)

<!-- DOD:END -->

## References

- `docs/reviews/2026-05-11_last-100-commits-multi-perspective/REVIEW.md` section C2 + section 9 process recommendation 1
- `backlog/config.yml:9` `auto_commit: true`
- `.githooks/pre-commit` (insert warning between adr-judge gate and bump-check; or as a separate post-everything block)
- `CLAUDE.md` `## Task Management (MANDATORY)` (line 7) — primary edit location
