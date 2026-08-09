---
id: "ADR-089"
title: "Judge ADRs declaratively per commit and semantically once a week"
status: "Proposed"
date: "2026-08-09"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "madr"
topics:
  - "tooling"
  - "pre-commit"
  - "adr-kit"
  - "developer-experience"
aliases:
  - "adr judge weekly pass"
  - "llm_enabled false with --llm override"
  - "one model call per ADR"
components:
  - ".githooks/pre-commit"
  - "docs/adr/.adr-kit.json"
  - "scripts/adr-judge-weekly.py"
symbols:
  - "judge.llm_enabled"
  - "judge.backend"
  - "ADR_KIT_NO_LLM"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-089 Judge ADRs declaratively per commit and semantically once a week

## Status

Proposed, 2026-08-09.

## Status History

```yaml
status_history:
  - date: 2026-08-09
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

adr-kit runs two passes over a staged diff. The declarative pass applies the
regex rules in each ADR's `Enforcement` block and is cheap. The LLM pass reads
the diff against the `Decision` text of every ADR carrying `llm_judge: true`, and
exists for decisions whose correctness is semantic rather than pattern-matchable.

The hook was disabled outright on 2026-05-31 by a bare `exit 0` at the top of
`.githooks/pre-commit` (commit `d86a2cd62`). Upgrading to plugin 0.47.0 replaced
that wrapper and made the cost question live again.

Three measured facts define the problem.

**The judge makes one model call per ADR, sequentially.** It used to batch every
`llm_judge` ADR into a single call; that was removed as a security fix (TASK-63),
and `bin/adr-judge` now states "one call per Accepted ADR on every commit"
(line 1208) and "each ADR gets its own" (line 1284). There is no thread pool or
`concurrent.futures` anywhere in the script, so the calls run one after another.

**Almost nothing is scoped.** Of the 68 ADRs the LLM pass evaluates (Accepted,
with an Enforcement block, `llm_judge` not reasoned off — the same set the
pre-commit hook reports as "checked 68 ADR(s)"), **58 declare no `path_glob` at
all**: their Enforcement block is `{"llm_judge": true}` with no rules. An ADR
without a scope is in scope for every commit, including one with an empty diff.
Ten are bounded. Counting this correctly requires reading Status from BOTH
frontmatter and the `## Status` prose section: most 1.x ADRs carry the latter,
and a frontmatter-only reader classifies a Superseded ADR as judgeable.

This is not visible by grepping. `llm_judge` defaults to TRUE as of 0.47.0, and
the migration removed the explicit `"llm_judge": false` rather than writing
`true`, so an opted-in ADR usually contains no `llm_judge` key at all.

**A call costs 20 to 28 seconds, and ADR size barely moves it.** Empirical
sample of 9 ADRs spanning a 21x range of Decision length (415 to 8722 chars),
each judged in isolation against the same 436 KB week diff: 20.2 s min, 21.7 s
median, 22.6 s mean, 27.8 s max, stdev 2.5 s. A 21x larger ADR costs 1.38x the
time, because most of a call is CLI startup rather than inference — a trivial
`claude -p "Answer with the single word: ok"` takes 23.5 s by itself. The
runner's per-call timeout is therefore 180 s (6.5x the observed max): bounded,
not strict, since the observed failure mode was never a slow call but many
sequential ones.

Multiplied out: 58 unscoped ADRs at ~23 s each is about 22 minutes of blocking
on **every** commit, including one that changes a single comment. That is what
an earlier observation of "an empty diff did not return within 90 s" actually
was: the first few of dozens of sequential calls.

## Decision Drivers

* A pre-commit hook that can block for 20 minutes gets bypassed with
  `--no-verify`, which also disables the cheap pass that does work.
* The semantic ADRs carry real signal; moving them off the commit path must not
  mean dropping them.
* A 20-minute run that prints nothing until it finishes is indistinguishable from
  a hung one.
* Whatever runs weekly must be readable by a human afterwards, not just an exit
  code.

## Considered Options

* Option A: declarative pass at commit time; the full semantic pass runs once a
  week over the week's diff, one call per ADR, reporting per ADR as it goes.
* Option B: enable the LLM pass in the hook and accept the latency.
* Option C: leave the hook disabled entirely, as it was since May.
* Option D: give all 58 unscoped ADRs a `path_glob` so the hook only calls for
  ADRs the commit actually touches.
* Option E: switch to a faster backend (OpenRouter or local Ollama) and keep the
  pass in the hook.

## Decision Outcome

Chosen option: **Option A**, because the cost is a per-ADR multiplier rather than
a per-call latency, so it belongs on a cadence where 20 minutes is free rather
than on the commit path where 20 seconds already is not.

Commit time keeps `judge.llm_enabled: false` in `docs/adr/.adr-kit.json`. The
declarative pass blocks on rule violations and completes in about 0.6 s.

The weekly pass is `scripts/adr-judge-weekly.py`. It derives the diff for a
window (7 days by default), then invokes `bin/adr-judge --llm
--dry-run-enforcement ADR-NNN` once per ADR. `--llm` outranks the config's
`llm_enabled: false` for that invocation only, so the hook keeps reading the
config and stays fast. Each ADR's verdict, elapsed time and scoped-or-not status
prints as soon as that ADR finishes, and a markdown report lands in
`logs/adr-judge-weekly.md`.

The pass throttles itself rather than relying on a scheduler, and it does so
**per ADR**. The tracked stamp file `docs/adr/.adr-judge-last-run.json` records,
for each ADR, when it was last judged, what the verdict was, and against which
commit. A run judges only the ADRs whose record is older than the interval (or
absent), so inside the interval the runner exits in under half a second, CI can
invoke it on every build, and the real cost is paid once a week.

Per ADR rather than per run, because that is what survives partial work: an ADR
stamps itself the moment it reaches a verdict, so an interrupted 25-minute sweep
keeps everything it established; an ADR that times out stays due while its
neighbours stay fresh; and a newly added ADR has no record and is judged on the
next invocation instead of waiting out someone else's interval. Only a real
verdict (OK or VIOLATION) stamps — a timeout or lookup failure never does. The
stamp also remembers the verdict: a skip over a recorded VIOLATION replays it
and exits non-zero, so a failing week cannot go green by waiting. `--force`
ignores every timestamp and judges the full set; `--only ADR-NNN` re-checks one
without resetting anything else.

The same design has been proposed upstream as adr-kit TASK-146 (with the
advisory-per-machine versus tracked-state tension named explicitly); this
runner is the bridge until that lands. Two upstream improvements from this
work already merged in adr-kit PR #82: the migration now reports the whole-set
cost picture (`summary: {judged_after, unbounded_after}`), and the upgrade
skill finishes the backend setup and forces the per-commit-versus-cadence
choice.

`judge.backend = host` with `host_client = claude-code-cli` stays as configured.
The backend is not the bottleneck: at 58 sequential calls, even a 2-second
backend costs over two minutes per commit, which is still unacceptable at commit
time and irrelevant once a week.

### Confirmation

`python scripts/adr-judge-weekly.py --only ADR-085` completed in 25.8 s against a
436 KB week diff, with the judge reporting "LLM pass evaluated 1 ADR(s) in
isolation" and writing the report. `git commit` runs the declarative pass and
reports `checked 68 ADR(s) with Enforcement blocks`, `OK — 0 violations, 58
advisory`; two commits on 2026-08-09 took 21 s and under 1 s.

## Decision Contract

### Must

* `.githooks/pre-commit` is the adr-kit wrapper with no early `exit 0` above the
  `ADR_KIT_HOOK_DISABLE` guard.
* `judge.llm_enabled` stays `false`, so the commit path never makes a model call.
* The weekly pass makes exactly one isolated call per ADR and prints that ADR's
  verdict before starting the next.
* The weekly pass writes a report a human can read after the fact.
* Stamps are per ADR and written only on a real verdict (OK or VIOLATION). A
  timeout or lookup failure leaves that ADR due while others keep their fresh
  record; an interrupted run keeps every verdict it reached.
* A recorded VIOLATION keeps the outcome non-zero, on runs and on skips, until a
  re-judge clears it.

### Must Not

* Do not enable `judge.llm_enabled` while unscoped ADRs outnumber scoped ones.
  That is the condition that makes the commit path unbounded, not the backend.
* Do not disable the hook by editing the wrapper; `/adr-kit:install-hooks`
  refreshes it and a hand-edit is silently reverted. Use `ADR_KIT_HOOK_DISABLE=1`
  for one commit, or `--uninstall` deliberately.
* Do not re-introduce `judge.llm_cmd` or `judge.llm_model`. Both are retired in
  0.47.0 and degrade silently instead of failing.
* Do not commit `docs/adr/.adr-kit.local.json`; it names one machine's signer.

### Exceptions

* `ADR_KIT_NO_LLM=1` and `ADR_KIT_HOOK_DISABLE=1` remain available per commit.
* `--only ADR-NNN` re-checks a single ADR after a fix without a full pass.

### Verification

* `docs/adr/.adr-kit.json`: `judge.llm_enabled` is `false`.
* `python scripts/adr-judge-weekly.py --only ADR-085` exits 0 and writes
  `logs/adr-judge-weekly.md`.
* `.gitignore` covers `docs/adr/.adr-kit.local.json`.

## Consequences

### Positive

* Commits are gated again after ten weeks with no check at all, at a cost that
  does not show up in the workflow.
* Every ADR is judged weekly, including the 58 that no regex can guard, which is
  strictly more semantic coverage than the batched pre-commit design ever gave.
* A long run is legible: the reader sees which ADR is being judged, what it said,
  and how long it took, instead of a silent process.

### Negative

* A violation is now found up to a week after it lands rather than at the commit
  that introduced it. Mitigation: the declarative rules still block immediately
  where a rule exists, and the weekly report names the ADR so the offending
  change is findable.
* The weekly pass takes roughly 20 to 28 minutes wall-clock. Mitigation: it is
  unattended and interruptible, and `--only` covers targeted re-checks.
* Scheduling is not part of this decision, so the pass only runs when invoked.
  Mitigation: named as an open question below rather than assumed.
* `--llm` in the runner diverges from the committed config, so reading
  `.adr-kit.json` alone understates what the project checks. Mitigation: this ADR
  and the runner's module docstring both state it.

## Pros and Cons of the Options

### Option A

* Good, because it puts a per-ADR multiplier on a cadence that can absorb it.
* Good, because per-ADR reporting turns a 20-minute silence into a progress log.
* Bad, because feedback latency grows from one commit to one week.

### Option B

* Good, because it is the tool's intended full configuration.
* Bad, because 58 unscoped ADRs at roughly 23 s each is about 22 minutes per
  commit, measured, not estimated.

### Option C

* Good, because it needs no decision.
* Bad, because it also discards the declarative pass, which is fast and was never
  the problem, and leaves a hand-edited wrapper every upgrade flags as stale.

### Option D

* Good, because it would make the hook's cost proportional to the commit, which
  is the theoretically right answer.
* Bad, because it requires authoring a defensible `path_glob` for 58 Accepted
  ADRs, and Accepted ADRs are immutable except for their Status line, so most
  would need superseding. Worth doing incrementally for new ADRs; not a
  precondition for having a working judge now.

### Option E

* Good, because per-call latency would drop; most of the measured 18 to 26 s is
  CLI startup, not inference, so an HTTP backend would plausibly reach a few
  seconds.
* Bad, because the calls are sequential and there are 58 of them: a 3-second
  backend still costs over three minutes per commit. It also sends every diff to
  a third party and adds a per-token cost, in exchange for making a weekly job
  faster than it needs to be.

## Open Questions

* [x] Why does the `host` backend not return promptly on an empty diff, when no
  ADR should be in scope? — **Answered 2026-08-09 by User: Robert van den
  Breemen:** 58 of the 68 judged ADRs declare no `path_glob`, so they are in
  scope for every diff including an empty one. The run was not hung; it was
  working through dozens of sequential calls at roughly 23 s each. (Interim
  counts of 65 and 74 in earlier drafts were reader errors: the first required
  an explicit `llm_judge: true` where absence means true, the second missed
  Status recorded in prose rather than frontmatter.)
* [x] Should the weekly pass be scheduled (cron, CI job, or a reminder)? —
  **Answered 2026-08-09 by User: Robert van den Breemen:** no external scheduler.
  The runner throttles itself on a stamp file, so CI calls it on every build and
  it decides for itself whether the interval has elapsed. A skip costs a file
  read.
* [ ] Should the stamp be committed by CI after a real pass, or held in a runner
  cache via `$ADR_JUDGE_STAMP`? Committing shares the interval across every
  checkout, which is the point; a cache keeps CI from writing to the repository.
  Until this is settled the stamp is tracked and updated by whoever ran the pass.

## Related Decisions

* None.

## References

* `bin/adr-judge` line 1208 ("one call per Accepted ADR on every commit") and
  line 1284 ("each ADR gets its own"); line 26 records that batching was removed
  as a security fix.
* `bin/adr-judge` lines 2573 to 2587: LLM mode precedence, `ADR_KIT_NO_LLM` then
  `--llm` then `judge.llm_enabled`.
* `bin/adr-judge` line 2165: `llm_cmd` and `llm_model` listed as retired.
* `.githooks/pre-commit` lines 187 to 202: the performance-budget comment and its
  per-call reasoning.
* `scripts/adr-judge-weekly.py`: the weekly pass.
* Commit `d86a2cd62`, 2026-05-31, which disabled the hook.
* Commit `20211f16`, 2026-08-09, the 0.47.0 footprint upgrade.
