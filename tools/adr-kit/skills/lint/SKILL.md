---
name: lint
description: Lints existing Architecture Decision Records against the four verification gates (Completeness, Evidence, Clarity, Consistency). Run on a single ADR file or on the whole docs/adr/ tree. Reports pass/fail per gate per file with file:line citations for failures. Read-only.
argument-hint: "[file or directory; defaults to docs/adr/]"
allowed-tools: [Read, Glob, Grep]
---

# adr-kit lint

You are running `/adr-kit:lint`. The user wants to know whether existing ADRs in their project pass the four verification gates that the main `adr` skill enforces. You read files; you do not modify them. You report.

## Inputs

- **No argument**: lint every `docs/adr/ADR-*.md` file in the project root.
- **A directory path**: lint every `ADR-*.md` file under that directory.
- **A file path**: lint that one file.

If the user passed something else (a glob, a non-existent path), explain politely and stop.

## What you check, gate by gate

For every ADR you read, evaluate each of the four gates separately. A gate either passes or fails for a given file. Cite line numbers when reporting a failure.

### Gate 1: Completeness

A passing ADR has every load-bearing section present, in the canonical order, with non-empty content.

Required sections (heading text, in this order):

- `## Status`
- `## Context`
- `## Decision`
- `## Alternatives Considered`
- `## Consequences`
- `## Related Decisions`
- `## References`

Sub-checks within sections:

- **Status** line is one of: `Proposed`, `Accepted`, `Deprecated`, `Superseded by ADR-XXX`, `Amended by ADR-XXX`. Date in `YYYY-MM-DD` format must be present.
- **Alternatives Considered** has at least 2 alternatives, each with rejection reasoning. A single bullet point of one word does not count as an alternative.
- **Consequences** has both a positive and a negative direction. Look for `Benefits`/`Trade-offs`/`Positive`/`Negative` subheadings or a clearly mixed list. A one-sided Consequences section fails this gate.
- **Risks** are named with at least one mitigation each (typically inside Consequences as `Risks and mitigations` or similar).

Failure example: `Completeness: missing ## Risks and mitigations subsection (line 47); only positive consequences listed.`

### Gate 2: Evidence

A passing ADR replaces challengeable claims with measurements or citations.

Heuristic checks:

- **Bare adjectives** in critical positions (Decision, Consequences, Alternatives) without an accompanying number or citation. Examples to flag: "fast", "slow", "faster", "slower", "better", "worse", "improves", "reduces", "more reliable", "more performant" when not followed by a measurement or citation.
- **Constraint claims** ("X has Y RAM", "Y supports up to Z connections") without an anchor (vendor doc, datasheet, measured, internal doc, RFC).
- **Code references**: when a claim is about code, look for `file:line` or `file.ext` references. A claim about an algorithm that does not cite the implementation is suspicious.
- **External claims** (specs, RFCs, vendor docs): should be linked, not paraphrased.

You will see false positives ("the new system is faster" might be acceptable in a Context paragraph if a measurement appears later in Consequences). Use judgment: if a measurement or citation backs the claim within ~5 lines, count it as supported.

Failure example: `Evidence: line 24 says "improves performance" with no measurement in the surrounding paragraphs. Suggest replacing with a measured improvement, e.g. "reduces request latency from 120 ms to 40 ms p99 (load-test on staging, see PERF-103)".`

### Gate 3: Clarity

A passing ADR is readable to a developer who has never seen the codebase.

Heuristic checks:

- **Acronyms**: any ALL-CAPS sequence of 2 to 6 letters that appears for the first time without an inline expansion. Example: "TLS" should be "TLS (Transport Layer Security)" on first use.
- **Project-specific jargon** without explanation: terms like internal service names, internal status codes, or proprietary protocols.
- **Decision summary at top**: the Decision section should open with a single declarative sentence. If the Decision opens with a multi-paragraph discussion before stating what was chosen, flag it.
- **Code examples** for technical decisions (algorithm choice, protocol selection): expected to be present in either Decision or Implementation Notes if applicable.

Failure example: `Clarity: line 12 uses "WAL" without expansion. Suggest "WAL (write-ahead log)" on first use. Line 31 opens the Decision section with "There are several considerations..." instead of a declarative sentence; suggest leading with the choice and unpacking after.`

### Gate 4: Consistency

A passing ADR fits the existing record without contradictions.

Heuristic checks:

- **Filename matches heading**: file `ADR-042-foo.md` should have a first-line heading `# ADR-042 ...`. Mismatch fails this gate.
- **Filename pattern**: `ADR-XXX-kebab-case-title.md` with uppercase prefix and 3-digit zero-padded number. Lowercase prefixes (`adr-`), unpadded numbers (`ADR-42-`), or non-kebab-case (`ADR-042-MyTitle.md`) fail.
- **Cross-references** in `## Related Decisions` follow the `ADR-XXX` format. References to non-existent files (broken links) fail.
- **Duplicate ADR numbers** in the directory: if two files share an ADR number, both fail the Consistency gate. Recommend a renumbering follow-up.
- **Status conflicts**: if this ADR's Status says "Supersedes ADR-XXX" but ADR-XXX's Status is not "Superseded by", flag the inconsistency.

Failure example: `Consistency: filename ADR-042-foo.md but heading says "# ADR-42 Foo" (missing zero-padding, line 1). Filename and heading must agree.`

## Output format

For each ADR file, produce a one-line summary plus details for any failing gate.

### Single-file output

```
ADR-042-foo.md
  Completeness: PASS
  Evidence:     FAIL
    line 24: "improves performance" with no measurement.
    line 38: "scales better" with no number.
  Clarity:      PASS
  Consistency:  PASS

Summary: 3 of 4 gates pass. To flip Status to Accepted, fix the Evidence gate.
```

### Directory-tree output

```
Linting docs/adr/ (12 ADRs)

PASS (9):
  ADR-001-foo.md
  ADR-003-bar.md
  ...

FAIL (3):
  ADR-002-baz.md
    Evidence: line 24 ("faster"), line 38 ("scales better").
  ADR-007-qux.md
    Completeness: missing ## Related Decisions section.
  ADR-011-quux.md
    Consistency: filename has lowercase prefix "adr-011-...", expected "ADR-011-...".

Aggregate:
  Most common failing gate: Evidence (2 of 3 failures).
  Next steps: see CONTRIBUTING.md (or the SKILL.md) for what each gate asks for.
```

## When you find no ADRs

If `docs/adr/` is empty or does not exist, say so plainly:

```
No ADRs found at docs/adr/. The toolkit is installed but no decisions have been recorded yet.
Create your first ADR with /adr-kit:adr.
```

## Reporting

Always end with a single recommended next action: which ADR to fix first (the one with the most gate failures, or the most-referenced one), and which gate to focus on. Concrete, not abstract.

## What you do *not* do

- You do not modify ADRs. Read-only by design.
- You do not invoke the `adr-generator` agent. The lint skill is informational; the user decides whether to fix.
- You do not call out style preferences that are not in the four gates. No bikeshedding on heading capitalisation, list-marker style, or whitespace. The gates are the contract.
