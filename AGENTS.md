
<!-- PROJECT VERSIONING GUIDELINES START -->
# Project Versioning

When asked to bump a prerelease version, keep the prerelease tag numeric and increment that number by one.
For release-relevant work, bump the prerelease number once for each relevant commit or coherent change set that should be reflected in the prerelease version.

Examples:

- `beta.13` becomes `beta.14`
- `alpha.3` becomes `alpha.4`
- `rc.1` becomes `rc.2`

Do not change the prerelease channel name unless the user explicitly asks for that. Update all matching version strings in the firmware version metadata so `_VERSION_PRERELEASE`, `_SEMVER_FULL`, `_SEMVER_NOBUILD`, and `_VERSION` stay consistent. If there are multiple relevant commits, apply the bumps cumulatively.

<!-- PROJECT VERSIONING GUIDELINES END -->

<!-- PROJECT BUILD GUIDELINES START -->
# Project Build Rules

When building release or validation artifacts, never build firmware-only. Any firmware build must also build the filesystem image in the same validation pass so firmware and LittleFS assets stay in sync.

Prefer the project build command that produces both artifacts. If using `build.py`, run the combined/default build or explicitly include both firmware and filesystem targets instead of `--firmware` alone.

<!-- PROJECT BUILD GUIDELINES END -->

<!-- BACKLOG.MD GUIDELINES START -->
<!-- DO NOT regenerate this block via `backlog agents --update-instructions`. -->
<!-- The full Backlog CLI reference is intentionally extracted to -->
<!-- .claude/backlog-cli-reference.md to keep this file focused. -->

# Task Management (MANDATORY)

**Every piece of work must have a backlog task before any code or project-file edits are made. No exceptions.**

# Backlog.md task management

All task operations go through the **`backlog` CLI** — never edit task files directly. The CLI owns file naming, frontmatter, AC indexing, and Git tracking; bypassing it desynchronises the project.

Full CLI reference: read `.claude/backlog-cli-reference.md` before any backlog operation.

## Task pickup (MANDATORY)

When Codex picks up a newly created or existing task, its first action before code, research, or project-file reading is:

```powershell
backlog task edit <id> -s "In Progress" -a '@codex'
```

Add an implementation plan only after the task is active and assigned. If Codex discovers in-flight work that was started without a task, stop further edits, create or pick up a follow-up task immediately, and document the gap plus the existing implementation and validation evidence there.

## Autonomous task completion

When every acceptance criterion and Definition-of-Done item is satisfied, required validation is green, implementation notes include branch and coding-agent metadata, and a final summary has been added, set the task status to **Done** immediately.

Leave a task at **In Progress** only when an acceptance criterion cannot be self-verified, a required validation or Definition-of-Done item is unmet, or coordinated sibling work requires the transition to wait. Record the blocker in the task notes or final summary.
<!-- BACKLOG.MD GUIDELINES END -->
