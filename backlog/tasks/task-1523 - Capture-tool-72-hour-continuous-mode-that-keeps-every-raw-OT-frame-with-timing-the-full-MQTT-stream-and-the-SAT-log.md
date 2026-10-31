---
id: TASK-1523
title: >-
  Capture tool: 72-hour continuous mode that keeps every raw OT frame with
  timing, the full MQTT stream and the SAT log
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T11:00:00Z'
updated_date: '2026-10-31T19:17:00Z'
labels:
  - capture
  - alpha
  - tooling
dependencies: []
priority: high
ordinal: 679000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request 2026-10-31 (in session): 'Kan je het capture script zo maken dat ook het ruwe OT berichten met timing bewaard. En ook 3 dagen aaneengesloten data kan loggen. Het moet maximale informatie verzamelen. Liefst inclusief de MQTT logs.' Today scripts/otgw_capture.py (ADR-202, TASK-1475) already records raw frames with host and device time (otframes.log, TASK-1503), the MQTT stream, telnet and HTTP probes, and --duration exists. What it lacks for 3 days: everything lands in one run folder and one merged transcript at the end (gruijter's 45 min capture was 12 MB, so 72 h is about 1 GB), no rotation or compression, no periodic device-state snapshots, and long-run robustness (device reboots, WiFi drops, broker restarts, PC sleep) is only tested over short runs. Feeds the SAT replay and simulation work (TASK-1521 SATTICK lines, TASK-1519).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 --long-run (or --duration 259200) writes hourly segments per stream (otframes, mqtt, telnet) and gzips each finished segment; the run folder has an index (start/end time, line counts, gaps) instead of one merged multi-GB transcript; a short merged summary transcript stays for upload
- [x] #2 No raw OT frame is lost across a /ws reconnect or a device reboot without a gap marker naming the start, end and reason; frames keep host time (ms) and device time
- [x] #3 MQTT: every message with receive timestamp, topic, retain flag and payload, across broker reconnects (gap markers); telnet: SAT log key 5 on, key 8 off by default (option to turn it on)
- [x] #4 Periodic device snapshots every 5 min: /api/v2/device/info, /api/v2/sat (detail=full), heap and loop-gap stats, so trends survive without telnet; at most 1 request in flight (2-concurrent rule)
- [x] #5 Disk budget printed at start (estimate per day) and checked: the run stops cleanly with a summary when free space drops below a floor
- [x] #6 Bench evidence: a run of at least 6 hours on the bench unit with a forced reboot, a WiFi drop (router) and a broker restart in it; every gap is marked and no stream stops for good; the SATTICK CSV (scripts/sat_log_to_csv.py) works on the rotated telnet segments
- [x] #7 README/launcher help explain the 72-hour mode for testers (Windows, Linux, macOS launchers from TASK-1475/1500)
- [x] #8 Each hour zip also contains the CSVs of that hour: SAT records (scripts/sat_log_to_csv.py on the telnet segment) and otframes as CSV (host_time, device_time, source, frame, msgid, type, value)
- [x] #9 At stop the tool writes one upload file (a zip with all hour zips, index.csv, gaps.log and the summary transcript) and prints its path; that one file is what a tester uploads
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 Robert: 'Kan je het script robuust de logging laten doen over langere tijd. Bv. per uur een logfile en zippen.' -> segments per hour, one .zip per finished hour holding all streams of that hour (otframes, mqtt, telnet, snapshots); the live hour stays plain text. Implementation delegated to an agent (scripts only, no firmware).

2026-10-31 Robert: 'Zorg ervoor dat in de logfile die gezipt wordt, ook de CSV-bestanden terechtkomen. Dus de logs moeten ook geconverteerd worden voordat er een zip van gemaakt wordt. Uiteindelijk moet er een bestand zijn die geupload wordt naar mij toe voor diagnose.' -> each hour zip also holds CSVs converted from that hour (scripts/sat_log_to_csv.py on telnet: sattick/satboot/... CSVs; otframes as CSV); at stop one upload bundle (all hour zips + index.csv + summary transcript + gaps.log) as a single file.

Bench run 1 (code before the review fixes, 13:09-14:12, OTGW32 .61 alpha.569/570, SAT sim + OT replay): reboot 13:14 -> telnet gap 12 s, /ws gap 14 s; network drop via MikroTik bridge filter 13:37-13:40 -> telnet 183 s, /ws 158 s; two OTA reboots 13:47; broker kill+restart 14:11:20 -> MQTT gap 39 s, reconnected 14:12:00; hour rotation 14:00 -> hours/20261031-13.zip (650 KB, testzip OK); clean stop by .stop file: 2 hour zips, live/ empty, SATtrace toggle restored; 5670 MQTT msgs, 1850 frames, 13 snapshots. Finding: the run turned on key 8 (SATtrace) -> fixed (--sat-trace, default off). Run 2 on the final code (8527824aa) started 14:16 for 6 h with reboot +15 min, net drop +45 min, broker restart +105 min.

2026-10-31 16:08: scripts/bench_check.py (scripted bench checks, JSON checks, transcript) with --telnet-log to read a running capture's telnet stream (one telnet client per device); live 4/4 PASS on .61 during run 2. discord-post-build.sh refuses an already-published or older prerelease tag (published-tags.txt), part of the speed-up of 2026-10-31.

Bench run 2 (final code, 2026-10-31 14:13:32-20:13:34, 6 h, OTGW32 .61, OT replay + SAT sim, broker .88.32): faults reboot 14:28:33 (telnet gap 11 s, /ws 13 s), MikroTik network drop 14:58:33-15:01:34 (telnet 184 s, /ws 158 s), broker kill+restart 15:58:35-15:59:05 (MQTT gap 39 s, 'disconnected'), plus three OTA reboots during the run (alpha.575/577 flashes 16:56, 17:28; gaps 14-52 s). A second outage 15:01:39-15:06:19 (telnet 280 s, /ws 275 s) right after the network drop: cause not determined, marked and recovered. 15:58:19-15:59 many 1 s telnet gaps: bench_check.py held the single telnet slot (before --telnet-log), marked each time. Every stream continued to 20:13 (index.csv: hours 14-20 all 6 streams, hours 18-19 zero gaps). Upload zip 20261031-141332-upload.zip 4.0 MB: transcript, summary.txt, gaps.log (89 gaps), index.csv, 7 hour zips; each hour zip holds crashlog/mqtt/otframes/otgw_simulation/snapshots/telnet logs + sattick/satboot/satmode/satdhw/otframes/mqtt CSVs (hour 14: sattick 88 rows, otframes 3500, mqtt 5199; hour 17: 121/4670/6476). Snapshots: 72 rounds every 300 s. Summary prints the disk budget and the 1.0 GB floor. Tests fresh this session: test_capture_tool 40 OK (1 skipped, incl. DiskGuardCase, hour zips, gap markers, upload zip), test_capture_otframes 6 OK, test_capture_launcher 7 OK. README scripts/README.md section '3-day capture (long-run mode, TASK-1523)'.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
72-hour long-run mode for scripts/otgw_capture.py: hourly segments per stream, each finished hour zipped with its CSVs (SAT records via sat_log_to_csv, otframes, mqtt), index.csv, gaps.log with start/end/reason per gap, 5-minute device snapshots (one request in flight), disk budget + clean stop below a free-space floor, one upload zip at the end.
Evidence per AC: AC1/AC8/AC9 the 6 h run's upload zip (7 hour zips with logs+CSVs, index.csv, summary transcript) and tests (two hours -> two zips, hour zip holds CSVs, upload zip). AC2/AC3 gaps.log marks the forced reboot, network drop, broker restart and OTA reboots for /ws, telnet and MQTT; otframes.csv carries host_time and device_time; mqtt.csv carries retain; tests for ws drop/reset gap markers. AC4 72 snapshot rounds at 300 s; single-slot poller (ADR-165). AC5 summary prints budget and floor; DiskGuardCase test. AC6 the 6 h bench run with reboot, router drop and broker restart; no stream stopped for good; sattick.csv produced per rotated hour. AC7 scripts/README.md long-run section and launcher help (launcher test 7 OK).
Not verified: a full 72 h run (6 h done); a real disk-full stop on the bench (unit test only); macOS launcher on a Mac (TASK-1504 asks a tester). One 280 s outage after the network drop has no determined cause (marked and recovered).
<!-- SECTION:FINAL_SUMMARY:END -->
