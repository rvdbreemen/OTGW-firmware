---
id: TASK-537
title: Port TASK-536 debug dump to ESP32/2.0.0 branch
status: To Do
assignee: []
created_date: '2026-05-04 06:51'
labels:
  - feature
  - debug
  - esp32
  - port
dependencies: []
references:
  - backlog/tasks/task-536 - Add-dump-debug-info-command-to-debug-menu.md
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the debug dump feature from TASK-536 (v1.5.x, ESP8266) to the feature-dev-2.0.0-otgw32-esp32-sat-support branch.\n\nTASK-536 added:\n- Telnet key 'D': streams all settings + state fields via Debugf(PSTR()), 14 sections, passwords masked\n- REST endpoint GET /api/v2/debug: same data as chunked JSON via sendStartJsonMap/sendJsonMapEntry/sendEndJsonMap\n\nPort considerations for ESP32/2.0.0:\n- The OTGWState struct on 2.0.0 includes additional SAT (Standalone Analog Thermostat) state fields (state.sat.*) — add those sections to dumpDebugInfo()\n- ESP32 heap API differs: use ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap() (no getHeapFragmentation on ESP32 — skip or use multi-heap stats)\n- Debug macros on 2.0.0 may use SATDebug* / OTDDebug* variants — verify which macro set covers the telnet stream on that branch\n- PROGMEM is a no-op on ESP32 (all flash-addressed), but keeping the macros is harmless and keeps code portable\n- The sendJsonMapEntry key-length constraint (nameBuf[35], max 34 chars) applies equally — same restriction\n- If the 2.0.0 branch has different settings struct layout (settings.sat.*), enumerate those fields too\n\nFiles to modify on 2.0.0 branch:\n- src/OTGW-firmware/handleDebug.ino (or equivalent SAT/OTD debug handler)\n- src/OTGW-firmware/restAPI.ino (add kRouteDebugDump, handleDebugDump — identical logic, adjust state fields)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Telnet 'D' command works on ESP32 build and streams all settings + state including SAT fields
- [ ] #2 GET /api/v2/debug returns valid JSON on ESP32 build
- [ ] #3 ESP32 heap fields use correct ESP32 API (getMinFreeHeap/getMaxAllocHeap instead of getHeapFragmentation)
- [ ] #4 Passwords masked as *** in both telnet and REST output
- [ ] #5 Build passes on 2.0.0 branch (python build.py --firmware)
<!-- AC:END -->
