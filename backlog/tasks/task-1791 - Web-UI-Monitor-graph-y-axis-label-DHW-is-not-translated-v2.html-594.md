---
id: TASK-1791
title: 'Web UI: Monitor graph y-axis label ''DHW'' is not translated (v2.html:594)'
status: To Do
assignee: []
created_date: '2026-11-10T22:50:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 885000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the beta.173 bench (.63, 2026-11-10): the Monitor graph axis label <text class="axis-txt" x="2" y="216">DHW</text> at src/OTGW-firmware/data/v2.html:594 has no data-i18n, so it shows 'DHW' in nl/de/el too (DOM dumps en/nl/de/el-GR). The flame label next to it uses data-i18n="h_mon.graph_flame". tests/webui/i18n-no-hardcoded.test.mjs did not flag it (check why: SVG text node or an ALLOW entry).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The axis label uses an i18n key with nl/de 'WW' and el 'ΖΝΧ' (glossary)
- [ ] #2 i18n-no-hardcoded catches an untranslated SVG text node like this one (red before, green after)
<!-- AC:END -->
