---
id: TASK-1136
title: >-
  Investigate: OTGW drops off WiFi entirely on an ASUS guest network (GitHub
  #681)
status: To Do
assignee: []
created_date: '2026-09-17 20:22'
labels:
  - bug
  - needs-info
dependencies: []
priority: medium
ordinal: 219000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by DenSinH on GitHub #681, 1.7.4+b77304b, WeMos/ESP8266, PIC 6.6. Data stops arriving in Home Assistant and the web dashboard is unreachable at the same time; the device recovers on its own after a while, or immediately when the router's wireless radio is restarted.

What the reporter's own debug dump argues against an MQTT-layer fault: over 20 days of uptime it shows MQTT Drops 0, WebSocket Drops 0, free heap 17960, fragmentation 10 percent, and only 5 reboots. If the gateway were losing the broker we would expect a non-zero drop count. Both HA and the web UI failing together points below the MQTT layer, at the WiFi association itself.

Leading non-firmware explanation: the device sits on a 2.4GHz GUEST network on an ASUS RT-AX57. Guest SSIDs commonly apply client isolation and an idle timeout that deauthenticates quiet stations.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reporter has run the gateway on the normal SSID (not the guest network) for at least one week and reported whether the dropouts persist
- [ ] #2 If they persist off the guest network: a capture is collected and a firmware-side root cause is identified or ruled out with evidence
<!-- AC:END -->
