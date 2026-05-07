---
id: TASK-569
title: 'fix(i18n): remove Dutch UI strings — English-only user-visible text'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 19:36'
updated_date: '2026-05-07 19:46'
labels:
  - webui
  - i18n
  - 2.0.0
dependencies: []
priority: medium
ordinal: 32000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Several user-visible strings remain in Dutch after the OTGW32 port. Maintainer feedback (2026-05-07): Heating Curve panel title shows '(Stooklijn)'; BLE-related fields say 'Geen sensoren ontdekt...' and '(Beheerd via het BLE Sensoren paneel...)'. UI must be English-only. Internal code comments / dev-only design.html may stay in Dutch for now. Confirmed Dutch user-visible occurrences: data/index.html:367 (Heating Curve title), data/index.js:3866 (BLE empty state), data/index.js:6278 (SATblemac help text), data/index.js:3493 (comment, can stay).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 data/index.html line 367: 'Heating Curve (Stooklijn)' replaced with 'Heating Curve'
- [x] #2 data/sat.js line 459 comment updated to remove the Dutch parenthetical (cosmetic, but consistency matters for grep)
- [x] #3 data/index.js line 3866 BLE empty-state message rewritten in English (suggested: 'No sensors discovered yet. Wait ~30-60s for the next broadcast.')
- [x] #4 data/index.js line 6278 SATblemac help text rewritten in English; preserve technical content (MAC format, auto-select rule)
- [x] #5 Code comments in data/design.html, data/echarts-theme.js, data/sat-slider.js may remain Dutch (internal, not user-visible) — out of scope for this task
- [x] #6 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [x] #7 python3 evaluate.py --quick — no new failures
- [x] #8 grep -in 'stooklijn|geen sensoren|beheerd via' src/OTGW-firmware/data/*.html src/OTGW-firmware/data/*.js returns no user-visible matches (comments OK)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Apply 4 known fixes from task description
2. Sweep for additional Dutch user-visible strings
3. Bump prerelease alpha.13->alpha.14
4. Build firmware (both targets)
5. Run evaluator quick
6. Commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed Dutch user-visible strings from the OTGW32 web UI; UI is English-only.

Changes:
- data/index.html: dropped '(Stooklijn)' from Heating Curve heading.
- data/sat.js: dropped '(Stooklijn)' from Heating Curve comment for grep consistency.
- data/index.js: BLE Sensors panel translated end-to-end — panel title ('BLE Sensoren' -> 'BLE Sensors'), labels ('Actieve sensor', 'Naam', 'Waarschuwing'), buttons ('Opslaan', 'Selecteer', 'Vergeet'), status text ('(geen verse data)', '(geen geselecteerd — kies hieronder)', 'actief', 'X s/m oud', '(geen naam)', empty-state 'Geen sensoren ontdekt...', roster-full warning), confirm/alert messages, SATblemac help text, and the TASK-508 code comment.

Verification:
- Build: ./build.sh --firmware exited 0 for both ESP8266 (0.81MB) and ESP32-S3 (1.84MB) targets.
- Evaluator: python3 evaluate.py --quick — 59 passed, 2 warnings (pre-existing), 0 failed.
- Grep: stooklijn|geen sensoren|beheerd via returns no matches in user-visible files.

Out of scope (per AC #5): comments in data/echarts-theme.js and data/design.html (dev-only).

Version: 2.0.0-alpha.13 -> 2.0.0-alpha.14.
<!-- SECTION:FINAL_SUMMARY:END -->
