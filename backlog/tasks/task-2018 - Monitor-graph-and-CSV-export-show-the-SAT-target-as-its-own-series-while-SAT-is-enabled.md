---
id: TASK-2018
title: >-
  Monitor graph and CSV export: show the SAT target as its own series while SAT
  is enabled
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T09:30:00Z'
updated_date: '2026-11-18T21:33:00Z'
labels:
  - webui
  - sat
  - graph
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1034000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
92's analysis of indigo's graph CSVs (via coordinator, 2026-11-18): the Monitor graph and its CSV export plot roomSet = MsgID 16 TrSet (v2.js:2392, live model; graphHistoryStuff.ino:61 for the device ring), also while SAT is active. The setpoint that actually drives the boiler under SAT is the SAT target, which the graph never shows. Checked 2026-11-18 in wt-ba-2015: the 24 h/7 d device history ring (ADR-193, ADR-245) does NOT store the SAT target: GraphHistSlot (graphHistoryStuff.h:62-72) holds flow, ret, chSet, roomSet, room, outside, mod, flags, static_assert 14 bytes. Options: A) add an int16 satTarget column (slot 14 -> 16 bytes, ring RAM/PSRAM +14 %, ring format and /graph-history response change: needs an ADR amendment of ADR-193/245 and Robert's OK); B) live-only series in the browser from /v2/sat/status (no ring change, history panel shows it only for the open-tab span); C) write the SAT target into the roomSet column while SAT is active (no format change, but the column's meaning becomes mode-dependent). Do after TASK-2015 / beta.371.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The Monitor graph shows a separate SAT target series while SAT is enabled, with its own legend entry and translation in en/nl/de/el
- [ ] #2 The CSV export carries the SAT target as its own column
- [ ] #3 Decision on the history source (ring column vs live-only) recorded in the task; a ring format change goes through an ADR amending ADR-193/245
- [ ] #4 Web UI test drives the real v2.js: red on the old code, green on the fix; screenshot with synthetic data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Groundwork 2026-11-18 (otgw-webui, read-only, no code). Ring sizes from graphHistoryStuff.h:41-47: without PSRAM the ring is 126 slots of 30 s = 1 h (GRAPH_HIST_SMALL_SLOTS, static array graphHistSmallSlots, graphHistoryStuff.ino:21, internal RAM), not 24 h; with PSRAM 5046 slots of 120 s = 7 d (GRAPH_HIST_LARGE_SLOTS, platformPsramAllocOnce, graphHistoryStuff.ino:29). The 24 h figure is the default response window (GRAPH_HIST_DEFAULT_WINDOW_S), not a ring size.

Option A cost (exact, slot 14 -> 16 bytes, int16 satTarget): no PSRAM 126 x 14 = 1764 B -> 126 x 16 = 2016 B, +252 B internal RAM (.bss). PSRAM 5046 x 14 = 70644 B -> 5046 x 16 = 80736 B, +10092 B PSRAM (of 8 MB). Response (estimate, not measured): one more JSON column of at most GRAPH_HIST_MAX_EMIT = 1008 values at about 4-5 chars each (e.g. 205, or null,), about +4-5 KB on a full 7 d response. Touches: GraphHistSlot + static_assert (graphHistoryStuff.h:62-72), graphHistEmptySlot, the writer (graphHistoryStuff.ino:57-71, source state.sat.fTargetTemp SATtypes.h:527, NO_TEMP while SAT is off), the emitter (graphHistoryStuff.h:~293), v2.js TCOLS (:3335) and series list (:3212), GSER_LABEL_KEY (:3423), i18n en/nl/de/el, tests that build ring JSON (graph-history-merge, graph-7day-window, graph-room-dropout, test_capture_device_data.py). The ring is RAM only, so no persisted-format migration; old browsers ignore an extra column. Needs an ADR amending ADR-193/245 (slot size is in both) and the needs-maintainer-ok label.

Option C (SAT target written into roomSet while SAT is on) breaks both: (1) legend and CSV: the series is labelled thermostat setpoint (v2.js:3212, i18n graph.series_room_set) and the CSV header is roomSet (exportCsv v2.js:5884-5888), so a SAT target would be exported and shown under the thermostat's name; test graph-control-setpoint-source.test.mjs:153 pins that label to MsgID 16. (2) the column turns mode-dependent with no marker in the CSV: the slot flags only carry flame/DHW/CH (graphHistoryStuff.h:54-56), so a window that spans a SAT on/off switch mixes two quantities in one column, the exact confusion 92 hit in indigo's CSVs. (3) the thermostat's own setpoint is lost while SAT runs, which the room tile shows on purpose (room-tile-sat-target.test.mjs). (4) live model (MsgID 16, v2.js:2392) and ring would disagree unless both change. Option B (live-only from /v2/sat/status) gives no history after a page reload and adds a poll path for the graph (2-in-flight cap).

Recommendation: A. Cost is small and exact (+252 B internal RAM without PSRAM, +10 KB PSRAM, ~+4-5 KB on the largest response), the column keeps one meaning, CSV and legend stay truthful, and both setpoints stay visible. Main risk: the ring format change touches every test fixture that builds ring JSON and needs the ADR amendment plus Robert's OK; mitigate by adding the column at the end of the emitter so older readers ignore it. C is rejected for semantic overload; B only as a fallback if Robert declines the format change. Label needs-maintainer-ok to be added if A is chosen.

Maintainer decision (Robert, 2026-11-18 12:14:15 UTC, Discord DM 1572585729098383494), verbatim reply: 'A'. It answers the coordinator's question 1572584352477618247, option A verbatim: "A) Eigen kolom in de grafiekhistorie (slot 14 -> 16 B): +252 B intern RAM zonder PSRAM, +10 KB PSRAM, eerlijke lijn + CSV-kolom; vraagt een ADR die 193/245 aanvult." Label needs-maintainer-ok removed. Next: Proposed ADR amending ADR-193/245 (User Decision), then the implementation in lane wt-ba-2018.

WIP 2026-11-18, PARKED (coordinator: the no-PSRAM BLE heap bug comes first). The implementation is uncommitted in wt-ba-2018 (lane/ba-2018, own tree): graphHistoryStuff.h/.ino (slot 16 B, satTarget, hasSatTarget in the snapshot, column only when the window holds a value), v2.js (GSER satTarget, live slot from satEnabledLive + sat.target, optional column in mergeGraphHistory, label key, dashed line), i18n nl/de/el series_sat_target (SAT-doel / SAT-Ziel / στόχος SAT, not native-reviewed), otgw_capture.py CSV column, openapi + API README. Evidence so far: test_graph_history red on origin (compile: no satTarget member), green on the fix; a mutant that always emits the column fails 6 checks (exact-JSON byte-identity guard). Measured dense 24 h body 23915 B -> 26809 B with SAT (+12.1 %), square-rule work ratio 1.257 (the ADR estimate was 12-14 % / 25-30 %). New tests/webui/graph-sat-target.test.mjs 5/5 on the fix, 0/5 on origin v2.js. graph-* web UI tests all green after adding the satEnabledLive stub to graph-control-setpoint-source and graph-dhw-temperature; i18n-lang 6/6, v2-undefined-names 2/2. Still to do: tests/test_capture_device_data.py test_header_and_rows (pins the CSV header, now with satTarget), the ADR-256 numbers update, prerelease bump number from the coordinator, build after Robert accepts ADR-256.

2026-11-18 (while 375 waits for the bench): tests/test_capture_device_data.py updated for the satTarget CSV column (header and rows get the extra cell after roomSet; new case: satTarget present, and a wrong-length array gives an empty column like the web UI): 21/21 on the fix; the same test on the origin capture tool fails on the header (no satTarget column). The ADR-256 text still carries the estimate (+12-14 % body, 25-30 % work); the measurement is +12.1 % (23915 -> 26809 B) and a work ratio of 1.257. It goes into ADR-256 together with its acceptance, so the text Robert is reviewing does not change under him. 2018 stays parked until his ADR-256 answer; it rebases onto 375 then and becomes 376.

2026-11-18: HELD LOCALLY (coordinator). The coordinator asked Robert whether 2018 is 2.0.0 or 2.1.0, recommending 2.1.0 (no features in the last RC days; release/2.x is cut from dev ~21 Nov). Milestone set to the default 2.0.0 meanwhile. No push until he answers, even if the ADR-256 accept comes first.

Maintainer decision (Robert, 2026-11-18, Discord DM 1572641412850393190), verbatim reply: 'B' (B = milestone 2.1.0, on the coordinator's question 2.0.0 vs 2.1.0). Milestone set to 2.1.0. The lane wt-ba-2018 stays local until release/2.x is cut for 2.0.0 stable (~21 Nov); the ADR-256 accept can still come and does not change where the code lands.

Maintainer decision (Robert, 2026-11-18, Discord DM 1572707835991818413), verbatim: 'Ja, maar bouw het voor 2.0.0'. ADR-256 Accepted on it; milestone back to 2.0.0. This reverses his earlier 'B' (DM 1572641412850393190), which had moved the task to 2.1.0. The lane now lands on dev like other 2.0.0 work (coordinator): rebased onto origin/dev 8b4128dfd, host tests red/green, evaluate, build after d3's 376/377 build, bench by 09, push. Measured on the rebased tree (tests/test_graph_history.py): dense 24 h body 23915 B without the SAT target, 26809 B with it (+12.1 %), formatting work ratio 1.257.

2026-11-18 rebased onto origin/dev f11764ecb, bump beta.378. Evidence on the rebased tree: tests/test_graph_history.py + tests/test_capture_device_data.py 22 passed (INFO dense 24 h body 23915 B -> 26809 B, +12.1 %, ratio 1.257); web UI graph-sat-target 5/5 on the fix, 0/5 with OTGW_V2_JS at the origin/dev 377 v2.js (red); graph-control-setpoint-source 11/11, graph-dhw-temperature 4/4; python evaluate.py --quick 88 passed, 0 failed, 0 warnings. Next: esp32-combo build, bench by 09.
<!-- SECTION:NOTES:END -->
