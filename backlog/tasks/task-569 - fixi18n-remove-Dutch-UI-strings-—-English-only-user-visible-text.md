---
id: TASK-569
title: 'fix(i18n): remove Dutch UI strings — English-only user-visible text'
status: To Do
assignee: []
created_date: '2026-05-07 19:36'
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
- [ ] #1 data/index.html line 367: 'Heating Curve (Stooklijn)' replaced with 'Heating Curve'
- [ ] #2 data/sat.js line 459 comment updated to remove the Dutch parenthetical (cosmetic, but consistency matters for grep)
- [ ] #3 data/index.js line 3866 BLE empty-state message rewritten in English (suggested: 'No sensors discovered yet. Wait ~30-60s for the next broadcast.')
- [ ] #4 data/index.js line 6278 SATblemac help text rewritten in English; preserve technical content (MAC format, auto-select rule)
- [ ] #5 Code comments in data/design.html, data/echarts-theme.js, data/sat-slider.js may remain Dutch (internal, not user-visible) — out of scope for this task
- [ ] #6 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [ ] #7 python3 evaluate.py --quick — no new failures
- [ ] #8 grep -in 'stooklijn|geen sensoren|beheerd via' src/OTGW-firmware/data/*.html src/OTGW-firmware/data/*.js returns no user-visible matches (comments OK)
<!-- AC:END -->
