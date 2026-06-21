---
id: TASK-896
title: 'fix(webui): dedupe network header — single transport+hostname+IP cluster'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-21 12:04'
updated_date: '2026-06-21 12:07'
labels: []
dependencies: []
ordinal: 120000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Header showed transport and IP twice: netStatusText rendered 'WiFi (IP)' while devName rendered 'hostname (IP)'. Icon already encodes transport, so the word 'WiFi' and the second IP are redundant. Redesign into one coherent cluster: [transport icon] [signal bars] hostname . IP, with the mDNS name in a hover tooltip ('Reachable at <host>.local'). Bars keep showing WiFi quality (color-tiered). Eth hides bars; AP keeps an 'AP MODE' label.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 netStatusText no longer prints the transport word or IP for WiFi/Ethernet (icon conveys transport); only 'AP MODE' shown in AP fallback
- [x] #2 devName shows 'hostname . IP' exactly once using a middot separator
- [x] #3 devName has a tooltip 'Reachable at <hostname>.local' on hover (cursor:help)
- [x] #4 WiFi signal bars unchanged and still color-tiered by quality; Ethernet hides bars; AP shows none
- [ ] #5 filesystem image rebuilds clean (python build.py)
<!-- AC:END -->
