---
applyTo: "**"
excludeAgent: "code-review"
---

# ADR Requirements for Coding Agent

## Before Implementing Changes

- **Check for existing ADRs**: Before implementing, verify if an ADR exists in `docs/adr/` that governs the change
- **Assess architectural significance**: Determine if the change affects architecture, NFRs, interfaces, dependencies, or build/tooling
- **Review ADR skill**: Consult `.github/skills/adr/SKILL.md` for comprehensive ADR creation guidance

## Creating New ADRs

If no ADR exists and the change is architecturally significant:

1. **Create new ADR** under `docs/adr/` following the naming convention `ADR-XXX-short-title.md`
2. **Use the ADR template** from `.github/skills/adr/SKILL.md`
3. **Include required sections**:
   - Status (Proposed/Accepted/Rejected/Superseded)
   - Context (problem statement, constraints, stakeholders)
   - Decision (choice made, rationale)
   - Alternatives Considered (2-3 options with pros/cons, rejection reasons)
   - Consequences (positive/negative impacts, risks with mitigation)
   - Related Decisions (links to other ADRs)
4. **Reference the ADR** in the PR description: "ADR: docs/adr/ADR-XXX-title.md"

## Superseding Existing ADRs

If an existing ADR would be reversed:

1. **Create a new superseding ADR** (do not modify the accepted ADR)
2. **In the new ADR**: Add "Supersedes: ADR-XXX"
3. **Update the old ADR**: Change status to "Superseded by ADR-YYY"
4. **Explain the change**: Document why the original decision is being reversed

## ADR Content Guidelines

- **Focus on "why"** (forces/trade-offs), not just "how" (implementation details)
- **Keep it short**: 1-2 screens is the guideline
- **Link externally**: Reference detailed design docs, issues, or PRs
- **Be specific**: Include code examples, measurements, and evidence
- **Be honest**: Document negative consequences and risks, not just benefits
- **Be clear**: Define technical terms, avoid unexplained jargon

## Implementation Checklist

- [ ] ADR exists or is created for architecturally significant changes
- [ ] ADR follows the template structure
- [ ] Alternatives are documented (minimum 2-3 options)
- [ ] Consequences include both positive and negative impacts
- [ ] PR description links to the ADR
- [ ] If superseding, old ADR is properly marked
- [ ] Code comments reference relevant ADRs where appropriate

## Definition of Done

- ADR exists and is readable
- ADR focuses on rationale ("why")
- PR/issue links to ADR
- Supersession chain is correct (if applicable)
- ADR is ready to merge (no placeholders)
