---
id: TASK-1836
title: 'OT-Direct: settings the PIC keeps in EEPROM are lost at reboot'
status: To Do
assignee: []
created_date: '2026-11-11T23:46:00Z'
updated_date: '2026-11-12T00:10:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 919000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N6), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC (EEPROM, restored at boot): HW (SaveConfig G:4408, :4617-4620), SM, GW mode, TS (G:4746); SW, SH (G:4187-4188, :4214-4215; restored and re-sent at boot G:1218-1229); MI (G:4610-4611), SB (G:4160-4161), FS (G:5300), IT and OH (G:4959-4965); UI/KI (G:4900-4905, reinstalled G:1298-1324), AA/DA (G:4677, :4698). Tests: serial-5.stc (SM), thermostat-5.stc (SH), dhw-7.stc (SW across reset).
- OT-Direct: HW, SW, SH, UI, IT, OH RAM only (O:541, :3098-3126, :559-562, :534-535). SM, SB, MI, FS assigned to settings.otd.* without marking settings dirty (O:3263, :3771, :3817, :3827; dirty flag settingStuff.ino:61-64, set only by updateSetting at :804) -> survive only if something else saves later. ADR-229 line 134 claims SM= is persisted.
- Consequence: HW=0 and SW=/SH= silently fall back after reboot/OTA; SM=1 over MQTT usually lost.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test on real handlers: SM=, SB=, MI=, FS= mark settings dirty; red on current code
- [ ] #2 HW, SW, SH and the UI list restored after reboot and applied
- [ ] #3 decision per item for IT, OH, AA/DA recorded
- [ ] #4 bench: HW=0, reboot, PR=W answers W=0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): whether the "boot commands" setting (settingsHelp.h:46) is the intended substitute.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
