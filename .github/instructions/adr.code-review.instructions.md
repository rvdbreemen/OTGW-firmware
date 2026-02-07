---
applyTo: "**"
excludeAgent: "coding-agent"
---

# ADR Checks for Code Review

## Review Checklist for Architectural Changes

When reviewing PRs, verify ADR compliance for architecturally significant changes.

### Check 1: ADR Exists

**If a PR changes any of the following, verify there is a linked ADR:**
- Architecture (service/module structure, deployment, integration patterns)
- NFRs (security, availability, performance, privacy, resilience)
- Interfaces/contracts (API changes, breaking changes, coupling/decoupling)
- Dependencies (frameworks, libraries, tooling with broad impact)
- Build/tooling (build system, CI/CD, development processes)

**Action if missing:**
- Request an ADR in the review
- Explain which architectural decision should be captured
- Suggest using `.github/skills/adr/SKILL.md` for guidance

### Check 2: ADR is Properly Linked

Verify the PR description includes:
- Link to the ADR: `docs/adr/ADR-XXX-title.md`
- Clear explanation of how the change relates to the ADR
- If implementing a Proposed ADR, check if status should be updated to Accepted

**Action if link is missing or unclear:**
- Request the ADR link in the PR description
- Ask for clarification on the relationship between code and ADR

### Check 3: No ADR Violations

Check if the code violates an accepted ADR:
- Review `docs/adr/README.md` for relevant ADRs
- Check ADR-specific compliance (e.g., ADR-004: static buffers, ADR-009: PROGMEM)
- Verify the change follows patterns established in ADRs

**Action if violation found:**
- **Option 1:** Request alignment with the existing ADR
- **Option 2:** Request a new superseding ADR if the decision needs to change
- Explain why the change conflicts with the documented decision

### Check 4: Supersession is Correct

If the PR supersedes an existing ADR:
- Verify the new ADR includes "Supersedes: ADR-XXX"
- Verify the old ADR is marked "Superseded by ADR-YYY"
- Check that the supersession rationale is clear
- Ensure the old ADR content is NOT modified (immutability)

**Action if supersession is incorrect:**
- Request proper supersession linking
- Ensure the old ADR remains intact with only status update

### Check 5: ADR Quality

Review the ADR content for quality:
- **Focus on "why"**: Does it explain rationale and trade-offs?
- **Alternatives documented**: Are 2-3 options documented with pros/cons?
- **Consequences**: Are both positive AND negative impacts documented?
- **Clarity**: Is it understandable? Are technical terms explained?
- **Evidence**: Are claims backed by measurements or specific evidence?

**Action if quality is insufficient:**
- Request improvements to specific sections
- Suggest using examples from existing ADRs (ADR-003, ADR-004, ADR-009)
- Reference `.github/skills/adr/SKILL.md` for guidance

### Check 6: Legacy Non-Compliance

If existing code doesn't comply with new ADRs:
- Acknowledge the legacy non-compliance
- Request a remediation plan (incremental cleanup or tech-debt ticket)
- Don't block the ADR adoption due to legacy issues
- Ensure new code follows the ADR

## Review Comments Examples

### Missing ADR
```
This change introduces a new caching pattern, which affects architecture.
Please create an ADR documenting:
- Why caching is needed (context/constraints)
- Cache options considered (Redis, Memcached, in-memory)
- Chosen approach and rationale
- Consequences (performance gains, memory/complexity costs)

See .github/skills/adr/SKILL.md for the ADR template.
```

### ADR Violation
```
This change uses the String class in performance-critical code, 
which violates ADR-004 (Static Buffer Allocation).

Please either:
1. Refactor to use static buffers as per ADR-004, or
2. Create a new ADR superseding ADR-004 if the decision needs to change

ADR-004: docs/adr/ADR-004-static-buffer-allocation.md
```

### Supersession Issue
```
This ADR supersedes ADR-015 but the old ADR hasn't been updated.

Please:
1. Update ADR-015 status to "Superseded by ADR-030"
2. Ensure ADR-015 content remains unchanged (immutability)
```

## Definition of Done for Review

- [ ] Architecturally significant changes have linked ADRs
- [ ] ADR links are in PR description
- [ ] No violations of accepted ADRs (or superseding ADR created)
- [ ] Supersession chains are correct
- [ ] ADR quality is sufficient (rationale, alternatives, consequences)
- [ ] Legacy non-compliance has a remediation plan (if applicable)
