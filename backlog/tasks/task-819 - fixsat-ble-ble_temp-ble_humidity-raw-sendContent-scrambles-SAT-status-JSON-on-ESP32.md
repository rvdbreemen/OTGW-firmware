---
id: TASK-819
title: >-
  fix(sat-ble): ble_temp/ble_humidity raw sendContent scrambles SAT status JSON
  on ESP32
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-03 16:08'
updated_date: '2026-06-03 16:15'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report @sergeantd (OTGW32, alpha.150): SAT dashboard tiles all show -- (NaN). Browser console: sat.js:156 [SAT] fetch error: SyntaxError: Unterminated fractional number in JSON at position 4096. Distinct from TASK-764 (that was a client-side sat.js init-order fix, alpha.97; it wrongly ruled out the malformed-JSON class by overlooking satBLESendStatusJSON). Root cause: satBLESendStatusJSON() (SATble.ino:696,700) emits ble_temp/ble_humidity via raw httpServer.sendContent(json) instead of restSendContent(json). On ESP32 (HAS_REST_TX_COALESCING=1) REST output is batched into the 4096-byte coalescing buffer sTxBuf (jsonStuff.ino). A raw sendContent flushes its chunk immediately, jumping ahead of the ~4KB of SAT fields still buffered in sTxBuf, so JSON bytes hit the wire OUT OF ORDER -> scrambled JSON -> JSON.parse fails near the 4095-byte flush boundary (~pos 4096). Only on ESP32 (ESP8266 flushes inline), only on the >4KB sat/status response, only when state.sat.bBleTempValid (the raw branch runs). Server logs 200/chunks=3 with valid data; only wire order is wrong. Sibling satBLERosterSendJSON was already fixed for this exact gotcha (comment SATble.ino:750-753). 2.0.0-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ble_temp/ble_humidity routed through restSendContent so sat/status JSON is well-formed when BLE temp is valid
- [x] #2 No other raw httpServer.sendContent site interleaves with the sTxBuf coalescing path (firmware-wide grep triaged)
- [x] #3 Build green ESP32 + ESP8266 (per-env SUCCESS verified in build log); evaluate --quick no new failures
- [ ] #4 Field-confirmed by @sergeantd on OTGW32 (browser console clean, tiles show live values)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause confirmed by code + the project's own documented gotcha (SATble.ino:750-753). Fix: SATble.ino:696,700 raw httpServer.sendContent -> restSendContent so ble_temp/ble_humidity share the sTxBuf coalescing buffer. Firmware-wide grep triaged: only interleaving sites; all other raw sendContent are pure-raw response paths (sTxBuf empty). Build green esp8266 fw+fs + esp32 fw+fs; evaluate --quick 0 fail (2 pre-existing warnings). Bumped alpha.151->alpha.152. AC#4 (field-confirm by @sergeantd on OTGW32) is the sole remaining gate; needs his retest. Distinct from TASK-764 (client-side init-order, alpha.97). Logged .wolf/buglog.json bug-112. Follow-up TASK-820 for the separate restFlushTxBuf String-temporary fragility.
<!-- SECTION:NOTES:END -->
