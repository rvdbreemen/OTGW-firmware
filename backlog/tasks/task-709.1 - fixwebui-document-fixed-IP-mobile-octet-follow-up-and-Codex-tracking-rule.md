---
id: TASK-709.1
title: 'fix(webui): document fixed-IP mobile octet follow-up and Codex tracking rule'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 15:05'
updated_date: '2026-05-26 15:16'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Record the fixed-IP responsive/entry correction already present in the working tree as the follow-up to TASK-709.
2. Align AGENTS.md with CLAUDE.md so Codex must track work in Backlog before editing and use the pickup/closure workflow.
3. Re-run relevant validation, attach branch/agent and evidence notes, check acceptance criteria, and close this follow-up when verified.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev. Coding agent: Codex (GPT-5).

Tracking correction: this task is the retrospective child of TASK-709 because the fixed-IP mobile correction was started before a Backlog item existed. AGENTS.md has now been aligned with CLAUDE.md so Codex must track work before edits, activate tasks under @codex, record retrospective gaps, and close only with validation and metadata.

Implemented UI scope: index.js uses text inputs with numeric mobile keyboards, maxlength=3, 0-255 normalization, keyboard/paste movement, save-time validation, and post-attachment initial IP splitting. index_common.css keeps each four-octet address row on one line with fixed-size octets at smartphone widths. index_dark.css supplies the fixed-IP section styling in dark theme.

Validation evidence (2026-05-26): node --check src/OTGW-firmware/data/index.js passed; git diff --check passed with only line-ending notices; python evaluate.py --quick reported 36 total checks, 34 passed, 0 warnings, 0 failures, 2 info, health score 100.0%; python build.py --no-color completed both firmware and LittleFS builds successfully. The build's generated version.h/version.hash refresh was removed afterward because it was a validation side effect outside this task scope.

Browser evidence: a temporary localhost fixture with mocked /api/v2/settings was exercised using headless Playwright Chromium. At 360px light, 1280px light, and 320px dark, all five fixed-IP address groups stayed within the viewport with all four octets on one line; smartphone octets were 40px each and stored values rendered initially. Dark mode used the fixed-IP dark styling. Interaction assertions passed for 9999 clamping to 255, maxlength=3, numeric input mode, three-digit/dot forward navigation, and blocking Save with an incomplete required IP address. Temporary browser fixture files and test-results were removed.
<!-- SECTION:NOTES:END -->
