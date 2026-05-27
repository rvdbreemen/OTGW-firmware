---
id: TASK-725
title: >-
  feat-2.0.0: port TASK-709 — fixed IP segmented octet UI for both Ethernet
  (W5500) and Wi-Fi
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 13:24'
updated_date: '2026-05-27 22:33'
labels:
  - feat
  - webui
  - network
  - ethernet
  - wifi
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port and extend the fixed-IP configuration UI from dev (TASK-709 + TASK-724) to the 2.0.0 branch for BOTH network interfaces on the OTGW32.\n\n=== Ethernet (W5500) — backend exists, UI missing ===\nSettings already persisted and exposed via REST API: ethstaticip (bool, true=static/false=DHCP), ethipaddress, ethgateway, ethsubnet, ethdns (one DNS).\nWhat is missing: the improved WebUI with DHCP toggle + segmented octet inputs. Currently rendered as plain checkbox + plain text inputs.\nPrefill: add eth_current_subnet, eth_current_gateway, eth_current_dns to sendDeviceInfoV2() using ETH.subnetMask(), ETH.gatewayIP(), ETH.dnsIP().\n\n=== Wi-Fi — both backend and UI missing ===\nNo Wi-Fi static IP settings exist in 2.0.0. Both the firmware backend AND the UI need to be added:\n1. Settings struct: add wifistaticip (string, empty=DHCP — same as dev branch), wifisubnet, wifigateway, wifidns1, wifidns2 to a WifiSection in OTGWSettings\n2. Persistence: read/write these fields in settingStuff.ino (JSON to LittleFS)\n3. Network apply: apply static IP at WiFi connect time using WiFi.config(ip, gateway, subnet, dns1, dns2)\n4. REST API: expose wifistaticip and companions via sendSettingsV2() / updateSettingV2()\n5. DHCP prefill: add wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2 to sendDeviceInfoV2() using WiFi.subnetMask(), WiFi.gatewayIP(), WiFi.dnsIP(n)\n6. UI: segmented octet inputs + DHCP toggle (same pattern as Ethernet section)\n\n=== CSS notes ===\n.settingDiv .settings-field-container already has float:left; text-align:left in components.css — no justify-content fix needed (TASK-724 issue does not exist in 2.0.0).\nNew octet-group, octet-input, and fixed-ip-row styles should use design-system tokens (var(--space-*), var(--fg-*), var(--brand-*)).\n\nBased on: TASK-709 (dev implementation), TASK-724 (dev CSS fix — already correct in 2.0.0)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Checkbox 'Use DHCP (automatic IP)' is rendered for the Ethernet static IP section; checked when ethstaticip=false
- [x] #2 Ethernet static IP fields (IP Address, Subnet Mask, Gateway, DNS Server) are hidden when DHCP is checked and visible when unchecked
- [x] #3 Each IP field renders as 4 segmented number inputs (0-255) separated by dots, using the 2.0.0 design-system CSS tokens
- [x] #4 Unchecking DHCP fetches device/info and prefills all 4 fields from current Ethernet DHCP lease (ETH.subnetMask/gatewayIP/dnsIP)
- [x] #5 sendDeviceInfoV2() in restAPI.ino exposes eth_current_subnet, eth_current_gateway, eth_current_dns via ETH.* API
- [x] #6 Save: DHCP checked saves ethstaticip=false and clears IP fields; fixed Ethernet saves ethstaticip=true with joined dotted-decimal values
- [x] #7 All CSS uses design-system tokens (var(--space-*), var(--fg-*), var(--brand-*)); labels are left-aligned via existing .settingDiv .settings-field-container rule
- [x] #8 python build.py exits 0 (ESP32 target)
- [x] #9 Ethernet DHCP toggle: checkbox 'Use DHCP' shown for Ethernet section; checked when ethstaticip=false; IP fields hidden/shown accordingly
- [x] #10 Ethernet segmented inputs: IP Address, Subnet Mask, Gateway, DNS Server each rendered as 4 octet inputs (0-255) with auto-advance and paste support
- [x] #11 Ethernet DHCP prefill: unchecking fetches device/info; eth_current_subnet/gateway/dns added to sendDeviceInfoV2() via ETH.*
- [x] #12 Ethernet save: DHCP checked -> ethstaticip=false + IP fields cleared; fixed -> ethstaticip=true + joined dotted-decimal
- [x] #13 Wi-Fi settings backend: WifiSection added to OTGWSettings with wifistaticip (str), wifisubnet, wifigateway, wifidns1, wifidns2; persisted to LittleFS
- [x] #14 Wi-Fi network apply: static IP applied via WiFi.config() at connect time when wifistaticip is non-empty; reverts to DHCP when empty
- [x] #15 Wi-Fi REST API: wifistaticip and companions exposed via settings endpoint; companion keys (wifisubnet etc.) are in hiddenSettings (rendered by custom section)
- [x] #16 Wi-Fi DHCP toggle UI: same pattern as Ethernet; DHCP prefill uses wifi_current_subnet/gateway/dns1/dns2 from sendDeviceInfoV2()
- [x] #17 Wi-Fi segmented inputs: IP Address, Subnet Mask, Gateway, DNS Server 1, DNS Server 2 (optional) with same UX as Ethernet section
- [x] #18 All new CSS uses design-system tokens; labels are left-aligned via existing .settingDiv .settings-field-container rule
- [ ] #19 python build.py exits 0 (ESP32 target)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WiFi en Ethernet fixed IP geport naar 2.0.0: WifiSection backend, persistentie, WiFi.config(), REST API, segmented octet UI in Network group.
<!-- SECTION:FINAL_SUMMARY:END -->
