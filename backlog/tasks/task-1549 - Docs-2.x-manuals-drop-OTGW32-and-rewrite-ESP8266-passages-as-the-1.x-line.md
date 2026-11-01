---
id: TASK-1549
title: 'Docs: 2.x manuals drop OTGW32 and rewrite ESP8266 passages as the 1.x line'
status: To Do
assignee: []
created_date: '2026-11-01T17:57:00Z'
labels:
  - docs
dependencies: []
ordinal: 703000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01 (AskUserQuestion after TASK-1548): one cleanup task. docs/manuals EN+NL on dev name OTGW32 1-22x per chapter and ESP8266/Wemos 1-39x (counts from scripts/docs-scope-2x.sh). OTGW32 is an unannounced prototype: remove it from the public manuals, describe OT-Direct neutrally. ESP8266 appears only as 'the 1.x line'. Run as /update-docs-2x --scope full before beta.2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/docs-scope-2x.sh reports OTGW32=0 for every EN chapter and the NL chapters grep clean
- [ ] #2 ESP8266/Wemos mentions only in 1.x-line or going-back context, each remaining one listed in the notes
- [ ] #3 EN and NL chapters stay in sync; no em dashes
- [ ] #4 Done before the v2.0.0-beta.2 tag
<!-- AC:END -->
