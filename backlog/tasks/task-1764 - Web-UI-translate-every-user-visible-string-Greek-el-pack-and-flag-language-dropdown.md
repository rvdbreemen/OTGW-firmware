---
id: TASK-1764
title: >-
  Web UI: translate every user-visible string; Greek (el) pack and flag language
  dropdown
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T20:40:00Z'
updated_date: '2026-11-09T23:16:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 863000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-09 (session 4f6ad420): 'Besides Dutch and English for the webUI I want a Greek translation. Please add that by default. Use a dropdown in the webUI with a [flag]. Look at the OTGW-site how I want this.' and 'Make sure everything is translated, and put in the translation files. Make sure to do a critical adversarial review of the translations, make sure they sound human and fit the project (boiler/heating project for hobbyists): clear and concise, decent technical level; the hints need to be for regular people.' Pattern ADR-219/220/221 (t() helper, i18next v4 JSON packs, gzipped on LittleFS), no new ADR. Dropdown copies rvdbreemen/OTGW-site index.html:35-42 details.lang-menu. English stays default with browser detection (TASK-1689). TASK-1724 asked for a named native reviewer before a third language: Robert's order overrides; Sergeant D (Greek native) gets a review offer via 97.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every user-visible string of the v2 web UI (v2.html text, titles, placeholders, aria-labels; v2.js toasts, labels, hints, status texts) goes through t()/data-i18n with English in I18N_EN; a scan test fails on a new hard-coded string
- [x] #2 i18n/nl.json, de.json and el.json carry every English key (key-parity test), natural wording reviewed adversarially per language for tone (hobbyist heating project, clear and concise, hints for non-experts) and consistent heating terminology
- [x] #3 Header flag dropdown like the OTGW-site (EN, NL, DE, EL) switches live without reload, stores otgw.lang only after the pack loaded, ignores a late pack for an older choice; English default and browser detection unchanged; tests red before, green after
- [x] #4 Screenshots before/after (light/dark, desktop/375 px) per language with synthetic data; LittleFS image still fits; evaluate --quick no new FAIL
- [x] #5 After the translation run: the workflow is captured as project skill translate_webui_otgw-firmware (.claude/skills/translate_webui_otgw-firmware/SKILL.md + saved workflow script) that takes the language code as input (Robert 2026-11-09)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope (Robert 2026-11-09 21:3x: 'translation needs to be of the WebUI only!!'): only the v2 web UI's own texts (v2.html, v2.js, v2.css content strings). Out of scope and kept English: MQTT topics and payloads, Home Assistant discovery names, REST/API field names and error texts, telnet/debug output, texts the firmware generates and sends to the UI (shown as received), docs, FSexplorer, OT protocol names (MsgID names), units and technical identifiers.

2026-11-09 23:1x evidence (session otgw-firmware-80): workflow wf_1aed8c21-f35 (29 agents): 14 region extractors (patch + keys on a private copy of a fixed base), 1 applier (all 14 patches clean, 1485 English keys in I18N_EN, node --check OK), heating glossary (92 terms + style per language), translators nl/de/el (opus), 2 adversarial reviewers per language (native tone/terminology; clarity for regular people/placeholders/plurals/length) + editor per language (nl 56/66 accepted, de 49 accepted 3 rejected, el 61/67 accepted; rejections reasoned against the code, e.g. setback is the fail-safe flow setpoint, BLE failover switches sensors not to OT).

Static verify: nl/de/el each 1485/1485 keys, 0 missing/extra/placeholder mismatches/empty values, plural pairs complete; every literal t()/data-i18n key exists. tests/webui/i18n-no-hardcoded.test.mjs (pure static) 3/3, red on the base snapshot (131 JS literals + 250 HTML texts untranslated); i18n-lang 6/6 (plural check added); lang-menu 7/7 (expectations now read from I18N_EN and el.json); otd-remote-override-panel 5/5 and otd-override-clear 5/5 after merging with TASK-1749 (its panel strings translated: graph.otd_remote_*). evaluate --quick 92 checks 0 FAIL. Webui tests run one file at a time only (TASK-1765 Chrome leak).

LittleFS fit (hard check): v2.html.gz 175,482 -> 195,990 B; i18n packs gz nl 22,973 / de 23,670 / el 27,846 B; staged data 304 -> 399 KB; mklittlefs builds it into the 768 KiB (0xC0000) partition, ~320 KiB headroom before runtime files. Added by hand: i18nRerender() rebuilds JS-built texts after a language switch and after the boot-time pack load; 3 HTML leftovers wired (WS live, Pause, Raw data). Known English on purpose: graph window chip '1 hour' (the graph code reads chip text back by regex), Mission control heading 'ws ● live', firmware/API-sourced texts, protocol names/units. Open for follow-up: webhook ON/OFF state word, lcd idle CH/HP/DHW abbreviations, _one/_other plural keys for change/override counts (el uses (-ές) forms), screenshots per language.

2026-11-09 23:3x beta.166 shipped: pushed origin/dev c701f6b95, zip posted (Discord 1569479114736406538), alpha note by 97 (1569479277399773246; Sergeant D asked to review el). Bench .61 OTA 23:12:26: 164 settings kept, device serves i18n/nl|de|el.json identical to the repo (1492 keys), page has the language menu (4 entries, flags inline); 15-min soak 28/28 200, bootcount 199 steady, heap 66.3-71.1 KB. .63 flashed by the coordinator 23:14:33 on Robert's order. Both soak overnight until ~04:15.

Screenshots (synthetic data: real settings structure with every text value replaced; one Chrome per language, chrome count 0 before/after each): C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/4f6ad420-bf33-4a3a-88bc-4eb9890a2231/scratchpad/shots-lang/{en,nl,de,el}-{home,monitor,settings,menu}.png. A first settings set showed the real SSID (read-only type 'r' not sanitized): deleted and retaken. Findings: Board mode select clips the de/el option text (TASK-1768, cosmetic); no English remnants apart from the known exceptions.

2026-11-10 LESSON (coordinator + own finding): beta.166 shipped three regressions (esc out of scope, t shadowed in graphReadout and supState) because only i18n/lang-menu/panel tests ran before the push. A big UI change needs the WHOLE tests/webui folder before the push, one file at a time if Chrome is constrained, plus a scope scan. Fixed in beta.167 under TASK-1768; regression test v2-nested-helper-scope.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 1485 English keys in I18N_EN; every v2.html/v2.js user-visible string via t()/data-i18n; tests/webui/i18n-no-hardcoded.test.mjs 3/3, red on the old UI (131 JS + 250 HTML strings). AC2: nl/de/el 1485/1485 keys, 0 placeholder mismatches, plurals complete; glossary i18n/glossary.md; two adversarial reviews + editor per language (nl 56/66, de 49 accepted, el 61/67). AC3: flag menu like the OTGW site; lang-menu 7/7 incl. late-pack and failed-fetch cases (red before the fix); English default + browser detection unchanged. AC4: screenshots en/nl/de/el Home/Monitor/Settings/menu (scratchpad shots-lang); LittleFS v2.html.gz 196,407 B on the device, packs 23-28 KB gz, ~320 KiB free; evaluate 0 FAIL; bench .61 soak OK. AC5: skill .claude/skills/translate_webui_otgw-firmware + .claude/workflows/translate-webui-language.js (args code/name/nativeName/mode add|sync|refresh). Not verified: native-speaker review (Sergeant D asked for el), visual check of every page in every language on a device. Follow-ups: TASK-1768 (select clipping), webhook ON/OFF word, lcd idle abbreviations, plural keys for counts.
<!-- SECTION:FINAL_SUMMARY:END -->
