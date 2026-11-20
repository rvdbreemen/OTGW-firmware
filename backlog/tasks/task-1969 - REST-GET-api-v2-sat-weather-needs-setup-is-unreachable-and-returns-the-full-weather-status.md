---
id: TASK-1969
title: >-
  REST: GET /api/v2/sat/weather/needs-setup is unreachable and returns the full
  weather status
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T10:43:00Z'
updated_date: '2026-11-16T08:26:00Z'
labels:
  - rest
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1006000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1964 route inventory (2026-11-15). In restAPI.ino the SAT weather handler checks the sub-route with wc == 5 (restAPI.ino ~1532), but /api/v2/sat/weather/needs-setup has 6 words ("", api, v2, sat, weather, needs-setup), so the check never matches and the request falls through to the full weather status. Coordinator: file low, red/green on the routing, fix in the ba lane when small.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real handleSAT weather routing: GET /api/v2/sat/weather/needs-setup reaches the needs-setup answer; red on the current code, green on the fix; /api/v2/sat/weather still returns the status
- [x] #2 Every web UI or doc caller of needs-setup checked (grep), openapi.yaml matches the answer
- [x] #3 evaluate.py --quick no new FAIL; esp32-combo build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 fix: restAPI.ino handleSAT weather branch checks wc > 5 (was wc == 5): processAPI() splits /api/v2/sat/weather/needs-setup into six words with a leading empty word, so words[5] is needs-setup only from wc 6. Same form as the humidity sub-route next to it. Evidence: tests/test_sat_weather_needs_setup_route.py compiles the REAL word split of processAPI() and the REAL condition, cut from restAPI.ino: fix 2/2 OK (needs-setup wc=6 hit=1; /sat/weather wc=5 and /sat/weather/other wc=6 do not hit); against origin's restAPI.ino FAILED: 'needs-setup wc=6 hit=0'. Callers: v2.js has none (grep); openapi.yaml already documents /v2/sat/weather/needs-setup with needs_setup, which now matches the answer.

2026-11-15 bench (09 on .63, ffd0a64): PASS: GET /api/v2/sat/weather/needs-setup returns the small {needs_setup} object; /api/v2/sat/weather keeps the full status.

2026-11-16 close-out (ba): AC3 evidence: evaluate.py --quick on origin/dev 52283d842 (contains fix 1d96c8708): 95 checks, 0 FAIL, 0 WARN, health 100 %; esp32-combo build exists and was benched: 09 on .63 with bench build ffd0a64 PASS (c11aedf4c); the fix ships in v2.0.0-beta.339.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
GET /api/v2/sat/weather/needs-setup reaches its own answer again (fix 1d96c8708, in v2.0.0-beta.339). AC1: the weather routing in handleSAT() handled only wc <= 5, so the 6-word path never matched; the fix routes wc > 5. tests/test_sat_weather_needs_setup_route.py 2/2 on the fix, red on the old code ('needs-setup wc=6 hit=0'); /api/v2/sat/weather still returns the status. AC2: callers grepped (web UI + docs), openapi.yaml /v2/sat/weather/needs-setup matches the answer. AC3: evaluate.py --quick on origin/dev 52283d842: 95 checks, 0 FAIL, 0 WARN; esp32-combo bench build ffd0a64 PASS on .63 (09, c11aedf4c).
<!-- SECTION:FINAL_SUMMARY:END -->
