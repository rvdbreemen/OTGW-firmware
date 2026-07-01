---
id: TASK-985
title: 'feat(v2-webui): webhook Send-test-call action in Settings'
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-01 22:24'
labels: []
dependencies: []
ordinal: 197000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area settings-webhook (findings 1,2). Append a .wh-actions row (Send test call button + whStatus line) to the webhook settings group in the v2.js settings renderer (DOM-property pattern, no innerHTML). Disabled-guard when webhook disabled (enable it above first). Fire the real POST /api/v2/webhook/test; show result status + timestamp. CSS: only .wh-actions/.wh-status/.wh-status.ok (skip the dead .wh-form/.wh-row/.wh-in per YAGNI); button styled with the existing .tbtn.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Send test call fires POST /api/v2/webhook/test and renders success/failure + timestamp
- [ ] #2 Guard message when webhook disabled
- [ ] #3 python evaluate.py --quick green; build green
<!-- AC:END -->
