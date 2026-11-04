---
id: TASK-1020
title: Combined-stress orchestrator (browser+API+WS+OT emulator)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:21'
updated_date: '2026-07-05 22:51'
labels: []
dependencies: []
ordinal: 231000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Run simultaneously: browser page-load, API polling, WS live-log subscriber, OT traffic via sat_boiler_emulator.py. Scenario S-combined = real-world worst case.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All four load sources run concurrently and are individually measurable
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Wrote scripts/loadtest_combined.py orchestrating all 4 sources concurrently via threads: browser page-load loop (imports loadtest_pageload.py), API load (subprocess of loadtest_harness.py), WS live-log subscriber (new, frame/error counter), OT traffic (auto-detects OTDirect port 25238 -> sat_boiler_emulator.py, else falls back to the device's own telnet 's' OTGW-serial-simulation-replay toggle for PIC boards like the bench esp32-classic — sat_boiler_emulator.py is OTDirect-bridge-only and doesn't apply here). Live-tested on 192.168.88.64: WS 22 frames/0 errors, browser 4 navigations (503s visible under combined load — ADR-147 gate audibly engaging), API 96 req/14x503 at concurrency=2, OT-sim toggle confirmed enabled/disabled via telnet banner match on 'simulation: true/false' (CBOOLEAN text, not English prose — first pass had a banner-text mismatch, fixed). Also fixed a latent bug: sat_boiler_emulator.py has no --duration flag (runs until Ctrl-C by design) — orchestrator now bounds it with subprocess.Popen + communicate(timeout=), not an invalid CLI flag.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/loadtest_combined.py runs the S-combined worst-case scenario: browser page-loads + API polling + WS subscriber + OT traffic simultaneously for a fixed duration, each source reporting its own independent metrics in one JSON summary (AC#1). OT-traffic source is auto-selected by board capability (OTDirect emulator vs onboard serial-sim replay) since the bench device for this study (esp32-classic, TASK-1016) is PIC-path, not OTDirect. Live run on 192.168.88.64 showed real 503s from the ADR-147 backpressure gate under combined load, exactly the signal Phase 1 (TASK-1022) needs.
<!-- SECTION:FINAL_SUMMARY:END -->
