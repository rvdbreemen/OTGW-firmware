---
id: TASK-1082
title: 'Fix: settings payload longer than 149 chars silently truncated and saved as OK'
status: Done
assignee:
  - '@claude'
created_date: '2026-08-24 19:03'
updated_date: '2026-08-25 17:52'
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed silent truncation of settings values. A webhook payload template longer than 149 characters was stored cut off mid-JSON while the REST API answered HTTP 200 Saved.

Two independent causes, both removed: postSettings read into char newValue[150] while settings.webhook.sPayload is 201 bytes, and extractJsonField's quoted-string branch discarded what did not fit yet still returned true. The unquoted branch already returned false on overflow, so the two halves of one function disagreed about the same error; the quoted branch now matches, returning false and clearing the destination so a caller ignoring the return value gets nothing rather than half a value.

Verified with a host-compiled harness under test/ that includes the real jsonStuff.ino rather than a copy: 4 of 18 checks fail against the source at HEAD, all 18 pass with the fix, same test code in both runs. Build exit 0, evaluator 35/37 with 0 failures. Cost: 51 bytes of stack in one HTTP handler.

Follow-up for the changed return contract at the commands endpoint is TASK-1083. Shipped in v1.7.5-beta.2.
<!-- SECTION:FINAL_SUMMARY:END -->
