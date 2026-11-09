---
id: TASK-1768
title: >-
  Web UI: board-mode select clips the German and Greek option text in Settings >
  System
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T22:34:00Z'
updated_date: '2026-11-09T23:16:00Z'
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
- [x] #1 Board mode select shows the full option text in en/nl/de/el at 1280 px and 375 px (screenshots)
- [x] #2 Footer h_set.foot translated consistently in el (or recorded as a deliberate product name in i18n/glossary.md)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 from 4c's screenshot gate (PASS): (1) the Board mode select also clips in nl ('Automatisch herk…'), so nl, de and el are affected; only English fits. (2) Footer h_set.foot: nl 'v2-webinterface', de 'v2-Weboberfläche', el keeps 'v2 Web UI' in English. The glossary has no rule that makes 'Web UI' a product name, so most likely an oversight. Proposed el: 'OTGW firmware · διεπαφή web v2 · ζωντανά από τον δίαυλο OpenTherm' (to confirm with the Greek native review).

2026-11-10 00:3x (beta.167): select fix in v2.css (.srow select width:auto, min 46%, max 62%; phone rule unchanged): tests/webui/settings-select-fits.test.mjs red on HEAD (nl 151 px needed / 115 free, de 143/115, el 122/115), green 2/2 (desktop + 375 px); screenshot nl-settings shows the full 'Automatisch herkennen'. el footer h_set.foot -> 'OTGW firmware · διεπαφή web v2 · ζωντανά από τον δίαυλο OpenTherm' (for Sergeant D's Greek review). Same commit fixes three TASK-1764 regressions in beta.166: (1) esc() called outside its local scope (SAT page render stopped, SAT-off lock not applied), (2) graphReadout time parameter t hid t() (Graph readout threw), (3) supState var t hid t() (OT support table threw). Regression test tests/webui/v2-nested-helper-scope.test.mjs: beta.165 2/2, beta.166 red (9 esc hits + 2 t-shadow hits), 167 2/2. 10 stale sandbox tests updated with tests/webui/lib/i18n-en.mjs (each green on 165 and 167; triage with mutant checks for pressure-switch-flag, trigger-bit-labels, sat-max-modulation). All 25 static webui tests green; evaluate 0 FAIL.
<!-- SECTION:NOTES:END -->
