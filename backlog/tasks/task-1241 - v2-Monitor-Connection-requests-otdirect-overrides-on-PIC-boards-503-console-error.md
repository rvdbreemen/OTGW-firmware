---
id: TASK-1241
title: >-
  v2: Monitor > Connection requests otdirect/overrides on PIC boards (503 +
  console error)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-17 09:24'
updated_date: '2026-10-17 14:25'
labels: []
dependencies: []
priority: low
ordinal: 379000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Opening Monitor > Connection always calls fetchOtdOvr() (v2.js:181), which GETs /api/v2/otdirect/overrides. On a PIC board that returns 503 'No OT-direct hardware' and logs a console error. otdirect/status is already gated on the board type (v2.js:1397); the overrides call is not. Found in the alpha.427 Classic-S3 bench rerun (.tmp/bench-classic/after427/ui_I.json, load 5). Also counts against the 2-in-flight REST budget for nothing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On a PIC board, opening Monitor > Connection makes 0 requests to /api/v2/otdirect/overrides and logs 0 console errors (browser capture on the Classic-S3)
- [ ] #2 On an OT-Direct board the overrides panel still loads (browser capture on the OTGW32)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (alpha.428): showMonTab('mconn') calls fetchOtdOvr() only when withDeviceInfo() reports otcommandinterface 'OT-Direct', or when the snapshot is unknown (null), following withDeviceInfo's no-negative-verdict-from-null contract. Same test as the otdOvr panel visibility. Old-vs-fix on the Classic-S3 (PIC), Playwright probe .tmp/bench-1241/conn_tab_probe.py, 3 fresh loads each: old v2.js (alpha.427+cdf90cf) 3/3 loads request otdirect/overrides -> 503 plus a console error (.tmp/bench-1241/old-cdf90cf.json); fixed minified v2.js uploaded to the device 0/3 requests, 0 console errors (fix-v2js.json). The built LittleFS staging v2.js is byte-identical to the tested file. AC2 simulation only: the real v2.js with device/info + health answered as OT-Direct (Playwright route) requests otdirect/overrides, shows the panel and renders the fake list 'SR MsgID 27 0x1234', 0 console errors (.tmp/bench-1241/otd-sim.json, otd-sim-connection.png). AC2 stays open until a browser capture on the real OT-Direct board: the OTGW32 did not answer on .88.61 on 2026-10-17. Gates: build.bat --target esp32-combo SUCCESS (fw + fs), evaluate --quick 0 FAIL.
<!-- SECTION:NOTES:END -->
