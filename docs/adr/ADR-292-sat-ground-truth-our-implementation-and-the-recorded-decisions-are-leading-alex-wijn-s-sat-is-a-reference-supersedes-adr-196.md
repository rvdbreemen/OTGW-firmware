---
id: "ADR-292"
title: "SAT ground truth: our implementation and the recorded decisions are leading, Alex Wijn's SAT is a reference (supersedes ADR-196)"
status: "Accepted"
date: "2026-11-21"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-196"
superseded_by: null
topics:
  - "sat"
  - "governance"
  - "ground-truth"
  - "thermo-nova"
  - "documentation-process"
  - "leading implementation"
aliases:
  - "SAT ground truth"
  - "source precedence"
  - "Sergeant D"
  - "George"
  - "sergeantd"
  - "sergeant-d-corrections"
  - "Sergeant D corrections"
  - "thermo-nova"
  - "thermo-nova-spec"
  - "SAT anti-drift rules"
  - "SAT-develop"
  - "leading implementation"
  - "Alex Wijn"
  - "Alexwijn"
  - "reference implementation"
components:
  - "SAT source precedence rule"
  - "docs/sat/decisions/sergeant-d-corrections.md"
  - "other-projects/SAT-develop/"
symbols: []
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-292 SAT ground truth: our implementation and the recorded decisions are leading, Alex Wijn's SAT is a reference (proposed successor to ADR-196)

## Status

Accepted, 2026-11-21, by Robert.

On acceptance this ADR supersedes ADR-196 as a whole. Following the repo
precedent for Proposed successors (commit 057535397, ADR-197 to ADR-199), the
supersede links (`supersedes` here, `superseded_by` and the status in ADR-196,
the README note) are written on acceptance, not now. ADR-196 stays Accepted
and in force until then.

Guideline-level per ADR-080, like ADR-196: no CI (continuous integration) gate
checks whether a SAT decision followed this rule.
`tests/test_sat_ground_truth_block.py` is a drift guard for the text of the
rule only (see Verification).

## Status History

```yaml
status_history:
  - date: 2026-11-21
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal, from the maintainer directive of 2026-11-21 (TASK-3013 named in the test docstring and the directive changelog)
    changed_via: adr-kit
  - date: 2026-11-21
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer in the coordinator session (AskUserQuestion, 2026-11-21); supersedes ADR-196
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat) was designed on paper by Sergeant D (George,
Discord user `sergeantd`) and first turned into code by Alex Wijn (Alexwijn) as
a Python Home Assistant component. Our firmware started as a port of that code
at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` (2026-03-18, then branch
`releases/thermo-nova`).

ADR-196 (Accepted 2026-10-24) ranked the sources for SAT behaviour as:
1. Sergeant D's recorded decisions, 2. thermo-nova at the pinned commit,
3. `docs/sat/thermo-nova-spec.md`, 4. our firmware and its docs, "never the
authority for what SAT should do". That ranking fitted a fresh port. Since then
three facts changed:

- **Our implementation has moved past the port by decision.**
  `docs/sat/decisions/sergeant-d-corrections.md` now has 450 numbered entries (count of
  `## <n>.` headings, 2026-11-21). Several deliberately leave 7beaaf5: entry 441
  (Robert, 2026-11-21) reverses his entry-64 choice to keep thermo-nova's held
  boiler temperature (`pwm.py:144-149` at 7beaaf5) and builds the live-flow
  duty of entries 437, 438 and 440 for 2.0.0. Under ADR-196 the firmware that
  implements those decisions still ranked last, below the code it deliberately
  differs from.
- **The pinned branch is gone.** Entry 449 (Sergeant D, 2026-11-21, message
  1573705702512398387): "keep in mind that the thermo-nova branch does not exist
  anymore. SAT custom component is in v5.0 now". Our check in that entry:
  Alexwijn/SAT has only `develop` and `master`; 7beaaf5 still exists and is an
  ancestor of 5.0.0 (140 commits behind it).
- **The local copy moved.** `other-projects/SAT-releases-thermo-nova/` no
  longer exists in this worktree (`ls` fails, 2026-11-21). The reference
  snapshot is now `other-projects/SAT-develop/`; its `UPSTREAM_COMMIT` reads
  `ba515c8a90dd63f884fe19bafa8519b2c07c3f15 develop 2026-11-21 ... 138 commits
  after the old pin 7beaaf5 (an ancestor)`. A full upstream clone sits next to
  the repo at `../OTGW-reference/SAT-git` (`git log -1 ba515c8` there: "Fixed
  some HACS warnings").

On 2026-11-21 the maintainer gave a directive, verbatim:

> "De implementatie die wij hebben van SAT is leidend. En alle keuzes die George
> heeft gemaakt en ik heb gemaakt, zijn tezamen de SAT-implementatie."

(English: "Our SAT implementation is leading. All choices George made and I
made together are the SAT implementation.") About Alex Wijn's SAT: it "is niet
langer de leidende implementatie, maar goede referentie en we willen uitgelijnd
blijven met deze implementatie" ("is no longer the leading implementation, but
a good reference, and we want to stay aligned with it"). George stays the SAT
designer. When he refers to thermo-nova or Alex's code, we compare our
implementation with it and give him the factual differences, the pros and
cons and a recommendation, "altijd feitelijk" ("always factual").

The operational text is already in the SAT-GROUND-TRUTH block of
`.claude/rules/sat-ground-truth.md` and AGENTS.md (uncommitted in this
worktree). `docs/process/directive-changelog.md` lines 17 and 36 record the
directive and that it replaces the 2026-10-22 ranking. It is not yet a
numbered entry in the corrections file (a grep for "leidend" there finds
nothing, 2026-11-21).

## Decision

For every statement or change about SAT behaviour the sources rank in this
order, and a higher source wins over a lower one:

1. **Recorded decisions** of Sergeant D and of Robert in
   `docs/sat/decisions/sergeant-d-corrections.md`: one numbered, dated entry each, with
   the words quoted and the Discord message link.
2. **Our SAT implementation**: `src/OTGW-firmware/SAT*.ino`, `SATtypes.h`, the
   SAT ADRs in `docs/adr/` and `docs/sat/firmware-sat-spec.md`. Where the code
   and a recorded decision disagree, the decision wins and the code is a bug.
3. **The OpenTherm specification and application notes**
   (`docs/opentherm specification/`, `other-projects/OT-Spec/`), and **field
   data** from testers (captures, long-run recordings).
4. **Reference, not leading**: Alex Wijn's SAT
   (`https://github.com/Alexwijn/SAT/tree/develop`, local snapshot
   `other-projects/SAT-develop/` at ba515c8, 2026-11-21) and
   `docs/sat/thermo-nova-spec.md` (written from 7beaaf5).

Sergeant D stays the SAT designer and decides SAT behaviour. When he refers to
thermo-nova or to Alex Wijn's SAT, that is a reason to look at that code, not
ground truth. We give him a comparison: the factual differences, pros and cons
of each, and a recommendation. It draws on every relevant recorded decision,
our rules, the OpenTherm specification and the low-load application notes,
field data, and where useful internet research and simulations. The
comparison follows the evidence both ways. If Alex's way is better on a point,
we adopt it through a normal recorded decision.

Field data validates our implementation: tester captures check that our SAT
does what the recorded decisions say. The same data is replayed through both
implementations to learn how each behaves.

What stays from ADR-196: cite as `file:line`, opened in the current session; no
claim without a citation; "not found in the code" instead of a plausible gap
filler; behaviour in time order; every Sergeant D correction or decision
recorded before it is implemented; every SAT decision checked against the
recorded entries; one question per Discord message to Sergeant D; a second
reader before a claimed difference is published.

What changes from ADR-196: thermo-nova drops from rank 2 to rank 4; our
implementation rises from rank 4 (never the authority) to rank 2 (below the
recorded decisions only); the OpenTherm spec and field data get an explicit
rank; the "pinned commit" rule becomes a "refresh `other-projects/SAT-develop/`
only on purpose" rule.

Scope: SAT behaviour only. The CLAUDE.md/AGENTS.md block stays the operational
text; this ADR records it so ADR authors and `/adr-kit:context` find it.

## Decision Contract

### Must

- Read `docs/sat/decisions/sergeant-d-corrections.md` before writing a SAT ADR, plan,
  review or SAT-control change. Cite the entry numbers it relies on, or state
  "no entry covers this topic".
- Cite our firmware as `file:line` and Alex's code as
  `custom_components/sat/<file>:<line>` at a named commit, opened in the
  current session.
- Answer a Sergeant D reference to Alex's code with a factual comparison:
  differences, pros and cons, recommendation, with citations.
- Treat a disagreement between our code and a recorded decision as a bug in
  the code.
- Check a historical 7beaaf5 citation with
  `git -C ../OTGW-reference/SAT-git show 7beaaf5:custom_components/sat/<file>`.
- Refresh `other-projects/SAT-develop/` only in one commit that records the new
  upstream commit in its `UPSTREAM_COMMIT` and in the other-projects README and
  says what changed.

### Must Not

- Treat thermo-nova, Alex's develop branch or `thermo-nova-spec.md` as ground
  truth for SAT behaviour.
- Adopt a behaviour from Alex's code without a recorded decision.
- Rewrite an existing corrections entry, or an earlier ADR that cites 7beaaf5,
  to the new ranking.
- Fill a gap with plausible behaviour.

### Exceptions

- None beyond ADR-196's: firmware-only features where Sergeant D defers to the
  maintainer are decided by the maintainer and recorded as an entry.

### Verification

- `python tests/test_sat_ground_truth_block.py`: keeps the block identical in
  `.claude/rules/sat-ground-truth.md` and AGENTS.md and checks the new
  ranking's text (for example "our implementation is leading", "Reference, not
  leading", `other-projects/SAT-develop/`, and no "2. The thermo-nova code").
  It guards the rule's text, not compliance. Evidence, 2026-11-21: in this
  worktree 6 tests OK; against the HEAD versions of the rule file, AGENTS.md and
  navigation.md (scratch copy), 1 FAIL
  (`test_block_makes_our_implementation_leading`), 4 OK, 1 skipped (no
  `other-projects/SAT-develop/` in the scratch copy).
- Review: the reviewer reads the corrections file and treats an unmentioned
  conflict with an entry as a finding.
- `/adr-kit:context "SAT ground truth"` surfaces this ADR.

## Alternatives Considered

### Alternative A: keep ADR-196 and move the pin to 5.0.0 or develop

Keeps the ranking, replaces 7beaaf5 with a current commit, re-diffs
`thermo-nova-spec.md`. Rejected: it keeps a code base we deliberately differ
from (entry 441) above our own implementation. Every new upstream commit would
again rank above our recorded choices. It also contradicts the maintainer
directive, which demotes Alex's code, not only its version. A full re-diff of
the 138 to 140 commits is also unreviewed work (entry 449, Open).

### Alternative B: keep ADR-196 with the pin at 7beaaf5 unchanged

No new work; entry 449's Settled line ("the pin stays 7beaaf5 until Robert
decides", our wording). Rejected: the branch is gone, the local copy is gone,
and the reference is frozen at March 2026. Our firmware, carrying 450 recorded
entries, would stay "never the authority".

### Alternative C: our firmware alone is ground truth, Alex's code dropped

Simplest. Rejected: the maintainer wants to stay aligned with Alex's code and
to learn from it. The code is not the decision either: rank 1 stays the
recorded decisions, so a code bug cannot become the rule (corrections entry 5
showed that a claim about our own firmware can be wrong).

### Alternative D: binding ADR with a CI gate

Rejected for the same reason as in ADR-196: whether a plan cited the right
entry or gave Sergeant D a factual comparison is a judgement `evaluate.py`
cannot read from a diff.

## Consequences

**Positive:**

- The ranking matches how decisions are made: Sergeant D and Robert decide, our
  code implements, Alex's code informs.
- A code/decision conflict has a clear outcome (fix the code) instead of a
  ranking that put the code last and the old reference above it.
- Comparisons with Alex's code become two-way and factual: the reference can
  still win a point, through a recorded decision.
- The reference is current (develop at ba515c8, 2026-11-21) instead of a
  deleted branch.

**Negative:**

- Earlier ADRs and entries cite 7beaaf5 (23 ADR files and 97 lines in the
  corrections file match `7beaaf5`, grep 2026-11-21). They are not rewritten;
  their citations stay valid through
  `git -C ../OTGW-reference/SAT-git show 7beaaf5:<path>`. Readers must know
  that a 7beaaf5 citation is historical, not the current reference.
- `docs/sat/thermo-nova-spec.md` stays as written from 7beaaf5 and is not
  re-diffed against develop. ADR-196's rule to re-diff it on every pin move
  lapses with this ADR.
- **Conflict with entry 449.** Entry 449's Settled line ("the pin stays 7beaaf5
  until Robert decides to move it", our wording, not a quote from Sergeant D)
  and its Open item ("whether to move the pin to 5.0.0 (Robert)") are
  overtaken by the maintainer directive of 2026-11-21. The directive goes
  further than a pin move: it demotes the reference to rank 4. Sergeant D's own
  words in entry 449 are information only and do not conflict.
- **Parity tests check nothing now.** `tests/test_sat_classifier_parity.py`
  (`OK (skipped=5)`), `tests/test_sat_cycle_classifier.py` (42 run, the
  `ThermoNovaOracle` class skipped), `tests/test_thermo_nova_pin.py`
  (`OK (skipped=1)`) and `scripts/inspect_sat_reference.py:33` read
  `other-projects/SAT-releases-thermo-nova/`, which is gone (runs 2026-11-21).
  `run_tests.py` reports them green while the parity checks do not run.
  Whether they are pointed at `SAT-develop/`, at a 7beaaf5 checkout, or
  retired is Robert's decision (open).
- More judgement per question to Sergeant D: a comparison with pros, cons and a
  recommendation costs more than citing one line. Mitigation: the second-reader
  rule and the one-question-per-message rule stay.

## Open Questions

- [ ] Robert: record the 2026-11-21 directive as a numbered entry in
  `docs/sat/decisions/sergeant-d-corrections.md` (rank 1 names Robert's SAT decisions,
  but this one is only in the rule block and the directive changelog).
- [ ] Robert: the parity tests and `scripts/inspect_sat_reference.py` (point at
  `other-projects/SAT-develop/`, at a 7beaaf5 checkout, or retire).
- [x] Robert: accept this ADR (accepted 2026-11-21); on acceptance write `supersedes: ["ADR-196"]`
  here, mark ADR-196 Superseded by ADR-292 and add the README note.

## Related Decisions

- **ADR-196 (SAT behaviour ground truth, Accepted)**: superseded on acceptance
  of this ADR; its citation, recording and questioning rules carry over, its
  ranking and pin rule do not.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is
  guideline-level under it; the test is a text drift guard.
- **ADR-085 (SAT Smart Autotune Thermostat integration)**: the umbrella
  decision that brought SAT in as a port; this ADR says the port is now our own
  implementation.
- **ADR-197 (SAT PID v3), ADR-198 (SAT heating curve), ADR-199 (SAT cold state
  and CH (central heating) enable rule)** and **ADR-253 (SAT tolerance band restarts)**: SAT ADRs
  that cite 7beaaf5 under ADR-196; their citations stay as historical
  references and are not rewritten.

## References

- Maintainer directive, 2026-11-21: SAT-GROUND-TRUTH block in
  `.claude/rules/sat-ground-truth.md` and AGENTS.md;
  `docs/process/directive-changelog.md` lines 17 and 36.
- `docs/sat/decisions/sergeant-d-corrections.md`: entry 441 (line 8683), entry 448
  (line 8802), entry 449 (line 8823,
  https://discord.com/channels/812969634638725140/1105556725714649128/1573705702512398387).
- `other-projects/SAT-develop/UPSTREAM_COMMIT` (ba515c8, develop, 2026-11-21).
- `tests/test_sat_ground_truth_block.py` (drift guard).
- `tests/test_sat_classifier_parity.py`, `tests/test_sat_cycle_classifier.py`,
  `tests/test_thermo_nova_pin.py`, `scripts/inspect_sat_reference.py:33` (read
  the removed folder).
- Precedent for Proposed successors: commit 057535397 (ADR-197 to ADR-199).
- Upstream: https://github.com/Alexwijn/SAT/tree/develop

The decision governs how SAT decisions are sourced, documented and reviewed. A
code diff cannot show whether the right entry was consulted or whether a
comparison was factual, so this ADR has no code surface and no Enforcement
block.
