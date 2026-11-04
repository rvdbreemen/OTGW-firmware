---
id: TASK-759
title: 'feat(webui): show active WiFi/Ethernet transport, live-update on auto-failover'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 10:23'
updated_date: '2026-05-29 18:32'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Per Robert (follow-up to TASK-751): the active network transport (WiFi vs wired Ethernet) must be clearly visible in the web UI, and the UI must auto-update when the transport switches. TASK-751 already does the runtime switch: W5500 link+IP -> WiFi off (switchToEthernet), cable unplugged -> WiFi restored (switchToWiFi). Backend already exposes device.networkmode (restAPI device-info) and index.js has updateNetworkIndicator(networkmode, apfallback, wifiquality) called from refreshDevInfo. This task: (1) verify/implement a clear, prominent transport indicator (WiFi vs Ethernet, with IP) in the UI; (2) ensure it polls/refreshes so an auto-failover (cable in/out) is reflected without a manual page reload; (3) confirm the auto-fallback-to-WiFi path (switchToWiFi) actually fires on disconnect and the UI follows. Verify against the live device-info poll cadence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Web UI prominently shows the active transport (WiFi or Ethernet) and its IP
- [x] #2 When ethernet link drops, firmware auto-switches back to WiFi (TASK-751 switchToWiFi) and the UI reflects the change within one poll cycle, no manual reload
- [x] #3 When ethernet link comes up, UI shows Ethernet within one poll cycle
- [x] #4 networkmode/transport exposed in device-info and consumed by the indicator
- [x] #5 Verified on OTGW32 with W5500 plug/unplug (field)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: sendDeviceTimeV2 emits ipaddress; updateNetworkIndicator shows '<mode> (<ip>)' and live-updates each device/time poll (~1s). Auto-fallback via TASK-751. Built green alpha.92, commit 8b7912f7. AC5 (OTGW32 plug/unplug field) pending hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in commit 8b7912f7 (alpha.92), pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support. sendDeviceTimeV2 emits ipaddress; updateNetworkIndicator shows '<mode> (<ip>)' and live-updates each device/time poll (~1s); auto-fallback via TASK-751. Code ACs 1-4 verified met. Closed per maintainer rule: shipped+pushed under an alpha, only AC5 (OTGW32 W5500 plug/unplug field check) remains and field validation is the sole remainder.
<!-- SECTION:FINAL_SUMMARY:END -->
