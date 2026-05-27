---
id: TASK-709
title: 'feat(webui): improved fixed IP configuration UI with segmented octet inputs'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-26 12:58'
updated_date: '2026-05-26 13:13'
labels:
  - bug
  - webui
  - network
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - '2026-05-26'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
André (andrebrait, Discord #beta-testing 2026-05-26) reported that the fixed IP settings UI lacks clarity: it is not obvious that DHCP is the default, the fields always show even in DHCP mode, and there is no format guidance for fields like the subnet mask.\n\nRequirements agreed with maintainer:\n1. 'Use DHCP' checkbox (checked by default). Fixed IP fields are hidden while DHCP is active.\n2. Each IP address field uses 4 segmented octet inputs [NNN].[NNN].[NNN].[NNN] with type=number min=0 max=255 validation.\n3. When user unchecks 'Use DHCP': auto-prefill all fields from current DHCP lease (IP, subnet, gateway, DNS1, DNS2 if available).\n4. DNS fields: prefill from current DHCP DNS if available, otherwise leave empty (empty = use router default).\n5. Backend adds wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2 to device/info for prefill.\n6. Save behaviour: DHCP checked -> all wifi static IP fields saved as empty string; fixed IP -> octets joined to dotted-decimal string.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Checkbox labelled 'Use DHCP' is rendered above the fixed IP fields; it is checked by default when sStaticIp is empty
- [x] #2 Fixed IP fields (IP, Subnet, Gateway, DNS1, DNS2) are hidden when 'Use DHCP' is checked and visible when unchecked
- [x] #3 Each IP address field renders as 4 segmented number inputs separated by dots; each input accepts 0-255 only
- [x] #4 Unchecking 'Use DHCP' auto-fetches device/info and prefills all IP fields with current DHCP lease values
- [x] #5 DNS1 and DNS2 prefill from current DHCP DNS if available; otherwise fields remain empty
- [x] #6 Auto-advance: 3 digits entered or '.' pressed moves focus to next octet; backspace on empty octet moves focus back
- [x] #7 Paste of full dotted-decimal IP into any octet field splits and fills all 4 octets of that group
- [x] #8 Save sends wifistaticip as empty string when DHCP is selected, or as joined dotted-decimal when fixed IP is selected
- [x] #9 restAPI.ino sendDeviceInfoV2() exposes wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2
- [x] #10 firmware compiles clean (python build.py --firmware exits 0)
- [x] #11 evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. restAPI.ino: add wifi_current_subnet/gateway/dns1/dns2 to sendDeviceInfoV2()\n2. index.js: add wifisubnet/wifigateway/wifidns1/wifidns2 to hiddenSettings\n3. index.js: add renderFixedIPSection() custom renderer with octet inputs\n4. index.js: wire toggle handler (show/hide + prefillFromDHCP)\n5. index.js: wire octet UX (auto-advance, backspace, paste)\n6. index.js: integrate with saveSettings() via collapseOctetGroups()\n7. index.css: style octet inputs as connected unit\n8. Build + evaluator check
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in 3 files:\n- restAPI.ino: added wifi_current_subnet/gateway/dns1/dns2 to sendDeviceInfoV2() using CSTR(WiFi.*().toString())\n- index.js: hiddenSettings extended with 4 companion keys; IP_FIELD_DEFS + 8 new functions (renderFixedIPSection, makeOctetGroup, wireOctetGroup, splitIpToOctets, joinOctetsToIp, markFixedIPChanged, collapseOctetGroupsForSave, prefillFromDHCP); special-case in refreshSettings loop; collapseOctetGroupsForSave() call added to saveSettings()\n- index.css: octet-group, octet-input, fixed-ip-section, fixed-ip-row, fixed-ip-fields, fixed-ip-notice styles added\nBuild running in background.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Improved fixed IP configuration UI based on Discord feedback from andrebrait (2026-05-26).

Changes:
- restAPI.ino: sendDeviceInfoV2() now exposes wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2 via WiFi.subnetMask/gatewayIP/dnsIP() for DHCP-prefill
- index.js: wifisubnet/wifigateway/wifidns1/wifidns2 added to hiddenSettings (rendered by custom section); 8 new functions implement the fixed IP section (renderFixedIPSection, makeOctetGroup, wireOctetGroup, splitIpToOctets, joinOctetsToIp, markFixedIPChanged, collapseOctetGroupsForSave, prefillFromDHCP); collapseOctetGroupsForSave() called at start of saveSettings()
- index.css: styles for octet-group, octet-input, fixed-ip-section, fixed-ip-row, fixed-ip-fields, fixed-ip-notice

UX behaviour:
- 'Use DHCP (automatic IP)' checkbox shown first, checked by default when no static IP is configured
- Fixed IP fields hidden while DHCP is active
- Unchecking DHCP auto-fetches device/info and prefills all 5 fields from current lease (0.0.0.0 values are skipped)
- Each IP field = 4 segmented number inputs (0-255), auto-advance on 3 digits or '.', backspace on empty returns to previous, paste of full IP splits across octets
- DNS fields are optional; empty = use router default
- Reboot notice shown below the fixed IP fields
- Save: DHCP selected clears all 5 static IP settings; fixed IP joins octets to dotted-decimal

Build: exit 0 (1.6.0-beta.22+1902fb0). Evaluator: 100% health.
<!-- SECTION:FINAL_SUMMARY:END -->
