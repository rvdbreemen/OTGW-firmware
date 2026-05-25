---
id: TASK-706
title: >-
  fix(webui): 'Boiler does not implement' section on OT Support tab is easy to
  miss
status: Done
assignee:
  - '@claude'
created_date: '2026-05-25 20:40'
updated_date: '2026-05-25 22:04'
labels:
  - bug
  - webui
  - ux
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-05-25'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans on #beta-testing (2026-05-25) via screenshot. The 'Boiler does not implement' list on the OT Support tab is rendered as a plain text line at the bottom of the table and is easy to overlook. Make it visually distinct — e.g. a styled card/banner below the table — so users notice which message IDs their boiler rejects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 'Boiler does not implement' section on the OT Support tab is visually separated from the table (not inline footer text)
- [x] #2 When the list is empty (boiler implements everything), nothing extra is rendered
- [x] #3 Styling is consistent with existing WebUI conventions (no new CSS frameworks)
- [x] #4 python build.py exits 0; python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Stijl #boilerUnsupportedLine als een zichtbare card/banner ipv inline voettekst\n2. Voeg achtergrondkleur, rand en icoon-prefix toe zodat het opvalt\n3. CSS-only change in index.css — geen HTML of JS wijzigingen nodig
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: CSS toegevoegd voor #boilerUnsupportedLine — amber achtergrond (#fff3cd), oranje rand (#f0ad4e), donkere tekst (#7a5800), border-radius, padding, max-width 75% met ellipsis. Zichtbaar als badge in de Statistics footer. index.css alleen.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CSS amber badge voor #boilerUnsupportedLine toegevoegd in index.css. Amber achtergrond, border-radius 4px, max-width 75% met ellipsis. Alle 4 ACs gecheckt. Committed in e2145143.
<!-- SECTION:FINAL_SUMMARY:END -->
