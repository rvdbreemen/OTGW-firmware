---
id: TASK-1791
title: 'Web UI: Monitor graph y-axis label ''DHW'' is not translated (v2.html:594)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T22:50:00Z'
updated_date: '2026-11-10T23:24:00Z'
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
- [x] #1 The axis label uses an i18n key with nl/de 'WW' and el 'ΖΝΧ' (glossary)
- [x] #2 i18n-no-hardcoded catches an untranslated SVG text node like this one (red before, green after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 implementation (not built yet: the coordinator pairs it with a4's next build). v2.html:594/595 axis labels get data-i18n h_mon.graph_dhw / h_mon.graph_ch (the CH label next to it was untranslated too); I18N_EN 'DHW'/'CH'; packs nl WW/CV, de WW/HZ, el ΖΝΧ/ΘΕΡΜ (same abbreviations as home.lcd_lead_ch / lcd_dhw). Why the scanner missed it: looksEnglish() drops KEEP words (DHW and CH are in KEEP) and treats all-caps tokens as codes. Now a text containing DHW, CH or HP counts as text before KEEP is applied; other caps codes stay codes. tests/webui/i18n-no-hardcoded.test.mjs: on the previous v2.html (OTGW_V2_DIR) it fails with exactly line 594 'DHW' and 595 'CH'; green on the fix (3/3, no new JS-scan hits). i18n-lang 6/6, i18n-followups 4/4, v2-undefined-names 2/2, v2-nested-helper-scope 2/2, sat-hot-water-one-card 3/3; evaluate --quick 85/0.

2026-11-11 00:30 build + bench. Build esp32-combo (OTGW_BUILD_JOBS=3) exit 0; build log empty (redirect through cmd /c failed), evidence instead: fresh artifacts (ino.bin 00:19:48, littlefs 00:20:14, zip 00:21:20, names beta.174+bceb92c = HEAD) and the image itself: grep -a on the .ino.bin finds '2.0.0-beta.174' x2 and 'bceb92c' x2, '2.0.0-beta.173' x0. OTA to .63 (scripts/ota_flash.py): device/info fwversion 2.0.0-beta.174+bceb92c, /version.hash bceb92c (was 0d02754), v2.html.gz complete, fsupdateincomplete=false, PIC gateway 6.8 found; settings 150 keys, 0 differences before/after (own GET compare). Live DOM (headless Chrome en/nl/de/el-GR): graph axis labels DHW/CH, WW/CV, WW/HZ, ΖΝΧ/ΘΕΡΜ.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: v2.html graph axis labels use h_mon.graph_dhw / h_mon.graph_ch (the CH label was untranslated too); nl WW/CV, de WW/HZ, el ΖΝΧ/ΘΕΡΜ; live on .63 in beta.174 (DOM dumps in all four languages). AC2: i18n-no-hardcoded counts DHW/CH/HP as text before KEEP; red on the previous v2.html with exactly line 594 'DHW' and 595 'CH', green on the fix, no new JS-scan hits. Build proof: image strings 2.0.0-beta.174 + bceb92c in the .ino.bin, device fwversion 2.0.0-beta.174+bceb92c, version.hash bceb92c; settings unchanged (150 keys).
<!-- SECTION:FINAL_SUMMARY:END -->
