---
id: TASK-1200
title: >-
  adr-lint --strict-from ADR-184 --gates all docs/adr ran 24 h at full CPU until
  killed
status: To Do
assignee: []
created_date: '2026-10-03 09:06'
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
- [ ] #1 The hang is reproduced with a wall-clock limit (e.g. timeout 300) on the same command, or shown not to reproduce. The stuck gate and the ADR file are named
- [ ] #2 Root cause fixed in the tool (bounded regex, timeout or iteration cap), or reported upstream in adr-kit with a minimal reproduction if the code lives there
- [ ] #3 The same command finishes within its normal runtime on the current docs/adr
<!-- AC:END -->
