---
id: TASK-485
title: 'Fix: ''AP not found'' on Netgear Orbi after upgrading to 1.4.1 (aagorine)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-29 23:49'
updated_date: '2026-05-26 09:56'
labels:
  - bug
  - needs-info
  - wifi
  - mesh
dependencies: []
references:
  - 'Discord #english-support'
  - user aagorine
  - '2026-04-27'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter aagorine upgraded firmware to v1.4.1, initially connected to home WiFi (2.4GHz) successfully. After router reboot, lost connection and entered AP mode. At 192.168.4.1 the OTGW correctly shows the previously used network name. After re-entering the password, the device attempts to connect but fails with 'AP not found'. The network is visible and other devices connect without issues. Password is correct, no special characters.

Setup: Netgear Orbi RBR50 (AP mode) with RBS50 satellite. Tester unable to use OTGW currently.

Tried: re-entering SSID/password multiple times, power cycling OTGW, enabling 20/40 MHz coexistence on router, connecting from both main router node and satellite.

Maintainer advised trying 1.5.0-beta.2 (DHCP fix); tester response: 'Not yet, but it seems to be the only solution'. Possibly related to TASK-432 (DHCP / first-reboot WiFi association) but symptoms differ — TASK-432 is reproducible reconnect failure, this is hard 'AP not found' on a multi-node mesh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 If still broken, capture serial log during AP scan + connect attempt to identify whether ESP8266 sees the SSID at all
- [ ] #2 Determine whether mesh-AP transparency (Orbi router-mode vs satellite) interacts with the ESP8266 WiFi driver
- [ ] #3 Tester retests on the current 1.5.0-beta line (beta.15 as of 2026-05-05) and reports whether 'AP not found' still occurs on the Orbi mesh
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-26: Current published beta is 1.6.0-beta.21 (version.h confirmed). Previous note recommended retest on beta.15 (2026-05-05). Since then the project has moved through betas 16-21, with no new reports from aagorine. AC#3 still needs tester retest — aagorine should now retest on 1.6.0-beta.21. The WiFiManager downgrade to 2.0.15-rc.1 (commit 38e37f6e, 2026-05-04) is now included in this beta and is the most likely fix candidate for AP-related WiFi issues, as the 2.0.17 version had known portal TCP abort regressions.
<!-- SECTION:NOTES:END -->
