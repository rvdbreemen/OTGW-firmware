---
id: TASK-863
title: >-
  fix(build): esp32-classic inherits esp32 asset slug via dict spread, artifact
  name collision
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-12 04:53'
updated_date: '2026-06-12 05:03'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TARGETS['esp32-classic'] = {**TARGETS['esp32'], ...} (build.py:120) inherits slug='esp32-otgw32' (added by TASK-856). collect_pio_artifacts then copies Classic firmware over the otgw32 asset names and the versioned rename hits FileExistsError WinError 183, failing python build.py. The TASK-854 comment 'assets stay distinct because filename is built from the target key' became stale when TASK-856 introduced the slug field. Fix: explicit slug 'esp32-classic' in the esp32-classic TARGETS entry + correct the stale comment.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 esp32-classic TARGETS entry carries explicit slug 'esp32-classic'
- [x] #2 Stale distinctness comment corrected
- [x] #3 python build.py completes all three targets with distinct asset names
<!-- AC:END -->
