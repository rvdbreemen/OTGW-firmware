---
id: TASK-1724
title: >-
  Web UI: a third language on request (PL first, then FR/IT) once a named native
  reviewer commits
status: To Do
assignee: []
created_date: '2026-11-08T09:45:00Z'
updated_date: '2026-11-16T15:37:00Z'
labels:
  - i18n
  - webui
dependencies: []
priority: low
ordinal: 833000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08 adopted the TASK-1689 language advice: no third language now. A language is added only when a named native-speaker reviewer commits to keep it current. Order of candidates: Polish (OTGW community on arturhome.pl, 13/158 located issue authors), then French and Italian; Spanish and Greek not unless a named reviewer appears. Evidence and refuted claims in TASK-1689 notes (workflow wf_604b1a26-22f).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A named native reviewer for the language has committed (name in the notes)
- [ ] #2 Font coverage decided for the language (Polish: Inter covers 2 of 18 diacritics; Greek: Inter has no Greek glyphs)
- [ ] #3 Pack passes the coverage and placeholder test; a language that stays below the agreed coverage for two releases is removed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 Robert's decision (DM 1571454228763779093, item 8, reply to the bot list 1571452589231439943 whose item 2 named TASK-1724): 'Third native language is Greek. And George should review. Next language after that is on hold.' So the third web UI language is Greek (el), not Polish; the named reviewer for AC1 is George; PL/FR/IT stay on hold. Note: i18n/el.json already ships as a first translation (TASK-1914 etc. say 'the Greek strings are a first translation'), so the work left is George's review, the font question of AC2 (Inter has no Greek glyphs) and the coverage test of AC3. To run through the translate_webui_otgw-firmware skill ('el refresh').

2026-11-16 read-only check by comms 92 (no web UI change before the 20:00 beta). AC3: i18n/el.json has 1613 keys, the same set as nl.json and de.json (0 missing, 0 extra), 0 placeholder mismatches; the gate is tests/webui/i18n-lang.test.mjs:147 (not run now). 34 el values hold no Greek letter (e.g. adv.pic_mode_gateway, conn.mqtt_name, conn.api_name, conn.tip_wifi): names or untranslated text, for George's review. AC2: data/fonts/inter-400.woff2 and inter-700.woff2 are 230-glyph subsets with 0 Greek code points (fontTools); --font-sans is 'Inter', Arial, Helvetica, sans-serif, so Greek falls back per character to Arial or the system sans; not checked on screen. Options: A keep the fallback (0 bytes, Greek in Arial next to Latin in Inter); B add a Greek Inter subset (two extra woff2 on the filesystem). Recommendation A once a screenshot reads well (coordinator: A noted, waits for George). AC1: Robert named George (DM 1571454228763779093); his commitment is not recorded yet.
<!-- SECTION:NOTES:END -->
