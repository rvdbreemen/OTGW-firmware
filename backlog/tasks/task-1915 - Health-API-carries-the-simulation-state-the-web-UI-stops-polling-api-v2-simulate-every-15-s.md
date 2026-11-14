---
id: TASK-1915
title: >-
  Health API carries the simulation state; the web UI stops polling
  /api/v2/simulate every 15 s
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-13T23:57:00Z'
updated_date: '2026-11-14T09:53:00Z'
labels: []
dependencies: []
priority: medium
ordinal: 981000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer go, Robert in #alpha-testing (message 1570947181945299066): "Yes add the simulation into there health api." Follow-up to TASK-1899. Today the v2 web UI asks /api/v2/simulate after every /api/v2/health poll (v2.js:3751 chain.then(fetchSim); fetchSim at v2.js:2296), about every 15 s per tab, only to show or hide the SIMULATION badge (#hdrSim). gruijter's capture had 7,668 such calls in 18.5 h. The health answer already comes every 15 s, so it can carry the state: /api/v2/health gets the simulation active flag and its source (file or builtin), taken from the loop-task state (the TASK-1899 LittleFSsimFilePresent flag and the simulation state), no LittleFS call in sendHealth. v2.js drops fetchSim from the poll chain and sets the badge from the health answer. /api/v2/simulate stays for its other callers (the simulation page and the one-time offer, v2.js:7978 and :8438). One request less per poll also leaves more room under the 2-request REST cap (TASK-1747).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 /api/v2/health carries the simulation state (active, source), from loop-task state; sendHealth makes no LittleFS call (test_health_no_fs_flash_ops and the TASK-1899 gate stay green)
- [x] #2 Test on the real v2.js: the 15 s poll chain sends no /api/v2/simulate request, and the SIMULATION badge follows the health answer (shown when active, hidden when not or when the field is missing); red on the old v2.js
- [x] #3 /api/v2/simulate still answers as before for its other callers: API clients and the bench and test scripts (scripts/sat_bench_scenarios_1412_20.py, heap_soak_driver.py, tests/run_coverage_test.py, diag_decode.py)
- [x] #4 Bench: in simulation mode the badge appears from the health poll and the browser network log shows no periodic /simulate request
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Correction to the description: the one-time simulation offer (v2.js maybePromptSimulation/showSimModal) switches simulation on through POST /api/v2/settings {name: simulation}, and there is no simulation page that polls /simulate. fetchSim() was the only /api/v2/simulate caller in the web UI. The endpoint stays for API clients and the bench/test scripts that read it (scripts/sat_bench_scenarios_1412_20.py, scripts/heap_soak_driver.py, scripts/tests/run_coverage_test.py, scripts/diag_decode.py). AC3 is reworded to that.

Implementation: sendHealth() (restAPI.ino) emits simulation_active (state.debug.bOTGWSimulation) and simulation_source (LittleFSsimFilePresent ? file : builtin), the same values as sendSimulationStatus(), with no LittleFS call. v2.js: fetchSim() is gone. fetchConn() calls applySimBadge(h), which shows #hdrSim only when simulation_active === true (missing field = hidden). A failed or busy health poll returns before it, so the badge keeps its state (TASK-1741). The chain after health is overrides, then otdirect/status on OT-Direct. Docs: openapi.yaml, docs/api/README.md, manuals en ch09 and nl h09 list the two fields; v2.html comment updated.

Tests: tests/webui/health-carries-simulation.test.mjs (new) 2/2 on the fix. On the old v2.js (OTGW_DATA_DIR = data with git show HEAD:v2.js) 2 FAIL: 'the poll chain asked /api/v2/simulate' (4 calls) and the badge did not follow health. tests/test_health_no_fs_flash_ops.py has a new source case: OK on the fix, FAIL on the old restAPI.ino. Updated for the new chain: rest-503-keeps-state (the badge now survives a 503 on /health, 3/3) and health-chain-one-in-flight (no simulate in the chain, still max 1 in flight, 2/2). Also OK: 1899 gate, flags_loop, fs_update_keep, dallas-discovery-toast, poll-spread-hidden, connection-map-text-fits; evaluate --quick 86 passed, 0 failed. Full webui folder: the sequential run was killed by the memory reaper after 20 of 76 files (96 tests passed, 0 failed in the 19 completed). The other 56 files have not run yet.

Rebased onto origin/dev 6eb3aa713 (after the beta.237 release); banner-only conflicts kept upstream, bump beta.237 -> beta.238 (commit 3ad0b57b2 before amend).

Build 2.0.0-beta.240+a3dff10 (HEAD a3dff1054 on origin/dev e0824f826): exit 0, 13 min, ESP32S3 image created x2, RAM 130236 B, Flash 2146491 B; sha256 ino.bin 0427b924..., littlefs.bin 46c46b20..., flash.zip 29c07ef0.... Gates on this commit: webui folder 76/76 files, 372 pass 0 fail; test_health_no_fs_flash_ops 5/5; evaluate --quick 86 passed 0 failed. Bench request (AC4) sent to 09 via the coordinator, unit .61.

Pushed e0824f826..a3dff1054 (origin/dev), #alpha-testing post 1571099699098947614 (zip 2.0.0-beta.240+a3dff10, also covers beta.238 TASK-1774 and beta.239 TASK-1808). 09 bench on .61 PASS steps 1-6: badge on after 6.1 s from health, off after 14.2 s, 0 /api/v2/simulate out of 84 page requests in 3.2 min (150 s with sim on: 0 simulate, 10 health), health 13/13 200, settings diff none, bootcount 633->635, no crashlog; GET/POST /api/v2/simulate start/stop still 200 (AC3). Evidence: 09 scratchpad b240/ev (flash.log, sim-network.json, sim-badge-off/on/off-after.png). AC4 is ticked by 09.

2026-11-14 10:44-10:48 (09, bench-tester) AC4 bench, .61, 2.0.0-beta.240+a3dff10 (artifact sha256 ino 0427b924...e4070, littlefs 46c46b20...8f5e2; OTA app+FS exit 0, 'changed: none', bootcount 633->635). One headless v2 tab with the CDP network log; sim start/stop POSTed from Node outside the browser, so the log holds only the page's own requests. Sim off: health simulation_active false, simulation_source builtin (= /api/v2/simulate), badge #hdrSim hidden. POST /api/v2/simulate/start 200: badge visible after 6.1 s without a reload, health active true. 150 s with sim on: /api/v2/simulate 0 requests, /api/v2/health 10. Whole 3.2 min session: 0 /api/v2/simulate out of 84 requests, health 13/13 HTTP 200. POST stop 200: badge hidden after 14.2 s (next health poll), health false. Afterwards bootcount 635 (no extra reboot), crashlog empty, hash-check match, settings diff [] (API + raw ini). Screenshots (header only, synthetic hostname/IP): sim-badge-off.png, sim-badge-on.png, sim-badge-off-after.png in 09 scratchpad b240/ev with sim-log.txt and sim-network.json. Not done on the bench: an old-v2.js browser run (red-on-old is the AC2 host test).
<!-- SECTION:NOTES:END -->
