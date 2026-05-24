---
id: TASK-692
title: >-
  feat-2.0.0: port TASK-686 — surface boiler unsupported-msgID map via REST,
  MQTT, WebUI
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 08:18'
updated_date: '2026-05-24 08:32'
labels:
  - port-from-dev
  - diagnostics
  - opentherm
  - observability
dependencies: []
priority: medium
ordinal: 60000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-686 / commits 7c029e48 + 2a7b85e2. Promotes the unsupported-msgID bitmaps from processOT-local to file-scope statics (boilerLastMasterWasWrite, boilerUnsupportedRead, boilerUnsupportedWrite) plus a dirty flag. Adds three consumer surfaces: REST endpoint GET /api/v2/otgw/boiler-support (streamed JSON), MQTT retained CSV otgw-firmware/boiler/unsupported_msgids published on (re)connect + minute-tick when dirty, WebUI hidden footer line on Statistics tab. Depends on TASK-691 (TASK-685 port) being merged first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Three file-scope uint8_t[32] bitmaps in OTGW-Core.ino — boilerLastMasterWasWrite, boilerUnsupportedRead, boilerUnsupportedWrite — replace the function-local statics introduced in TASK-685 port. One file-scope boilerUnsupportedDirty bool set on every 0->1 transition.
- [x] #2 OTGW-Core.ino exposes four accessors: isBoilerMsgIdUnsupportedRead/Write(uint8_t id), getBoilerUnsupportedDirty(), clearBoilerUnsupportedDirty(). Declarations in OTGW-firmware.h.
- [x] #3 GET /api/v2/otgw/boiler-support returns 200 with JSON {"unsupported_read":[{"id":N,"label":"..."},...],"unsupported_write":[...]}. Streamed via httpServer.sendContent with a 64-byte stack buffer per entry; no full-payload allocation.
- [x] #4 MQTT retained topic otgw-firmware/boiler/unsupported_msgids carries a direction-suffixed CSV ('14W,16W,24R,...'). publishBoilerUnsupportedMsgids() is published immediately after sendMQTTversioninfo() on every MQTT (re)connect and once per minute from doTaskMinuteChanged() when getBoilerUnsupportedDirty() is true (cleared after publish). Single 256-byte stack buffer with (pos+6>size) truncation guards.
- [x] #5 data/index.html: hidden span 'Boiler does not implement: ...' added in the Statistics tab footer; revealed by refreshBoilerSupport() in index.js when the REST response has non-empty arrays. refreshBoilerSupport() is invoked from openLogTab when the Statistics tab is activated.
- [x] #6 Hot-path code adds zero String allocations. All formatting uses snprintf_P into local stack char[]. Net new firmware RAM = 1 byte (dirty flag); the 96 B of bitmaps were already accounted for by TASK-691 port.
- [x] #7 python build.py --firmware exits 0.
- [x] #8 python evaluate.py --quick shows no new failures vs current 2.0.0 baseline.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-Core.ino: promote the three function-local bitmaps (from TASK-691 port) to file scope and rename to boilerLastMasterWasWrite / boilerUnsupportedRead / boilerUnsupportedWrite. Add boilerUnsupportedDirty bool. Add accessors isBoilerMsgIdUnsupportedRead/Write, getBoilerUnsupportedDirty, clearBoilerUnsupportedDirty.
2. Inside processOT, use the renamed file-scope bitmaps and set boilerUnsupportedDirty on 0->1 transition.
3. OTGW-firmware.h: declare the accessors + publishBoilerUnsupportedMsgids.
4. MQTTstuff.ino: add publishBoilerUnsupportedMsgids() (256-byte stack buffer, CSV with R/W suffix). Call it after sendMQTTversioninfo in handleMQTT post-connect and from doTaskMinuteChanged when dirty.
5. restAPI.ino handleOtgw: add 'boiler-support' branch streaming JSON of unsupported_read / unsupported_write arrays.
6. data/index.html: add hidden span in Statistics-tab footer.
7. data/index.js: add refreshBoilerSupport() and invoke from openLogTab when Statistics selected.
8. Bump prerelease (alpha.59 -> alpha.60).
9. Build + eval.
10. Commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-686 (commits 7c029e48 + 2a7b85e2) to 2.0.0: surfaces the boiler unsupported-msgID map via REST, MQTT, and WebUI. The function-local bitmaps from TASK-691 port were promoted to file scope and renamed, plus a dirty flag, plus four accessors. The three consumer surfaces read from the same source of truth.

## Changes
- OTGW-Core.ino: removed the function-local lastMasterWasWrite / unknownLoggedRead / unknownLoggedWrite statics (TASK-691). Added file-scope statics boilerLastMasterWasWrite, boilerUnsupportedRead, boilerUnsupportedWrite plus boilerUnsupportedDirty bool. processOT's classification block uses the renamed bitmaps and sets boilerUnsupportedDirty on every 0->1 transition. Accessors isBoilerMsgIdUnsupportedRead/Write, getBoilerUnsupportedDirty, clearBoilerUnsupportedDirty added at file scope.
- OTGW-firmware.h: forward declarations for the four accessors plus publishBoilerUnsupportedMsgids().
- MQTTstuff.ino: publishBoilerUnsupportedMsgids() function added next to publishStatU32. Single 256-byte stack buffer, (pos+6>size) truncation guards. Direction-suffixed CSV. Retained. Called from handleMQTT post-(re)connect (after publishAllPICsettings, then clearBoilerUnsupportedDirty), and from doTaskMinuteChanged when getBoilerUnsupportedDirty() returns true.
- restAPI.ino: new branch in handleOtgw for 'boiler-support'. Streams JSON unsupported_read / unsupported_write arrays via httpServer.sendContent with a 64-byte stack buffer per entry.
- data/index.html: hidden span 'Boiler does not implement: ...' added in Statistics-tab footer.
- data/index.js: refreshBoilerSupport() added after sortStats(); invoked from openLogTab when Statistics tab is activated.

## Notes vs dev
- 2.0.0 uses platformFreeHeap()/platformMaxFreeBlock() in the MQTT post-subscribe diagnostic, not ESP.getFreeHeap()/getMaxFreeBlockSize(); landing position unchanged.
- 2.0.0's handleOtgw uses 'label' (sendOTLabel) not dev's 'label' (sendOTGWlabel); new branch slots in alongside cleanly.
- platformMaxFreeBlock() on ESP32 is getMaxAllocHeap() not getMaxFreeBlockSize() (per .wolf/cerebrum.md), but the publishBoilerUnsupportedMsgids() function does not touch heap diagnostics so this is not relevant here.

## Memory
1 byte net new RAM (dirty flag). 96 B bitmap moved from processOT stack to file scope (no functional change in RAM accounting). Stack buffers only during request / publish.

## Verification
- python build.py --firmware --target esp8266 exits 0 (2.0.0-alpha.60).
- python evaluate.py --quick: 60/1/0 (98.5%, unchanged from baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
