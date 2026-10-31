---
id: TASK-1523
title: >-
  Capture tool: 72-hour continuous mode that keeps every raw OT frame with
  timing, the full MQTT stream and the SAT log
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T11:00:00Z'
updated_date: '2026-10-31T12:20:00Z'
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
- [ ] #1 --long-run (or --duration 259200) writes hourly segments per stream (otframes, mqtt, telnet) and gzips each finished segment; the run folder has an index (start/end time, line counts, gaps) instead of one merged multi-GB transcript; a short merged summary transcript stays for upload
- [ ] #2 No raw OT frame is lost across a /ws reconnect or a device reboot without a gap marker naming the start, end and reason; frames keep host time (ms) and device time
- [ ] #3 MQTT: every message with receive timestamp, topic, retain flag and payload, across broker reconnects (gap markers); telnet: SAT log key 5 on, key 8 off by default (option to turn it on)
- [ ] #4 Periodic device snapshots every 5 min: /api/v2/device/info, /api/v2/sat (detail=full), heap and loop-gap stats, so trends survive without telnet; at most 1 request in flight (2-concurrent rule)
- [ ] #5 Disk budget printed at start (estimate per day) and checked: the run stops cleanly with a summary when free space drops below a floor
- [ ] #6 Bench evidence: a run of at least 6 hours on the bench unit with a forced reboot, a WiFi drop (router) and a broker restart in it; every gap is marked and no stream stops for good; the SATTICK CSV (scripts/sat_log_to_csv.py) works on the rotated telnet segments
- [ ] #7 README/launcher help explain the 72-hour mode for testers (Windows, Linux, macOS launchers from TASK-1475/1500)
- [ ] #8 Each hour zip also contains the CSVs of that hour: SAT records (scripts/sat_log_to_csv.py on the telnet segment) and otframes as CSV (host_time, device_time, source, frame, msgid, type, value)
- [ ] #9 At stop the tool writes one upload file (a zip with all hour zips, index.csv, gaps.log and the summary transcript) and prints its path; that one file is what a tester uploads
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 Robert: 'Kan je het script robuust de logging laten doen over langere tijd. Bv. per uur een logfile en zippen.' -> segments per hour, one .zip per finished hour holding all streams of that hour (otframes, mqtt, telnet, snapshots); the live hour stays plain text. Implementation delegated to an agent (scripts only, no firmware).

2026-10-31 Robert: 'Zorg ervoor dat in de logfile die gezipt wordt, ook de CSV-bestanden terechtkomen. Dus de logs moeten ook geconverteerd worden voordat er een zip van gemaakt wordt. Uiteindelijk moet er een bestand zijn die geupload wordt naar mij toe voor diagnose.' -> each hour zip also holds CSVs converted from that hour (scripts/sat_log_to_csv.py on telnet: sattick/satboot/... CSVs; otframes as CSV); at stop one upload bundle (all hour zips + index.csv + summary transcript + gaps.log) as a single file.
<!-- SECTION:NOTES:END -->
