---
id: TASK-983
title: 'feat(v2-webui): header identity chips + SIMULATION badge'
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-01 22:21'
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
- [ ] #1 Header shows hostname+version chip and IP+WiFi-bars chip with correct tooltip, one-shot fetch
- [ ] #2 WiFi bars + dBm refresh from existing health poll; Ethernet/AP variants render correctly
- [ ] #3 SIMULATION badge appears only when simulation active
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->
