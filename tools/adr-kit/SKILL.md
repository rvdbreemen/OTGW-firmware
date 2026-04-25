---
name: adr
description: 'Architecture Decision Record (ADR) management skill. Creates, maintains, and enforces architectural decisions with anti-rationalization guards and named verification gates. Drop into any project to give an AI coding agent a shared, enforceable ADR workflow.'
license: MIT
---

# ADR Skill: Architecture Decision Record Management

## Overview

This skill enables systematic creation, maintenance, and enforcement of Architecture Decision Records. ADRs document significant architectural choices along with their context, alternatives considered, and consequences. They serve as living documentation so current and future developers (and AI agents) understand why the system is built the way it is.

The skill bundles three patterns that distinguish it from a basic ADR template:

1. **Anti-Rationalization Guards**: a table of excuses agents and humans use to skip writing an ADR, paired with counter-arguments. Pre-flight discipline.
2. **Verification Gates**: four named gates an ADR must pass before its Status flips from `Proposed` to `Accepted`. Reviewers can block on a single named gate.
3. **Human Decision Documentation**: explicit `Decision Maker:` attribution so user-driven choices are visible in the record, distinct from agent-generated ADRs.

---

## When to Use

### Automatic triggers

Use this skill automatically when:

- Reviewing a PR or analysing code changes for architectural impact.
- A change introduces a new pattern, dependency, or contract.
- Refactoring proposals affect system design.
- A user asks an architectural question ("why did we choose X?", "should we use Y or Z?").

### Explicit triggers

Use this skill when the user mentions:

- "Create an ADR" / "Document this decision" / "Architecture decision".
- "Why did we choose...?" / "What alternatives did we consider?".
- "Document my choice" / "Capture this trade-off".

### Decision triggers

Create a new ADR when making a decision that:

- Has long-term impact on architecture.
- Affects multiple components or modules.
- Involves trade-offs between alternatives.
- Constrains future development choices.
- Addresses a significant technical challenge.
- Changes existing architectural patterns.
- Requires a human decision that should be preserved.

### Do NOT create an ADR for

- Bug fixes that do not change architecture.
- Refactors that maintain the same external behaviour.
- Configuration changes within an existing range.
- Documentation updates (unless the doc *is* an ADR).
- Minor feature additions within established patterns.
- Temporary workarounds or experiments.

---

## Anti-Rationalization Guards

ADRs fail not from lack of templates but from the agent (or human) talking themselves out of writing one. Below is a table of common excuses with their counter-arguments. When you hear yourself reaching for one of the left-column entries, stop and apply the right-column response.

| Excuse | Counter-argument |
|---|---|
| "This decision is obvious" | Obvious to you, today, with the context fresh in your head. Not obvious to a maintainer in six months, an open-source contributor, or an AI agent reviewing the PR. Document the *why*. |
| "I'll document it later" | You will not. The pattern goes in once and only comes out if a future bug forces it out. Cost of writing now: 15 to 30 minutes. Cost of re-deriving later: hours, sometimes a full incident. |
| "The code speaks for itself" | Code shows *what* and *how*. ADRs document *why*. The why is never derivable from source: it lives in the constraints, the alternatives that were rejected, and the trade-offs accepted. |
| "Everyone knows this pattern" | Everyone on the team *today*. Not the maintainer in three years. Not the contributor who shows up next week. Not the model running a future code review. |
| "This is just the framework default" | Then say so explicitly. "Status: Accepted. We took the framework default for X because Y." Defaults embed assumptions; an ADR makes those assumptions inspectable. |
| "It is too small to need an ADR" | Small *architecturally*? Then no ADR. Small *implementation* of a contract change? Still an ADR, because the contract is what future developers will trip over. |
| "We can always change it later" | Every architectural choice constrains the next one. Document the chosen path now so a future change is a *deliberate* supersede, not an accidental drift. |
| "I don't have time" | Pay 30 minutes now or pay hours when someone challenges the choice in code review six months from now and nobody can remember the rationale. |
| "There are no real alternatives" | Almost never true. If you cannot name one, the alternative is "do nothing" or "the framework default": that is an alternative. Document it and the reason it lost. |

This pattern is adapted from [addyosmani/agent-skills](https://github.com/addyosmani/agent-skills).

---

## Initial Codebase Analysis

### First-time use: discovering undocumented decisions

When introducing this skill to an existing codebase, perform a comprehensive architectural analysis to identify and document existing but undocumented decisions.

#### Workflow

**Step 1: identify architectural patterns.** Areas to analyse:

- Platform and runtime choices (language, framework, deployment target).
- Data and storage (database, schema, persistence patterns).
- Protocol and integration (sync vs async, REST vs RPC, message bus).
- Memory and resource management (when load-bearing).
- Network and security (TLS posture, auth model, trust boundaries).
- Build and tooling (build system, CI gates, test strategy).
- Cross-cutting concerns (logging, error handling, observability).

**Step 2: ask critical questions.** For each pattern discovered:

- WHY was this approach chosen? (context, constraints).
- WHAT alternatives exist? (at least 2 to 3 viable options).
- WHY were alternatives rejected? (specific technical reasons).
- WHAT are the consequences? (benefits, costs, risks).
- HOW is this implemented? (code references, key files).
- WHEN was this decided? (estimate from git log or release history if unknown).

**Step 3: generate ADRs systematically.** For each undocumented decision:

1. Use a research subagent to understand the pattern.
2. Review code, comments, git history for context.
3. Identify constraints (memory, performance, compatibility, security).
4. Research alternatives even if obvious.
5. Document consequences: positive AND negative.
6. Create ADR with `Status: Accepted` (since already implemented).
7. Link to actual implementation (files, commits).

**Step 4: prioritize.** Start with foundational decisions that:

- Affect multiple components.
- Constrain future choices.
- Are non-obvious or counterintuitive.
- Have significant trade-offs.
- Are frequently questioned.

#### Trigger phrases

```
"Analyze this codebase to identify undocumented architectural decisions"
"Generate ADRs for existing architectural patterns in this codebase"
"What architectural decisions should be documented in this project?"
```

---

## ADR Principles

### The golden rules

1. **One decision per ADR.** Each ADR captures a single architectural choice.
2. **Immutable history.** Never modify accepted ADRs; supersede with a new one instead.
3. **Context is king.** Explain WHY the decision was made, not just WHAT.
4. **Alternatives matter.** Document what was considered but rejected.
5. **Human decisions marked.** Clearly indicate when the decision came from a user or stakeholder.
6. **Critical analysis.** Be thorough, question assumptions, document trade-offs honestly.
7. **Understandable language.** Write for a developer unfamiliar with the decision; avoid unexplained jargon.

### Best practices

```
GOOD                                     AVOID
Write for future developers              Use jargon without explanation
Include code examples                    Assume reader knows the context
Reference related ADRs                   Skip alternatives ("only way")
Use clear, simple language               Make assumptions unstated
Document driving constraints             Forget to update status when superseding
Explain consequences both ways           Be vague ("better", "improves performance")
Link to implementation                   Skip negative consequences
Be critical, document risks              Write marketing copy
Provide specific evidence                Use jargon without defining it
```

---

## Verification Gates

An ADR moves from `Status: Proposed` to `Status: Accepted` only after all four gates pass. The gates are intentionally *named* so a reviewer can cite a single gate when blocking acceptance: "This ADR fails the Evidence gate, please add measurements."

### Gate 1: Completeness

The ADR has filled every load-bearing section.

- [ ] Problem statement is unambiguous; a reader knows what the decision is *about*
- [ ] Decision is stated as a single, declarative sentence (not a discussion)
- [ ] At least 2 alternatives considered, each with a "why not chosen" line
- [ ] Consequences are listed in *both* directions (positive and negative); a one-sided ADR fails this gate
- [ ] Risks are named explicitly, each with at least one mitigation

### Gate 2: Evidence

Every claim that a reader could challenge is backed by something verifiable.

- [ ] No bare adjectives where numbers are achievable: "saves 5 to 8 KB RAM" beats "saves memory"; "reduces request latency from 120 ms to 40 ms" beats "faster"
- [ ] Constraint claims are anchored to a source: "the runtime allows 256 MB per worker" cites where (vendor doc, measured, ops policy)
- [ ] Code references include `file:line` so the reviewer can verify
- [ ] External claims (specs, RFCs, vendor docs) are linked, not paraphrased

### Gate 3: Clarity

A reader who has never seen this codebase can follow the argument.

- [ ] Acronyms defined on first use: "TLS (Transport Layer Security)"
- [ ] Technical concepts explained when load-bearing: "back-pressure: a feedback signal from a slower consumer to a faster producer"
- [ ] Code examples are concrete and runnable, not pseudo-code
- [ ] The decision is explainable in one paragraph at the top, before the reader needs to read the alternatives

### Gate 4: Consistency

The ADR fits the existing record without creating contradictions.

- [ ] No conflict with an `Accepted` ADR; if there is, this ADR is a supersede with explicit reference
- [ ] Cross-references to related ADRs are present and accurate
- [ ] Terminology matches the project glossary and other ADRs
- [ ] ADR number is the next sequential, no gap, no reuse

This pattern is adapted from [trailofbits/skills](https://github.com/trailofbits/skills).

---

## ADR Template

Default template. Override with project specifics in your project's local copy of this file.

```markdown
# ADR-XXX Title in title case

## Status

Accepted. Date: YYYY-MM-DD.

(Or: Proposed; Deprecated; Superseded by ADR-YYY; Amended by ADR-YYY.)

## Context

One paragraph stating the problem clearly enough for a reader who has never seen this codebase. Include the constraints that drove the decision and the existing state that triggered the choice.

## Decision

A single declarative sentence stating what was chosen. Optionally followed by 2 to 4 short paragraphs unpacking the choice.

## Alternatives Considered

### Alternative A: name

Description and rejection reasoning.

### Alternative B: name

(repeat structure)

### Alternative C: do nothing

If "do nothing" was rejected, document why. If it was the right choice, this ADR is probably not needed.

## Consequences

**Benefits**

- Specific positive outcomes, with measurements when achievable.

**Trade-offs**

- The cost we accepted by making this choice.

**Risks and mitigations**

- *Risk*: what could go wrong. *Mitigation*: how we address it.

## Related Decisions

- **ADR-YYY (Title)**: how it relates (supersedes, amends, depends on, complements).

## References

- Implementation tasks or issue IDs.
- Source files affected (file:line).
- External specs, RFCs, vendor docs, or measurements.
```

A clean copy is available at `examples/ADR-template.md`.

---

## Naming Convention

### Default

```
Format: ADR-XXX-short-descriptive-title.md

Where:
- XXX = zero-padded sequential number (001, 002, ..., 099, 100, etc.)
- short-descriptive-title = kebab-case description

Examples:
- ADR-001-postgresql-for-sensor-data.md
- ADR-014-rest-api-versioning.md
- ADR-042-grpc-internal-rpc.md
- ADR-099-deprecated-static-bundling.md

Wrong:
- ADR-1-postgresql.md (not zero-padded)
- adr-001-PostgreSQL.md (lowercase prefix, mixed case)
- ADR-001_PostgreSQL.md (underscore instead of hyphen)
```

### Number assignment

- Sequential numbering starting from 001.
- Check `docs/adr/` for the highest number and increment.
- Do not reuse numbers from deprecated or superseded ADRs.
- Do not leave gaps in numbering.

### Project override

If your project uses a different convention (e.g. `adr-NNNN-` lowercase 4-digit, or `0001-...` without prefix), override this section in your local copy of the skill and the agent. Mixing conventions in the same `docs/adr/` directory is the worst outcome.

---

## ADR Categories

Group ADRs by architectural domain for navigation. The category list is project-specific; below is a neutral starting set you can replace:

- **Platform and Runtime**: language, framework, runtime, deployment target.
- **Data and Storage**: database, schema, persistence, caching.
- **Protocol and Integration**: sync vs async, REST/RPC/messaging, contracts.
- **Memory and Resources**: when load-bearing for the project's constraints.
- **Network and Security**: TLS posture, auth, trust boundaries, secrets.
- **Build and Tooling**: build system, CI gates, packaging, testing.
- **Observability**: logging, metrics, tracing, alerting.
- **Cross-Cutting**: error handling, configuration, internationalization.

In your `docs/adr/README.md`, replace these with categories that match your domain.

---

## ADR Workflow

### 1. Before making architectural changes

- Review existing ADRs (`docs/adr/README.md` index).
- Search for related decisions.
- Check whether the change conflicts with any `Accepted` ADR.
- Determine if a new ADR is needed (long-term impact? multiple components? alternatives exist? future context?).

### 2. During implementation

- Create the ADR with `Status: Proposed`.
- Get the next ADR number (`ls docs/adr/ADR-*.md` and increment).
- Write a comprehensive ADR using the template.
- Reference the ADR in code comments at non-obvious enforcement sites: `// See ADR-XXX for why we use this pattern.`
- Implement according to the ADR's decision.

### 3. After implementation

- Update Status: `Proposed` -> `Accepted`.
- Add the implementation date in the Status line: `Accepted. Date: YYYY-MM-DD.`
- Add a `## Related Decisions` entry to any other ADR that newly relates.
- Update `docs/adr/README.md` with the new ADR in the right category.

### 4. When superseding an ADR

- Write the new ADR explaining the change.
- Reference the original in `## Related Decisions`: "Supersedes ADR-XXX (Title)".
- Update the old ADR's Status line only: `Superseded by ADR-YYY`.
- Do NOT modify the old ADR's decision text or reasoning. Immutability is the rule.
- Update `docs/adr/README.md` to mark both ADRs.

### 5. When amending an ADR (lighter alternative to supersession)

- Write the amending ADR.
- Reference the original in `## Related Decisions`: "Amends ADR-XXX (Title)".
- Update the old ADR's Status line: `Amended by ADR-YYY` (or leave as `Accepted` if the amend is content-additive).
- Do NOT modify the old ADR's decision text.

---

## Code Review Integration

### Automatic checks during review

1. Do changes violate any existing ADRs?
2. Do changes require a new ADR?
3. Are ADR references in code accurate?
4. Is the ADR Status up to date for any ADR being implemented in this PR?

### Example review comments

```
NEGATIVE
"This change violates ADR-NNN (Title).
Please refactor or write a superseding ADR."

POSITIVE
"This change aligns with ADR-NNN (Title). Good use of the documented pattern."

QUESTION
"This introduces a new architectural pattern.
Please create an ADR documenting the decision before merging."

STATUS NUDGE
"ADR-NNN is referenced but its Status is still 'Proposed'.
Please flip it to 'Accepted' since this PR implements it."
```

### PR checklist

```markdown
## ADR Compliance Checklist

- [ ] Changes reviewed against existing ADRs
- [ ] No violations of architectural decisions (or a superseding ADR is included)
- [ ] New ADR created if needed (Status: Proposed)
- [ ] ADR Status updated if this PR implements an existing ADR
- [ ] Code comments reference relevant ADRs at non-obvious enforcement sites
- [ ] docs/adr/README.md updated if a new ADR was added
- [ ] All four verification gates pass on any new or amended ADR
```

---

## Human Decision Documentation

When a user explicitly makes an architectural choice, document it as a human decision so the attribution survives.

```markdown
# ADR-XXX Title

## Status

Accepted. Date: YYYY-MM-DD.

**Decision Maker:** User: [Name or role]   <- IMPORTANT: mark as human decision

## Context

[User's problem or request]

## Decision

**User Decision:** [What the user chose]

The user explicitly chose [X] over [Y] because [reason given].

## Alternatives Considered

[What was presented to the user]

### Alternative A: [Option presented]

**User Feedback:** [User's reasoning for or against]

### Alternative B: [Option presented]

**User Feedback:** [User's reasoning]

## Rationale

**User's stated reasons:**

- Reason 1
- Reason 2

**Technical context (from the agent):**

[Agent's analysis of the decision's technical implications. Useful for future readers who want the technical lens alongside the user's intent.]
```

The Decision Maker line is the load-bearing part: it tells future readers that the choice was a human one, not an agent-generated default.

---

## Code Examples in ADRs

### Show, don't just tell

```markdown
WRONG (no example)
Use idempotent retry logic.

RIGHT (concrete example)
Use idempotent retry logic. Each request includes an Idempotency-Key
header derived from the client's request body hash, and the server
deduplicates within a 24-hour window.

Example client wrapper:

```python
def submit(payload):
    key = hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest()
    return http.post("/submit", json=payload, headers={"Idempotency-Key": key})
```
```

### Diagrams when helpful

```
[Client] -- POST /submit (with Idempotency-Key) --> [Gateway]
                                                         |
                                                         v
                                                    [Dedup Cache (24h)]
                                                         |
                                            cache miss   |   cache hit
                                                  v      |       v
                                            [Worker]     |   [Return cached response]
                                                  |      |
                                                  v      |
                                            [Response]   |
```

ASCII diagrams travel well across tools and reviewers.

---

## ADR Index Management

Maintain `docs/adr/README.md` as the navigation hub. Required sections:

1. **What are ADRs?** Brief explanation for new readers.
2. **Quick Navigation** with category counts.
3. **ADR Index**: full categorized list with one-line summaries.
4. **ADR Template**: link to or embed the template.
5. **Key Architectural Themes**: cross-cutting concerns referenced by multiple ADRs.
6. **Architectural Dependencies**: which ADRs depend on which.
7. **When to Create an ADR**: guidance for new contributors.
8. **Superseding ADRs**: how to handle changes.
9. **Resources**: links to ADR best practices and external references.

When adding a new ADR:

1. Add an entry under the appropriate category.
2. Update the category count in Quick Navigation.
3. Update "Foundational ADRs" if the new one is highly referenced.
4. Add cross-references in any older ADRs that relate.

---

## ADR Metrics and Maintenance

### Health indicators (healthy repository)

- All ADRs have a clear Status.
- Superseded ADRs reference their replacement.
- Code references match existing ADRs.
- README index is up to date.
- Recent ADRs include implementation dates.
- Each ADR has at least 2 alternatives documented.

### Needs attention

- Multiple ADRs with `Proposed` status for >30 days (review and accept or deprecate).
- ADR numbers with gaps (consolidate or document the gap).
- Code comments referencing non-existent ADRs (broken links).
- ADRs without an alternatives section (low quality).
- README categories that do not match actual ADRs (drift).

### Periodic review (quarterly)

```
1. Are any 'Proposed' ADRs abandoned? (Mark deprecated.)
2. Are any 'Accepted' ADRs being violated? (Update enforcement.)
3. Do new patterns need ADRs? (Create them.)
4. Are superseded ADRs properly linked? (Verify.)
5. Is README accurately reflecting current state? (Update.)
```

---

## Quick Reference

### ADR creation checklist

```markdown
Creating a new ADR? Check these:

- [ ] Next sequential number assigned (check docs/adr/)
- [ ] Filename follows convention
- [ ] Status field present
- [ ] Date field present (YYYY-MM-DD)
- [ ] Decision Maker identified (Agent or User: Name)
- [ ] Context section explains the problem clearly
- [ ] At least 2 to 3 alternatives documented
- [ ] Pros and cons listed for each alternative
- [ ] "Why not chosen" for each rejected alternative
- [ ] Consequences section complete (positive, negative, risks)
- [ ] Code examples included (if applicable)
- [ ] Related ADRs referenced
- [ ] Implementation notes with affected files
- [ ] Added to docs/adr/README.md index
- [ ] Category assigned
- [ ] References / external links included
- [ ] Four verification gates pass
```

### Common mistakes

```
- Writing "we should use X" without explaining why
- Skipping alternatives ("this is the only way")
- No code examples for technical decisions
- Forgetting to update status after implementation
- Not referencing related ADRs
- Vague consequences ("it will be better")
- No decision maker attribution
- Missing constraints that drove the decision
- Modifying accepted ADRs instead of superseding
- Not updating README.md index
- Using jargon without defining it
- Being superficial: not digging into the "why" behind constraints
- Hiding negative consequences
- Writing marketing copy instead of honest technical analysis
- Skipping measurements ("faster" vs "68% latency reduction")
- Not explaining technical trade-offs in understandable terms
```

### When in doubt

Ask these questions:

1. Will a developer in 6 months understand WHY we did this?
2. Have I explained what we REJECTED, not just what we chose?
3. Could this ADR help someone avoid making a wrong decision?
4. Have I included enough code examples?
5. Is the decision maker clearly identified?
6. Can someone unfamiliar with this technology understand the core trade-offs?
7. Have I been honest about negative consequences?
8. Have I quantified impacts with actual measurements?

If you answered "No" to any of these, improve the ADR.

---

## Resources

### Official ADR resources

- ADR best practices: <https://adr.github.io/>
- Michael Nygard's original post (2011): <https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions>
- MADR template: <https://github.com/adr/madr>
- Joel Parker Henderson's ADR collection: <https://github.com/joelparkerhenderson/architecture-decision-record>

### Tooling

- adr-tools (npryce): <https://github.com/npryce/adr-tools>
- Log4brains: <https://github.com/thomvaill/log4brains>

### Related skill patterns

- Anti-rationalization guards: <https://github.com/addyosmani/agent-skills>
- Verification gates: <https://github.com/trailofbits/skills>

---

**Remember:** ADRs are *living documentation* stored as docs-as-code in the same repository as the implementation. They should be consulted during development, referenced in code reviews, and evolved through supersession (not modification). Good ADRs make architectural decisions visible, understandable, and enforceable.
