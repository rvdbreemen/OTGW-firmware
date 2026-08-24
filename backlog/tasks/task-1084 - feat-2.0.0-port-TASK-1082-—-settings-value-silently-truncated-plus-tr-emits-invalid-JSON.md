---
id: TASK-1084
title: >-
  feat-2.0.0: port TASK-1082 — settings value silently truncated, plus {tr}
  emits invalid JSON
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 19:38'
updated_date: '2026-08-24 19:48'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: medium
ordinal: 262000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Peer of the 1.x TASK-1082. Two defects on this line.

A) extractJsonField reported success on a truncated value. Note this tree's scanner differs from 1.x: BOTH branches truncated silently here (xjfReadString tracked a 'full' flag it never reported, and the bare-token branch did strlcpy-style truncation at jsonStuff.ino:912). postSettings also read into char newValue[150] while settings.webhook.sPayload is char[201].

B) 2.0.0-only: Tr is NAN-initialised (OTGW-Core.h:73, TASK-522) and the {tr} webhook substitution emitted the literal '--', which is not valid JSON in a numeric position, so the documented example template produced {"tr":--} until a room temperature was observed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 extractJsonField returns false rather than reporting success for any value that does not fit, in both the string and the bare-token branch
- [x] #2 The destination buffer is left empty and NUL-terminated on every false return, so return-ignoring callers cannot read a partial value
- [x] #3 postSettings accepts the full 200-character payload and returns 400 for an oversized value
- [x] #4 {tr} with no reading expands to the JSON literal null; 0.0f-initialised variables are unchanged
- [x] #5 A host-compiled harness exercises the real shipped code (not a copy) and demonstrably fails before the fix and passes after
- [x] #6 All extractJsonField callers audited for the changed return contract; behaviour changes documented
<!-- AC:END -->
