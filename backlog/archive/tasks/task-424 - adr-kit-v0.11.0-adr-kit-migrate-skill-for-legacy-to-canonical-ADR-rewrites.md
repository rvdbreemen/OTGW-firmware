---
id: TASK-424
title: 'adr-kit v0.11.0: /adr-kit:migrate skill for legacy-to-canonical ADR rewrites'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 21:12'
updated_date: '2026-04-25 21:16'
labels:
  - adr-kit
  - external-repo
  - feature
  - tier-1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why

The v0.9.0 lint with grandfathering lets projects coexist with legacy-shaped ADRs by treating them as ADVISORY rather than FAIL. That solves the noise problem at the read side. It does not solve the underlying drift: an established project may *want* to bring its legacy ADRs into the canonical-seven-section shape over time, but doing it by hand on 80+ files is tedious and error-prone.

`/adr-kit:migrate` is a guided rewrite skill. The user invokes it; Claude reads each legacy ADR, identifies which canonical sections are missing, proposes targeted structural edits (promote inline `**Status:**` to `## Status` heading; promote `### Alternatives considered` to top-level `## Alternatives Considered`; rename `## Related` -> `## Related Decisions`; split off file/URL refs into `## References`), and applies them after user confirmation.

This was the manual work I did today on ADR-082 through ADR-087. Repeatable as a skill.

## Scope

**In scope:**

1. `skills/migrate/SKILL.md` with frontmatter `name: migrate`, `disable-model-invocation: true`, `allowed-tools: [Read, Edit, Glob, Grep]`, `argument-hint: "[file or directory; defaults to docs/adr/]"`.
2. Skill body documents the migration patterns observed in real-world legacy ADRs:
   - Pattern A: inline `**Status:** ...` -> `## Status` heading at top.
   - Pattern B: `### Alternatives considered:` inside Context -> `## Alternatives Considered` between Decision and Consequences.
   - Pattern C: `### Alternatives considered and rejected` inside Consequences -> `## Alternatives Considered` before Consequences.
   - Pattern D: `## Related` -> `## Related Decisions`, with external refs (file paths, PRs, URLs, arbitrary docs) split into a new `## References` section.
   - Pattern E: missing `## References` with no source content -> create section with `<!-- TODO: populate from inline citations -->` placeholder, never fabricate.
3. Read-only-by-default operation: skill always shows the proposed restructure first (read + plan), then asks for explicit confirmation, then applies via Edit. Never silent writes.
4. Respects existing markers: files with `<!-- adr-kit-lint: skip -->` are skipped entirely; files with `<!-- adr-kit-lint: advisory -->` get a warning before migration (the marker becomes meaningless once the file is canonical-shaped).
5. Reads `docs/adr/.adr-kit.json` for `template.required_sections`: if the project codifies a different template, migrate respects that target.
6. Skips files that already PASS strict (no missing sections) -- nothing to do.
7. Reports: per file the proposed transformation; aggregate at the end.
8. README "What it does" section gains a fourth bullet for migrate. README Quickstart gains a step for "Migrate legacy ADRs".
9. `.github/workflows/validate.yml` required-files set extended to include `skills/migrate/SKILL.md`.
10. plugin.json + marketplace.json bump to 0.11.0; CHANGELOG entry; tag `adr-kit--v0.11.0`; GitHub Release with --latest; mirror sync to OTGW-firmware/tools/adr-kit/.

**Out of scope:**

- A deterministic `bin/adr-migrate` Python CLI. Migration is judgement-heavy; same reasoning that put Evidence and Clarity gates as opt-in for `bin/adr-lint`.
- Auto-fabrication of Alternatives Considered content where none exists in the source. The skill leaves such cases as `<!-- TODO -->` placeholders for the human author.
- Filename renaming (Consistency-gate FAILs). That is a separate concern with cascading impact (git history, cross-refs).
- Reverting a migration (one-way operation; revert via git).

## Acceptance criteria (set via task_edit)

(separately due to the known XML-bug on initial create)

## Real-world smoke test

After release: invoke `/adr-kit:migrate docs/adr/ADR-001-esp8266-platform-selection.md` against this project's first ADR. Expected: skill proposes promoting inline `**Status:**` to a `## Status` heading and renaming `## Related` to `## Related Decisions` (or whatever the actual diff is). After user confirmation, ADR-001 lints PASS strict instead of ADVISORY. Without invoking the skill, no files change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 skills/migrate/SKILL.md exists with frontmatter name=migrate, disable-model-invocation=true, allowed-tools=[Read,Edit,Glob,Grep], argument-hint
- [x] #2 Skill body documents Patterns A-F (Status promotion, Alternatives lift, Related/References split, TODO placeholders for genuine gaps)
- [x] #3 Skill workflow is read-then-confirm: always prints plan, asks confirmation, then applies; never silent writes
- [x] #4 Skill respects per-ADR markers (skip / skip <gates> / advisory) and .adr-kit.json template.required_sections
- [x] #5 Skill is idempotent: no-op on already-canonical ADRs
- [x] #6 Skill never fabricates content; missing Alternatives/References get TODO placeholder
- [x] #7 README What-it-does section mentions migrate (and updates lint/CLI roles)
- [x] #8 .github/workflows/validate.yml required-files set includes skills/migrate/SKILL.md
- [x] #9 plugin.json 0.11.0, marketplace.json plugin entry 0.11.0, CHANGELOG [0.11.0] with link reference
- [x] #10 Tag adr-kit--v0.11.0 + GitHub Release with --latest, release notes file in .github/
- [x] #11 OTGW-firmware tools/adr-kit/ mirror synced byte-for-byte
- [x] #12 All edits English, em-dash-free, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-424 -- adr-kit v0.11.0 published

`/adr-kit:migrate` skill ships, closing the loop opened by v0.9.0's scoped lint with grandfathering. v0.9.0 lets a project tolerate legacy shape; v0.11.0 lets the project fix it.

### Skill design

- **User-only invocable** (`disable-model-invocation: true`); same discipline as setup. Never auto-fired.
- **Read-then-confirm**: skill always prints a per-file plan, asks for explicit user confirmation, then applies via Edit. Silent writes are forbidden.
- **No fabrication**: missing Alternatives or References get `<!-- TODO: ... -->` placeholders, never invented content.
- **Idempotent**: running migrate on an already-canonical ADR is a no-op.
- **Marker-aware**: respects `<!-- adr-kit-lint: skip -->` (skipped) and `<!-- adr-kit-lint: advisory -->` (warns first).
- **Template-aware**: respects `template.required_sections` from `.adr-kit.json`.

### Six transformation patterns

| Pattern | Source | Target |
|---|---|---|
| A | inline `**Status:** ...`, `**Date:** ...`, `**Supersedes:** ...` | `## Status` heading with combined sentence |
| B | `### Alternatives considered` inside Context | top-level `## Alternatives Considered` between Decision and Consequences |
| C | `### Alternatives considered and rejected` inside Consequences | top-level `## Alternatives Considered` before Consequences |
| D | `## Related` (mixed ADR + external refs) | `## Related Decisions` (ADR/TASK) + new `## References` (files/URLs/PRs) |
| E | missing `## References`, no source content | section with TODO placeholder |
| F | missing `## Alternatives Considered`, no source discussion | section with TODO placeholder |

### Out of scope (intentional)

- Filename renaming (Consistency-FAILs); use `<!-- adr-kit-lint: skip consistency -->` marker or rename by hand.
- Body-prose rewriting; restructure shape only.
- Auto-running `/adr-kit:lint` after migration; user decides when to verify.
- Deterministic Python CLI variant (`bin/adr-migrate`); migration is judgement-heavy.

### Release outputs

- Commit: `0fb29dc chore(release): v0.11.0 (/adr-kit:migrate skill for legacy-to-canonical rewrites)` (7 files; +335 insertions)
- Tag: `adr-kit--v0.11.0` pushed to origin
- GitHub Release: <https://github.com/rvdbreemen/adr-kit/releases/tag/adr-kit--v0.11.0> (marked --latest)
- Mirror: `tools/adr-kit/` byte-equivalent in OTGW-firmware

### Future work (deferred)

- README slash-commands reference section (queued as next request from user during this task).
- `/adr-kit:migrate --whole-tree` policy mode that fixes a defined batch in one go.
<!-- SECTION:FINAL_SUMMARY:END -->
