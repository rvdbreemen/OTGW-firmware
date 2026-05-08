---
id: TASK-581
title: 'feat(network): dynamic Ethernet-to-WiFi failover at runtime'
status: To Do
assignee: []
created_date: '2026-05-08 15:59'
labels:
  - network
  - hardware
  - otgw32
  - reliability
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 polls Ethernet link status every 5 seconds and switches transparently between Ethernet and WiFi. The 2.0.0 firmware detects the Ethernet link at boot only (state.net.eMode) and makes no runtime adjustments. On the Nodoshop NODO hardware, users who unplug or replug the Ethernet cable lose connectivity until the next reboot. Implement a runtime poller that detects link-state changes and switches the active network interface accordingly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A timer (5s interval) checks Ethernet link status on HAS_ETH_CAPABLE builds
- [ ] #2 When Ethernet link drops and WiFi is configured, firmware transparently switches to WiFi (no reboot required)
- [ ] #3 When Ethernet link comes back up, firmware switches back to Ethernet (preferred interface)
- [ ] #4 state.net.eMode updates correctly on each transition (NET_ETHERNET / NET_WIFI)
- [ ] #5 MQTT and WebSocket connections are re-established after interface switch
- [ ] #6 Transitions are logged via DebugTln and published to MQTT (otgw-firmware/network/mode)
- [ ] #7 No regression on ESP8266 PIC build (code gated behind HAS_ETH_CAPABLE)
<!-- AC:END -->
