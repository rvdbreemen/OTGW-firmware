---
id: TASK-1050
title: >-
  Ignore DHCP-provided NTP servers (sntp_servermode_dhcp(0)) to stop per-renewal
  SNTP leak
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-28 17:39'
updated_date: '2026-07-28 17:40'
labels:
  - bug
  - network
dependencies: []
priority: high
ordinal: 169000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence (beta.4 reporter with Pi-hole DHCP + static-IP fix; pi-hole discourse 81736; TASK-1037 uptime-locked onset at T1 renewal) converges on lwIP DHCP option 42 handling: our prebuilt lwIP2 has LWIP_DHCP_GET_NTP_SRV=1 (lwipopts.h:958), so every DHCP ACK/renewal pushes the DHCP-supplied NTP server into the SDK SNTP module, leaking heap on routers that send option 42 (Pi-hole, some D-Link). Firmware manages its own NTP (pool.ntp.org), so DHCP-supplied NTP is unwanted. Fix: call sntp_servermode_dhcp(0) at boot before WiFi/DHCP so option 42 is ignored.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sntp_servermode_dhcp(0) is called at boot before DHCP can deliver option 42
- [ ] #2 Firmware NTP sync via pool.ntp.org still works (settings-driven loopNTP unaffected)
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
- [ ] #5 Field validation: reporter on DHCP (Pi-hole network) survives well past the ~90min renewal boundary on a build with this change
<!-- AC:END -->
