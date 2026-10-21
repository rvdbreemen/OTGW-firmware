---
id: TASK-1278
title: >-
  Log the WiFi disconnect reason, BSSID and channel when the link drops (GH
  #682)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-21 05:30'
updated_date: '2026-10-21 17:54'
labels:
  - enhancement
  - wifi
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: medium
ordinal: 250000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Requested by mrfox7688 on GH #682 (2026-10-20): daytime WiFi losses of ~2 min with a 9-minute rhythm on an ASUS AiMesh node; NAS and gateway lose the link together. The firmware logs no disconnect reason today (no onStationModeDisconnected handler in 1.x), so deauth, roaming and silent loss cannot be told apart. Same gap blocks TASK-1262 (auth/deauth loop, reason 3 hypothesis).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On every station disconnect the telnet debug log shows the 802.11 reason code (number and short name), the BSSID and the channel of the AP that was left
- [ ] #2 On every connect the log shows the BSSID and channel joined
- [ ] #3 Shown on the bench: a MikroTik-forced disconnect and a reconnect produce the expected lines; no heap regression; esp8266 build and evaluate.py --quick green
<!-- AC:END -->
