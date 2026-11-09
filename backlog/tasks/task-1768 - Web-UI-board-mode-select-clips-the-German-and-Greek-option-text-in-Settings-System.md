---
id: TASK-1768
title: >-
  Web UI: board-mode select clips the German and Greek option text in Settings >
  System
status: To Do
assignee: []
created_date: '2026-11-09T22:34:00Z'
updated_date: '2026-11-09T22:38:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 865000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in the TASK-1764 screenshots (beta.166, 1280 px): the Board mode select shows 'Automatisch erker' (de) and 'Αυτόματη ανίχνευσ' (el) because the select has a fixed width. Cosmetic; English fits. Fix in v2.css (min-width or auto width for that select), screenshot before/after per language.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Board mode select shows the full option text in en/nl/de/el at 1280 px and 375 px (screenshots)
- [ ] #2 Footer h_set.foot translated consistently in el (or recorded as a deliberate product name in i18n/glossary.md)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 from 4c's screenshot gate (PASS): (1) the Board mode select also clips in nl ('Automatisch herk…'), so nl, de and el are affected; only English fits. (2) Footer h_set.foot: nl 'v2-webinterface', de 'v2-Weboberfläche', el keeps 'v2 Web UI' in English. The glossary has no rule that makes 'Web UI' a product name, so most likely an oversight. Proposed el: 'OTGW firmware · διεπαφή web v2 · ζωντανά από τον δίαυλο OpenTherm' (to confirm with the Greek native review).
<!-- SECTION:NOTES:END -->
