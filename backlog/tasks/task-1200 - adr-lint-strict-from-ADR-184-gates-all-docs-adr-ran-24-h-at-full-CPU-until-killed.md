---
id: TASK-1200
title: >-
  adr-lint --strict-from ADR-184 --gates all docs/adr ran 24 h at full CPU until
  killed
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 09:06'
updated_date: '2026-10-03 10:49'
labels:
  - tooling
  - adr
dependencies: []
priority: low
ordinal: 322000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-10-03. A python process ran 'bin/adr-lint --strict-from ADR-184 --gates all docs/adr' (interpreter /c/Tools/Codex/python, started 2026-10-02 11:05:22 from a bash.exe whose own parent was gone). It burned about 85,800 CPU-seconds, one core continuously for about 24 h. Nothing was waiting on it, so it was killed (PIDs 26232 and 44264).
The commit hooks that ran adr tooling today finished normally, so the hang looks specific to this invocation: --strict-from ADR-184 with --gates all. The memory notes an earlier ReDoS in the ADR tools (adr-retire) and an adr-judge regex budget.
While it ran it also loaded the host during the TASK-1162 storm measurements.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The hang is reproduced with a wall-clock limit (e.g. timeout 300) on the same command, or shown not to reproduce. The stuck gate and the ADR file are named
- [x] #2 Root cause fixed in the tool (bounded regex, timeout or iteration cap), or reported upstream in adr-kit with a minimal reproduction if the code lives there
- [x] #3 The same command finishes within its normal runtime on the current docs/adr
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
bin/adr-lint (vendored from adr-kit v0.15.0) hung in the policy gate. ENFORCEMENT_BLOCK_RE = '^##\s+Enforcement\s*$\n+(?:.*?\n)*?```json\s*\n(.*?)\n```' with DOTALL nests a quantifier: under DOTALL, .*? also matches newlines, so (?:.*?\n)*? has exponentially many ways to cut the rest of the file into 'lines'. When an Enforcement section holds no json block the search can never succeed and runs through all of them. ADR-116's Enforcement section is prose only, and that kept a core busy for 24 h. adr-kit 0.36+/0.57 (adr_catalog.py) already uses a single lazy .*?, and the vendored regex now matches that. A full vendor upgrade is out of scope.

Evidence per AC:
- AC#1: timeout 300 python bin/adr-lint --strict-from ADR-184 --gates all docs/adr gave rc 124. Per gate (120 s limit each): only 'policy' hung; the other six finished in about 1 s. faulthandler after 20 s showed _extract_enforcement_block, ENFORCEMENT_BLOCK_RE.search (bin/adr-lint:430). With a 5 s limit per file, only ADR-116-mqtt-on-change-publishing-default.md hung (its Enforcement section has no json fence); ADR-120 took 2.1 s.
- AC#2: the fixed regex is identical to upstream. Over all 185 ADRs, old and new give the same result for 184 files (all 33 json blocks found by both); the old one hangs on ADR-116, the new one returns None there. Regression test tests/test_adr_lint_enforcement_regex.py runs each search in a subprocess with a 10 s limit. RED on the HEAD copy ('did not finish within 10 s'), GREEN on the fix (2 tests, 0.37 s).
- AC#3: the same command now finishes in 1.5 s (rc 1 = findings, a verdict, not a hang); the policy gate alone takes 0.8 s.
Upstream needs no report: adr-kit no longer carries this pattern.
<!-- SECTION:FINAL_SUMMARY:END -->
