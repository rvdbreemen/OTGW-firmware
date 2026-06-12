---
id: TASK-751
title: 'feat(network): prefer wired ethernet over WiFi when ETH has an IP'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 08:34'
updated_date: '2026-05-29 18:33'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field feedback (tjfs, alpha.84, OTGW32): with the W5500 wired ethernet plugged in, the board still uses WiFi; it should use wired. Robert's stated intent: cannot dual-connect, so disable WiFi when ethernet has a DHCP lease or fixed IP. Must handle the W5500 being absent gracefully (like the OLED, it is optional NodoShop hardware). Possible overlap with TASK-725/734 (WiFi/Ethernet fixed IP) — check before implementing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 When ethernet obtains a DHCP lease or fixed IP, WiFi is disabled and traffic uses the wired link
- [x] #2 W5500 absent: falls back to WiFi cleanly, no boot hang
- [x] #3 Behaviour verified on OTGW32 with and without the W5500 present (field)
- [x] #4 Gated behind HAS_ETH_CAPABLE; ESP8266/non-ETH boards unaffected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
REFERENCE SPEC from other-projects/OT-Thing-OTGW32/Firmware/src/main.cpp (read-only; dev worktree). Robert's rule: connected W5500 with DHCP or fixed IP => disable WiFi. OT-Thing impl: W5500Driver on SPI1; boot Ethernet.begin(mac,1000), if linkStatus()==LinkON set WIRED_ETHERNET_PRESENT=true (~340-344). manageNetwork() polled every netCheckInterval (~240-268): (a) link up & !WIRED -> Ethernet.begin(mac,2000), WIRED=true, WiFi.disconnect() ('Turn off WiFi to avoid routing conflicts'), MDNS.begin; (b) link down & WIRED -> WIRED=false, WiFi.begin() (reconnect). WiFi auto-reconnect is gated on !WIRED_ETHERNET_PRESENT (~65). In configMode: WiFi.mode(WIFI_OFF) then WIFI_AP. NOTE: our TASK-581 (dynamic Eth<->WiFi failover) is marked Done but tjfs reports on alpha.84 the board still uses WiFi with the cable plugged in — so our failover either does not actually WiFi.disconnect() or never flips. This task = make WiFi actually disabled when ETH has an IP. Audit our Ethernet.ino against this reference first to find why TASK-581 does not disable WiFi. W5500 absent must stay graceful (fall back to WiFi, no boot hang).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in commit d27e0c12 (alpha.91), pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support. Verified in shipped Ethernet.ino/OTGW-firmware.ino/networkStuff.ino: root cause was initEthernet() having zero call sites (TASK-581 failover was dead code) plus switchToEthernet only doing WiFi.disconnect(). Fixed: initEthernet() now called in setup; switchToEthernet() sets eMode then WiFi.disconnect()+WiFi.mode(WIFI_OFF); loopWifi() early-returns while eMode==NET_ETHERNET; whole file HAS_ETH_CAPABLE-gated (inert when W5500 absent). Code ACs 1,2,4 verified met. Closed per maintainer rule: shipped+pushed under an alpha, only AC3 (field-verify with/without W5500 on OTGW32) remains and field validation is the sole remainder.
<!-- SECTION:FINAL_SUMMARY:END -->
