---
id: TASK-548
title: 'Feature: Static IP address settings'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-06 09:04'
updated_date: '2026-05-25 22:38'
labels:
  - feature
  - networking
  - wifi
dependencies: []
references:
  - 'GitHub #561'
  - reporter andrebrait
  - '2026-05-04'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Request to add a static IP address configuration option in the firmware. Motivation: if the DHCP server is unavailable or malfunctions, the OTGW cannot get an IP address and loses network connectivity. Since the OTGW may be part of critical home-automation infrastructure (heating control), having a static IP fallback ensures it remains accessible even when other network services are down. Reporter: andrebrait (GitHub #561).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 User can configure a static IP address, subnet mask, gateway, and DNS server in the firmware settings
- [ ] #2 If static IP is configured, the firmware uses it instead of DHCP on boot
- [ ] #3 If static IP is not configured (default), behaviour is unchanged (DHCP)
- [ ] #4 Settings are persisted to LittleFS and survive reboot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. WifiSection struct toevoegen aan OTGW-firmware.h (sStaticIp[16], sSubnet[16], sGateway[16], sDns1[16], sDns2[16]) en wifi-member aan OTGWSettings\n2. networkStuff.ino: WiFi.config() aanroepen voor WiFi.begin() als sStaticIp niet leeg is\n3. settingStuff.ino: writeSettings() + updateSetting() + readSettings() show-blok uitbreiden\n4. restAPI.ino: nieuwe velden in de GET settings response en POST handler\n5. index.html: 4 inputvelden toevoegen in het bestaande Network/WiFi settings blok\n6. Build groen + evaluator groen\n7. Commit + push
<!-- SECTION:PLAN:END -->
