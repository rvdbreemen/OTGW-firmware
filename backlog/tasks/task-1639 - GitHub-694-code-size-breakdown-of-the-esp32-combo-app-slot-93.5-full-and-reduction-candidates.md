---
id: TASK-1639
title: >-
  GitHub #694: code size breakdown of the esp32-combo app slot (93.5% full) and
  reduction candidates
status: To Do
assignee: []
created_date: '2026-11-04T23:51:00Z'
updated_date: '2026-11-04T23:57:00Z'
labels:
  - github
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 765000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/694 by andrebrait. Measured breakdown of the 2240 KiB app0 slot (esp-idf-size, nm) and ranked reduction candidates, none implemented. Proposal: needs Robert's go before any work.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): released: implement the top-3 low-risk candidates, each measured before/after with the size tool and tested separately.
<!-- SECTION:NOTES:END -->
