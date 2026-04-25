# ADR Rules for Coding Work

When you are about to implement or change code, run through these rules **before** writing.

## Before implementing

- **Check for existing ADRs**: scan `docs/adr/` for an ADR that governs the area you are about to touch. The fastest path: read `docs/adr/README.md` index, then open the relevant ADRs.
- **Assess architectural significance**: does the change affect any of the following?
  - Architecture: service or module boundaries, deployment topology, integration patterns
  - Non-functional requirements: security, availability, performance, privacy, resilience
  - Interfaces and contracts: public APIs, message topics, RPC contracts, breaking changes
  - Dependencies: new framework, new library, swap-out with broad impact
  - Build and tooling: build system, CI gates, development workflow

  If yes, an ADR is in scope. If clearly no (bug fix, refactor that preserves contract, doc fix), skip the ADR work.
- **Use the canonical guide**: `docs/adr/README.md` is the source of truth for the project's ADR conventions. The ADR `SKILL.md` (in `.claude/skills/adr/`, `.cursor/skills/adr/`, or `.github/skills/adr/` depending on your tool) is the comprehensive how-to.

## Creating a new ADR

If no ADR exists and the change is architecturally significant:

1. **Hand the work to the ADR generator subagent** (`adr-generator`). Provide it with the title, decision, alternatives, constraints, and any related ADRs. The agent writes a fully-formed ADR file at `docs/adr/ADR-XXX-title.md`.
2. **Review the generated ADR yourself** before linking from code: confirm it passes the four verification gates (Completeness, Evidence, Clarity, Consistency) defined in the skill.
3. **Reference the ADR** in your commit message and (if relevant) the PR description: `ADR: docs/adr/ADR-XXX-title.md`.
4. **Add code comments** that point to the ADR at non-obvious enforcement sites: `// See ADR-XXX for why we use this pattern.`

## Superseding an existing ADR

If your change reverses or replaces an `Accepted` ADR:

1. **Do not modify the accepted ADR's decision text or reasoning.** ADRs are immutable once `Accepted`, except for the Status line.
2. **Write a new superseding ADR** that explains: (a) what is changing, (b) why the original choice no longer fits, (c) what the new choice is, (d) the migration path if consumers are affected.
3. **Update the old ADR's Status line only**: `Superseded by ADR-YYY`. Rationale lives in the new ADR.
4. **Cross-link** in both ADRs' `## Related Decisions` sections.

## Amending (vs superseding)

Some ADRs have a stable contract but a narrow scope-change. The toolkit distinguishes:

- **Supersede**: the old decision is no longer in force. Status line of old ADR: `Superseded by ADR-YYY`.
- **Amend**: the old decision still stands, but a specific aspect is narrowed or extended. Status line of old ADR: `Amended by ADR-YYY` (optional: leave `Accepted` if amend is content-additive only).

Use amend when the existing ADR's other consumers should not have to change. Use supersede when the contract itself is replaced.

## Implementation checklist

For every commit that lands an architecturally significant change:

- [ ] An ADR exists or has been created via the adr-generator agent
- [ ] The ADR follows the project structure in `docs/adr/README.md`
- [ ] At least 2 alternatives are documented with rejection reasoning
- [ ] Consequences include both positive and negative impacts; risks have mitigations
- [ ] The four verification gates from the skill all pass
- [ ] Commit message and PR description link to the ADR
- [ ] If superseding, the old ADR's Status line is updated and cross-references are in place
- [ ] Code comments point at the ADR at non-obvious enforcement sites

## Definition of Done

- ADR exists, is readable, and is reachable from `docs/adr/README.md`
- ADR focuses on rationale ("why"), not implementation detail ("how")
- The PR or commit links to the ADR by path
- Supersession or amendment chain is correct (if applicable)
- ADR has no placeholders, no `TODO:` markers, no em dashes

## When the ADR rule does not apply

Skip the ADR work for:

- Bug fixes that do not change a contract
- Refactors that preserve external behaviour
- Documentation updates (unless the doc *is* an ADR)
- Configuration tweaks within an existing range
- Minor feature additions that fit an established pattern

If you are unsure, default to creating an ADR. The cost of writing one is 15 to 30 minutes; the cost of re-deriving the rationale 6 months from now is hours. (See the Anti-Rationalization Guards table in `SKILL.md`.)
