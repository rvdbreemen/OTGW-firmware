---
id: TASK-3121
title: 'OTGW32: web UI shows PIC-only or confusing items'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T08:53:00Z'
updated_date: '2026-11-27T09:05:00Z'
labels:
  - webui
  - hardware
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1176000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From otgw-ux-designer's check of the web UI on an OTGW32 (OTGW-field-data/_comms/ux-webui-otgw32-newcomer-check.md, 2026-11-27, read at origin/dev and run in headless Chrome against a fake OT-Direct device, not a real unit). Gaps: (a) Settings > OT-Direct shows three bypass controls (Open bypass now, Auto bypass, Bypass relay fitted: v2.js:4401-4405, settingsHelp.h:86-90) while the docs say bypass is not available in this release; the two ventilation rows and the relay row use the word bypass for different things and the relay tick is dead. (b) Settings > OpenTherm shows 'PIC temperature sensor' (v2.js:4362, help says PIC Classic only, settingsHelp.h:48) and the 'Boot commands' help says PIC commands (settingsHelp.h:47). (c) The command bar placeholder 'Command, e.g. PS=1, TT=20.5, GW=1' (v2.html:525, v2.js:481) is shown on OT-Direct although PS=1 is a PIC command. (d) The Advanced navigation subtitle 'pic . debug . files . system' (v2.html:69) stays although the PIC tab is hidden. (e) The S0 counter, GPIO outputs and LED blink settings are visible (S0 and outputs are handled by TASK-3120). (f) Nowhere does the UI say that something is not available on this board: the only such text (adv.pic_none, v2.js:124, v2.html:840) sits in the hidden PIC tab. Owner: otgw-webui. Relayed by the coordinator 2026-11-27.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each item (a) to (f) is hidden or labelled 'not available on this board' according to the board flag (otcommandinterface or hardware_type), with the wording agreed with the docs (bypass not available in this release)
- [ ] #2 A web UI test per item, red on the old files and green on the change
- [ ] #3 Screenshots of the affected screens for the OTGW32 board type (fake OT-Direct device is enough), and the same screens on a PIC board unchanged
- [ ] #4 NL, DE and EL packs follow the new or changed strings through the translation sync
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 (otgw-webui) first part, local lane ba-otd (stacked on ba-3118), NOT pushed. Done with ux strings (_comms/ux-otgw32-ui-strings.md, ux NL/DE/EL drafts used): hide 'PIC temperature sensor' on a non-PIC board; Advanced nav subtitle 'debug · files · system' on OT-Direct (key h_home.nav_advanced_sub_otd, EN + nl/de/el); OT-Direct category line 'Not available on this board: PIC firmware updates and Bluetooth sensors.'; labels 'Open ventilation bypass now' and 'Ventilation bypass: automatic' (EN + packs); help of the two ventilation rows 'This is not the thermostat bypass relay.' (settingsHelp.h + Gz; pack translations of those two help texts come with TASK-2927, which must pick up the new English); GPIO outputs group hidden when device/info has_output_pin === false (otgw-poc-dev, TASK-3120: S0 stays visible, absent = shown). The board gate now runs at start (initPicTabGate at 1.2 s) and not only after Advanced was opened. Test tests/webui/otd-board-hints-3121.test.mjs 4 tests: green on the lane, 2 red on origin/dev (the two PIC/control tests pass on both). Neighbours green: otd-hidden-in-pic-mode 2, adv-pic-tab-hidden 3, settings-help 29, i18n-lang 6, i18n-no-hardcoded 3, lang-menu 7, hscroll-and-rail 4, otd-mode-running-note 1, progmem_assets_deflated 4. OPEN: command-bar placeholder and boot-commands help (wait for otgw-dev on PS=1 / boot commands), otdhasbypassrelay help (wait for otgw-dev: does the flag change anything), the S0 group with an unset pin (after otgw-poc-dev's field), has_output_pin exists only in the test fixture until his lane lands, screenshots (not taken), nl/de/el for the two new help texts.
<!-- SECTION:NOTES:END -->
