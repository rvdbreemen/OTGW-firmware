---
id: TASK-815
title: 'feat(sat): human-readable test-observability narration (Telnet + Web UI)'
status: To Do
assignee: []
created_date: '2026-06-02 22:25'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a single SAT narration source that emits plain-language, transition-only lines to two sinks: Telnet (DebugTln) and the Web UI live-log (sendEventToWebSocket prefix 'S'). Add a 'SAT only' filter toggle to the log panel. Minimal backend: 1 helper + ~10 call sites + 1 prefix char; no new REST/settings/state. Design: docs/superpowers/specs/2026-06-03-sat-test-observability-design.md. 2.0.0 only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satNarrate_P/satNarratef_P helper emits each line once to both Telnet and sendEventToWebSocket('S',...)
- [ ] #2 Narration fires only on state transitions (flame on/off, cycle verdict, scenario, window/summer, mode change, safety trip/clear, sim start/stop, real-boiler-detected); no periodic lines; existing terse SATDebugTln edges replaced (no double-log)
- [ ] #3 Web UI log panel gains a 'SAT only' toggle filtering to S-prefixed lines; optional severity coloring
- [ ] #4 No String in helper (ADR-004); PSTR literals; no new REST/settings/state
- [ ] #5 Playwright test asserts the SAT-only filter shows exactly the S lines
- [ ] #6 Build clean esp32+esp8266; evaluate.py --quick green; prerelease bumped
<!-- AC:END -->
