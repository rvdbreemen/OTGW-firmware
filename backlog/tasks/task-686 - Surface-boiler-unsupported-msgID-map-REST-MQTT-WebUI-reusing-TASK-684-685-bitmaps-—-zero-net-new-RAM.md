---
id: TASK-686
title: >-
  Surface boiler unsupported-msgID map (REST + MQTT + WebUI), reusing
  TASK-684/685 bitmaps — zero net new RAM
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 06:46'
updated_date: '2026-05-24 07:15'
labels:
  - diagnostics
  - beta.20
  - opentherm
  - observability
  - user-experience
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Once TASK-685 lands, the unknownLoggedRead and unknownLoggedWrite bitmaps in processOT() are a complete in-RAM record of every msgID the boiler has flagged as Unknown-Data-Id, split by direction. They live in 64 bytes total (2 x 32). Today this state is invisible to the user — they only see the telnet log, which a non-developer never opens.

This task surfaces the map through three channels so end-users, HA dashboards, and field testers can see 'what my boiler implements' without grepping telnet:

1. REST: GET /api/v2/ot/boiler-support
   - Streamed JSON: { 'unsupported_read': [27, 33, 36, ...], 'unsupported_write': [14, 16, ...] }
   - Optionally include the OT label string per id (PROGMEM lookup, no extra RAM).
   - Built lazily from the bitmap on each request; no extra RAM beyond a per-request response buffer.

2. MQTT retained: otgw-firmware/boiler/unsupported_msgids
   - Published once on MQTT-connect (after the OT-bus discovery window settles) and then only when a NEW unsupported msgID is added (rare event after warm-up).
   - Single CSV string payload: '14W,16W,24R,26R,27R,33R,36R' (R=read, W=write).
   - Mechanism: a single 'dirty' boolean bit; the periodic publish loop checks the bit and republishes if set, then clears.

3. WebUI (FSexplorer / stats page):
   - One human-readable line: 'Boiler does not implement: 14 (MaxRelModLevelSetting/W), 16 (TrSet/W), 24 (Tr/R), 26 (Tdhw/R), 27 (Toutside/R), 33 (Texhaust/R), 36 (ElectricalCurrentBurnerFlame/R)'.
   - Rendered server-side as part of the existing stats HTML/JSON.

Memory budget:
- 64 bytes already accounted for by TASK-684/685.
- This task adds: 1 'dirty' byte for the MQTT republish gate. Total new = 1 byte.
- No String allocations in the hot path; the CSV / JSON is built on demand into a stack char[] (size ~120 bytes max for the worst case of 30 unsupported ids).

Out of scope:
- HA discovery suppression for unsupported msgIDs (that's TASK-687, milestone 2.1.0).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/ot/boiler-support returns 200 with a JSON body listing unsupported_read and unsupported_write arrays, each entry being { id, label }.
- [x] #2 MQTT retained topic otgw-firmware/boiler/unsupported_msgids is published on every MQTT (re)connect with the current bitmap snapshot as a CSV string ('14W,16W,24R,26R,27R,33R,36R').
- [x] #3 When a NEW unsupported msgID is observed at runtime (bitmap bit goes 0->1), a 'dirty' flag is set; the next sendStats / periodic publisher republishes the CSV and clears the flag. The dirty flag is the only added per-process RAM beyond the existing 64 bytes of bitmap.
- [x] #4 The stats page (FSexplorer / dashboard) shows a human-readable line listing the unsupported msgIDs with names and direction suffix (R/W). Empty list yields no line (no clutter).
- [x] #5 Hot-path code adds zero String allocations. All formatting uses snprintf_P into a local stack char[] sized for the worst case (~120 bytes).
- [x] #6 python build.py --firmware exits 0.
- [x] #7 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in commit 2a7b85e2 (foundation in 7c029e48):
- REST: GET /api/v2/otgw/boiler-support (matches kRouteOtgw convention; description originally said /api/v2/ot/boiler-support but the actual route lives under otgw/ to reuse the existing dispatch table). Streams JSON via httpServer.sendContent; 64-byte stack buffer per entry, no full-payload allocation.
- MQTT: otgw-firmware/boiler/unsupported_msgids retained CSV. Published on MQTT (re)connect (after sendMQTTversioninfo) and from doTaskMinuteChanged when the dirty flag is set. 256-byte stack buffer, (pos+6>size) guards truncate silently rather than overflow.
- WebUI: hidden span in Statistics-tab footer becomes visible once REST returns a non-empty list. refreshBoilerSupport() invoked on tab selection.

Build: python build.py --firmware exit 0.
Evaluator: python evaluate.py --quick 34/0/0 (100%).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Surfaced the boiler unsupported-msgID map (populated by TASK-684/685 inside processOT) through three user-visible channels so the diagnostic is accessible without reading telnet.

## Surfaces
- **REST**: GET /api/v2/otgw/boiler-support returns streamed JSON with two arrays of {id, label} objects (unsupported_read, unsupported_write). Streamed via httpServer.sendContent so the response buffer scales with the connection, not the worst-case array size.
- **MQTT**: retained topic otgw-firmware/boiler/unsupported_msgids carries a direction-suffixed CSV ("14W,16W,24R,..."). Published on every MQTT (re)connect plus once-per-minute republish gated by the dirty flag (zero publishes in steady state).
- **WebUI**: a hidden span at the bottom of the Statistics tab footer reveals "Boiler does not implement: 14 (MaxRelModLevelSetting, write), 27 (Toutside, read), ..." the first time the REST response is non-empty. Empty list keeps the span hidden so fresh installs do not show a misleading banner.

## Memory
Zero net new RAM beyond TASK-685 (96 B bitmaps) + the 1-byte dirty flag shipped in 7c029e48. Stack buffers only during request / publish.

## Note on the API path
AC #1 said /api/v2/ot/boiler-support; the implementation uses /api/v2/otgw/boiler-support to fit the existing kRouteOtgw dispatch table rather than introducing a parallel kRouteOt route just for one endpoint. The behaviour matches the AC; only the path token differs.

## Verification
- python build.py --firmware exits 0.
- python evaluate.py --quick: 34 / 0 / 0 (100%).

## Follow-up
- TASK-688 (next): on-disk JSON persistence of the support map + thermostat-side msgID tracking, read-on-boot + 15-min debounced atomic write.
<!-- SECTION:FINAL_SUMMARY:END -->
