---
id: TASK-904
title: >-
  fix(webui): friendly labels for unmapped Debug tab keys (wifi_current_*,
  otgwsimulation)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-23 05:22'
updated_date: '2026-06-23 05:24'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Debug Information tab renders device/info keys via translateToHuman(); 5 of 59 keys are absent from the translateFields dictionary so they show their raw key name: wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2 (runtime WiFi values, distinct from the mapped static-config wifisubnet/wifigateway/wifidns1/wifidns2) and otgwsimulation. Add friendly labels matching the existing style.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 5 unmapped device/info keys have translateFields entries
- [ ] #2 Labels match existing style and distinguish current/runtime from configured/static WiFi values
- [ ] #3 Debug tab on device shows the friendly labels after LittleFS flash
<!-- AC:END -->
