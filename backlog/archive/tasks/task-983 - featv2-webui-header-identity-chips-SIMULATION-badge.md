---
id: TASK-983
title: 'feat(v2-webui): header identity chips + SIMULATION badge'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-01 22:21'
updated_date: '2026-07-04 04:23'
labels: []
dependencies: []
ordinal: 195000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area header (findings 1,2,4,6). Add .hdr-net with hdrHost chip (hostname.local + firmware version) and hdrNet chip (green dot + IP + 4-bar WiFi glyph, tooltip IP/DHCP/SSID/dBm/quality) from a one-shot GET /api/v2/device/info at page load (heavy chunked endpoint, never poll); live RSSI/bars refresh via the existing fetchConn /health poll. Handle Ethernet (Wired) and AP-fallback variants that device/info already distinguishes. Add hdrSim SIMULATION badge (hidden by default) from GET /api/v2/simulate on a slow poll piggybacking the fetchConn cadence; note /simulate is PIC-path only and inert on OT-Direct. Add .fw-badge CSS for reuse by the Advanced System tab. Keep the v2-only Classic-UI button (intentional divergence from mockup).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Header shows hostname+version chip and IP+WiFi-bars chip with correct tooltip, one-shot fetch
- [x] #2 WiFi bars + dBm refresh from existing health poll; Ethernet/AP variants render correctly
- [x] #3 SIMULATION badge appears only when simulation active
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by impl-983: hdrHost/hdrNet identity chips (one-shot withDeviceInfo reuse from TASK-982, never polls device/info), live bars+dBm via existing fetchConn health poll, wifiQualFromRssi 1:1 replica of firmware signal_quality_perc_quad (-50 perfect/-85 worst), Ethernet=Wired 4 bars, AP-fallback amber dot, DHCP/static from cached settings wifistaticip. hdrSim via /api/v2/simulate on fetchConn cadence, errors silently = not simulating (OT-Direct 404-safe); sysSim intentionally stays on device/info otgwsimulation (same firmware flag, verified restAPI.ino:333 vs :2959). Mobile <480px hides hdr-net (3-row header prevention). evaluate.py 0 FAIL, drift gate clean, parse OK. On-device verify pending.
<!-- SECTION:NOTES:END -->
