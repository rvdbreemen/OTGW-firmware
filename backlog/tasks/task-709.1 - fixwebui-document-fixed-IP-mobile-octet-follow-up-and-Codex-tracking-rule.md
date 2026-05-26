---
id: TASK-709.1
title: 'fix(webui): document fixed-IP mobile octet follow-up and Codex tracking rule'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:05'
updated_date: '2026-05-26 15:06'
labels:
  - bug
  - webui
  - network
  - documentation
  - codex
dependencies: []
references:
  - src/OTGW-firmware/data/index.js
  - src/OTGW-firmware/data/index_common.css
  - src/OTGW-firmware/data/index_dark.css
  - CLAUDE.md
  - AGENTS.md
parent_task_id: TASK-709
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-709. On 2026-05-26 the fixed-IP segmented inputs were reported to wrap vertically on smartphones instead of remaining in the dotted four-octet form [XXX].[XXX].[XXX].[XXX] for each address type. The correction was implemented in the working tree before a Backlog task was opened because AGENTS.md did not expose CLAUDE.md's mandatory pre-code task rule. This task records and verifies the correction and aligns Codex-facing instructions so future work is tracked before edits.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each fixed-IP address type displays its four dotted octet inputs on one non-wrapping line at smartphone and desktop widths.
- [ ] #2 Each octet accepts numeric entry only, is limited to three digits and values from 0 through 255, and supports forward keyboard/mobile entry without breaking the row layout.
- [ ] #3 Existing configured fixed-IP values populate when the screen first renders, and incomplete or invalid fixed-IP values cannot be saved.
- [ ] #4 The fixed-IP section is usable in both light and dark themes.
- [ ] #5 AGENTS.md tells Codex to create or pick up a Backlog task before any code or file work and to follow the CLI pickup and closure workflow from CLAUDE.md.
- [ ] #6 Implementation notes contain branch and coding-agent metadata plus validation evidence for the UI behavior and evaluator result.
<!-- AC:END -->
