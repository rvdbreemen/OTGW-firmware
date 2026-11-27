---
id: TASK-3121
title: 'OTGW32: web UI shows PIC-only or confusing items'
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T08:53:00Z'
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
