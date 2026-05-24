---
id: TASK-686
title: >-
  Surface boiler unsupported-msgID map (REST + MQTT + WebUI), reusing
  TASK-684/685 bitmaps — zero net new RAM
status: To Do
assignee: []
created_date: '2026-05-24 06:46'
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
- [ ] #1 GET /api/v2/ot/boiler-support returns 200 with a JSON body listing unsupported_read and unsupported_write arrays, each entry being { id, label }.
- [ ] #2 MQTT retained topic otgw-firmware/boiler/unsupported_msgids is published on every MQTT (re)connect with the current bitmap snapshot as a CSV string ('14W,16W,24R,26R,27R,33R,36R').
- [ ] #3 When a NEW unsupported msgID is observed at runtime (bitmap bit goes 0->1), a 'dirty' flag is set; the next sendStats / periodic publisher republishes the CSV and clears the flag. The dirty flag is the only added per-process RAM beyond the existing 64 bytes of bitmap.
- [ ] #4 The stats page (FSexplorer / dashboard) shows a human-readable line listing the unsupported msgIDs with names and direction suffix (R/W). Empty list yields no line (no clutter).
- [ ] #5 Hot-path code adds zero String allocations. All formatting uses snprintf_P into a local stack char[] sized for the worst case (~120 bytes).
- [ ] #6 python build.py --firmware exits 0.
- [ ] #7 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->
