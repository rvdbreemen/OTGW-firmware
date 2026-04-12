---
id: TASK-251
title: Show WiFi signal quality in network status icon
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 11:32'
updated_date: '2026-04-11 11:41'
labels:
  - frontend
  - wifi
  - ux
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The header network icon currently shows only the network mode (WiFi/Ethernet/AP). Extend it to also display the WiFi signal strength visually (e.g. as a signal-bar icon or quality percentage badge) so the user can see connection quality at a glance without opening the settings page.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 WiFi signal quality (RSSI-derived percentage) is visible in/near the network icon in the header
- [x] #2 Signal quality updates dynamically with each devtime poll (no page reload required)
- [x] #3 Ethernet and AP Fallback modes show a neutral/non-applicable indicator (not a signal bar)
- [x] #4 Indicator degrades gracefully: poor signal visually distinct from good signal (e.g. color or bar fill)
- [x] #5 Works in both light and dark theme
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add wifiquality field to sendDeviceTimeV2() in restAPI.ino
2. Add signal-bar SVG element to header in index.html (4 bars, always present, hidden when not WiFi)
3. Update updateNetworkIndicator(mode, apFallback, quality) in index.js to drive bar fill
4. Add CSS signal bar rules with color tiers: good>=70 green, medium>=40 orange, poor<40 red
5. Ethernet and AP mode: show bars as neutral (all grey, no color)
6. Test light+dark theme styling
7. Run build and commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 4-bar signal strength indicator to the header WiFi icon.

Changes:
- restAPI.ino: wifiquality field added to sendDeviceTimeV2() so signal
  strength updates on every devtime poll (~1s), not just on page load
- index.html: added .net-signal-bars span with 4 .sig-bar children
- index.js: updateNetworkIndicator() now accepts quality param; sets
  data-quality attribute and toggles .active class on each bar
- index.css / index_dark.css: signal bar CSS with transitions; tiers:
  good>=70 green, medium>=40 orange, poor<40 red (theme-adapted)

Behavior:
- WiFi: 1-4 bars lit, color-coded by quality
- Ethernet: bars hidden (cable icon is sufficient)
- AP Fallback: bars visible but neutral grey (no real signal)
<!-- SECTION:FINAL_SUMMARY:END -->
