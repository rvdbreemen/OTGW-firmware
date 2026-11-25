---
id: TASK-1964
title: >-
  Capture tool: collect every read-only diagnostic GET, the graph history as CSV
  and a running diag recording into the upload zip
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T10:03:00Z'
updated_date: '2026-11-25T06:40:00Z'
labels:
  - capture
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1004000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, 2026-11-15 (in session 'Ja', and DM 1571452141237968907: 'collect ALL possible data with the capture script, so users don't have to share anything separately'; relayed by the coordinator). Testers such as gruijter should not have to export or share anything by hand. scripts/otgw_capture.py (stdlib only, ADR-202): (1) fetches GET /api/v2/graph/history (ADR-193) at start, at stop and every hour in a long run, and writes graph-history-<ts>.csv with the same columns and values as the web UI export (v2.js exportCsv: epoch_ms,flow,ret,chSet,roomSet,room,outside,mod,flame,dhw_on,ch; slot time as mergeGraphHistory); also ?hours=168 (7 days, 10-min points, ADR-245 by 92) when the firmware answers it, never failing on an older firmware; (2) when a diag recording (ADR-213) is active, downloads GET /api/v2/diag/recording into the upload and never sends start or stop (stop makes the PSRAM ring unreadable, TASK-1960); (3) walks the REST route table (restAPI.ino) and fetches every read-only diagnostic GET at start and stop, hourly for the ones that change; never secrets (settings stays masked as now), never an endpoint with side effects. The task notes list which endpoints are in and out, with the reason. Optional: a gate that warns when a new GET route is in neither list. README of the capture tool updated; the flash zip carries the script through build.py. Scripts only, no firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a recorded /api/v2/graph/history response converts to the web UI CSV (header, epoch_ms per slot, scaled temperatures, empty cells for null, gap slots dropped); red on the current script, green on the fix
- [x] #2 Graph history fetched at start, stop and hourly in long-run mode (24 h, plus 7 d when supported); every graph-history-*.csv is in the upload zip; a failed or unsupported fetch is one logged line, never a capture stop
- [x] #3 With a diag recording active the upload holds the downloaded recording; with none active nothing is fetched; the tool never sends recording start or stop (test checks no POST)
- [x] #4 Every read-only diagnostic GET of the route table is fetched at start and stop (hourly where it changes) into the upload; the in/out list with reasons is in the task notes; no secret and no side-effect endpoint is fetched (test on the list)
- [x] #5 No new dependency: stdlib only (ADR-202)
- [x] #6 Bench (09 on .63): the capture CSV has the same 11 columns in the same order as the web UI export, and the slot timing matches (coordinator split of the former AC6, 2026-11-15)
- [ ] #7 Field: the values of a capture CSV match the CSV the tester exported by hand for the overlapping slots (first gruijter capture)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 endpoint inventory (route table of restAPI.ino processAPI and every handler, plus the non-/api routes; read-only research by a subagent, cross-checked against the handlers for the side-effect and secret claims). IN, at start and stop (* = also hourly in a long run): health*, device/info*, device/time, device/crashlog, flash/status, settings (passwords already masked by the firmware; webhook URL/payload redacted by the tool), debug* (passwords shown as ***), debug/flags, sensors/status*, sensors/labels, simulate, discovery, filesystem/files (listing, never ?delete=), filesystem/hash-check, pic/flash-status, pic/settings (interpretation: Robert named it; its GET starts the same PR= readout the web UI PIC panel starts, ~15 PR= over 45 s, read-only on the PIC), otgw/otmonitor* (rate limit 1/1.5 s, one request at a time stays under it), otgw/overrides*, otgw/boiler-support*, otgw/ot-support*, otdirect/status*, otdirect/settings, otdirect/overrides*, sat/status?detail=full*, sat/markers, sat/sensor-areas, sat/ble/roster (MACs, bind keys never sent), sat/ble/discovery*, sat/weather* (latitude/longitude redacted), diag/recording/status*. LittleFS state files at start and stop: reboot_log.txt, reboot_count.txt, bootdetect.log, dallas_labels.ini, ot-boiler.json, ot-thermo.json, dhw_water.json, sat_pid_state.json, sat_pwm_state.json, sat_tune.json, sat_markers.json, sat_energy.json, sat_energy_estimate.json, sat_hcr.json. Separately: graph/history (24 h + ?hours=168), diag/recording (download, since=), device/coredump (existing fetch).

OUT, with the reason: settings.ini (passwords in plain text, and readable without auth when no HTTP password is set), pic/update-check (queues an outbound request to otgw.tclcode.com), firmware/files (can write .ver files to LittleFS), network/scan (starts a WiFi scan, a read consumes the result), otgw/messages/<id> and otgw/id/<id> (134 single-value requests; otmonitor + ot-support hold the data), otgw/label/<label> (same), otgw/telegraf (same body as otmonitor), sat and sensors without a sub-path (aliases of sat/status, sensors/status), webhook and mqtt (no GET route), /ReBoot (any method reboots), /ResetWireless (wipes WiFi and reboots), /pic (no auth; can queue a PIC flash, a download or a file delete: by design, feedback memory), the static pages and /ws. The tool never sends a POST/PUT, never adds ?delete=. Gate: tests/test_capture_device_data.py RouteGateCase fails when a top-level REST resource of restAPI.ino is in neither SWEEP_ENDPOINTS nor SWEEP_EXCLUDED (it does not see a new sub-route of a covered resource). Side finding: /api/v2/sat/weather/needs-setup is unreachable (restAPI.ino:1532 checks wc==5, the path has 6 words); reported to the coordinator.

2026-11-15 implementation (scripts/otgw_capture.py, stdlib only): graph_history_rows/graph_history_csv (the web UI mergeGraphHistory + exportCsv rules), fetch_graph_history (24h + 7d, 7d kept only with a coarser interval_s), fetch_diag_recording (status first, GET download, since=, never start/stop), sweep_device + redact_json (SWEEP_ENDPOINTS/SWEEP_FILES/SWEEP_EXCLUDED), DeviceDataCapture thread (start round off the main thread, hourly rounds in long-run mode; pic/settings start+stop only), stop round in main(), upload zip globs, flags --skip-device-state/--skip-graph-history/--skip-diag-recording (+ PowerShell names). Evidence: tests/test_capture_device_data.py 18/18 OK on the fix; the same file against origin's script (temp copy) FAILED (failures=1, errors=15) of its 16 cases at the time; SameAsWebUiCase runs the REAL v2.js mergeGraphHistory + GSER via node and compares the CSV byte for byte on 3 responses: identical. tests/test_capture_tool.py: 3 watchdog cases went red on the first version (the sweep's /api/v2/health GETs count as probes there); those cases now skip the device-data features like they skip http-probes, 52/52 OK (origin script 52/52 OK too); new end-to-end ToolCase.test_j2 (device-state-start and -stop in the upload zip, 404 lines for graph/diag). All 6 capture test files PASS via run_tests (-j 3). Flash zip: OTGW-firmware-esp32-combo-2.0.0-beta.306+041400e-flash.zip already holds otgw_capture.py (build.py:1262 copies scripts/otgw_capture.py), so the next build ships this version.

2026-11-15 AC6 split by the coordinator: bench part = columns (11, same order, identical) + slot timing, PASS by 09 on .63 with beta.306 and the pushed script (coordinator relay); the value comparison is a field AC (gruijter's first capture, who also exports the CSV by hand). New AC6 ticked from 09's PASS; AC7 open (field).

2026-11-16 close-out (ba), tick fix: 09's .63 PASS on beta.306 (coordinator relay, 2026-11-15) covered the 11 columns in the web UI order and the slot timing, which is the text of AC7, so AC7 is ticked. AC6 (values match a web UI export of the same minute) had been ticked on that same run, but the coordinator's split of 2026-11-15 moved the value comparison to the field (AC8, gruijter's first capture with his own CSV export); no bench run compared values. AC6 is therefore removed as a duplicate of AC8 (its claim was not evidenced), not ticked. Open: AC8 (field).

2026-11-19 triage (otgw-dev): AC7 field-blocked: needs gruijter's first capture next to his hand-exported CSV for the overlapping slots.

2026-11-20 (ba, coordinator decision): AC7 via a tester ask. 92 posted in #beta-testing, message 1573143902511431691: gruijter exports Monitor > Graph 24 h CSV during a capture run (beta.392) and sends it with the zip. Field archive checked first: no overlap (newest hand exports 2026-11-13/15, first capture graph-history CSVs 2026-11-16 after reflashes). Supporting bench variant by 09 on .63 after the beta soak; AC7 is ticked only on field data, otherwise it goes to the coordinator's waiver list for Robert.

2026-11-20 milestone 2.0.0 -> 2.1.0 (otgw-docs, coordinator's assignment). Robert DM 2026-11-20 01:15 (1573144814600462457), relayed by the coordinator: "C — field/hardware ACs outside the 2.0.0 scope, to 2.1.0". Status, assignee and ACs unchanged.
<!-- SECTION:NOTES:END -->
