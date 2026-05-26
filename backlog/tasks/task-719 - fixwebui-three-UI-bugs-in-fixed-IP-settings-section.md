---
id: TASK-719
title: 'fix(webui): three UI bugs in fixed-IP settings section'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-26 17:44'
updated_date: '2026-05-26 17:51'
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
