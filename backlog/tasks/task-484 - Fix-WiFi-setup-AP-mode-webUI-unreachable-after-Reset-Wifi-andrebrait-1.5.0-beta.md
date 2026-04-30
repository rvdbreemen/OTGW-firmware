---
id: TASK-484
title: >-
  Fix: WiFi setup AP mode webUI unreachable after Reset Wifi (andrebrait,
  1.5.0-beta)
status: To Do
assignee: []
created_date: '2026-04-29 23:48'
updated_date: '2026-04-30 00:37'
labels:
  - bug
  - needs-info
  - wifi
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - 2026-04-27 to 2026-04-29
priority: medium
ordinal: 12000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter andrebrait clicked Reset Wifi in the webUI by mistake. After that the device serves the OTGW-mac_address SSID and assigns IP 192.168.4.2 to clients, but http://192.168.4.1 is unreachable. curl -v shows TCP connect succeeds, GET request sent, then 'Recv failure: Software caused connection abort'. Linux laptop can ping 192.168.4.1 but cannot open the webUI. Hard reset (mainboard button), wifi module reset, and power cycle do not help. Tester resorted to reflashing.

Setup: Wemos D1 Mini classic, NodoShop OTGW v2.x, firmware 1.5.0-beta+ (after the DHCP fix in beta.2).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a clean device with WiFi reset triggered
- [ ] #2 Identify why the WifiManager AP-mode HTTP server accepts TCP connections but aborts the GET response
- [ ] #3 Either fix the AP-mode webserver or add diagnostics so future occurrences surface in serial output
- [ ] #4 Add a confirmation modal in the webUI before Reset Wifi (defensive UX, requested by reporter)
<!-- AC:END -->
