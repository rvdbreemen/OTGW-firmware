---
id: TASK-2927
title: >-
  Web UI: the settings help popups stay English in NL, DE and EL (ADR-219 scope
  includes the settings help)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T19:38:00Z'
updated_date: '2026-11-27T22:39:00Z'
labels:
  - webui
  - i18n
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 1062000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the read-only UI walk of 397 (7e31bc2) on .61, 2026-11-19, German UI at 375 px: all 75 settings help popups (11 Settings categories) open, fit the screen and have text, but 59 of them are plain English sentences under German labels (e.g. 'Hilfe: Hostname' -> 'Network name of this gateway (mDNS and DHCP)...'). Cause (code reading): helpFor() (v2.js:4437) returns SET_META[k].help or SETTINGS_HELP[k] from the firmware's settingsHelp.h, both English only; the packs i18n/nl.json, de.json, el.json have no help section. Robert's decision 3 on TASK-1689 (2026-11-08) and ADR-219 (Accepted) put the settings help in the translation scope ('help.<setting>' keys); the per-key English fallback is what users see now. Not a functional break (the text is readable English), so not blocking; the maintainer decides whether it belongs in 2.0.0. Size note from ADR-219: settingsHelp.h is 17,683 B raw / 6,766 B gz per language.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With the UI in NL, DE or EL the settings help popups show the pack's text for every setting the pack covers; a missing key still falls back to English per key
- [ ] #2 Pack-completeness test covers the help keys; web UI test on the real v2.js red before, green after
- [ ] #3 LittleFS/pack size impact measured and recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) v2.js helpFor(): the pack key settings.help.<key> wins, the English text of settingsHelp.h is the per-key fallback (done, local). 2) Packs nl/de/el: settings.help.<key> for the English help keys via translator agents plus two adversarial reviews (done, local). 3) Completeness test for the help keys (done: settings-help-pack-keys-2927). 4) Measure pack size and LittleFS space at the next real build of this lane (AC3). Stacked on TASK-3118 (corrected English).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 (ba): ADR-219 lines that put the settings help in scope (checked by the coordinator): docs/adr/ADR-219 line 67 (User Decision: 'only the web UI (including the settings help) is translated'), line 72 (Scope: 'all visible web UI text and the settings help'), line 117. Coordinator puts the milestone question to Robert by DM: A = 2.1.0 plus a release-note line, B = before Sunday's stable; recommendation A. Label needs-maintainer-ok until his answer.

Maintainer decision (Robert, 2026-11-19 19:54 UTC, Discord DM 1573063839715557535, relayed by the coordinator; the only open question, read as the answer to it), verbatim reply: 'A' (A = milestone 2.1.0 plus a release-note line; the settings help stays English in 2.0.0). Label needs-maintainer-ok removed, milestone stays 2.1.0.

2026-11-27 otgw-ux-designer, plan (no code: Robert chose A on 2026-11-19, milestone 2.1.0, and the work is translation, not CSS or markup): (1) run the translate_webui_otgw-firmware sync for NL, DE, EL with the help keys added to i18n/*.json (59 sentences each; ADR-219 scope), and make helpFor() (v2.js ~4437) read t('help.<key>') first with the English text as the per-key fallback: otgw-webui. (2) Measure the size change of data/ and i18n/ (AC3) before and after, because settingsHelp.h is 17,683 B raw per ADR-219 and three packs triple it on LittleFS. (3) Test: pack-completeness covers the help keys; web UI test opens a popup in NL and expects the pack text. UX input from me: apply the style rule of 2026-11-26 (no em-dash, plain words) to the 59 English sources first, so the translators do not copy the dashes (see docs/reviews/2026-11-26_style-em-dashes-and-ai-language).

2026-11-27 Note (coordinator, from otgw-webui): the NL, DE and EL packs grow by +6.5, +6.6 and +8.3 KB with the translated help texts. The free space on LittleFS has NOT been measured. Decision: this is 2.1.0 and nothing is pushed until the free space is measured (the OTGW32 unit has 768 KB LittleFS and no PSRAM). Also found in the TASK-2927 review: i18n/glossary.md item 50 gives Dutch 'ruimtesensor' for room sensor, but the nl pack labels and help texts use 'kamersensor' (satmultiarea label, satsensorarea*); the new help texts follow the pack. The glossary owner decides which term wins; if the glossary wins, the labels and the four satsensorarea help texts change.

2026-11-27 Related: TASK-3118 (two English help defects, sathpcycle/satcyclesperhour and ethdns).

2026-11-27 Owner of i18n/glossary.md: otgw-webui (he runs the translate skill and the packs), decided by the coordinator. The conflict ruimtesensor (glossary item 50) versus kamersensor (nl pack labels and help texts) is his decision; if the glossary wins, the labels and the four satsensorarea help texts change.

2026-11-27 (otgw-webui, 2.1.0 prep, local lane ba-2927, stacked on ba-3118): code half: helpFor() shows the pack text settings.help.<key> when the active pack has it, English per key otherwise; tests/webui/settings-help-translated-2927.test.mjs 4 (2 red on the old v2.js). Translations: 166 help texts x nl/de/el (169 keys per pack), three translator agents then two adversarial reviews (86 findings; 52 must/should applied, 7 already covered by the first replacement, nits not applied); judgement calls: ethdns says router address (Ethernet.ino:70), nl keeps kamersensor like the pack labels (glossary #50 says ruimtesensor: conflict left to the glossary owner). Tests: settings-help-pack-keys-2927 (6) completeness gate; i18n-lang and lang-menu relaxed for settings.help.* (English lives in settingsHelp.h); settings-help 29, i18n-followups 4, i18n-lang 6, i18n-no-hardcoded 3, lang-menu 7 green; evaluate --quick 0 failed. Size (gzip): nl 27067 to 33554 B, de 27826 to 34459 B, el 32897 to 41194 B (+6.5, +6.6, +8.3 KB); LittleFS free space NOT measured (needs a build). The three PWM-cycle/ethdns packs texts follow the corrected English of TASK-3118. NOT done: browser check of a translated popover, native reviewer. Task file repaired: an earlier rebase of mine left three duplicate updated_date lines in this file (commits 4109aed94, a103c84e5, 9942dbb92); this commit restores dev's valid file plus this note.

2026-11-28 otgw-webui: the six English helps that changed on dev after the first translation (gpiosensorspin, s0counterpin, ledblink, otdopenbypass, otdautobypass, satzonecount) were translated for nl/de/el and then reviewed by two adversarial reviewers (A native tone and heating terms, B clarity/numbers/length). Findings applied: s0counterpin gets the OTGW32 DI terminal and 'on other boards'; the bypass sentence says thermostat bypass relay as a thing, not 'of the thermostat'; satzonecount nl 'tot en met 4', el 'except the main room'; gpiosensorspin de 'Standardpins'; ledblink de/el wording. Both reviewers found no other defect; nits not applied (nl 'Statusled' spelling, nl ledblink word order, 'warmtevraag'). Tests one file at a time: settings-help-pack-keys-2927 6/6, settings-help-translated-2927 4/4, settings-help 29/29, i18n-lang 6/6, i18n-no-hardcoded 3/3, lang-menu 7/7. Not a native-speaker review: 'DI-contact/Klemmleiste/κλέμα' wording is the reviewers' and my choice.
<!-- SECTION:NOTES:END -->
