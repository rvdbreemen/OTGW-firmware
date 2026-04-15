---
id: TASK-273
title: 'Config: switch to lwIP2 Low Memory variant (MSS=536)'
status: To Do
assignee: []
created_date: '2026-04-15 19:58'
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
- [ ] #1 Build configuration sets lwIP2 Low Memory variant (MSS=536, LWIP_FEATURES=1)
- [ ] #2 Firmware builds and boots correctly with the new lwIP variant
- [ ] #3 MQTT and WebSocket communication still function normally
- [ ] #4 logHeapStats shows equal or lower WS_drops and MQTT_drops compared to Higher Bandwidth baseline
<!-- AC:END -->
