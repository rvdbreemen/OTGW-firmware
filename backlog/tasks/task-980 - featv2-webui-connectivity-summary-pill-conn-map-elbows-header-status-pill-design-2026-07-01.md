---
id: TASK-980
title: >-
  feat(v2-webui): connectivity summary pill + conn-map elbows + header status
  pill (design 2026-07-01)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 22:13'
updated_date: '2026-07-01 22:39'
labels: []
dependencies: []
ordinal: 192000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit docs/audits/2026-07-01-v2-design-update-audit.md areas connstrip-connmap (findings 1,4) + header (findings 3,5). Collapse the 6-pill connstrip into ONE worst-of health-summary pill (.connsum: All systems connected / N connection issues / N degraded; click opens Monitor Connection). Re-route the four OT-bus link paths as elbows converging on the gateway (M150,86 H264 V180 H300 style). Add header status pill hdrStatus/hdrStatMode (Connected/Disconnected dot + mode) synced from CONN model in renderConnStrip, extended for the wider v2 mode vocabulary (detecting, n/a on OT-Direct). Add .hdr-spacer layout fix (drop margin-right:auto from .big).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Connstrip renders exactly one .connsum pill with worst-of aggregation over ws/mqtt/wifi/boiler/therm and correct issue count
- [x] #2 Conn-map OT-bus links use elbow geometry per mockup
- [x] #3 Header shows status pill with Connected/Disconnected + mode incl. detecting and n/a, live-synced
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by subagent impl-980: connsum worst-of pill (DOM-built, mode excluded from roll-up), elbow re-route of lk/fl-therm+boiler paths, hdrStatus pill with disambiguated mode vocabulary (n/a -> OT-Direct only when CONN.ot.detail confirms; detecting; capitalized fallback), hdr-spacer layout. evaluate.py --quick 98.7% (0 FAIL, 1 pre-existing env warning), JS parse OK, ADR-091 drift gate clean. Kept .connpill CSS (mockup keeps both; avoids drift-gate risk). Build + on-device verify pending.
<!-- SECTION:NOTES:END -->
