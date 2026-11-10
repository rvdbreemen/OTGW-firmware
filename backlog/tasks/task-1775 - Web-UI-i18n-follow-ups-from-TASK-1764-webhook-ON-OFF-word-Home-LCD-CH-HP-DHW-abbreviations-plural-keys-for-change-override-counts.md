---
id: TASK-1775
title: >-
  Web UI i18n follow-ups from TASK-1764: webhook ON/OFF word, Home LCD CH/HP/DHW
  abbreviations, plural keys for change/override counts
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T06:40:00Z'
updated_date: '2026-11-10T08:50:00Z'
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
- [ ] #1 Webhook ON/OFF test buttons and states use translated words in nl/de/el (static test on the key usage + pack values)
- [ ] #2 Home LCD lead and DHW abbreviations come from keys with natural nl/de/el abbreviations (glossary), fitting the LCD width
- [ ] #3 Change and override counts use _one/_other plural keys in all packs; i18n-lang plural check green
- [ ] #4 Whole tests/webui folder green one file at a time (TASK-1764 lesson), evaluate 0 FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 otgw-firmware-80: (1) webhook: the four st.toUpperCase() state words now t(st === 'on' ? 'home.on' : 'home.off') (existing reviewed keys: AAN/UIT, AN/AUS, ΕΝΕΡΓΟ/ΑΝΕΝΕΡΓΟ). (2) LCD: lead via home.lcd_lead_ch / lcd_lead_hp, DHW via home.lcd_dhw {{temp}}: nl CV/WP/WW, de HZ/WP/WW (glossary WW), el ΘΕΡΜ/ΑΘ/ΖΝΧ (ΖΝΧ from the glossary; ΘΕΡΜ and ΑΘ are my choice, for Sergeant D's Greek review). (3) h_set.unsaved_suffix, h_mon.sat_unsaved, h_home.ovr_badge_suffix are _one/_other; updateSaveBar() and the Home override badge set the suffix with count (spans dirtyCountSfx, satDirtyCountSfx, aOvrCountSfx, data-i18n-skip because JS fills them); updateSaveBar added to i18nRerender. New static test tests/webui/i18n-followups.test.mjs: 3/3 on the fix, 3/3 RED on origin/dev v2.js+v2.html. i18n-lang 6/6 (1 of 9 runs failed the gzip-pack case at ~7 s while another session ran Chrome tests: load-related, not proven), i18n-no-hardcoded 3/3, v2-nested-helper-scope 2/2.

2026-11-10 09:36 UTC Greek correction from Sergeant D (native speaker), #dev-sat-mqtt msg 1569631735581253643, verbatim: 'The domestic hot water enable switch in the Hot water card should be remained to “Ενεργοποίηση ΖΝΧ”'. Ambiguous between h_mon.sat_dhw_standalone ('Αυτόνομος έλεγχος ΖΝΧ', always visible) and h_mon.sat_dhw_enable ('Ενεργοποίηση ΖΝΧ (HW=)', only with a DHW tank, v2.js:7060). Asked him one A/B question via 97; not applied until he answers; his answer goes into docs/sat/sergeant-d-corrections.md first.
<!-- SECTION:NOTES:END -->
