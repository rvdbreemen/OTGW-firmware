---
id: "ADR-089"
title: "Run the ADR pre-commit judge declarative-only until the LLM pass is bounded"
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
  - "adr judge llm pass disabled"
  - "pre-commit hook re-enabled"
  - "llm_enabled false"
components:
  - ".githooks/pre-commit"
  - "docs/adr/.adr-kit.json"
symbols:
  - "judge.llm_enabled"
  - "judge.backend"
  - "ADR_KIT_HOOK_DISABLE"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-089 Run the ADR pre-commit judge declarative-only until the LLM pass is bounded

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

adr-kit runs two passes over the staged diff at commit time. The declarative pass
applies the regex rules in each ADR's `Enforcement` block and is cheap. The LLM
pass reads the diff against the `Decision` text of every ADR carrying
`llm_judge: true`, and exists for decisions whose correctness is semantic rather
than pattern-matchable.

The hook was disabled outright on 2026-05-31 by a bare `exit 0` at the top of
`.githooks/pre-commit` (commit `d86a2cd62`). Upgrading the project footprint to
plugin 0.47.0 replaced that wrapper, which re-enabled both passes and made the
cost question live again.

Three facts drove the outcome:

* This repository has **65** ADRs with `llm_judge: true` after the 0.47.0
  migration turned on six that carried an explicit `false` from before it became
  the default.
* `judge.llm_timeout_seconds` bounds **one** call and defaults to 120 s. There is
  one call per opted-in ADR whose scope the diff touches. The hook's own comments
  state the ceiling is an hour precisely because "a ten-ADR project has a
  legitimate twenty-minute commit". The hook measures its wall-clock budget and
  warns; it does not kill the run.
* Measured on this machine with the `host` backend and the Claude Code CLI: an
  **empty** diff did not return within 90 s, and did not return under
  `--llm-timeout 25` either. An empty diff puts no ADR in scope, so the delay is
  not the judging itself.

The configuration was also silently stale. `llm_cmd` and `llm_model` are retired
in 0.47.0 in favour of an explicit `--set-backend`, so the project's
`judge.llm_model: "claude-sonnet-4-6"` was ignored and the judge reported "no LLM
backend is configured" while the `claude` CLI worked fine. The symptom pointed at
the wrong cause.

## Decision Drivers

* A pre-commit hook that can hang a commit is worse than no hook: it gets
  bypassed with `--no-verify`, which disables the cheap pass too.
* The declarative rules carry real signal and cost nothing measurable.
* The semantic ADRs should stay reviewable, just not on the commit path.
* Whatever is chosen must be reversible in one line once the hang is understood.

## Considered Options

* Option A: enable the hook, set `judge.backend = host`, and set
  `judge.llm_enabled = false`.
* Option B: enable the hook with the LLM pass on and accept the latency.
* Option C: leave the hook disabled by keeping the `exit 0`.
* Option D: point the judge at the local Ollama runtime instead.

## Decision Outcome

Chosen option: **Option A**, because it restores the blocking behaviour that is
both cheap and proven, without putting an unbounded call on the commit path. The
declarative pass completes in about 0.6 s and blocks on rule violations. ADRs
whose only rule is semantic are reported as advisory with a pointer to
`/adr-kit:judge` for in-session review, so their coverage moves rather than
disappears.

`judge.backend = host` with `host_client = claude-code-cli` is recorded even
though the pass is off, so re-enabling is a single boolean once the hang is
diagnosed.

### Confirmation

`git commit` runs the hook and reports `checked 68 ADR(s) with Enforcement
blocks` followed by `OK — 0 violations, 58 advisory`. Two commits on
2026-08-09 (`20211f16`, `9c6d8c04`) took 21 s and under 1 s respectively. A
no-op judge over an empty diff returns in 0.6 s.

## Decision Contract

### Must

* `.githooks/pre-commit` is the adr-kit wrapper with no early `exit 0` above the
  `ADR_KIT_HOOK_DISABLE` guard.
* `judge.llm_enabled` stays `false` until a run over an empty diff is shown to
  return within the hook's budget.
* The declarative pass keeps blocking: a rule violation exits non-zero.

### Must Not

* Do not disable the hook again by editing the wrapper. The wrapper is refreshed
  by `/adr-kit:install-hooks` and a hand-edit is silently reverted on the next
  upgrade; use `ADR_KIT_HOOK_DISABLE=1` for a single commit, or
  `--uninstall` to remove it deliberately.
* Do not re-introduce `judge.llm_cmd` or `judge.llm_model`. They are retired and
  degrade silently rather than failing.
* Do not commit `docs/adr/.adr-kit.local.json`. It holds the signer name and the
  client choice, which are facts about one machine.

### Exceptions

* `ADR_KIT_NO_LLM=1` and `ADR_KIT_HOOK_DISABLE=1` remain available per commit.

### Verification

* `docs/adr/.adr-kit.json`: `judge.llm_enabled` is `false`, `judge.backend` is
  `host`.
* `echo "" | bin/adr-judge --diff - --adr-dir docs/adr/` exits 0 promptly.
* `.gitignore` covers `docs/adr/.adr-kit.local.json`.

## Consequences

### Positive

* Commits are gated again after ten weeks with no check at all, at a cost that
  does not show up in the workflow.
* The failure mode that caused the original 2026-05-31 disable is addressed at
  the configuration level rather than by defeating the hook.
* Re-enabling the semantic pass is one boolean, with the backend already chosen
  and recorded.

### Negative

* The 65 semantic ADRs are not enforced at commit time; they report as advisory.
  Mitigation: `/adr-kit:judge` covers them in-session, and code review remains
  the backstop that carried them for the previous ten weeks.
* An advisory line per semantic ADR makes commit output long. Mitigation: the
  hook prints a single aggregate count, not one line per ADR, unless a rule
  actually fires.
* The reason for `llm_enabled: false` lives in this ADR rather than in the config
  file, so a reader of `.adr-kit.json` alone sees a bare boolean. Mitigation:
  this ADR is the searchable record, and the commit that set it names the
  measurement.

## Pros and Cons of the Options

### Option A

* Good, because it restores the cheap blocking check immediately.
* Good, because it is reversible in one line.
* Bad, because semantic enforcement moves off the commit path.

### Option B

* Good, because it is the tool's intended full configuration.
* Bad, because the measured behaviour is an unbounded wait on a commit, and the
  hook warns rather than kills. On this repository that is 65 potential calls at
  a 120 s per-call bound.

### Option C

* Good, because it needs no decision.
* Bad, because it also discards the declarative pass, which is fast and was never
  the problem, and it leaves a hand-edited wrapper that every upgrade flags as
  stale.

### Option D

* Good, because a local runtime has no marginal cost and keeps diffs off the
  network. The machine has an RTX 3080 and an embedding runtime already.
* Bad, because it does not address the hang: the delay was measured with no ADR
  in scope, so it is not a property of the model. Judgement quality on subtle ADR
  semantics is also lower than the alternative, which matters for the pass whose
  entire purpose is nuance.

## Open Questions

* [ ] Why does the `host` backend not return on an empty diff, when no ADR is in
  scope and therefore no call should be made? Resolving this is the precondition
  for flipping `judge.llm_enabled` back to `true`.

## Related Decisions

* None.

## References

* `.githooks/pre-commit` lines 187 to 202: the performance-budget comment stating
  the hour ceiling and its per-call reasoning.
* `bin/adr-judge` line 2165: `llm_cmd` and `llm_model` listed as retired keys.
* Commit `d86a2cd62`, 2026-05-31, which disabled the hook.
* Commit `20211f16`, 2026-08-09, "chore(adr-kit): upgrade the project footprint
  to plugin 0.47.0", which carries the measurements in its body.
* adr-kit plugin 0.47.0, `templates/githooks/pre-commit`.
