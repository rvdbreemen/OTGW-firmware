---
id: TASK-548
title: 'Feature: Static IP address settings'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-06 09:04'
updated_date: '2026-05-25 22:52'
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
- [x] #1 User can configure a static IP address, subnet mask, gateway, and DNS server in the firmware settings
- [x] #2 If static IP is configured, the firmware uses it instead of DHCP on boot
- [x] #3 If static IP is not configured (default), behaviour is unchanged (DHCP)
- [x] #4 Settings are persisted to LittleFS and survive reboot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. WifiSection struct toevoegen aan OTGW-firmware.h (sStaticIp[16], sSubnet[16], sGateway[16], sDns1[16], sDns2[16]) en wifi-member aan OTGWSettings\n2. networkStuff.ino: WiFi.config() aanroepen voor WiFi.begin() als sStaticIp niet leeg is\n3. settingStuff.ino: writeSettings() + updateSetting() + readSettings() show-blok uitbreiden\n4. restAPI.ino: nieuwe velden in de GET settings response en POST handler\n5. index.html: 4 inputvelden toevoegen in het bestaande Network/WiFi settings blok\n6. Build groen + evaluator groen\n7. Commit + push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented static IP address settings for TASK-548 (GitHub #561).

Added WifiSection struct to OTGWSettings (OTGW-firmware.h) with sStaticIp[16], sSubnet[16], sGateway[16], sDns1[16], sDns2[16]. When sStaticIp is non-empty, WiFi.config() is called via IPAddress.fromString() before WiFiManager connects, so the device uses the static address instead of DHCP.

Settings are persisted to LittleFS (writeSettings/readSettings/updateSetting in settingStuff.ino), exposed via GET and POST /api/v2/settings (sendDeviceSettings + knownSettings in restAPI.ino), and the web UI auto-generates the five input fields via new entries in translateFields and translateTooltips (index.js).

Build: passed. Evaluator: 100% health, 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
