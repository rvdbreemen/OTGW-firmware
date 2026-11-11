---
id: TASK-1820
title: >-
  Monitor > Values names stay in the language that was active when the row was
  built
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T20:53:00Z'
updated_date: '2026-11-11T23:21:00Z'
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
- [x] #3 Bench: on a 2.x unit switch EL -> EN -> NL on Monitor > Values without reload, names follow; screenshot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host test tests/webui/values-names-follow-language.test.mjs (runs shipped renderValues/fillValName/buildValRow/valDisplayName/t): old v2.js 2 fail (actual 'Κατάσταση φλόγας' expected 'Flame Status'; actual 'Flame Status' expected 'Flammenstatus'), fix 2 pass. Neighbours green: i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, lang-menu 7, v2-undefined-names 2, graph-room-dropout 1, graph-readout 6, graph-break 8, graph-time-axis-and-png-export 11. evaluate.py --quick: 93 checks, 0 FAIL.

Real-browser old-vs-fix (headless Chrome, real v2.html/v2.js via tests/webui/lib/page-harness.mjs, fake device, packs from i18n/): start EL, switch EN then NL via the header menu without reload. OLD: tab text becomes 'Values'/'Waarden' but names stay Greek (exactly Robert's screenshot). FIX: names 'Flame Status' (EN) then 'Vlamstatus','Status cv','Cv ingeschakeld' (NL); 0 page exceptions. Script + screenshots: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench (old-*.png, fix-*.png, old.json, fix.json). Device bench (AC3) waits for the coordinator's go on .61 (queue: d3 -> 09 -> this); commit df326266b gets re-bumped to beta.186 then.

Maintainer directive via coordinator (~22:50): only otgw-bench-tester (09) flashes units and runs bench tests. Flow here: after go -> rebase, re-bump beta.186, build, no push; hand zip + checklist to 09 via coordinator; push and post after 09 passes. AC3 is evidenced by 09's bench run.

Rebased onto fa5790118 and re-bumped through the hook: TASK-1820 6fb23b77f beta.186, TASK-1821 579e0d172 beta.187. tests/webui 66 files 0 failing, evaluate --quick 93 checks 0 FAIL. build.bat esp32-combo from 579e0d172: 'Build completed successfully', build/OTGW-firmware-esp32-combo-2.0.0-beta.187+579e0d1-flash.zip (6155.7 KB), archived to OTGW-build-archive/2.0.0-beta.187+579e0d1.zip. Bench requested from 09 via the coordinator; push waits for its OK.

Bench (otgw-bench-tester 09, 2026-11-12 ~00:10, .61): scripts/ota_flash.py app+fs exit 0, running 2.0.0-beta.187+579e0d1, bootcount 350->352, settings 164 unchanged, MQTT connected, crashlog none. PASS: language switched EN->EL->EN->NL->DE without reload (navigation type 0), 12 rows each time, names follow every switch incl. EL->EN: EL 'Κατάσταση φλόγας'/Τιμές, EN 'Flame Status'/Values, NL 'Vlamstatus','Status cv'/Waarden, DE 'Flammenstatus','Heizbetrieb'/Werte. Screenshots C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/4f6ad420-bf33-4a3a-88bc-4eb9890a2231/scratchpad/b187/B-values-{el,en,nl,de}.png. Pushed fa5790118..579e0d172; git cherry origin/dev HEAD empty; zip posted (Discord message 1570216799935406160).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/AC2: values-names-follow-language.test.mjs 2 red on the old code, 2 green on the fix; headless Chrome on the real v2 page old vs fix (Greek stays vs names follow). AC3: 09's bench on .61 with beta.187+579e0d1, EL/EN/NL/DE live switch PASS, screenshots B-values-*.png. Commit 6fb23b77f (beta.186), pushed, posted with beta.187.
<!-- SECTION:FINAL_SUMMARY:END -->
