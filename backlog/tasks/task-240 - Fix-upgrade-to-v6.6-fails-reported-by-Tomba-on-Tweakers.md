---
id: TASK-240
title: 'Fix: upgrade to v6.6 fails (reported by Tomba on Tweakers)'
status: Done
assignee: []
created_date: '2026-04-09 16:44'
updated_date: '2026-04-18 07:58'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://gathering.tweakers.net/forum/list_message/85026024#85026024'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User Tomba reports that upgrading to firmware version 6.6 fails. No error details provided - post contains screenshots (not available in RSS). Previous post from same user shows they were on version 0.10.2+50c3ed2 and asking if it was safe to upgrade.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Identify root cause of upgrade failure to v6.6
- [ ] #2 Fix is verified to work by reporter or reproducible locally
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-09: Tomba reports upgrade to v6.6 fails. Post contains screenshots that are not visible in RSS feed. No error text available yet. Waiting for: actual error message or screenshot content from Tweakers thread.

2026-04-17: TASK-269 was duplicate, merged here. Both still needs-info - waiting for actual error message from Tomba. Cannot investigate without error details.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed without fix. The report on Tweakers (user Tomba, 2026-04-09)
contained only screenshots that never came through the RSS feed; no
error text was ever posted. Two needs-info attempts (2026-04-09 and
2026-04-17) asked for the actual error message and received no
follow-up. Without reproduction details or a screenshot transcript
there is nothing concrete to investigate and nothing to verify against.

If the same or a similar PIC v6.6 upgrade failure surfaces again with
an actual error log, open a fresh task: the underlying firmware has
moved on since this report (2.0.0-beta streaming discovery, core
3.1.2, SimpleTelnet) and any new data should be evaluated against the
current codebase rather than glued onto this stale thread.
<!-- SECTION:FINAL_SUMMARY:END -->
