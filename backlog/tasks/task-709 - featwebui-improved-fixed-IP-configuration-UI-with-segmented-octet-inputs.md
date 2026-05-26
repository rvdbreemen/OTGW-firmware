---
id: TASK-709
title: 'feat(webui): improved fixed IP configuration UI with segmented octet inputs'
status: To Do
assignee: []
created_date: '2026-05-26 12:58'
updated_date: '2026-05-26 12:59'
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
- [ ] #1 Checkbox labelled 'Use DHCP' is rendered above the fixed IP fields; it is checked by default when sStaticIp is empty
- [ ] #2 Fixed IP fields (IP, Subnet, Gateway, DNS1, DNS2) are hidden when 'Use DHCP' is checked and visible when unchecked
- [ ] #3 Each IP address field renders as 4 segmented number inputs separated by dots; each input accepts 0-255 only
- [ ] #4 Unchecking 'Use DHCP' auto-fetches device/info and prefills all IP fields with current DHCP lease values
- [ ] #5 DNS1 and DNS2 prefill from current DHCP DNS if available; otherwise fields remain empty
- [ ] #6 Auto-advance: 3 digits entered or '.' pressed moves focus to next octet; backspace on empty octet moves focus back
- [ ] #7 Paste of full dotted-decimal IP into any octet field splits and fills all 4 octets of that group
- [ ] #8 Save sends wifistaticip as empty string when DHCP is selected, or as joined dotted-decimal when fixed IP is selected
- [ ] #9 restAPI.ino sendDeviceInfoV2() exposes wifi_current_subnet, wifi_current_gateway, wifi_current_dns1, wifi_current_dns2
- [ ] #10 firmware compiles clean (python build.py --firmware exits 0)
- [ ] #11 evaluate.py --quick shows no new failures
<!-- AC:END -->
