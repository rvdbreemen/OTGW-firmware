---
id: TASK-1838
title: >-
  Footer: replace 'OTGW firmware · v2 Web UI · live from the OpenTherm bus' with
  the 1.x copyright line
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:49:00Z'
updated_date: '2026-11-12T00:03:00Z'
labels:
  - webui
dependencies: []
priority: low
ordinal: 916000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12: the footer text may change, like the 1.x firmware, or go. 1.x (v1.7.9 index.html) shows bottom right '2021-2026 © Robert van den Breemen'. v2.html has one page footer div.foot data-i18n=h_set.foot under every page. Decision (Robert delegated it): use the 1.x line '© 2021-2026 Robert van den Breemen' (authorship, MIT), drop the marketing text; the version is already in the header.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every page footer reads '© 2021-2026 Robert van den Breemen' (same in all languages); the old text and its pack entries are gone
- [x] #2 Test red on the old code; screenshot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision (Robert delegated it: like 1.x or drop it): the 1.x line '© 2021-2026 Robert van den Breemen' (v1.7.9 index.html bottom right), untranslated; the old marketing line and h_set.foot are removed from EN/NL/DE/EL; the firmware version stays in the header. Test footer-copyright.test.mjs 2 red on the old code, 2 green; i18n-no-hardcoded allows the copyright line as a name (3 pass); i18n-lang 6, v2-undefined-names 2. Screenshot C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1838-footer.png.
<!-- SECTION:NOTES:END -->
