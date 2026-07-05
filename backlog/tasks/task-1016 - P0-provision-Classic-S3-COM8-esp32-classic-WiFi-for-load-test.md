---
id: TASK-1016
title: 'P0: provision Classic-S3 (COM8) esp32-classic + WiFi for load-test'
status: To Do
assignee: []
created_date: '2026-07-05 22:19'
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
- [ ] #1 esp32-classic flashed on COM8 (MAC ac:27:6e:ce:45:d8)
- [ ] #2 Device WiFi-provisioned, IP known, telnet + WS reachable
<!-- AC:END -->
