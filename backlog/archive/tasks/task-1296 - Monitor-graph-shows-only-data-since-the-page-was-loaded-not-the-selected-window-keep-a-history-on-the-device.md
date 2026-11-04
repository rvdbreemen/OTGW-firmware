---
id: TASK-1296
title: >-
  Monitor graph shows only data since the page was loaded, not the selected
  window; keep a history on the device
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T11:57:00Z'
updated_date: '2026-10-23T06:18:00Z'
labels:
  - webui
  - monitor
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 421000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-22 08:20 (#alpha-testing, screenshot): with the 1 h window selected the graph (and the CSV export) only holds data from the moment the page was opened or refreshed. Cause: the v2 graph samples are collected client-side from the WebSocket stream (v2.js pushSample() ~549, SAMPLES_MAX 1800) and there is no history endpoint (TASK-986 design note). Proposal: a small device-side ring buffer (for example 1 h at 10 s = 360 samples of flow/return/control setpoint/thermostat setpoint/room/outside/modulation/flame/DHW/CH, ~20 B each, ~7 KB RAM, static per ADR-053) exposed as GET /api/v2/graph/history, loaded once when the Monitor page opens and merged with the live samples; the CSV export then covers the same window. Keep the 2-in-flight rule (one request). Record the RAM and the heap impact on the bench.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CSV export covers the same window as the graph
- [x] #2 docs/api openapi + ch03/h03 updated; a host test covers the history endpoint JSON shape
- [x] #3 Opening the Monitor graph after the device has run for an hour shows the selected window filled up to the board's history length: 1 h without PSRAM, 24 h with PSRAM (bench screenshot before/after on a no-PSRAM unit; the PSRAM path host-tested)
- [x] #4 RAM/heap before and after measured on the bench; the internal-DRAM buffer is static (ADR-053) and bounded, the 24 h ring lives in PSRAM only on boards that have it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Measured first (bench OTGW32 .88.61, alpha.461, no PSRAM): free heap 81.4 KB / maxblock 38.9 KB with BLE off; 35.2 KB / 21.5 KB with BLE on (risk-ack mode) -> BLE costs ~46 KB. The combo image also runs on no-PSRAM boards, so the store is static internal DRAM (ADR-053) and must stay small.

Coverage decision: the UI offers 10 min / 1 h / 4 h / 24 h (v2.html:627). AC1 says 'the whole selected window', so the store covers 24 h: one ring, GRAPH_HIST_SLOTS 720 x GRAPH_HIST_INTERVAL_S 120 s, 14 B per slot (6 x int16 temp*10 with INT16_MIN = no value: flow, ret, chSet, roomSet, room, outside; uint8 mod 0..100, 255 = no value; uint8 flags flame/dhw/ch) = 10,080 B static. Rejected: 1 h @ 10 s (5 KB, leaves 4 h/24 h empty, fails AC1); 24 h @ 60 s (20 KB, too much with BLE on); two tiers (more code for a small gain). Trade-off stated: history is 2-minute resolution, the live part keeps 5 s.

Firmware:
- graphHistory.h: pure C++ header (no Arduino deps, host-compilable): slot struct, ring push/commit with gap fill (a commit k intervals late pushes k-1 empty slots, capped at capacity, so the timeline stays true across flashes), and a templated emitter writing the JSON through a JsonEmit-like writer.
- graphHistory.ino: graphHistoryTick() called from doTaskEvery1s() (loop task, same task as processOT -> no lock to read OTcurrentSystemState): ORs the status bits every second; at each 120 s boundary stores the last values. Value present only when seen (getMsgLastUpdated(id) != 0; Tr also !isnan). Outside follows the browser rule: state.sat.weather.fTemperature when bValid, else Toutside. Writer takes a platform mutex only around the slot write + head/count update.
- GET /api/v2/graph/history (kV2Routes 'graph', sub 'history'; GET only, 405 otherwise; not rate-limited, loaded once): handler takes the mutex once (bounded 100 ms, 503 on timeout), snapshots head/total and caps the emitted count at capacity-30, so the writer needs 30 commits (1 h) before it can touch an emitted slot -> zero-copy and byte-identical across restSendChunked passes (jsonChunked.h determinism rule, no lock inside the filler). JSON columnar: {"interval_s":120,"newest_age_ms":N,"count":n,"flow":[...],"ret":[...],"chSet":[...],"roomSet":[...],"room":[...],"outside":[...],"mod":[...],"flame":[0/1...],"dhw_on":[...],"ch":[...]} oldest->newest, null for no value, 1 decimal. Keys match gHist so the merge is a straight map.

Web UI (v2.js): on the first showTab('mgraph') of a page load, one GET (through the existing fetch queue; retried on the next open if it failed), map slot i to t = Date.now() - newest_age_ms - (n-1-i)*interval_ms snapped to GSLOT, keep only slots older than gHist[0].t (live wins, gHist stays sorted, no duplicate t), prepend, trim to GHIST_MAX, render. CSV export then covers the window automatically (it reads windowedSamples()).

Docs: openapi.yaml path + schema, docs/api/README.md, ch03/h03 graph bullet (and the stale classic history lines), ch09/h09 API reference, c4-code-rest-api.md route list, scripts/tests/openapi_compliance.py ENDPOINTS. Proposed ADR via adr-kit for the device-side history store (RAM cost, endpoint contract), not self-accepted. Not on otgw-1.x.x (1.x keeps its client-side graph, ADR-039).

Verification:
- AC1: bench, device up >1 h with replayed OT traffic, open Monitor > Graph fresh: screenshot old build (graph starts at page load) vs new (whole 1 h window filled; 4 h after 4 h if time allows).
- AC2: CSV exported right after opening: first epoch_ms row ~ now - window on the new build, ~ page-load on the old.
- AC3: RAM static delta from the build line (116180 B before), free heap after boot before/after on the bench with BLE off, same conditions.
- AC4: C++ host test (tests/test_graph_history.cpp) including the real graphHistory.h + real jsonEmit.h with stubs: commit/gap/wrap, OR flags, cap rule, JSON shape and byte-identical re-runs; node test for the v2.js merge function (extract the real function); red on HEAD (missing function/header) then green.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 plan revision after advisor review: (1) ring 726 slots (GRAPH_HIST_SLOTS) with a writer margin of 6 (GRAPH_HIST_MARGIN): the response emits min(count, 720) slots = full 24 h; margin counts commits, so a gap-fill burst during a stream cannot reach emitted slots unless >6 commits land mid-stream (a stream lasts seconds). (2) head/count/newest_age_ms are computed ONCE in the handler under the mutex into a per-response snapshot captured by the restSendChunked closure; nothing in the closure reads millis() or the live head. (3) JSON carries the stored int16 x10 temperatures as integers (no float formatting on async_tcp, deterministic), v2.js divides by 10; mod and flags as small ints, null for no value. (4) v2.js: a 404 (old firmware) is silent and marks history unavailable for the page; 503 goes through the retry helper. (5) Dallas sensors are not in the device history (their lines start at page load, as today); stated in docs. (6) Bench: heap baseline after a reboot at fixed uptime, BLE off and BLE on (risk ack), old and new build; settings saved/restored around the filesystem flash; ~1 h of replay traffic before the AC1 screenshot. Header naming per ADR-079/081 checked before creating files.

2026-10-23 00:15 evidence so far.
- Build (build.bat combo, SUCCESS 00:05): RAM 116180 -> 126372 B (+10192: ring 726x14 = 10164 + clock/handle), flash 2079019 -> 2081879 B (+2860).
- Bench heap (OTGW32, no PSRAM, reboot then 5 min uptime, same conditions; logs %TEMP%/bench-1296/heap-old.log, heap-new.log):
  - BLE off: free 82116 -> 71844 (-10272), maxblock 38900 -> 31732.
  - BLE on (risk ack): free 35572 -> 25276 (-10296), maxblock 23540 -> 15348.
  - Side effect: with BLE on, maxblock < 16000 makes restEffectiveInflightCap() drop the REST cap from 2 to 1 (restAPI.ino ~56-64), so BLE-on users on a no-PSRAM board see more 503 retries. Needs a decision (accept, or shrink/relocate the ring).
- Before evidence (old build alpha.460+10d9ffb, after 5 min replay, fresh page): CSV 3 rows, oldest 0.3 min old; graph only shows data at the right edge (before-graph-1hour.png, before-graph-1hour.csv).
- New build deployed: app over USB (settings kept), v2.js uploaded alone via /upload (cmp identical to .pio/data-staging/v2.js). /api/v2/graph/history empty ring: HTTP 200, 163 B, 0.107 s. Replay started 00:14:22 for the 1 h AC1 run.

2026-10-23 00:40 review round 1 (4 lenses: correctness opus, rules, tests, UI). Verified each against the code.
Fixing: (a) history lanes drawn one interval late: history records now stamped at slot START like live records; (b) holes inside the live range (laptop sleep/WS drop) stayed empty: merge by slot time, re-load on reopen after 10 min; (c) lines drawn straight across outages: renderGraph breaks a series/lane on a gap > 150 s, pure gap slots dropped in the merge; (d) count 0 / unusable payload marked 'done' forever, no timeout: done only when records were added, 20 s AbortController; (e) the byte-identical host test was vacuous: replaced by a RestChunkWindow reassembly with 1..6 commits between windows (identical) and 7 (differs); (f) node fixtures diversified (non-grid age, scale 100, distinct columns, live-equal t); (g) margin comment reworded (counts pushed slots; needs a >12 min tick stall during a stream; corrupt stream fails JSON.parse and retries); (h) docs: c4 integration layer, container, web-assets; openapi null wording.
Rejected: getMsgLastUpdated() returns 0 at tracked second 0 (once per ~18 h per MsgID) -> one null point; pre-existing helper shared with the REST emits, not changed here.
To measure: CPU of the O(n^2) chunked re-run with a full ring (temporary bench build with a 1 s interval to fill 720 slots, curl time_total + max_loop_gap_ms); not committed.
Open decision for the maintainer: BLE-on no-PSRAM maxblock drops below 16000 -> REST inflight cap 2 -> 1.

2026-10-23 01:1x PAUSED: the AC1/AC2 after-capture (graphshot.py, windows 10 min/1 h/4 h/24 h) was stopped by Claude Code because the PC ran critically low on memory; not restarted (needs the maintainer's go). State: bench .88.61 runs the new app (alpha.461+298c660 working tree, compiled 00:03:32) with the round-1 v2.js uploaded (cmp identical to .pio/data-staging/v2.js), synthetic replay /otgw_simulation.log active since 00:30 (history varies: flame cycles, flow 29-42, mod 0-76, setpoints, room, outside). Review round 1 applied; 174/174 node tests, host test incl. chunk-window reassembly green, evaluate --quick 0 fail. TO RESUME: rerun graphshot.py after-capture (scratchpad, needs >1 h ring), then the CPU measurement (temporary 1 s interval build, full ring, curl time_total + hd_max_loop_gap_ms), restore the real build, bump, commit, push. Open decision: BLE-on no-PSRAM REST cap 2->1.

2026-10-23 06:25 maintainer decision (session): boards without PSRAM keep 1 h of history at 30 s slots (126-slot static ring, ~1.8 KB, ADR-053), boards with PSRAM keep 24 h at 120 s slots in a 726-slot ring allocated once in PSRAM at boot (never freed; falls back to the static 1 h ring when the allocation fails). Reason: the 10 KB static ring cost 7-8 KB of max block and dropped the REST in-flight cap to 1 with BLE on (no-PSRAM). The PSRAM allocation is a recorded exception to ADR-053 (it protects internal DRAM; a one-time PSRAM block does not fragment it), via a new platformPsramAllocOnce() shim. AC1 is narrowed to the board's history length.

2026-10-23 07:00 two-size build: build.bat --target all SUCCESS (esp32 06:41, esp32-classic 06:46, esp32-combo 06:51); combo static RAM 116180 -> 117972 B (+1792). Bench heap after reboot, 5 min, same conditions (heap-new2.log): BLE off free 82116 -> 80232 (-1884), maxblock 38900 -> 36852; BLE on free 35572 -> 33680 (-1892), maxblock 23540 -> 23540 (unchanged, REST cap stays 2). /api/v2/graph/history reports interval_s 30 = small static ring chosen on this no-PSRAM unit. App flashed USB (settings kept), v2.js uploaded alone (cmp identical). Replay /otgw_simulation.log restarted 07:00:22. Host test 102 PASS (both ring sizes, chunk-window reassembly, margin pinned), node 176/176, evaluate --quick 0 fail, adr governance OK, ADR index regenerated.

2026-10-23 08:20 review round 2 (correctness opus + rules): both found spurious line breaks at the history/live seam on 120 s slots (newest slot end up to 120 s before the fetch, start stamp added another 120 s; worst 230 s > GBREAK_MS). Fix: renderGraph measures the gap from the END of a history point's data (t + span, span = interval) instead of its start; merge keeps the 'no live record in the slot' rule and stamps span. Proven: seam property test (sweep of page-load offset x newest_age_ms, >1000 cases) worst seam < 150 s; graph-break span test red on the no-span rule, green on the fix. Also fixed: stale OTGW-firmware.ino:762 comment, docs/adr/README.md ADR-193 entry, ADR-193 Verification numbers + line ref. Gates: build.bat combo SUCCESS 08:16 (static RAM 117972 B), node 179/179, host test 102 PASS 0 FAIL, evaluate --quick 82/75/0/0, adr governance OK, ADR-193 lint pass. Final v2.js uploaded to the bench (cmp identical): 1 h window 122 rows back to 59.7 min, continuous lines (final-graph-1hour.png). Replay stopped; /otgw_simulation.log left on the bench LittleFS (bench-only).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The device now keeps the Monitor graph series itself and serves them as GET /api/v2/graph/history; the v2 graph loads it when Monitor > Graph opens (again after 10 min) and merges it by slot time with the live records, so the selected window and the CSV export are filled straight away. Boards without PSRAM keep 1 h (static 126 x 30 s ring, ~1.8 KB, ADR-053); boards with PSRAM keep 24 h (726 x 120 s ring, one never-freed PSRAM block via the new platformPsramAllocOnce() shim, a documented ADR-053 exception). Maintainer decision 2026-10-23 after the first 10 KB static design measured too costly with BLE on. ADR-193 (Proposed) records the decision. Dallas sensors stay live-only.

Evidence per AC (logs and screenshots in %TEMP%/bench-1296, no-PSRAM OTGW32 bench unit, synthetic replay with burner cycles):
- AC1 CSV: after 1 h the 1 h CSV held 123 rows back to 59.9 min (after-graph-1hour.csv) vs 3 rows (0.3 min) on the old build (before-graph-1hour.csv).
- AC2 docs + host test: openapi.yaml, docs/api/README.md, ch03/h03, ch09/h09, c4 docs, CHANGELOG; tests/test_graph_history.cpp (real header + real jsonEmit.h, both ring sizes, gap fill, JSON shape, chunk-window reassembly with the margin pinned: 6 commits identical, 7 differ) 102 PASS 0 FAIL; node 179/179 incl. graph-history-merge (18) and graph-break (8), red on HEAD / on the pre-fix rules.
- AC3 window filled: after-graph-1hour.png and final-graph-1hour.png show the whole hour, 10 min window history+live, 4 h window exactly 1 h (no PSRAM); before-graph-1hour.png shows data only at page load.
- AC4 RAM/heap (reboot, 5 min uptime, same conditions): combo static RAM 116180 -> 117972 B; BLE off free 82116 -> 80232, max block 38900 -> 36852; BLE on free 35572 -> 33680, max block 23540 -> 23540 (REST cap stays 2). Rejected 10 KB static ring measured: BLE on max block 15348 (cap 2 -> 1).
Builds: build.bat --target all SUCCESS for esp32, esp32-classic, esp32-combo (platform header changed); final combo 08:16. evaluate --quick 0 fail.

Not verified: the PSRAM (24 h) path on hardware (no PSRAM board on the bench; host-tested only, including the 24 KB chunked body determinism; its async_tcp CPU cost is not measured on a device); the firmware on the bench was built from the tree before two comment-only edits; hd_max_loop_gap_ms 3038-3110 ms seen during the replay runs is not attributed (no old-build value measured).
<!-- SECTION:FINAL_SUMMARY:END -->
