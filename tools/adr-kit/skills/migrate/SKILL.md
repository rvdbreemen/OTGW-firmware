---
name: migrate
description: Guided rewrite of legacy-shaped ADRs into the canonical-seven-section template enforced by /adr-kit:lint. Promotes inline status / date lines to a ## Status heading, promotes nested Alternatives subsections to top-level ## Alternatives Considered, renames ## Related to ## Related Decisions, and splits external references into ## References. Read-then-confirm: always shows the proposed restructure before applying. Respects per-ADR markers and template.required_sections from .adr-kit.json.
argument-hint: "[file or directory; defaults to docs/adr/]"
disable-model-invocation: true
allowed-tools: [Read, Edit, Glob, Grep]
---

# adr-kit migrate

You are running `/adr-kit:migrate`. The user wants to rewrite one or more legacy-shaped ADRs into the canonical template that `/adr-kit:lint` enforces. This is a write skill: you propose targeted structural edits, the user confirms, then you apply them.

## Inputs

- **No argument**: migrate ADRs in `docs/adr/` (the whole tree).
- **A directory path**: migrate ADRs under that directory.
- **A file path**: migrate that one file.

If the path does not exist or contains no ADR files, say so plainly and stop.

## Cardinal rules

1. **Read-then-confirm.** Always read every target file before proposing edits. Always show the proposed restructure (file by file, summarised) and ask for explicit confirmation before calling Edit. Silent writes are forbidden.
2. **No fabrication.** If a canonical section is missing AND the source contains no inline content that maps to it, do NOT invent content. Leave a `<!-- TODO: populate -->` placeholder so a human author can fill it in later.
3. **Preserve content.** The migration restructures shape, not substance. Bullet points, prose, examples, and links from the source must appear in the target unchanged. Reorder; do not rewrite.
4. **Idempotent.** Running migrate on an already-canonical ADR is a no-op (the skill detects "no missing sections" and skips the file).
5. **Skip files that opt out.** A file with `<!-- adr-kit-lint: skip -->` is left untouched. A file with `<!-- adr-kit-lint: advisory -->` gets a warning ("this file is currently in advisory mode; migrating will make the marker meaningless") and a confirmation prompt.

## Workflow

### Step 1: read the policy

Look for `docs/adr/.adr-kit.json` (relative to project root, or the directory passed). If `template.required_sections` is set, target *that* list. Otherwise target the canonical seven:

- `## Status`
- `## Context`
- `## Decision`
- `## Alternatives Considered`
- `## Consequences`
- `## Related Decisions`
- `## References`

Order matters. The migration must end with sections in the configured order.

### Step 2: classify each input file

For each file, determine:

- **Already canonical** (no missing sections, sections in the right order) -> skip, report as "no changes needed".
- **Marker says skip** -> skip, report as "skipped per marker".
- **Marker says advisory** -> ask "advisory marker is on this file; migrate anyway?"
- **Migratable** -> identify which patterns apply (see next section).

### Step 3: identify patterns

The skill body below documents the patterns observed in real-world legacy ADRs. Apply each pattern that fits, in this order. Do not invent new transformations.

#### Pattern A: inline status promotion

Source:

```markdown
# ADR-NNN Title

**Status:** Accepted
**Date:** 2026-04-25
**Supersedes:** ADR-XXX (optional)
```

Target:

```markdown
# ADR-NNN Title

## Status

Accepted, 2026-04-25. Supersedes ADR-XXX (optional).
```

If only `**Status:**` exists with no `**Date:**`, the Status section reads `Accepted` (without date).

#### Pattern B: alternatives inside Context

Source has a `### Alternatives considered:` (or `### Alternatives considered and rejected`) heading nested inside `## Context`. Target: lift the entire block out, change `### ` to `## `, and place the new top-level `## Alternatives Considered` heading between `## Decision` and `## Consequences`. Preserve content verbatim; only the heading level and position change.

#### Pattern C: alternatives inside Consequences

Same as Pattern B but the source nests alternatives inside `## Consequences`. Target: same restructure, place between Decision and Consequences.

#### Pattern D: Related to Related Decisions + References split

Source: a `## Related` section that mixes ADR/TASK references with file paths, PR links, vendor doc URLs, internal docs.

Target: rename to `## Related Decisions`. Move pure-external references (files, URLs, PRs that are not ADR or TASK identifiers) to a new `## References` section that follows. Keep ADR-NNN and TASK-NNN entries in `## Related Decisions`.

Heuristic for splitting:
- Lines starting with `ADR-`, `TASK-`, or referencing those identifiers -> `## Related Decisions`.
- Lines starting with backticks (file path), URL, or `PR `/`Issue ` references -> `## References`.
- When ambiguous, leave in `## Related Decisions` (safer default).

#### Pattern E: missing References with no inline content

If after Pattern D there are no external references to populate `## References`, create the section with a placeholder:

```markdown
## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->
```

Never invent references. The placeholder makes it clear to a human reviewer that this is a known gap.

#### Pattern F: missing Alternatives Considered with no source

If the source legitimately has no alternatives discussion anywhere, create the section with a placeholder:

```markdown
## Alternatives Considered

<!-- TODO: document at least 2 alternatives that were considered and rejected, with reasoning. -->
```

This is a real gap a human should fill, but the skill must not fabricate.

### Step 4: present the plan

Before applying any edit, show the user a per-file summary:

```
Proposed migration plan (3 files):

ADR-007-timer-based-task-scheduling.md
  Pattern A: inline `**Status:**` -> `## Status` heading
  Pattern D: `## Related` -> `## Related Decisions` + new `## References`

ADR-029-simple-xhr-ota-flash.md
  Pattern A: inline `**Status:**` -> `## Status` heading
  Pattern F: missing `## Alternatives Considered`, will create with TODO placeholder

ADR-058-nonblocking-pic-command-response.md
  Pattern A: inline `**Status:**` -> `## Status` heading
  Pattern F: missing `## Alternatives Considered`, will create with TODO placeholder
  Pattern E: missing `## References`, will create with TODO placeholder
  No `## Related Decisions` content found in source; will create with `- None.`

Confirm to apply (y/n)?
```

If the user declines, stop without writing.

### Step 5: apply edits

After confirmation, apply each transformation via Edit. One Edit per logical change, so the diff is reviewable. Report what was changed per file.

### Step 6: post-migration verification

After all edits, suggest the user run `/adr-kit:lint <path>` to confirm the migrated files now PASS strictly. Do NOT run lint yourself: that is a separate skill the user invokes.

## What you do not do

- You do not modify the body content of sections. Headings move; prose stays.
- You do not auto-fabricate Alternatives Considered or References content. Use TODO placeholders.
- You do not rename ADR files (Consistency-gate filename FAILs). Out of scope.
- You do not edit ADRs that already PASS strict.
- You do not invoke `/adr-kit:lint` after migration. The user decides when to verify.

## Reporting format

Single-file migration:

```
ADR-007-timer-based-task-scheduling.md migrated.
  Applied: Pattern A (Status promotion), Pattern D (Related split).
  Run /adr-kit:lint on this file to verify.
```

Directory migration:

```
Migrated 3 of 80 candidate files. 1 already canonical (skipped). 76 deferred (no patterns matched, manual review needed).

Applied:
  ADR-007 (A, D)
  ADR-029 (A, F)
  ADR-058 (A, F, E, no Related content)

Skipped:
  ADR-022 (already canonical)

Deferred (manual review): 76 files.
  Reason: complex shape that did not match any of patterns A through F. Inspect by hand.

Run /adr-kit:lint docs/adr/ to confirm overall result.
```

The aggregate's bottom line tells the user one concrete next step (run lint), never invents a status the migration did not actually achieve.

## Edge cases

- **Multiple inline metadata lines**: source has `**Status:**`, `**Date:**`, `**Supersedes:**`, `**Amended by:**`, etc. Fold all into the new `## Status` section as a comma-separated sentence. Order: Status, date, supersedes/amended.
- **Empty Related section**: source has `## Related` with no body or only whitespace. Target: `## Related Decisions` with `- None.` body.
- **Anchor comments inline at top**: source has a `Renumbered from ADR-XXX ...` line before `**Date:**`. Fold into the new `## Status` section as a trailing sentence: "Renumbered from ADR-XXX on YYYY-MM-DD to resolve duplicate numbering. Content unchanged."
- **Body has Markdown that confuses heading-detection**: e.g. a `## ` inside a fenced code block. The skill treats only headings *outside* code fences as canonical sections. If unsure, ask the user.
- **Source uses `## Pros and Cons` or `## Decision drivers`**: do not rename these; they are project-specific. The migration concerns the canonical-required sections only. The user can address custom subsections in a follow-up pass.

## Anti-patterns to refuse

If the migration would require any of these, refuse and explain:

- Renaming ADR files. Out of scope; surfaces commit / cross-ref breakage.
- Modifying body prose ("the Decision section reads better if reorganised..."). Restructure shape only.
- Fabricating Alternatives or References. TODO placeholder is the answer.
- Skipping the user confirmation. Read-then-confirm is non-negotiable.
- Running `/adr-kit:lint` automatically after migration. The user decides.
