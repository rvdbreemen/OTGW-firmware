# Git hooks (OTGW dev)

This directory holds the project's git hooks. They are NOT active by default
in a fresh clone — git looks at `.git/hooks/` unless told otherwise. Activate
once per clone:

```
git config core.hooksPath .githooks
```

## Hooks

### `pre-commit`

Two passes:

1. **adr-kit gate** — runs `bin/adr-judge` against the staged diff to enforce
   `## Enforcement` blocks in Accepted ADRs. Degrades gracefully (logs and
   continues) when `adr-judge` is not installed. See `.claude/adr-kit-guide.md`.
   Bypass: `ADR_KIT_HOOK_DISABLE=1 git commit ...`

2. **bump-prerelease check** (TASK-560) — if the staged set includes paths
   under `src/OTGW-firmware/` (excluding `version.h` itself) or
   `src/libraries/`, the same commit must change `_VERSION_PRERELEASE` in
   `src/OTGW-firmware/version.h` (both a `+` and a `-` line on that
   `#define`). Run `bin/bump-prerelease.sh` and stage `version.h` +
   `data/version.hash` alongside your code change.
   Bypass: `OTGW_BUMP_HOOK_DISABLE=1 git commit ...`

### `commit-msg` (TASK-659)

Two passes:

1. **Task reference required for firmware/docs commits** — if any staged path
   matches `src/OTGW-firmware/**`, `src/libraries/**`, `docs/**`, or
   `backlog/**`, the commit subject or body must reference at least one
   `TASK-NNN` identifier OR carry one of these exemption tags:

   - `[no-task]` anywhere in the message
   - Subject prefix `chore(housekeeping):`, `chore(release):`,
     `chore(meta):`, or `chore(daily-report):`

2. **Referenced tasks must have a record on disk** — every `TASK-NNN` named in
   the message must have a matching `backlog/tasks/task-NNN-*.md` file
   tracked or staged in this commit. Prevents commit messages from referring
   to backlog entries that never made it into git.

   Bypass: `OTGW_TASK_HOOK_DISABLE=1 git commit ...`
   or: `git commit --no-verify` (skips ALL hooks).

## Why hooks live in `.githooks/` (not `.git/hooks/`)

`.git/hooks/` is a per-clone path that git creates fresh on every clone and
never tracks. Hooks committed under `.githooks/` are versioned with the
codebase — every contributor gets the same enforcement rules after the
one-time `git config core.hooksPath .githooks`.
