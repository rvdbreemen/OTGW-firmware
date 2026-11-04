---
id: TASK-1626
title: Run evaluate and ADR-governance CI on pushes to dev
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T22:39:00Z'
updated_date: '2026-11-04T23:32:00Z'
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
- [ ] #1 evaluate.yml and adr-governance.yml also trigger on push to dev
- [ ] #2 A push to dev produces a visible run; link the green run in the task notes
- [ ] #3 Decision on build.yml on push recorded in the notes with its measured run time
- [ ] #4 docs/process/agentic-setup.md finding F2 updated to the new state
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. No workflow assumes a PR context (no github.event.pull_request.* or base-ref diff): evaluate.yml, adr-governance.yml and build.yml only check out and run. Added push triggers on dev: evaluate.yml (all paths), adr-governance.yml (same paths as the PR trigger), build.yml with paths-ignore docs/**, backlog/**, .claude/**, **/*.md. Decision on build.yml (AC3), Robert 2026-11-05 via AskUserQuestion: build on push to dev except docs-only pushes. Measured run time (PR #696, head b00cfaf50): pio run esp32 2m17s, esp32-classic 2m56s, esp32-combo 2m35s, in parallel; evaluate 10s. YAML parsed with PyYAML: all three have pull_request and push keys.
<!-- SECTION:NOTES:END -->
