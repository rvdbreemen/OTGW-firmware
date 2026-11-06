---
id: TASK-1670
title: >-
  v2 Home: boiler schematic shows a fake 'ΔT 14.2°' when flow or return is
  unknown
status: To Do
assignee: []
created_date: '2026-11-06T06:58:00Z'
labels:
  - webui
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 793000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the web UI check on .61 (beta.103+18e612a, 2026-11-06): Home > System view shows 'ΔT 14.2°' while FLOW and RETURN show '—' (OT bus offline). Cause: v2.html:295 ships the mockup text 'ΔT 14.2°' in #aDt, and v2.js:1137 only overwrites it when flow and return are both known, so the placeholder (or the last value) stays. The other two ΔT renders (v2.js:1212 bDt, v2.js:1287 table) already fall back to '—'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 #aDt shows 'ΔT —' when flow or return is null (v2.js:1137 uses the same fallback as v2.js:1212), and v2.html:295 no longer ships a sample value
- [ ] #2 Screenshot before/after on a bench unit with the OT bus offline
<!-- AC:END -->
