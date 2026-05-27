---
id: TASK-724
title: >-
  fix(webui): static IP field labels right-aligned in flex row — fix flex layout
  and text-align
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 13:01'
updated_date: '2026-05-27 13:03'
labels:
  - bug
  - webui
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - '2026-05-27T11:20Z'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In the IP Configuration section (when DHCP is disabled), the labels 'IP Configuration', 'IP Address', 'Subnet Mask', 'Gateway', 'DNS Server 1/2' appear right-aligned instead of left-aligned. The octet input fields themselves are fine.\n\nRoot cause: commit dad43030 added text-align:left CSS overrides (.fixed-ip-field-label and .fixed-ip-section .settings-field-container) but some browsers (Chrome on Android) interpret text-align:right on a flex container as justify-content:flex-end, which pushes flex items to the right. The CSS text-align override on child elements does not fix the flex container's item layout.\n\nReported by: andrebrait on Discord #beta-testing, 2026-05-27
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Labels 'IP Configuration', 'IP Address', 'Subnet Mask', 'Gateway', 'DNS Server 1' and 'DNS Server 2' are left-aligned when DHCP is disabled
- [ ] #2 Input fields (octet groups) remain unchanged and visually correct
- [ ] #3 python build.py exits 0 (no filesystem asset issues)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. CSS (index.css): Change .fixed-ip-row { text-align: right } to text-align: left and add justify-content: flex-start - fixes root cause (Chrome/Android interprets text-align:right on flex as justify-content:flex-end)\n2. JS (index.js): Add fixed-ip-field-label class to toggleLbl (IP Configuration header) for consistency\n3. Run python build.py to verify\n4. Commit
<!-- SECTION:PLAN:END -->
