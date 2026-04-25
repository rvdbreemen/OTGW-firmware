# ADR Checks for Code Review

When you are reviewing a PR or a diff, run these six checks for ADR compliance. The canonical guide is `docs/adr/README.md`; the comprehensive skill is `SKILL.md`.

## Check 1: ADR exists for architecturally significant changes

If the diff touches any of:

- **Architecture**: service or module boundaries, deployment, integration patterns
- **Non-functional requirements**: security, availability, performance, privacy, resilience
- **Interfaces and contracts**: API behaviour, message topics, RPC contracts, breaking changes
- **Dependencies**: new frameworks or libraries with broad impact
- **Build and tooling**: build system, CI gates, development workflow

then there should be a linked ADR.

**Action if missing:**

- Request an ADR. Be specific about which decision should be captured.
- Suggest the `adr-generator` subagent for fast scaffolding.

## Check 2: ADR is properly linked

Verify the PR description includes:

- A path to the ADR: `docs/adr/ADR-XXX-title.md`
- A clear explanation of how the diff relates to the ADR
- If implementing a `Proposed` ADR, check whether the Status should flip to `Accepted` in this PR

**Action if link is missing or unclear:**

- Request the ADR link in the PR description.
- Ask for one sentence connecting the code change to the ADR's decision.

## Check 3: No ADR violations

Check whether the diff violates an `Accepted` ADR:

- Cross-reference `docs/adr/README.md` for relevant ADRs to the touched files
- Check ADR-specific compliance for ADRs that include enforcement rules
- Verify the change follows patterns established by `Accepted` ADRs

**Action if violation found:**

- *Option 1*: request alignment with the existing ADR.
- *Option 2*: request a new superseding ADR if the decision needs to change.
- Always explain *why* the change conflicts with the documented decision.

## Check 4: Supersession is correct

If the PR supersedes an existing ADR:

- Verify the new ADR includes a `## Related Decisions` entry "Supersedes ADR-XXX"
- Verify the old ADR's Status line is updated to `Superseded by ADR-YYY`
- Verify the supersession rationale appears in the new ADR (not the old one)
- Verify the old ADR's content (context, decision, alternatives, consequences) is **not modified** beyond the Status line. ADR immutability is the rule.

**Action if supersession is incorrect:**

- Request proper supersession linking.
- If the old ADR's content was edited, request a revert of that part.

## Check 5: ADR quality (the four verification gates)

Apply the four named gates from `SKILL.md`:

- **Completeness gate**: problem stated, decision unambiguous, 2+ alternatives with rejection reasoning, consequences in both directions, risks named with mitigations.
- **Evidence gate**: bare adjectives replaced with measurements; constraint claims anchored to a source; code references include `file:line`; external claims linked.
- **Clarity gate**: acronyms defined on first use, technical concepts explained when load-bearing, decision explainable in one paragraph at the top.
- **Consistency gate**: no conflict with another `Accepted` ADR (or explicit supersede), cross-references present, terminology matches other ADRs, ADR number sequential.

**Action if a gate fails:**

- Cite the failing gate by name: "This ADR fails the Evidence gate. Please add a measurement to back the 'reduces fragmentation' claim."
- Suggest a fix.
- Reference an existing ADR in the project as a positive example, if available.

## Check 6: Legacy non-compliance

If existing code does not comply with a *new* ADR:

- Acknowledge the legacy non-compliance explicitly.
- Request a remediation plan: an incremental cleanup, a tech-debt issue with a target version, or an explicit "out of scope for this PR" note.
- Do not block the ADR adoption itself due to legacy issues.
- Ensure the *new* code in this PR complies.

## Review-comment templates

### Missing ADR

```
This change introduces a new caching pattern, which affects architecture.
Please create an ADR documenting:
- Why caching is needed (context, constraints)
- Cache options considered (Redis, Memcached, in-memory)
- Chosen approach and rationale
- Consequences (performance gains, memory and complexity costs)

The adr-generator subagent can scaffold this. See SKILL.md for the template.
```

### ADR violation

```
This change uses pattern X in performance-critical code, which violates
ADR-NNN (Title). Two options:

1. Refactor to follow ADR-NNN's pattern.
2. Write a superseding ADR if the decision needs to change.

ADR-NNN: docs/adr/ADR-NNN-title.md
```

### Supersession issue

```
This ADR supersedes ADR-OLD but ADR-OLD has not been updated.

Please:
1. Update ADR-OLD Status line to "Superseded by ADR-NEW".
2. Confirm ADR-OLD's content (context, decision, alternatives, consequences)
   is unchanged. ADR immutability is the rule; only the Status line moves.
3. Cross-reference both ADRs in their `## Related Decisions` sections.
```

### Verification gate failure

```
This ADR's Evidence gate fails. Two specific spots:

1. "Improves performance" (line N): replace with a measurement
   ("reduces request latency from X to Y") or a benchmark reference.
2. "Memory constraint requires this" (line M): cite where (datasheet,
   measured, vendor doc) and how much.

ADR-NNN in this repo is a good positive reference for the Evidence gate.
```

## Definition of Done for review

- [ ] Architecturally significant changes have a linked ADR
- [ ] ADR link is in the PR description
- [ ] No `Accepted` ADR is violated, or a superseding ADR is in place
- [ ] Supersession chain is correct
- [ ] All four verification gates pass on the ADR
- [ ] Legacy non-compliance, if any, has a remediation plan
- [ ] Code comments at non-obvious enforcement sites point at the ADR
