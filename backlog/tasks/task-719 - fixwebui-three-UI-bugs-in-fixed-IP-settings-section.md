---
id: TASK-719
title: 'fix(webui): three UI bugs in fixed-IP settings section'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-26 17:44'
updated_date: '2026-05-26 17:53'
labels:
  - bug
  - webui
  - network
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three bugs found in fixed-IP settings UI based on andrebrait feedback in #beta-testing (2026-05-26, screenshots attached to TASK-709):\n\n1. CSS right-alignment: labels in IP Configuration section are right-aligned. Root cause: .fixed-ip-row inherits text-align:right from .settingDiv but has no label override.\n\n2. DNS2 partial prefill: when DHCP secondary DNS is unavailable, WiFi.dnsIP(1).toString() returns '255' or '255.255.255.255'. prefillIfEmpty only guards against '0.0.0.0', so the garbage value passes through and splitIpToOctets fills the first octet with '255' leaving the rest empty. This causes a validation failure on save.\n\n3. settingMessage orange box persists after save: sendPostSetting clears textContent after 2s but does NOT clear className. If a prior validation error set className='error', the coral/orange background lingers as an empty rectangle after successful save. User (Robert) reported this as 'SAVE badge not hidden after saving'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Labels in IP Configuration section (IP Configuration header, IP Address, Subnet Mask, Gateway, DNS Server 1, DNS Server 2) are left-aligned in both desktop and mobile views
- [x] #2 Unchecking DHCP when no secondary DNS is configured no longer prefills DNS Server 2 with partial garbage values (255, 0.0.0.0, 255.255.255.255, or any non-4-octet value)
- [x] #3 After a successful save, the orange/coral settingMessage box disappears (className is cleared along with textContent)
- [x] #4 Build compiles clean (python build.py exits 0)
- [x] #5 evaluate.py --quick shows no new failures
- [x] #6 When DNS Server 1 (or DNS Server 2) is available and valid from the DHCP lease, it is pre-filled when the user unchecks DHCP
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed three UI bugs in the fixed-IP settings section, all reported by andrebrait in #beta-testing on 2026-05-26.

Bug 1 — Right-aligned labels (index.css):
Added text-align:left to .fixed-ip-field-label and .fixed-ip-section .settings-field-container. The parent .settingDiv has text-align:right (for the input columns), so labels inside the fixed-IP section were inheriting that and rendering right-aligned. Now the IP Configuration header and all field labels (IP Address, Subnet Mask, Gateway, DNS Server 1/2) are left-aligned.

Bug 2 — DNS2 partial garbage fill (index.js, prefillFromDHCP):
Replaced the simple '0.0.0.0' guard with isValidPrefillIP() that requires exactly 4 dotted-decimal parts each in the 0-254 range. ESP8266 WiFi.dnsIP(1) returns INADDR_NONE (or a partial address) when no secondary DNS is configured; the previous guard let '255' or '255.255.255.255' through, filling the first DNS2 octet with 255 and leaving the rest empty, which triggered a validation failure on save. DNS1 and DNS2 now prefill correctly when valid values are available from DHCP.

Bug 3 — Orange settingMessage badge persists (index.js/index.css):
Added #settingMessage.success CSS class (green background). sendPostSetting now sets className='success' on a successful save response and clears both textContent and className after 2 seconds. Previously only textContent was cleared, leaving the 'error' class (coral background) as an empty orange rectangle after a successful save following a validation failure.

Build: exit 0 (1.6.0-beta.24+2005a79). Evaluator: 100% health, 36 checks, 0 failures.
Pushed to origin/dev: dad43030.
<!-- SECTION:FINAL_SUMMARY:END -->
