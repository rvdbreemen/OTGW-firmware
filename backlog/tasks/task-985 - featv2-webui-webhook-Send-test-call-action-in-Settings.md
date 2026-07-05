---
id: TASK-985
title: 'feat(v2-webui): webhook Send-test-call action in Settings'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-01 22:24'
updated_date: '2026-07-04 04:23'
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
- [x] #1 Send test call fires POST /api/v2/webhook/test and renders success/failure + timestamp
- [x] #2 Guard message when webhook disabled
- [x] #3 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by impl-985: webhookTestRow() appended to the webhook .set-group inside renderSettings (rebuild-lifecycle safe, not a one-shot IIFE), DOM/textContent. Disabled-guard via #setCols .srow[data-key=webhookenable] input.sw. Fires REAL POST /api/v2/webhook/test?state=on (verified restAPI.ino:842 handleWebhook; state param mandatory else 400; returns 200 {status:ok} fire-and-forget - remote HTTP result not exposed, so success line is honest '✓ ok · ts' not the mockup's fabricated '200 OK · POST url'). credentials:same-origin (ADR-056), 401/403 auth hint. CSS: only .wh-actions/.wh-status/.wh-status.ok, button uses existing .tbtn. v2.html untouched. evaluate.py 0 FAIL, drift gate clean, parse OK. On-device verify pending.
<!-- SECTION:NOTES:END -->
