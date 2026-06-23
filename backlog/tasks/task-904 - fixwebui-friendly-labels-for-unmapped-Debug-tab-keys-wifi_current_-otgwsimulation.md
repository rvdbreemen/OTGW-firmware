---
id: TASK-904
title: >-
  fix(webui): friendly labels for unmapped Debug tab keys (wifi_current_*,
  otgwsimulation)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-23 05:22'
updated_date: '2026-06-23 05:40'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Debug Information tab renders device/info keys via translateToHuman(); 5 of 59 keys are absent from the translateFields dictionary so they show their raw key name: wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2 (runtime WiFi values, distinct from the mapped static-config wifisubnet/wifigateway/wifidns1/wifidns2) and otgwsimulation. Add friendly labels matching the existing style.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All 5 unmapped device/info keys have translateFields entries
- [x] #2 Labels match existing style and distinguish current/runtime from configured/static WiFi values
- [x] #3 Debug tab on device shows the friendly labels after LittleFS flash
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 5 friendly labels to translateFields in data/index.js for the device/info keys that fell through to their raw name on the Debug Information tab: wifi_current_subnet/_gateway/_dns1/_dns2 (runtime WiFi values, given a '(current)' qualifier to read against the mapped static-config counterparts) and otgwsimulation -> 'OTGW Simulation'. Committed 60a62a1e (beta.34), built fw+LittleFS, flashed bench, visually confirmed all 4 WiFi rows now render friendly labels on the Debug tab.
<!-- SECTION:FINAL_SUMMARY:END -->
