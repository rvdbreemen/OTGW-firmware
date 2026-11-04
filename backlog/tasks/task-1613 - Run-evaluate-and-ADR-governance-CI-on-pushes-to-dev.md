---
id: TASK-1613
title: Run evaluate and ADR-governance CI on pushes to dev
status: To Do
assignee: []
created_date: '2026-11-04 21:50'
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
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1611.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 evaluate.yml and adr-governance.yml also trigger on push to dev
- [ ] #2 A push to dev produces a visible run; link the green run in the task notes
- [ ] #3 Decision on build.yml on push recorded in the notes with its measured run time
- [ ] #4 docs/process/agentic-setup.md finding F2 updated to the new state
<!-- AC:END -->
