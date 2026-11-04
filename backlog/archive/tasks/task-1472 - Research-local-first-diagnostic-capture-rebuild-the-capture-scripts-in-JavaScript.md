---
id: TASK-1472
title: >-
  Research: local-first diagnostic capture, rebuild the capture scripts in
  JavaScript?
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T12:43:00Z'
updated_date: '2026-10-29T14:08:00Z'
labels:
  - tooling
  - research
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 632000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: prefers captures done locally (follow-up of gruijter's TASK-1469 and TASK-1470). Research whether the capture scripts (scripts/capture-mqtt-debug.bat/.sh, capture-otgw.sh and helpers) should be rebuilt in JavaScript (browser page served by the device, or Node.js), compared with the on-device 24 h recorder of TASK-1470, and write a proposal with options, consequences and a recommendation for Robert to decide.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Inventory of what the current capture scripts collect, with sources (telnet, MQTT, REST, browser devtools, crashlog) and which of those a browser page can reach
- [x] #2 At least two open-source candidates per JS route evaluated (licence, maintenance, size)
- [x] #3 Proposal with options, concrete consequences and a weighed recommendation put to Robert interactively
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-10-29: local first; cloud upload parked as a later feature (task-1473). Captures and recordings stay on the user's own network.

2026-10-29 research report (agent, spot-checked by lead):
- Capture scripts: capture-mqtt-debug.bat (2623 lines, PowerShell payload) and .sh (1884, bash+python3); collect telnet:23 debug, MQTT via mosquitto_sub, headless Edge/Chrome CDP devtools, crashlog + reboot_log poll, REST metadata and curl probes. Pain points are reliability/drift (TASK-1299, 1320, 1396, 1453), fixed 2-3 times per change; not a language problem.
- Browser reach: /ws carries OT log lines + SAT narration only (webSocketStuff.ino:225,266,289-301), max 3 clients; debug text is telnet only, 1 client (networkStuff.ino:33). REST CORS echoes Origin (restAPI.ino:143-151). MQTT only via a broker WS listener.
- v2 UI already has a capture mode (200k lines, 15-min auto-download, Stream-to-file v2.js:3541). Stream-to-file uses showSaveFilePicker, which needs a secure context: likely never available on the plain-HTTP device page (to verify on bench).
- Routes: A browser page (no debug text, background-tab throttling over 24 h); B Node/Deno/Bun single binary (62-150 MB per OS, breaks zip delivery); B' one Python script (Windows often lacks Python); C on-device recorder (full stream 13-17 MB/day does not fit: LittleFS 768 KB ~1 h, PSRAM 2 MB ~3 h; compact OT-only ~2-3 MB/day estimate).
- OSS: MQTT.js 5.16.1 MIT (105 KB gz); paho-mqtt JS 1.1.0 EPL stale; telnet-client 2.2.14 MIT; telnet-stream AGPL (excluded); streamsaver 2.0.6 needs service worker = secure context (excluded on HTTP); native-file-system-adapter 3.0.1 MIT.
- Recommendation: C+A with one host script kept for telnet/MQTT; no JS binary.

2026-10-29 Robert chose 'Unify on Python' (interactive question; options were combination C+A / JS rewrite / Python / browser-only). Follow-up: TASK-1475 (Python capture tool). Side finding filed separately: Stream-to-file secure-context check.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Research done (report in notes, key repo facts spot-checked: v2.js:3541 showSaveFilePicker detection, ADR-193 graph history). Proposal with four routes, consequences and a recommendation (C+A) was put to Robert interactively; he chose to unify the capture scripts on Python, implemented under TASK-1475. Cloud upload parked as TASK-1473. Not verified: the secure-context behaviour on the device page and the 24 h browser/recording estimates (filed as a bench check).
<!-- SECTION:FINAL_SUMMARY:END -->
