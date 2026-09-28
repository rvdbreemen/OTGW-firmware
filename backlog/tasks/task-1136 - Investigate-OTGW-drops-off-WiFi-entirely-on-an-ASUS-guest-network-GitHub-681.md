---
id: TASK-1136
title: >-
  Investigate: OTGW drops off WiFi entirely on an ASUS guest network (GitHub
  #681)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-17 20:22'
updated_date: '2026-09-28 20:08'
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
- [x] #1 Resolution recorded: the reporter's outages stopped after a router-side change (ASUS 2.4 GHz Wireless mode Auto -> Legacy), no firmware defect found
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-19: Removed the original AC #1 (run a week on the normal SSID). Its premise was wrong and was corrected on the issue on 2026-09-18 19:38 UTC: DenSinH stated in his original report that the main 2.4/5 GHz network showed the same dropouts, and his router's wireless log shows the OTGW still Associated and Authorized while the outage is in progress. So this is not a WiFi association loss and moving SSID tests nothing. His last reply (2026-09-18 07:09) mentions 4 dropout-free days after switching Wireless mode to Legacy, with a router restart in between, so that number is not yet meaningful. Blocked on the reporter running the capture snippet; no reply since my correction.

2026-09-28: GH #681 was closed on 2026-09-26. DenSinH reported the gateway stable since switching the ASUS router's 2.4 GHz "Wireless mode" from Auto to Legacy (reported 2026-09-18, confirmed stable 2026-09-26); he will revert settings one by one after a holiday and may reopen. The capture ACs were removed: their condition (outages persisting) did not occur. A second user, SiFU-WU (Zyxel DX5401-B1, 1.7.5), reported association drops and is moving to Ethernet; answered with the router-mode tip (issuecomment-5877585948).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
No firmware defect. GH #681 outages (web UI and HA dropping together while the router still listed the gateway as associated) stopped after the reporter set the ASUS RT-AX57 2.4 GHz Wireless mode from Auto to Legacy; stable for over a week, issue closed 2026-09-26. The ESP8266 supports 802.11b/g/n only, so a router negotiating newer features on 2.4 GHz is the likely cause (inference, not measured). Capture ACs removed because the outages stopped. A second report (Zyxel router) got the same tip.
<!-- SECTION:FINAL_SUMMARY:END -->
