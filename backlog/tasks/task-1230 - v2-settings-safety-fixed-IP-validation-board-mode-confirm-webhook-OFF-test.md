---
id: TASK-1230
title: 'v2: settings safety: fixed-IP validation, board-mode confirm, webhook OFF test'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:09'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 368000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md rows 44, 45, 48: a typo in a static IP locks the user out; changing Board mode wrongly leaves the gateway non-functional; webhook test only has ON.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fixed-IP fields are validated per octet and prefilled from the current lease
- [x] #2 Changing Board mode asks for confirmation, and the webhook OFF test call exists
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: a bad octet (300) gives a red border and tooltip, and Save posts nothing, with a toast naming the field. Board mode: confirm appears, cancel reverts. Webhook: ON and OFF test buttons; OFF with the webhook disabled shows a hint and sends nothing. Prefill from the live lease was shown in the harness. Node test tests/webui/static-ip-validation.test.mjs 5/5 (all 5 fail on the old v2.js).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 settings safety: per-octet IP validation that blocks Save, prefill from the current lease, a board-mode confirm, and a webhook OFF test. Evidence: bench, node test old-vs-fix.
<!-- SECTION:FINAL_SUMMARY:END -->
