---
id: TASK-1503
title: >-
  Capture tool: raw OpenTherm frame log with timing, in a format the replay can
  read (field data for replays)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T06:15:00Z'
updated_date: '2026-10-30T06:55:00Z'
labels:
  - tooling capture replay
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 659000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-30: before the first beta the capture tool must also record a raw log of every OpenTherm frame with its timing, so field captures can be replayed properly. Today otgw_capture.py records telnet/MQTT/browser/crashlog/HTTP probes but no dedicated raw frame file, and the firmware replay (/simulate, /otgw_simulation.log) reads bare frames ('T10010A00', otgwSimulationFixture.h) at a fixed interval (state.debug.iOTGWSimulationIntervalMs, OTGW-Core.ino:4263, :4278): it ignores timing. Overlap: TASK-1470 (device-side 24 h recording + host replay, To Do) - this task is host-side capture only, TASK-1470 stays the on-device route.

Design constraints (coordinator review 2026-10-30): (1) the source must work by default on PIC and OT-Direct boards without a persistent settings change; if it needs a toggle (e.g. port 25238 or a debug key), restore it at stop like the debug toggles (TASK-1453); (2) keep the device timestamp AND the host receive time (known quirk: frame gap = this timestamp minus the previous line's, see reference_otlog_timestamp_skew); (3) keep the source letters (T/B/R/A/E...) so a replay can drop SAT's own frames; (4) write a second file in exactly the replay format (bare frames) so it can be uploaded as /otgw_simulation.log today; (5) a separate follow-up task scopes a timed replay (firmware or host) that honours the recorded deltas.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Capture writes otframes.log: one line per frame with host receive time (ms), device timestamp if present, source letter and 8 hex digits; plus otgw_simulation.log in the replay format; both listed in summary.txt
- [x] #2 Works on a PIC board and on OT-Direct (or OT-Direct stated as not verified while no unit is on the bench), without leaving a changed setting behind
- [x] #3 A captured file replays on the bench through /simulate and the decoded values match the capture (frame count and a few MsgIDs compared)
- [x] #4 Docs: capture guide describes the file and how to use it for a replay; a follow-up task for timed replay is filed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 (coordinator otgw-firmware-b8, wt-1503 on top of TASK-1500). Source: the device /ws OT log stream (on by default on PIC and OT-Direct; OTGW-Core.ino processOT ~5353-5581, sent :5652 via sendLogToWebSocket webSocketStuff.ino:289; timestamp getOTLogTimestamp helperStuff.ino:19). Rejected: port 25238 (off by default, MQTTstuff.h:73), telnet (needs debug toggles, later timestamp), no REST stream. WS cap 3 clients (webSocketStuff.ino:60, :169): recorder + headless browser leave one slot for the user's browser (README). Files: otframes.log (TSV: host ISO time ms+offset, device timestamp, letter, 8 hex; # header) and otgw_simulation.log (bare frames, LF). tests/test_capture_otframes.py: 6 errors before the implementation, 6 OK after (re-run by the coordinator: OK). Bench .63 alpha.549: 75 s run -> 75 frames, all R80000200 (no boiler/thermostat on the bench), median device gap 0.9995 s, telnet cross-check within ~3 ms, /api/v2/otgw/messages/0 = 512 = 0x0200. AC#3: the captured otgw_simulation.log uploaded via /upload (750 B served), POST /api/v2/simulate/start -> source:file, active; stopped and deleted afterwards (404, source builtin). Limits: live and replayed frames are identical on this bench, so decoding of replayed frames is not distinguishable from live ones; replay interval is a fixed 750 ms vs the recorded 1.0 s -> follow-up TASK-1508. Not verified: OT-Direct (no unit), T/B/A/E letters live (parser tests only), reconnect against the real device, frame loss under heap pressure.
<!-- SECTION:NOTES:END -->
