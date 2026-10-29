---
id: TASK-1475
title: >-
  Capture tool: one Python implementation replaces capture-mqtt-debug .bat/.sh
  (local first)
status: To Do
assignee: []
created_date: '2026-10-29T14:05:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 635000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29 (TASK-1472, interactive choice 'Unify on Python'): rebuild capture-mqtt-debug.bat (PowerShell payload) and capture-mqtt-debug.sh (bash + python3 workers) as ONE cross-platform Python script, so each fix lands once. Keep the thin .bat/.sh/.command launchers as entrypoints that bootstrap Python where it is missing (Windows: like build.bat's venv/toolchain bootstrap, e.g. the embeddable CPython zip; macOS/Linux: system python3). Same collected sources as today: telnet :23 debug (reconnect, keepalive, toggles, q/D), MQTT (paho-mqtt replaces mosquitto_sub, so no winget/brew install), headless Edge/Chrome CDP devtools, crashlog + reboot_log poll, REST metadata, curl probes; same merged transcript format. Fold in TASK-1396 (q/D during setup, probe bodies cut at 200 chars) and TASK-1453 (broker optional on dev). Local only: no upload anywhere (TASK-1473 parked). Needs an ADR (tooling change, new dependency paho-mqtt) and an open-source check recorded per project rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR (Proposed via adr-kit) for the Python capture tool, its bootstrap and the paho-mqtt dependency
- [ ] #2 One Python implementation collects every source today's scripts collect; transcript format compatible (diff of section headers against a current .bat and .sh capture)
- [ ] #3 Launchers run on stock Windows 11 without a preinstalled Python, and on macOS and Linux; evidence = a capture run on each available OS
- [ ] #4 TASK-1396 and TASK-1453 behaviour fixed in the new tool (tests red on the old scripts' behaviour, green)
- [ ] #5 Old .bat/.sh payloads removed; docs/guides and the flash-zip README updated
<!-- AC:END -->
