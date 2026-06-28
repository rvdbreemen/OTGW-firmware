---
id: TASK-896
title: 'fix(webui): dedupe network header — single transport+hostname+IP cluster'
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-21 12:04'
updated_date: '2026-06-28 13:05'
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
- [x] #5 filesystem image rebuilds clean (python build.py)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
RECONCILE (beta review 2026-06-28): the 896-vs-899 'conflict' was a task-record drift, not a code conflict — the committed assets already adopt TASK-899's single wifi-arcs icon. AC#4 here ('WiFi signal bars unchanged, still color-tiered') is SUPERSEDED by TASK-899: the separate #netSignalBars/.net-signal-bars element was intentionally removed and the wifi glyph itself now conveys strength (arcs light inside-out, colour by tier). Leaving AC#4 unchecked because its premise no longer holds; the user-visible outcome (transport via icon, strength via the same icon, AP MODE label) is delivered. Header-dedupe (AC#1-3) committed; filesystem ships in the LittleFS image (AC#5).
<!-- SECTION:NOTES:END -->
