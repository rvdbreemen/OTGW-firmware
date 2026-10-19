---
id: TASK-1257
title: >-
  SAT 'Detect location' always fails on the device: browser geolocation needs a
  secure context
status: To Do
assignee: []
created_date: '2026-10-19 17:33'
labels:
  - bug
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 389000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter 2026-10-19 (screenshot: Weather Open-Meteo, Detect location -> 'Location error'). v2.js (~4289-4302) uses navigator.geolocation, which browsers only allow on HTTPS or localhost; the device is plain HTTP by design (no HTTPS), so the button cannot work. Workaround: enter latitude/longitude by hand in Settings > SAT > Weather.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a non-secure context the button is hidden or replaced by a hint pointing to the manual latitude/longitude fields (screenshot)
<!-- AC:END -->
