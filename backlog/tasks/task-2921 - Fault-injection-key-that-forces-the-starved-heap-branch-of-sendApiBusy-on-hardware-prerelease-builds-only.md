---
id: TASK-2921
title: >-
  Fault-injection key that forces the starved-heap branch of sendApiBusy on
  hardware (prerelease builds only)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T16:15:00Z'
updated_date: '2026-11-19T19:49:00Z'
labels:
  - rest
  - testing
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1059000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2910 AC4 (Robert, DM 1573005843849547881, 2026-11-19: 'Follow your advice A + B separate follow up task for 2.1.0.'). The < 4096 B maxblock branch of sendApiBusy (restAPI.ino, REST_BUSY_MIN_MAXBLOCK) is host-proven (tests/test_rest_busy_starved_heap.py) but unreachable on the bench: under the deep heap hold no TCP connection reached the unit. A prerelease-only key (compiled out when _VERSION_PRERELEASE is unset) makes sendApiBusy take that branch, so the abort path runs on a real AsyncWebServer connection.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A prerelease-only debug key forces the sendApiBusy abort branch; absent from a production build (check on the build output)
- [x] #2 Host test: the key on reaches webAbortCurrentRequest, the key off keeps the 503 path
- [x] #3 Bench (otgw-bench-tester) on .61: key on + 2 simultaneous REST requests: no panic, bootcount unchanged, the second request sees a closed connection; key off afterwards, settings diff clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev WIP (not committed, bump + build after the 08:00 beta, lane-2921 in wt-2026): telnet key 'B' (prerelease only, handleDebug.ino) toggles s_restForceStarved in restAPI.ino (owner of the gate; prototype restDebugToggleForceStarved() in OTGW-firmware.h under _VERSION_PRERELEASE). While on: restEffectiveInflightCap() returns 1 and sendApiBusy() closes the connection (webAbortCurrentRequest) as on a starved heap; production compiles restForceStarved() to false. Off at every boot. tests/test_rest_force_starved_key.py compiles the real block + cap + sendApiError/sendApiBusy twice (prerelease / production): red on origin restAPI.ino (block not found), green 4/4; test_rest_busy_starved_heap gets a restForceStarved() stub, 2/2. Bench plan for AC3 (09, .61): 'B' on, then 2 simultaneous REST GETs: one answers, the other sees a closed connection, no panic, bootcount unchanged; 'B' off, settings diff clean.

2026-11-19 bench (otgw-bench-tester, .61): PASS on 2.0.0-beta.398+97d79b8 (app-only OTA 20:34, app sha256 0c0e4aa9f9e843c7, settings changed none, bootcount 1079). 'h' lists 'B) bench: REST gate acts as on a starved heap, on/off (prerelease builds only)' after z) and H). Baseline B off, 2 simultaneous raw GET /api/v2/device/info: 200 | 200. 'B' -> 'REST starved-heap simulation: ON (B toggles, off at boot)'. 3 rounds of 2 parallel GETs with B on: 200 (4454-4457 B) | reset (RST), 0 B after 0.06-0.09 s, no 503. 'B' -> off; 3 rounds: 200 | 200. No panic, bootcount unchanged; 10 min regression 20:35-20:45 clean (device/info 200, MQTT true, reboot_log only the OTA line, settings.ini diff 0). Evidence: 09's scratchpad b398\ (steps.log, telnet.raw, reg\). AC3 ticked by otgw-dev on 09's evidence at his request. AC1 still open: absence from a production build is shown host-side only (tests/test_rest_force_starved_key.py production compile); no production build exists on dev.

2026-11-19 AC1 route (coordinator decision A, process): checked on the first production build, the release/2.x build of /release-2x (_VERSION_PRERELEASE commented out). On its OTGW-firmware-esp32-combo-<v>.ino.bin each of these must give 0: grep -c 'REST starved-heap simulation', grep -c 'B) bench: REST gate', and for the heap-hold hook (TASK-2026/2924) grep -c 'H) bench: starve the heap' and grep -c 'heaphold:'. (grep -c -a on the binary, or Python bytes.count.) For reference the prerelease 398 image gives 1 for the first two. Task stays In Progress until then; the coordinator hands the check to the /release-2x checklist.
<!-- SECTION:NOTES:END -->
