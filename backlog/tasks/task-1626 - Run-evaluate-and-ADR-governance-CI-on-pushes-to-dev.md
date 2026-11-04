---
id: TASK-1626
title: Run evaluate and ADR-governance CI on pushes to dev
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:39:00Z'
updated_date: '2026-11-04T23:55:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: high
ordinal: 753000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F2. .github/workflows/build.yml, evaluate.yml and adr-governance.yml trigger only on pull_request, but all work is committed and pushed directly to dev. The main line is therefore never checked by CI; the only objective gate is local (git hooks that are opt-in per clone and can be bypassed with env vars).
Keep the scope cheap: evaluate + ADR governance on push; the firmware build on push only if the run time is acceptable (measure it).
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.yml and adr-governance.yml also trigger on push to dev
- [x] #2 A push to dev produces a visible run; link the green run in the task notes
- [x] #3 Decision on build.yml on push recorded in the notes with its measured run time
- [x] #4 docs/process/agentic-setup.md finding F2 updated to the new state
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. No workflow assumes a PR context (no github.event.pull_request.* or base-ref diff): evaluate.yml, adr-governance.yml and build.yml only check out and run. Added push triggers on dev: evaluate.yml (all paths), adr-governance.yml (same paths as the PR trigger), build.yml with paths-ignore docs/**, backlog/**, .claude/**, **/*.md. Decision on build.yml (AC3), Robert 2026-11-05 via AskUserQuestion: build on push to dev except docs-only pushes. Measured run time (PR #696, head b00cfaf50): pio run esp32 2m17s, esp32-classic 2m56s, esp32-combo 2m35s, in parallel; evaluate 10s. YAML parsed with PyYAML: all three have pull_request and push keys.

Push evidence on dev. Push 0e1abb524 (this task): 'evaluate.py CI gates' event push success (run 41936345028); 'PlatformIO firmware build' event push started; 'ADR governance' event push FAILED (run 41936344866): lint --strict exit 1 on a pre-existing WARN (ADR-62), invisible until now because no PR touched docs/adr. Fixed in TASK-1638. Push a29b9903e: evaluate push success (41937030743), ADR governance push success (41937030771), PlatformIO firmware build push success (41937030596), CodeQL success.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
evaluate.yml, adr-governance.yml and build.yml now also run on pushes to dev; build.yml skips docs-only pushes (Robert's choice, measured 2m17s-2m56s per target). AC1/AC2: push-triggered runs on dev, all green on a29b9903e (evaluate 41937030743, ADR governance 41937030771, build 41937030596). AC3: build decision and run times in the notes. AC4: finding F2 marked resolved in docs/process/agentic-setup.md. The very first push run already caught a hidden red: ADR lint --strict failed on ADR-62, fixed in TASK-1638.
<!-- SECTION:FINAL_SUMMARY:END -->
