---
id: TASK-1491
title: >-
  webui test pic-flash-status fails intermittently under load (picAfterFlash
  sequence, picUpdatePoll banner)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T22:39:00Z'
updated_date: '2026-10-29T23:05:00Z'
labels:
  - webui
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 650000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 during the TASK-1489 load run (2026-10-29). Not a browser test, so the port fix does not touch it. Load iteration 4: 'picAfterFlash() runs the real refresh functions one request at a time' - the 'banner 6.8' step was missing from the request sequence. Load iteration 6: 'picUpdatePoll: a persistent network error ends on Update check failed' - banner stuck. Both look timing-sensitive (a wall-clock budget inside the code under test). Logs: d5 scratchpad soak1/load_4.txt, load_6.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each failure is reproduced deterministically (e.g. by slowing the event loop or timers) and the cause is named with file:line
- [x] #2 After the fix, pic-flash-status passes 20/20 under parallel load
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Cause (d5, 2026-10-30): test-side fixed waits, tests/webui/pic-flash-status.test.mjs:226 (60 ms after picAfterFlash()) and :257 (80 ms in runPoll), for chains of real timers (2 ms fake fetch :215, 1 ms poll hop :254). App not racy: v2.js picUpdatePoll counts tries (tries < 3), picAfterFlash is a callback chain; no wall clock. Repro: preload delaying timers <=5 ms by SLOW_MS (scratchpad t1491_slowtimers.mjs). Old: SLOW_MS=20 3/3 fail picUpdatePoll-persistent; SLOW_MS=40 3/3 fail all 3 tests, picAfterFlash missing 'banner 6.8' + 'fetchPicSettings full=true' exactly as load_4. Fix: waitFor(end signal, 5 s cap). Fixed: 14/14 normal, SLOW_MS=40 3x, SLOW_MS=200. Mutant v2.js without the 'Update check failed' banner: still red (1 fail). AC#2: 20/20 sequential runs (1.5-2.4 s) with two parallel loops of the 14 non-browser webui tests (19 iterations, 0 failures); this load is lighter than the original failing run (no Chromes, memory), the deterministic SLOW_MS old-vs-fix is the stronger evidence. Local commit in ../wt-1491, tests only, no bump.

2026-10-30 coordinator: merged to dev as 8d0dd3bd3. Spot-check: node --test tests/webui/pic-flash-status.test.mjs exit 0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
pic-flash-status flaked under load because two tests checked the real v2.js picAfterFlash/picUpdatePoll chain after a fixed 60/80 ms (test :226, :257) while the chain ran on real timers; late timers cut it short. Not an app race (try counter, callback chain, no wall clock). Tests now wait for the chain's end signal with a 5 s cap. Evidence: deterministic repro by delaying short timers (old 3/3 red with the load-run symptom, fix green up to 200 ms per hop), mutant still red, 20/20 under parallel load. Local commit in ../wt-1491 for coordinator otgw-firmware-75; In Progress until merged.
<!-- SECTION:FINAL_SUMMARY:END -->
