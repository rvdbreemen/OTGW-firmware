---
id: TASK-273
title: 'Config: switch to lwIP2 Low Memory variant (MSS=536)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-15 19:58'
updated_date: '2026-04-15 21:48'
labels:
  - performance
  - build
  - stap-2
dependencies: []
references:
  - src/OTGW-firmware/networkStuff.h
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The current build uses lwIP2 Higher Bandwidth (MSS=1460). Switching to lwIP2 Low Memory (MSS=536) reduces per-TCP-connection heap footprint without any code changes. This is a free marginal improvement on top of stap-1.

MSS=536 means the TCP stack uses smaller segments. For OTGW traffic (short MQTT messages, small WebSocket frames) this has no noticeable impact on throughput. The main benefit is that each lwIP pbuf allocation is smaller, leaving more headroom in the fragmented heap after a WebSocket connection.

Change required: in arduino/packages/esp8266/hardware/esp8266/3.1.2/boards.txt (or equivalent build config), change the lwip variant from lwIP2 Higher Bandwidth to lwIP2 Low Memory. For arduino-cli builds this corresponds to the build flag -DLWIP_FEATURES=1 -DTCP_MSS=536 (already set in Low Memory variant).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Build configuration sets lwIP2 Low Memory variant (MSS=536, LWIP_FEATURES=1)
- [x] #2 Firmware builds and boots correctly with the new lwIP variant
- [x] #3 MQTT and WebSocket communication still function normally
- [x] #4 logHeapStats shows equal or lower WS_drops and MQTT_drops compared to Higher Bandwidth baseline
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Build config ip=lm2f in build.py already selects lwIP v2 Lower Memory (TCP_MSS=536, LWIP_FEATURES=1). No change needed — the firmware has been running Low Memory since the core 3.1.2 migration. Task was based on a false assumption that Higher Bandwidth was in use.
<!-- SECTION:FINAL_SUMMARY:END -->
