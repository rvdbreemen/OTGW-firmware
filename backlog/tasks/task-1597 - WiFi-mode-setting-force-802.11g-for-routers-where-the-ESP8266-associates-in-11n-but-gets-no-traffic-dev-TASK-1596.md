---
id: TASK-1597
title: >-
  WiFi mode setting: force 802.11g for routers where the ESP8266 associates in
  11n but gets no traffic (dev TASK-1596)
status: To Do
assignee: []
created_date: '2026-11-03T22:19:00Z'
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
- [ ] #1 settings.wifi.bForce11g (JSON WifiForce11g, REST wififorce11g, default false) read/write/REST/whitelist/debug dump; web UI label + help
- [ ] #2 PHY mode set explicitly (11G when forced, 11N otherwise) right after WiFi.mode(WIFI_STA) in startWiFi(), read back via getPhyMode() and logged
- [ ] #3 Config portal field 'WiFi mode' (auto/11g, always submitted) applied and saved before WiFiManager connects
- [ ] #4 Bench .68: default logs phy 11n and connects; forced logs phy 11g and stays reachable; back to auto logs 11n
- [ ] #5 Full tests/ + evaluate.py green, build green, released as a 1.7.9 beta prerelease
<!-- AC:END -->
