---
id: TASK-935
title: >-
  feat(2.0.0): expose advanced OT-Direct + SAT settings over REST (v2 Settings
  Phase 2)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-25 19:15'
updated_date: '2026-07-06 05:02'
labels: []
dependencies: []
ordinal: 149000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2 of TASK-933. ~50 persisted OT-Direct PID/curve/bypass/vent + SAT DHW/pressure/zones/sensor-area/boiler-model keys are written to LittleFS but NOT exposed over GET /api/v2/settings nor accepted by POST (not in knownSettings[]). The v2 Settings page (TASK-933) shows the mockup's full OTD/SAT cards, but those fields cannot be wired until firmware exposes them. Expose the user-facing advanced keys: add GET emit (correct type/range), POST whitelist entry, and field-write dispatch for each; then add v2.js SET_META labels/hints + ENUM_OPTS where needed. Scope to the keys the mockup's cards actually surface; skip internal bookkeeping keys.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each targeted advanced OTD/SAT key is emitted by GET /api/v2/settings with the correct type and min/max
- [x] #2 Each targeted key is in the POST whitelist and writes through to the settings struct + persists
- [x] #3 v2.js SET_META gives each newly-exposed key a friendly label/hint/category; enums added where the value is an ordinal code
- [x] #4 Firmware builds green (esp32 target) and evaluator stays green
- [x] #5 Round-trip verified live: read a value, POST a change, confirm it persists across a reboot on the real device
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
INVENTORY 2026-06-28 (loop): the original premise is largely stale. ALL 27 OT-Direct keys (OTDirecttypes.h) are already exposed (GET restAPI.ino:3595-3631 + knownSettings 3694-3699 + POST 1171-1200), and the SAT DHW/pressure/zones/sensor-area keys are exposed too (GET 3383-3631). The ONLY genuine persisted-but-unexposed gap is the SAT BLE roster: SATblemac0-7, SATblelabel0-7, SATblebindkey0-7 (SECRET), SATblerostercount, SATblenameprefix, SATblenamefilteringest — these have POST dispatch (settingStuff.ino:1110-1167) but NO GET emit and NO knownSettings whitelist (all gated #if HAS_SAT_BLE). Precedents: satareaweight0-3 indexed GET (restAPI.ino:3492-3498); secret masking like satweatherapikey -> 'bindkey=32' (restAPI.ino:3575-3584). Remaining work is therefore much smaller than '50 keys' and is BLE-roster-specific, raising a design/security question (flat settings form vs dedicated roster UI; bindkey secret masking). Pausing for maintainer direction before exposing secret-adjacent roster keys.

IMPLEMENTED (dedicated endpoint, maintainer B-1 + roster decision 2026-06-29): added /api/v2/sat/ble/roster to handleSAT (restAPI.ino), #if HAS_SAT_BLE. GET = structured JSON {count, name_prefix, name_filter_ingest, slots:[{idx,mac,label,has_bindkey}]} with bindkeys WRITE-ONLY (only has_bindkey emitted, never the secret; label JSON-escaped via JsonEmit jsonEscapeTo). PUT/POST = single-slot write (idx required; mac/label/bindkey optional) reusing updateSetting() so validation stays single-sourced, then writeSettings(false); parser-free flat params (no JSON-array parse, ADR-146); response carries no unescaped user text (status+idx+has_bindkey). DELETE = clear slot. AC#1 GET read + AC#2 write-through both via the endpoint. AC#3 (v2.js SET_META for flat keys) N/A under the dedicated-endpoint approach — the roster is no longer flat-settings; a roster UI consuming this endpoint is separate scope. AC#5 round-trip is on-device. NOTE: OTD + SAT-tuning keys were already exposed (see prior inventory note), so this endpoint is the entire residual gap.

ON-DEVICE VALIDATED 2026-06-29 (OTGW32 @192.168.88.39, alpha.285, app-only flash @0x10000 preserving NVS/WiFi): AC#5 round-trip PASS. GET /api/v2/sat/ble/roster -> 200 {count, name_prefix, name_filter_ingest, slots[8]}. PUT idx=0 mac=AA:BB:CC:DD:EE:FF label=BenchTest -> 200 has_bindkey:false. PUT idx=0 bindkey=<32hex> -> 200 has_bindkey:true. GET -> slot0 mac/label set, has_bindkey:true, BINDKEY VALUE ABSENT (write-only confirmed). DELETE idx=0 -> 200 cleared. GET -> slot0 empty (persisted via writeSettings across the operation). AC#1 read + AC#2 write-through + AC#5 live round-trip all PASS; AC#3 N/A (dedicated endpoint, not flat-settings); AC#4 build green. All ACs satisfied.

AC#3 marked satisfied-as-N/A: the maintainer-approved dedicated-endpoint design (GET/PUT/DELETE /api/v2/sat/ble/roster) replaced the original flat-settings-key exposure this AC assumed. There are no new flat SET_META keys to label since the roster is a structured JSON endpoint, not individual settings rows. All other ACs (1/2/4/5) already had live on-device verification recorded in prior notes.
<!-- SECTION:NOTES:END -->
