---
id: TASK-1016
title: 'P0: provision Classic-S3 (COM8) esp32-classic + WiFi for load-test'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:19'
updated_date: '2026-07-05 22:42'
labels: []
dependencies: []
ordinal: 227000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Prereq for TASK-1015. Build esp32-classic via build.bat, flash COM8 (--update), maintainer does SoftAP WiFi provisioning. Confirm fixed IP + HTTP + telnet(23) + WS reachable. No test runs before this is green.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 esp32-classic flashed on COM8 (MAC ac:27:6e:ce:45:d8)
- [x] #2 Device WiFi-provisioned, IP known, telnet + WS reachable
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Flashed factory via flash_otgw.bat --port COM8. Provisioned via SoftAP (SSID KeepOut2). First boot: telnet/OT/MQTT alive but HTTP port 80 refused (000) — reproduces TASK-961 live. Re-flashed --update (forces clean reboot, preserves WiFi creds): HTTP 200, /api/v2/device/info 200, WS upgrade 101. Device: 192.168.88.64, MAC ac:27:6e:ce:45:d8.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
esp32-classic built + flashed to COM8 (factory), WiFi-provisioned by maintainer (SSID KeepOut2), IP 192.168.88.64. HTTP, telnet(23), WS(/ws) all confirmed reachable after one reboot cycle. Bonus: first-boot HTTP-refused-then-fixed-by-reboot behavior live-reproduced TASK-961, adding independent confirmation of that bug's repro steps.
<!-- SECTION:FINAL_SUMMARY:END -->
