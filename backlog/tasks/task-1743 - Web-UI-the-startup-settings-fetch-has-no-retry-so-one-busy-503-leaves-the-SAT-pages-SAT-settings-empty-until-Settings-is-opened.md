---
id: TASK-1743
title: >-
  Web UI: the startup settings fetch has no retry, so one busy 503 leaves the
  SAT page's SAT settings empty until Settings is opened
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T22:56:00Z'
updated_date: '2026-11-09T06:33:00Z'
labels:
  - webui
  - bench-finding
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 848000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench (not by a tester), otgw-firmware-80, 2026-11-08 23:44, .61 beta.157+8b67ad0 under a 15-min soak (4 REST polls per 30 s) plus a headless UI load. v2.js:6431 calls fetchSettings() once at startup (TASK-989 staggered burst); fetchSettings() (v2.js:2629-2631) returns early on !r.ok and never retries. setData stays empty, so renderSatSettings() (v2.js:2822) renders nothing under the SAT page's 'SAT settings' heading; only showPage('settings') (v2.js:263) fetches again. Seen on .61: SAT page Control view showed an empty 'SAT settings' section after 15 s; after opening Settings first, 41 rows rendered. In the same soak 1 of 81 REST calls was a 503 on /api/v2/settings. Related: TASK-1741 (busy 503 keeps the shown state; two-tab 503s), ADR-165/188 (REST cap 2, TX cap 4), ADR-184/single-flight rule (at most 2 API calls in flight).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A browser test against the real v2.html/v2.js shows the SAT settings section empty after a startup settings 503 on the old code and filled after the retry on the fix
- [x] #2 The retry respects the 2-in-flight rule and backs off (no tight loop while the device stays busy)
- [x] #3 Bench .61: SAT page opened directly after load shows the SAT settings rows
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
fetchSettings() (v2.js:2629) uses the existing fetchWithRetry() (v2.js:100, TASK-1033: retries only on HTTP 503 and network errors, strictly sequential, doubling backoff, through the ADR-184 2-in-flight queue) with 5 tries from 400 ms (400/800/1600/3200 ms, ~6 s), instead of a bare fetch. No new mechanism; the r.ok guard stays, so a final failure still keeps the shown state. Test: new tests/webui/settings-fetch-retry.test.mjs on the real v2.html/v2.js: the fake device answers the first GET /api/v2/settings with 503 and later ones with 200; open the SAT page Control view and expect the SAT settings rows without visiting Settings; plus a check that the GET count stays bounded while the device stays busy. Red on origin/dev (OTGW_DATA_DIR at the old data/), green on the fix. Bench .61 after 06:45 (it is 4c's until then): open the SAT page directly after load under the soak poller. Web-only change: no firmware bump needed beyond the fs; it ships with the next build. Not for otgw-1.x.x (no v2 SAT page there).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 otgw-firmware-80: fix in v2.js fetchSettings(): fetchWithRetry(APIGW + 'v2/settings', undefined, 5) instead of fetch (existing TASK-1033 helper: retries only 503 and network errors, sequential, 400/800/1600/3200 ms). tests/webui/settings-fetch-retry.test.mjs: (1) first settings GET 503, later 200, SAT page Control view without opening Settings: fix PASS, old data/ (8b67ad039) FAIL 'SAT settings rows render after a 503 on the first settings GET (GETs: 1)'; (2) device stays busy: GET count bounded (<=6) and flat after the backoff, PASS on both (guard). No test slices fetchSettings: otd-mode-running-note 1/1 and sat-area-sensor-mapping 1/1 still green. evaluate.py --quick 0 FAIL 0 WARN. Waiting: bump + commit after 4c pushes beta.158 (no duplicate tag), then build and bench .61 (AC3) once 4c releases it.

2026-11-09 review (read-only reviewer agent) follow-ups applied: (a) setFetchSeq sequence guard: fetchSettings() applies a response only while it is the newest call, and saveSettings() bumps the sequence, so a late startup retry never overwrites newer data or a just-saved value and side effects (fetchBle, area choices, onboarding) do not run twice; settings-error-toast (slices saveSettings) gets a var setFetchSeq stub, 5/5. (b) Comment names the wizard 'go' callers too. (c) Test 2 now records the arrival time of every settings GET: exactly 5 GETs, first gap >= 300 ms, each gap >= 1.5x the previous. Old code: both tests red ('got 1'); no-backoff mutant (delayMs 0): test 2 red, gaps [23,12,6,5] ms. Neighbours green: settings-help 29/29, sat-settings-locked-when-off 6/6, otd-mode-running-note 1/1, sat-area-sensor-mapping 1/1, ble-nopsram-warning 5/5. Rejected: deferring renderSettings() while an input has focus (same rebuild as the existing showPage fetch, no new class of problem; noted). After rebase onto 8a4b25d60 + bump beta.159: tests 2/2 and 5/5, evaluate.py --quick 86 passed 0 WARN 0 FAIL.

2026-11-09 07:17-07:33 bench .61 (no PSRAM) on beta.159+b9eaeb2 (OTA app+fs, 164 settings unchanged): 15-min soak, 27 polls of device/info + sat/status + settings, 81/81 HTTP 200, no restart (uptime continuous, bootcount 175), freeheap 67.4-71.7 KB, maxblock 31,732 B flat, MQTT connected. AC3: headless Chrome opened the SAT page Control view directly after load (Settings never opened) under that soak load: 41/41 SAT rows rendered in 3 of 3 runs. Not observed on the bench: whether the first settings GET got a 503 in those runs (no browser network log); the retry itself is proven by the host test. Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\evidence\beta159
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/webui/settings-fetch-retry.test.mjs test 1 (first settings GET 503, then 200; SAT page Control view without opening Settings) green on the fix, red on 8b67ad039 ('GETs: 1', no rows). AC2: test 2 (device stays busy) asserts exactly 5 GETs with the first gap >= 300 ms and each gap >= 1.5x the previous; green on the fix, red on the old code (1 GET) and on a no-backoff mutant (gaps 23/12/6/5 ms); retries go through the ADR-184 2-in-flight queue (fetchWithRetry is sequential). AC3: .61 beta.159 soak, SAT page opened directly after load showed 41/41 rows in 3/3 runs, 81/81 REST 200, no restart. Review follow-up: setFetchSeq guard (newest response wins, save bumps it). evaluate.py --quick 86/0/0. Commit 8bb4cd352 (rebased), beta.159.
<!-- SECTION:FINAL_SUMMARY:END -->
