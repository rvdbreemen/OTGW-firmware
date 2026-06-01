---
id: TASK-803
title: >-
  Fix stale comments in OTGW-Core.ino: is_value_valid override-rejection +
  print_f88 Write-Ack-only suppression
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-01 17:44'
updated_date: '2026-06-01 17:44'
labels:
  - bug
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two code comments in src/OTGW-firmware/OTGW-Core.ino predate the ADR-066/069/075 split and now misdescribe behavior (surfaced during Toutside/Data-Invalid investigation 2026-06-01). (a) is_value_valid docblock (~line 1118) states overriden R/A messages are 'not valid for use', but is_value_valid does NOT exclude them by rsptype — that exclusion lives in is_value_valid_for_master_topic; is_value_valid accepts them (msgtype-based) so source-separated topics work. (b) print_f88 comment (~line 1871) says 'only the per-spec-undefined Write-Ack data byte is suppressed', but the same validForMaster gate also suppresses gateway-substituted T and answer-override A frames (ADR-069/075). Comment-only; no behavior change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 is_value_valid docblock no longer claims is_value_valid itself rejects overrides; clarifies the rsptype-based exclusion lives in is_value_valid_for_master_topic
- [ ] #2 print_f88 comment reflects that gateway-substituted/answer-override frames are also suppressed from canonical, not only the Write-Ack data byte
- [ ] #3 No code/logic changes; comments only
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->
