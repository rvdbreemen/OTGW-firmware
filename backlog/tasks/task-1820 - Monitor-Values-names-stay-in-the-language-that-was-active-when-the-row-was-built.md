---
id: TASK-1820
title: >-
  Monitor > Values names stay in the language that was active when the row was
  built
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T20:53:00Z'
updated_date: '2026-11-11T21:52:00Z'
labels:
  - webui
  - i18n
  - bug
dependencies: []
priority: high
ordinal: 903000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-11: Values names show Greek while the rest of the UI is English. fillValName() runs only in buildValRow(); renderValues() (called by i18nRerender on a language switch) never refreshes the name, so a row keeps the language of its first render. Mirror case: rows built before the pack arrives stay English.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host test running the shipped renderValues/fillValName fails on the old code: rows built in Greek keep Greek names after switching to English
- [x] #2 Same test passes on the fix; NL and DE names show after a switch
- [ ] #3 Bench: on a 2.x unit switch EL -> EN -> NL on Monitor > Values without reload, names follow; screenshot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host test tests/webui/values-names-follow-language.test.mjs (runs shipped renderValues/fillValName/buildValRow/valDisplayName/t): old v2.js 2 fail (actual 'Κατάσταση φλόγας' expected 'Flame Status'; actual 'Flame Status' expected 'Flammenstatus'), fix 2 pass. Neighbours green: i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, lang-menu 7, v2-undefined-names 2, graph-room-dropout 1, graph-readout 6, graph-break 8, graph-time-axis-and-png-export 11. evaluate.py --quick: 93 checks, 0 FAIL.

Real-browser old-vs-fix (headless Chrome, real v2.html/v2.js via tests/webui/lib/page-harness.mjs, fake device, packs from i18n/): start EL, switch EN then NL via the header menu without reload. OLD: tab text becomes 'Values'/'Waarden' but names stay Greek (exactly Robert's screenshot). FIX: names 'Flame Status' (EN) then 'Vlamstatus','Status cv','Cv ingeschakeld' (NL); 0 page exceptions. Script + screenshots: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench (old-*.png, fix-*.png, old.json, fix.json). Device bench (AC3) waits for the coordinator's go on .61 (queue: d3 -> 09 -> this); commit df326266b gets re-bumped to beta.186 then.

Maintainer directive via coordinator (~22:50): only otgw-bench-tester (09) flashes units and runs bench tests. Flow here: after go -> rebase, re-bump beta.186, build, no push; hand zip + checklist to 09 via coordinator; push and post after 09 passes. AC3 is evidenced by 09's bench run.
<!-- SECTION:NOTES:END -->
