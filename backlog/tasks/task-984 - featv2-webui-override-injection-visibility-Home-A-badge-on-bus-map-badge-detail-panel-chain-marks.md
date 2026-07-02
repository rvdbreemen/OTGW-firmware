---
id: TASK-984
title: >-
  feat(v2-webui): override-injection visibility (Home A badge, on-bus map badge,
  detail panel, chain marks)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 22:23'
updated_date: '2026-07-02 04:25'
labels: []
dependencies: []
ordinal: 196000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit areas home-viewpick-override (findings 2,3,4) + connstrip-connmap (findings 2,3). Poll GET /api/v2/otgw/overrides (ADR-118; OT-Direct variant /api/v2/otdirect/overrides + overrides_active) on the fetchConn cadence. Home A: .ovr-badge N-injected button in the aStatus line (hidden when zero). Conn-map: cnOvr group ON the converged bus segment (pairs with the elbow re-route from TASK-980). Floating ovrPanel with per-item label/command/now+was values, opened by either badge, outside-click close keyed on cnOvr/aOvrBadge. Per-value chain marks: append chain glyph + tooltip to aRoomSet when MsgID 9/16 active and to aOut when MsgID 27 active in the Design A render.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Badge on Home A and on conn-map bus show live override count, hidden at zero
- [x] #2 Detail panel lists active overrides with values; opens from both badges; outside-click closes
- [x] #3 Room-setpoint and outside-temp readouts get chain mark + tooltip when overridden
- [x] #4 Works on both PIC (otgw/overrides) and OT-Direct (otdirect) sources
- [ ] #5 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by impl-984: dual-endpoint override polling keyed on otcommandinterface (PIC v2/otgw/overrides / OT-Direct v2/otdirect/overrides), piggybacked on fetchConn 15s cadence, ovrRows cache. Home A .ovr-badge + on-bus #cnOvr SVG badge (on the TASK-980 y=180 elbow segment) + read-only #ovrPanel (DOM/textContent, opens from both, outside-click keyed on both). Chain marks additive via setMark: MsgID 9/16 -> aRoomSet, 27 -> aOut, no clobber of value formatting. Panel read-only per ADR-118 store (id/kind/value/lastSeen only). evaluate.py 0 FAIL, drift gate clean, parse OK. On-device verify pending.
<!-- SECTION:NOTES:END -->
