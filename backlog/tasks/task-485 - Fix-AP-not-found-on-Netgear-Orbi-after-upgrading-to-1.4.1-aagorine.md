---
id: TASK-485
title: 'Fix: ''AP not found'' on Netgear Orbi after upgrading to 1.4.1 (aagorine)'
status: To Do
assignee: []
created_date: '2026-04-29 23:49'
labels:
  - bug
  - needs-info
  - wifi
  - mesh
dependencies: []
references:
  - 'Discord #english-support'
  - user aagorine
  - '2026-04-27'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter aagorine upgraded firmware to v1.4.1, initially connected to home WiFi (2.4GHz) successfully. After router reboot, lost connection and entered AP mode. At 192.168.4.1 the OTGW correctly shows the previously used network name. After re-entering the password, the device attempts to connect but fails with 'AP not found'. The network is visible and other devices connect without issues. Password is correct, no special characters.

Setup: Netgear Orbi RBR50 (AP mode) with RBS50 satellite. Tester unable to use OTGW currently.

Tried: re-entering SSID/password multiple times, power cycling OTGW, enabling 20/40 MHz coexistence on router, connecting from both main router node and satellite.

Maintainer advised trying 1.5.0-beta.2 (DHCP fix); tester response: 'Not yet, but it seems to be the only solution'. Possibly related to TASK-432 (DHCP / first-reboot WiFi association) but symptoms differ — TASK-432 is reproducible reconnect failure, this is hard 'AP not found' on a multi-node mesh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tester confirms whether 1.5.0-beta.2 or beta.4 resolves the issue
- [ ] #2 If still broken, capture serial log during AP scan + connect attempt to identify whether ESP8266 sees the SSID at all
- [ ] #3 Determine whether mesh-AP transparency (Orbi router-mode vs satellite) interacts with the ESP8266 WiFi driver
<!-- AC:END -->
