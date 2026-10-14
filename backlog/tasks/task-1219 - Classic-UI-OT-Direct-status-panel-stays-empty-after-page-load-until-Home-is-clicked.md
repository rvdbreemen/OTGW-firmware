---
id: TASK-1219
title: >-
  Classic UI: OT-Direct status panel stays empty after page load until Home is
  clicked
status: To Do
assignee: []
created_date: '2026-10-12 05:23'
labels:
  - webui
  - otdirect
  - bug
dependencies: []
priority: medium
ordinal: 339000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On an OTGW32 (OT-Direct) the classic Web UI's OT-Direct Status panel shows '--' in every row after a normal page load and never refreshes. showMainPage() (index.js:3724) calls startOTDStatusPolling() synchronously, but otDirectAvailable is still false then: applyOTDirectAvailability() only sets it after the async /api/v2/device/info answer (index.js:3620/5842/6133). startOTDStatusPolling() returns early on !otDirectAvailable and nothing starts the poll once the flag turns true. Clicking Home (showMainPage again) or a tab visibility change starts it.

Bench evidence 2026-10-12, OTGW32 192.168.88.61, alpha.418 combo (index.js identical to HEAD apart from the version banner): two fresh loads, 20 s after load otDirectAvailable=true, _otdStatusTimer=null, 0 requests to /api/v2/otdirect/status, panel '--'. After a Home click: timer set, panel shows Mode MASTER, Schedule 104/104, Active Overrides 1. Evidence: %LOCALAPPDATA%/OTGW-capture/otgw32-validation-20261012/EVIDENCE.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Old-vs-fix on the OTGW32: a fresh page load with no clicks shows populated OT-Direct Status rows within 10 s and /api/v2/otdirect/status is polled every 5 s (old build: panel stays '--', 0 requests)
- [ ] #2 On a PIC board (otdirectavailable false) the poll still never starts (0 requests to /api/v2/otdirect/status)
- [ ] #3 build.bat esp32-combo SUCCESS and evaluate.py --quick green
<!-- AC:END -->
