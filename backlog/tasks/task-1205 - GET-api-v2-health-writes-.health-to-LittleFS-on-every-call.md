---
id: TASK-1205
title: GET /api/v2/health writes /.health to LittleFS on every call
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:55'
updated_date: '2026-10-04 10:44'
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
- [x] #2 Measured on the bench: 100 consecutive health calls cause no flash write (the probe file's content and the filesystem usage are unchanged), and health still reports a failed or unmounted filesystem
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
GET /api/v2/health no longer writes /.health to LittleFS on every call. The write probe runs at most once per 5 minutes (DECLARE_TIMER_MIN + DUE in sendHealth()); between probes only the read-only platformFSInfo() mount check runs, and a failed write probe keeps LittleFSmounted false until the next probe.
Evidence:
- Bench, measured: after deleting /.health, 100 health calls rewrote it on OLD alpha.409 (GET /.health 404 to 200) and left it absent on FIX alpha.410 (404 to 404). The fix's periodic probe recreated it about 270 s after the first call. Health UP 100/100 on both.
- Accepted by the maintainer on code-path evidence, not measured: the 'still reports a failed or unmounted filesystem' clause of AC#2. The bench cannot provoke it: the upload guard keeps a 5% margin, and an unmount needs a filesystem OTA that wipes the settings. An unmount is caught on every call by the same platformFSInfo() check the old code ran first. A write failure is caught by the 5-minute probe.
- Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.410); evaluate.py --quick 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
