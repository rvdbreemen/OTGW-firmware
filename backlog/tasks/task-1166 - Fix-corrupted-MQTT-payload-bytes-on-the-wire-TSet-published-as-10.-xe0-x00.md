---
id: TASK-1166
title: 'Fix: corrupted MQTT payload bytes on the wire (TSet published as 10.\xe0\x00)'
status: To Do
assignee: []
created_date: '2026-09-26 15:04'
labels:
  - bug
  - mqtt
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 235000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #682, 2026-09-26, on 1.7.6-beta.5) saw Home Assistant reject a payload on OTGW/value/<id>/TSet: b'10.à
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The code path that publishes TSet is traced from formatting to write, with file:line, and every buffer it passes through is shown to outlive the write (or the defect is identified)
- [ ] #2 The failure is reproduced on the bench or the mechanism is demonstrated in a host test, before any fix
- [ ] #3 Fix verified: after the fix, a forced short write/retry on the payload path cannot put bytes on the wire that differ from the formatted payload
- [ ] #4 Reporter informed on GH #682
<!-- AC:END -->
