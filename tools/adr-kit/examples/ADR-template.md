# ADR-XXX Title in title case

## Status

Accepted. Date: YYYY-MM-DD.

(Or one of: `Proposed`, `Deprecated`, `Superseded by ADR-YYY`, `Amended by ADR-YYY`.)

## Context

One paragraph stating the problem clearly enough for a reader who has never seen this codebase. Include the constraints that drove the decision and the existing state that triggered the choice. If a prior ADR is being amended or superseded, name it here. Avoid implementation detail; that lives in the code.

## Decision

A single declarative sentence stating what was chosen. Optionally followed by 2 to 4 short paragraphs unpacking the choice: what it does, what it explicitly does not do, what its scope is. The reader should be able to summarize the decision in one sentence after this section.

## Alternatives Considered

### Alternative A: name

One paragraph describing the option. One paragraph (or a short bullet list) explaining why it was rejected. Be specific: "did not fit" is not a reason; "would require a TLS stack that exceeds the device's RAM" is.

### Alternative B: name

(repeat structure)

### Alternative C: do nothing

If "do nothing" was rejected, document why in one or two sentences. If it was the right choice, this ADR probably is not needed.

## Consequences

**Benefits**

- Specific positive outcomes. Use measurements when achievable: "reduces request latency from 12 ms to 4 ms", not "improves performance".
- Cross-cutting wins: maintainability, security, simpler deployment, and similar.

**Trade-offs**

- The cost we accepted by making this choice. Be honest about negatives. A consequences list with no negatives fails the Completeness gate.

**Risks and mitigations**

- *Risk*: a concrete way the decision could fail, with the conditions that would trigger it.
  *Mitigation*: a test, monitor, doc, gate, or runbook that addresses the risk.

## Related Decisions

- **ADR-YYY (Title)**: how it relates. Use one of: supersedes, amends, depends on, complements, conflicts with.

## References

- Implementation tasks (e.g. issue or backlog ID).
- Source files affected, with file:line references where helpful.
- External specs, RFCs, vendor docs, or measurements.
- Standards or frameworks used.
