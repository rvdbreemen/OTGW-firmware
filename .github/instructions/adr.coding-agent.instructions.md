---
applyTo: "**"
excludeAgent: "code-review"
---

# ADR Requirements for Coding Agent

## Before Implementing Changes

- **Check for existing ADRs**: Before implementing, verify if an ADR exists in `docs/adr/` that governs the change
- **Assess architectural significance**: Determine if the change affects architecture, NFRs, interfaces, dependencies, or build/tooling
- **Use the canonical guide**: Follow `docs/adr/README.md` for ADR format, lifecycle, and superseding rules
- **Review ADR skill**: Consult `.github/skills/adr/SKILL.md` for comprehensive ADR creation guidance

## Creating New ADRs

If no ADR exists and the change is architecturally significant:

1. **Create the ADR under `docs/adr/`** using the repository format documented in `docs/adr/README.md`
2. **Use the ADR template and examples** from `docs/adr/README.md` and `.github/skills/adr/SKILL.md`
3. **Reference the ADR** in the PR description: "ADR: docs/adr/ADR-XXX-title.md"

## Superseding Existing ADRs

If an existing ADR would be reversed:

1. **Create a new superseding ADR** (do not modify the accepted ADR beyond the allowed status update)
2. **Follow the superseding workflow in `docs/adr/README.md`** for status text and cross-references
3. **Explain the change**: Document why the original decision is being reversed

## Implementation Checklist

- [ ] ADR exists or is created for architecturally significant changes
- [ ] ADR follows the repository structure in `docs/adr/README.md`
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
