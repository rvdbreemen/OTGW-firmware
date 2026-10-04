---
id: TASK-1205
title: GET /api/v2/health writes /.health to LittleFS on every call
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-03 17:55'
updated_date: '2026-10-04 08:36'
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
- [x] #1 The health endpoint checks LittleFS without writing on every call (for example a read-only check, or a cached status refreshed by at most one write per several minutes)
- [ ] #2 Measured on the bench: 100 consecutive health calls cause no flash write (the probe file's content and the filesystem usage are unchanged), and health still reports a failed or unmounted filesystem
- [x] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-04, OTGW32 bench. The probe file /.health is hidden from the FS listing but served by the static server, so GET /.health shows whether it was written. Procedure (t1205_health_writes.py, kept out of the repo): delete /.health, make 100 sequential GET /api/v2/health, then GET /.health.
- OLD (alpha.409+c2eaa66): /.health 404 before, 200 after the 100 calls (rewritten). Health UP 100/100.
- FIX (alpha.410+d8fefbc): /.health 404 before and still 404 after the 100 calls. Health UP 100/100.
- With one health call every 30 s afterwards, the file stayed 404 until t+240 s and was 200 from t+270 s. The write probe still runs, once per 5 minutes after the first call.
- usedBytes cannot serve as evidence: the listing applies a 5% margin and clamps, so it reads 100% full both before and after.
NOT measured on the bench: health reporting a failed filesystem.
- I tried to fill LittleFS so the write probe would fail. The /upload guard uses the same 5% margin and refused at 507 after about 54 KB, while real free space remained, so the probe kept succeeding (UP). The test files were deleted afterwards.
- An unmount cannot be triggered on the bench without a filesystem OTA, which would reset settings.
- By code: an unmount is caught on every call, because the read-only platformFSInfo() check is the same function the old path called first. A failed write is caught by the 5-minute probe, and the else-if keeps LittleFSmounted false until the next probe.
AC#3: esp32, esp32-classic and esp32-combo SUCCESS (alpha.410); evaluate.py --quick 0 failures.
<!-- SECTION:NOTES:END -->
