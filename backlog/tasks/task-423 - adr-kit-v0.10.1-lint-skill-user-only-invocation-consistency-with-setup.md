---
id: TASK-423
title: 'adr-kit v0.10.1: lint skill user-only invocation (consistency with setup)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 21:03'
updated_date: '2026-04-25 21:05'
labels:
  - adr-kit
  - external-repo
  - patch
  - ux
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why

`skills/lint/SKILL.md` lacks `disable-model-invocation: true` while `skills/setup/SKILL.md` has it. Effect in Claude Code's autocomplete: `/adr-kit:setup` appears with the plugin-prefix form (correct, user-only), but `/lint` appears at the root namespace with an `(adr-kit)` tag (auto-invocable by Claude, can be user-typed without prefix).

Two consequences:

1. **Inconsistent UX within one plugin.** Users see `/adr-kit:setup` with prefix and `/lint` without; the pattern is unclear.
2. **Auto-invocation risk on a deliberate-action skill.** Lint is meant as a user-triggered checking tool, not something Claude should auto-fire when interpreting a vague request like "check my ADRs". Without `disable-model-invocation`, Claude could trigger it without explicit user intent.

The same reasoning that put `disable-model-invocation: true` on the setup skill applies to lint: deliberate user action, not background helper.

## Scope

One-line frontmatter change in `skills/lint/SKILL.md`. Plus version bump (0.10.0 -> 0.10.1) + CHANGELOG entry + tag + GitHub release + mirror sync. Patch release.

## Out of scope

- Changing the skill body (no behaviour change, only invocation discipline).
- Re-running pytest fixtures (frontmatter is not exercised by the tests).
- Touching other skills.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 skills/lint/SKILL.md frontmatter contains disable-model-invocation: true
- [x] #2 plugin.json version 0.10.1, marketplace.json plugin entry 0.10.1, CHANGELOG [0.10.1] entry with link reference
- [x] #3 Tag adr-kit--v0.10.1 + GitHub Release published with --latest, release notes file in .github/
- [x] #4 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte
- [x] #5 All edits English, em-dash-free, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-423 -- adr-kit v0.10.1 published

One-line frontmatter fix to align lint skill invocation discipline with setup skill.

### Change

`skills/lint/SKILL.md` frontmatter gains `disable-model-invocation: true`. Result: only `/adr-kit:lint` is registered in Claude Code's autocomplete; the root-namespace `/lint` form is gone. Matches `/adr-kit:setup` which already had this discipline.

### Design pattern that emerges

| Skill | Type | `disable-model-invocation` | Forms |
|---|---|---|---|
| `adr` | knowledge / guide | no | `/adr` + `/adr-kit:adr` (auto + user) |
| `lint` | deliberate check | yes (after v0.10.1) | `/adr-kit:lint` (user only) |
| `setup` | one-time write | yes | `/adr-kit:setup` (user only) |

Rule: knowledge / reference skills stay auto-invocable; write actions and checks are user-only. The `adr` skill keeps auto-invocation on purpose because it benefits from Claude triggering it when context calls for it.

### Out of scope (intentional)

- No skill body change. The lint logic is identical to v0.10.0.
- No `bin/adr-lint` change. The CLI is independent of the skill's invocation discipline.
- The 15 pytest tests remain green; they exercise the CLI, not the skill frontmatter.

### Release outputs

- Commit: `a3d4e8f chore(release): v0.10.1 (lint skill user-only invocation)` (5 files; +64 insertions)
- Tag: `adr-kit--v0.10.1` pushed to origin
- GitHub Release: <https://github.com/rvdbreemen/adr-kit/releases/tag/adr-kit--v0.10.1> (marked --latest)
- Mirror: `tools/adr-kit/` byte-equivalent in OTGW-firmware

### Note on `/adr` working as root form

User asked during this task whether `/adr` (root) also works alongside `/adr-kit:adr` (prefix). Answer: yes, already does, because `skills/adr/SKILL.md` lacks `disable-model-invocation` (intentional: knowledge skill). No change needed.
<!-- SECTION:FINAL_SUMMARY:END -->
