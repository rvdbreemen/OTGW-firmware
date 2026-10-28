---
id: TASK-1430
title: >-
  Fix: PIC reset_cause published as a raw 0xFF byte, Home Assistant cannot
  decode it (GH #688)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T20:25:00Z'
updated_date: '2026-10-28T21:05:00Z'
labels:
  - bug
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/688
priority: medium
ordinal: 257000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #688, 2026-10-25, 1.7.8-beta.5+0de3b19, PIC 6.8): otgw-pic/settings/reset_cause carries the single byte 0xFF. HA logs 'Can't decode payload b'\xff' ... with encoding utf-8' (106x in 10 h) and the entity stays unknown. Same root cause as dev TASK-1406 (PR=Q reply stored verbatim in state.picSettings.sResetCause; published at OTGW-Core.ino:861/910 and served at restAPI.ino:1514, so /api/v2 JSON is invalid UTF-8 too). 1.x sibling of TASK-1406.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A non-printable or non-ASCII PR=Q reply is published on MQTT and served on /api/v2 as readable ASCII (e.g. unknown or hex), proven by a host test with a 0xFF answer that is red on HEAD
- [ ] #2 Valid printable reset causes are published unchanged
<!-- AC:END -->
