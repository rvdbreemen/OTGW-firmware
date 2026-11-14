---
id: TASK-1935
title: File paths are normalised before the access check (1.x port of dev TASK-1934)
status: To Do
assignee: []
created_date: '2026-11-14T13:16:00Z'
labels:
  - 1.x
  - security
  - bug
milestone: 1.x
dependencies: []
priority: high
ordinal: 263000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x port of dev TASK-1934. Web file requests are normalised (decoded path, dot segments resolved, paths above the root refused) before the access check and before the file is opened, aligned with the 2.x change. Details are kept locally until both lines have shipped. Robert's scope: trusted LAN; without an admin password the behaviour stays as it is.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real handleFile() and the real core 2.7.4 littlefs path lookup: red on the old code, green on the fix; plain and authorised requests keep working
- [ ] #2 Other path-taking routes on 1.x checked; findings filed
- [ ] #3 Bench by otgw-firmware-09 on .35 (1.x, password set): the probe set from the pre-fix run returns 0 protected bodies without credentials; web UI and authorised reads unchanged; without a password unchanged
<!-- AC:END -->
