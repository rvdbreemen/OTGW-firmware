---
id: TASK-2928
title: >-
  Web UI: the Settings SAT category expands SAT as Standalone Adaptive
  Thermostat, the SAT page and manual say Smart Autotune Thermostat
status: To Do
assignee: []
created_date: '2026-11-19T19:56:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1063000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the static i18n audit of 2026-11-19 (origin/dev 75aedd1d3). The Settings rail SAT category reads title 'SAT — Smart Thermostat' and description 'Standalone Adaptive Thermostat.' (v2.js:1023-1024 settings.cat.sat.*, v2.js:4150 category table), while the SAT page header says 'SAT · Smart Autotune Thermostat' (v2.html:332, 340; v2.js:8378) and the manuals say 'Smart Autotune Thermostat' 31 times (docs/MANUAL.md, docs/manuals/en). The packs copy the inconsistency (nl/de keep 'Standalone Adaptive Thermostat.'; de keeps the English title). One visible string pair per language; no behaviour change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Settings SAT category title and description use the same expansion as the SAT page (Smart Autotune Thermostat) in v2.js and in nl/de/el.json
- [ ] #2 i18n pack tests and i18n-no-hardcoded green; a grep shows no 'Standalone Adaptive' left in data/ and i18n/
<!-- AC:END -->
