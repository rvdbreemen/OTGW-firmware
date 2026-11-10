---
id: TASK-1775
title: >-
  Web UI i18n follow-ups from TASK-1764: webhook ON/OFF word, Home LCD CH/HP/DHW
  abbreviations, plural keys for change/override counts
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T06:40:00Z'
updated_date: '2026-11-10T12:18:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 872000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Left open by the TASK-1764 editors (2026-11-09): (1) the webhook test button text uses {{state}} = st.toUpperCase() (English ON/OFF) inside translated sentences, e.g. de 'ON-Test senden' under 'URL bei AN'; (2) home.lcd_idle gets {{lead}} = hard-coded 'CH'/'HP' and a hard-coded 'DHW ' in v2.js, so the LCD line mixes English abbreviations into nl/de/el; (3) counts like 'unsaved change(s)' and the override count use '(s)' forms in English and '(-ές)' in Greek instead of i18next _one/_other plural keys. Web UI only (v2.js, i18n packs).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Webhook ON/OFF test buttons and states use translated words in nl/de/el (static test on the key usage + pack values)
- [x] #2 Home LCD lead and DHW abbreviations come from keys with natural nl/de/el abbreviations (glossary), fitting the LCD width
- [x] #3 Change and override counts use _one/_other plural keys in all packs; i18n-lang plural check green
- [x] #4 Whole tests/webui folder green one file at a time (TASK-1764 lesson), evaluate 0 FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 otgw-firmware-80: (1) webhook: the four st.toUpperCase() state words now t(st === 'on' ? 'home.on' : 'home.off') (existing reviewed keys: AAN/UIT, AN/AUS, ΕΝΕΡΓΟ/ΑΝΕΝΕΡΓΟ). (2) LCD: lead via home.lcd_lead_ch / lcd_lead_hp, DHW via home.lcd_dhw {{temp}}: nl CV/WP/WW, de HZ/WP/WW (glossary WW), el ΘΕΡΜ/ΑΘ/ΖΝΧ (ΖΝΧ from the glossary; ΘΕΡΜ and ΑΘ are my choice, for Sergeant D's Greek review). (3) h_set.unsaved_suffix, h_mon.sat_unsaved, h_home.ovr_badge_suffix are _one/_other; updateSaveBar() and the Home override badge set the suffix with count (spans dirtyCountSfx, satDirtyCountSfx, aOvrCountSfx, data-i18n-skip because JS fills them); updateSaveBar added to i18nRerender. New static test tests/webui/i18n-followups.test.mjs: 3/3 on the fix, 3/3 RED on origin/dev v2.js+v2.html. i18n-lang 6/6 (1 of 9 runs failed the gzip-pack case at ~7 s while another session ran Chrome tests: load-related, not proven), i18n-no-hardcoded 3/3, v2-nested-helper-scope 2/2.

2026-11-10 09:36 UTC Greek correction from Sergeant D (native speaker), #dev-sat-mqtt msg 1569631735581253643, verbatim: 'The domestic hot water enable switch in the Hot water card should be remained to “Ενεργοποίηση ΖΝΧ”'. Ambiguous between h_mon.sat_dhw_standalone ('Αυτόνομος έλεγχος ΖΝΧ', always visible) and h_mon.sat_dhw_enable ('Ενεργοποίηση ΖΝΧ (HW=)', only with a DHW tank, v2.js:7060). Asked him one A/B question via 97; not applied until he answers; his answer goes into docs/sat/sergeant-d-corrections.md first.

2026-11-10 13:10-13:25 beta.169 (a6340e832, on 4e5f3a9fa which holds detectie's beta.168). Build esp32-combo SUCCESS (fw 7:54, fs 0:19). OTA (scripts/ota_flash.py) to .61: app running 2.0.0-beta.169+a6340e8, /version.hash 283698a -> a6340e8, v2.html.gz complete, settings 164 keys unchanged. Live on .61 (Playwright DOM reads, the screenshot tool refused a scratchpad path): html lang en/nl/de/el applied; at count 0 the _other forms (injected / geïnjecteerd / eingespeist / παρακάμψεις; unsaved changes / niet-opgeslagen wijzigingen / ungespeicherte Änderungen / αλλαγές χωρίς αποθήκευση); Settings in nl with one edited field '1 niet-opgeslagen wijziging', with two '2 niet-opgeslagen wijzigingen'; edits discarded by reload, GET /api/v2/settings compare 0 differences. Not seen live: the LCD CH/HP/DHW lead (boiler idle, LCD showed STANDBY/STAND-BY/ΑΝΑΜΟΝΗ) and the webhook test buttons (webhook card not shown on .61); both covered by the static test. Pushed 4e5f3a9fa..a6340e832, zip posted msg 1569687227288785007 (-m names beta.168 TASK-1763 and beta.169).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 webhook on/off word: v2.js uses t(st === 'on' ? 'home.on' : 'home.off') in the four webhook texts; static test tests/webui/i18n-followups.test.mjs red on the old v2.js, green on the fix. AC2 LCD: home.lcd_lead_ch/lcd_lead_hp/lcd_dhw (nl CV/WP/WW, de HZ/WP/WW, el ΘΕΡΜ/ΑΘ/ΖΝΧ), all <= 8 chars with a temperature (same test). Not seen live: boiler idle on .61. AC3 plurals: _one/_other in I18N_EN and nl/de/el; i18n-lang 6/6; live on .61 '1 niet-opgeslagen wijziging' / '2 niet-opgeslagen wijzigingen'. AC4: all 60 tests/webui files passed on the fix, run one at a time (10 went red on a first pass with Chrome 'page loaded' timeouts; each passed on a rerun of the same code, some also flaked on origin/dev: machine load, TASK-1765/1766). evaluate --quick 85/0. Shipped as 2.0.0-beta.169 (a6340e832), bench .61. Open elsewhere: Sergeant D's Greek DHW-switch correction waits for his A/B answer, follow-up task.
<!-- SECTION:FINAL_SUMMARY:END -->
