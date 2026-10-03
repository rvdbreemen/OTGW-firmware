---
id: TASK-1205
title: GET /api/v2/health writes /.health to LittleFS on every call
status: To Do
assignee: []
created_date: '2026-10-03 17:55'
labels:
  - rest-api
  - littlefs
  - bug
dependencies: []
priority: medium
ordinal: 327000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1162 reviewer, confirmed in code 2026-10-03. restAPI.ino:3363 calls updateLittleFSStatus(F("/.health")), and helperStuff.ino:196-205 opens the file for writing, prints 'ok', flushes and closes it. Every health request therefore costs a flash write plus heap churn, whoever calls it (monitoring, Home Assistant integrations, test probes at 1 Hz). That wears the flash and perturbs heap measurements such as TASK-1162.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The health endpoint checks LittleFS without writing on every call (for example a read-only check, or a cached status refreshed by at most one write per several minutes)
- [ ] #2 Measured on the bench: 100 consecutive health calls cause no flash write (the probe file's content and the filesystem usage are unchanged), and health still reports a failed or unmounted filesystem
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
