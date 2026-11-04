---
id: TASK-1597
title: >-
  WiFi mode setting: force 802.11g for routers where the ESP8266 associates in
  11n but gets no traffic (dev TASK-1596)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T22:19:00Z'
updated_date: '2026-11-04T04:56:00Z'
labels:
  - wifi
  - esp8266
  - field-report
dependencies: []
priority: medium
ordinal: 262000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port/1.x implementation of dev TASK-1596. Field report robou1337 (#english-support 2026-11-03): Bouygues Bbox Wi-Fi 7; in 11n the ESP8266 associates but gets no DHCP/ARP, config AP returns; same on 1.7.7, 1.7.8 and otgwmcu; his own debug build with WIFI_PHY_MODE_11G gets an IP at once. Robert's choice: opt-in setting (default auto = today's 11n) plus a field in the WiFiManager config portal. Core 2.7.4 ESP8266WiFiGeneric.cpp:359 setPhyMode(); Tasmota precedent Wifi 3 (802.11b/g).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 settings.wifi.bForce11g (JSON WifiForce11g, REST wififorce11g, default false) read/write/REST/whitelist/debug dump; web UI label + help
- [x] #2 PHY mode set explicitly (11G when forced, 11N otherwise) right after WiFi.mode(WIFI_STA) in startWiFi(), read back via getPhyMode() and logged
- [x] #3 Config portal field 'WiFi mode' (auto/11g, always submitted) applied and saved before WiFiManager connects
- [ ] #4 Bench .68: default logs phy 11n and connects; forced logs phy 11g and stays reachable; back to auto logs 11n
- [x] #5 Full tests/ + evaluate.py green, build green, released as a 1.7.9 beta prerelease
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04: shipped in v1.7.9-beta.2 (622bbbe6c, release https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.7.9-beta.2). Evidence: test/run_tests.bat green, evaluate.py --quick 38 passed 0 failed, build.bat firmware+fs exit 0 (777128 B sketch, 74%). AC4 (bench .68) NOT done: .68 unreachable (ping ok, HTTP/telnet dead) and off USB (no COM3); Robert chose to release without the bench test. Field confirmation asked from robou1337 (#english-support msg 1567401891200245933). v1.7.9-beta.1 was tagged by mistake from eeeae1e21 (the code commit had been refused by the commit-msg hook over a dev TASK id); release notes of beta.1 now say so, tag skipped. Research: Tasmota (Wifi 2/3/4, support_command.ino:2993-3029) and ESPHome (phy_mode, PR #16055) offer a manual PHY option only, no automatic 11n->11g fallback.
<!-- SECTION:NOTES:END -->
