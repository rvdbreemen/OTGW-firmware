---
id: TASK-1082
title: 'Fix: settings payload longer than 149 chars silently truncated and saved as OK'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 19:03'
updated_date: '2026-08-24 19:04'
labels:
  - bug
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning'
  - marceld91d
  - '2026-08-24'
  - msg 1541374278895796284
priority: medium
ordinal: 184000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while investigating the Discord webhook report (marceld91d, 2026-08-24). postSettings read setting values into char newValue[150] while settings.webhook.sPayload is 201 bytes and the web UI maxlen is 200. extractJsonField's quoted-string branch gated storing on ri < resultSize-1 but advanced the read pointer regardless and returned true, so an over-long payload template was stored truncated mid-JSON while the API returned HTTP 200 Saved. The unquoted branch already returned false correctly on overflow; only the quoted branch was wrong.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 extractJsonField returns false when a quoted value does not fit the destination buffer, instead of returning a truncated value with true
- [x] #2 postSettings accepts the full 200-character payload and surfaces a 400 error via sendApiError when the value is too long, instead of reporting Saved
- [x] #3 A host-compiled validation harness exercises the real extractJsonField (not a copy) and demonstrably fails before the fix and passes after
- [x] #4 All callers of extractJsonField audited for the changed return contract; any behaviour change outside this defect documented
<!-- AC:END -->
