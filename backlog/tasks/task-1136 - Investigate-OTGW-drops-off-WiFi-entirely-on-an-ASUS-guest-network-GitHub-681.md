---
id: TASK-1136
title: >-
  Investigate: OTGW drops off WiFi entirely on an ASUS guest network (GitHub
  #681)
status: To Do
assignee: []
created_date: '2026-09-17 20:22'
updated_date: '2026-09-19 15:56'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/681'
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
- [ ] #1 If they persist off the guest network: a capture is collected and a firmware-side root cause is identified or ruled out with evidence
- [ ] #2 During an outage, a capture shows whether the telnet stream (port 23) keeps flowing while HTTP returns no response, which separates a wedged web server from a dead network stack; the reporter runs the bash snippet posted on issue 681
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-19: Removed the original AC #1 (run a week on the normal SSID). Its premise was wrong and was corrected on the issue on 2026-09-18 19:38 UTC: DenSinH stated in his original report that the main 2.4/5 GHz network showed the same dropouts, and his router's wireless log shows the OTGW still Associated and Authorized while the outage is in progress. So this is not a WiFi association loss and moving SSID tests nothing. His last reply (2026-09-18 07:09) mentions 4 dropout-free days after switching Wireless mode to Legacy, with a router restart in between, so that number is not yet meaningful. Blocked on the reporter running the capture snippet; no reply since my correction.
<!-- SECTION:NOTES:END -->
