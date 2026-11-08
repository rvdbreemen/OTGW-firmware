---
id: TASK-1731
title: >-
  otd-mode-running-note.test.mjs flaky: row not dirty after the
  Switch-to-gateway click
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-08T14:42:00Z'
updated_date: '2026-11-08T17:55:00Z'
labels:
  - tests
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 838000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
system32-78 2026-11-08: tests/webui/otd-mode-running-note.test.mjs (TASK-1702) fails intermittently (3x today, also on old data; old 1/2, fix 0/2, once under -j 4): after the 'Switch to gateway' click the row is not dirty and the count is 0, although the button existed. markDirty('otdmode', cv, row) runs only while otdRunningDiffers(cv); otdMode is only ever set, never cleared. Suspects (unproven): (a) a settings refetch/re-render detaches the row right at the click; (b) the button lookup hits a render window. Needs instrumentation (log setDirty and row identity at failure).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause shown with instrumentation at the moment of failure (test bug or real UI race)
- [ ] #2 Fix: 20 consecutive runs green, also under run_tests -j 4
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 root cause 1 (real UI race, not only the test): fetchSettings() (data/v2.js, re-apply of unsaved edits) drops a dirty key whose value equals the stored value. The 'Switch to <stored mode>' button (TASK-1702) marks otdmode with exactly the stored value on purpose; markDirty has the otdRunningDiffers exception, fetchSettings did not. Any settings refetch that lands after the click (the page-start GET arriving late under load, or re-opening Settings, showPage line 161) erased the mark: row clean, count 0, and Save then POSTs nothing, so the user stays in Master. Deterministic reproduction added to the test (click, re-open Settings, wait for the extra GET /api/v2/settings): on origin/dev data FAIL 'the mark survives a settings refetch {dirty:false,count:"0"}' and 'Save POSTs otdmode=1 ()'; with the fix 6/6, control OTD_RUNNING=gateway 3/3. Fix: fetchSettings keeps otdmode when otdRunningDiffers(value).

Root cause 2 (the hang, environment): instrumented with OTGW_TEST_TRACE=1 (phase times + every request with its finish time). A failing run showed Chrome receiving v2.js (417 KB) at 958 ms and making its next request only at 19,932 ms; another run needed 46.9 s for the first page load against ~0.5 s normally, at 85-90% commit charge with headless Chromes from other sessions' webui tests running. The test had no CDP timeout and an iteration-bounded until() (up to 500 s with 10 s evals), so a stall hung the suite. Test fixes: send() resolves empty after 10 s, until() is bounded by wall time, the first load gets 60 s and the row 30 s, a 240 s watchdog fails the run with the request log, and Chrome is stopped as a whole tree (taskkill /T) because chrome.kill() left renderer/GPU children behind on Windows. Leftover trees seen at the end (28 processes) belonged to other tests (sat-settings-locked-when-off, ble-nopsram-warning), which have the same kill pattern: follow-up.

Runs: 20 sequential before the timing change: 19 PASS, 1 watchdog stall; after it 10 sequential: 9 PASS (about 5 s each, load ~0.5 s), 1 FAIL with a 46.9 s first load at 88% commit. AC1 met (both causes shown with instrumentation at the failure). AC2 (20 consecutive green, also under -j 4) NOT met: on this machine state a Chrome stall of 20-47 s still occurs about 1 in 10-20 runs; to be re-run on a quiet machine tonight. The UI fix rides the TASK-1689 build (beta.150/151, bench .63).
<!-- SECTION:NOTES:END -->
