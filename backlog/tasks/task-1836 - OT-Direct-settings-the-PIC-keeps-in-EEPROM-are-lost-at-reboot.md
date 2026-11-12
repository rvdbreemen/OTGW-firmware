---
id: TASK-1836
title: 'OT-Direct: settings the PIC keeps in EEPROM are lost at reboot'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:46:00Z'
updated_date: '2026-11-12T01:37:00Z'
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
- [x] #1 host test on real handlers: SM=, SB=, MI=, FS= mark settings dirty; red on current code
- [x] #2 HW, SW, SH and the UI list restored after reboot and applied
- [x] #3 decision per item for IT, OH, AA/DA recorded
- [ ] #4 bench: HW=0, reboot, PR=W answers W=0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): whether the "boot commands" setting (settingsHelp.h:46) is the intended substitute.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implementation (wt-95fw): new settings.otd fields (OTDirecttypes.h) iDhwOverride (HW 0/1, 0xFF = A), fDhwSetpoint (SW), fMaxChSetpoint (SH), bIgnoreTransitions (IT), bOverrideHB (OH), sUnknownIds (UI list, space separated); written/read in settingStuff.ino as OTDdhwoverride, OTDdhwsetpoint, OTDmaxchsetpoint, OTDignoretransitions, OTDoverridehb, OTDunknownids. Internal command state like the PIC EEPROM: not in GET/POST /api/v2/settings (so the whitelist and UI are unchanged). Helpers otdPersistBool/Float/U16, otdPersistHW, otdPersistUnknownIds (markers otdPersist) mark the settings dirty only on a real change (settingsMarkDirty, the deferred flush); SM=, SB=, MI=, FS= now use them (AC1); IT=, OH=, SW=, SH=, UI=, KI= and HW= (one otdPersistHW call before the reply, not for HW=P, agreed with ba who owns the HW branch structure for TASK-1845) store their state. otdRestorePersistedCommands() (markers) runs in initOTDirect after the vent restore: HW 0/1 back (HW=A keeps the boot default), SW/SH re-sent through enqueueWriteCommand (PIC re-sends at boot, gateway.asm:1218-1229), IT/OH back, UI list back with its schedule entries disabled. Placed next to the command handlers so the gateway-loop harness slices stay as they were.

Decision AC3 (per item, taken under Robert's 'decide yourself' rule): IT and OH: persisted, as the PIC (gateway.asm:4959-4965); the OH default stays 0 here and the PIC default 1 is TASK-1750. AA/DA: not persisted. On OT-Direct AA=/DA= enable or disable schedule entries for the alternative slots (TASK-1707 notes a-d, an intentional difference from the PIC's alternative list), so a stored PIC-style list has no meaning there; revisit with TASK-1707 if that model changes. GW mode is already persisted (OTDmode), TS is not applicable on OT-Direct (no thermostat-type detection). Dependency: GW= still wipes HW and the UI list in RAM until reboot (resetTransientState), which is N7 / TASK-1839.

Evidence AC1+AC2: tests/test_otd_persist_pic_eeprom.py (4 tests; real SM, SB, MI, FS, IT, OH, SW, SH, UI, KI and HW branches, the real otdPersist helpers and otdRestorePersistedCommands, the real OTDirectSettingsSection; settingsMarkDirty() a counter; a reboot keeps settings.otd, wipes RAM and calls the restore). Fix: 4/4 OK, stored 'sm=1 sb=12.5 mi=500 fs=0 it=0 oh=1 sw=55.0 sh=70.0 hw=0 ui=[70]', boot 'hw=0 dhwbit=0 it=0 oh=1 unknown=1 id0=70 sched70=1 enq=2 last=57:4600'. Old code (origin/dev ba41264e6 + its OTDirecttypes.h): 3 FAIL ('SM dirty=0', 'IT dirty=0', 'boot none'), 1 ok (the unchanged-value case is trivially true there). evaluate --quick 0 failed; the same targeted suites as TASK-1840 are green.

Open: AC4 bench via the bench-tester: HW=0, reboot, PR=W answers W=0 (plus SW= and UI= across a reboot). Not run: esp32-combo build (build slot via the coordinator).

Bench checklist for otgw-bench-tester: on .61 (any OT-Direct mode; loopback is fine): (1) HW=0, SW=55, SH=70, UI=70, IT=0, OH=1; (2) GET /settings.ini (check the downloaded size first) shows OTDdhwoverride 0, OTDdhwsetpoint 55.00, OTDmaxchsetpoint 70.00, OTDignoretransitions false, OTDoverridehb true, OTDunknownids "70" (the flush is deferred: wait a few seconds); (3) reboot; (4) PR=W answers W=0 (AC4), PR=T answers T=01 (IT=0, OH=1); (5) in loopback the boot re-sends SW and SH: R frames WRITE MsgID 56 data 0x3700 and MsgID 57 data 0x4600 appear after boot; (6) UI=70 still active: no R frame READ MsgID 70 in loopback (its schedule entry is disabled). Clean up afterwards: HW=A, SW=0, SH=0, KI=70, IT=1, OH=0, and check settings.ini again. Settings unchanged otherwise (save /api/v2/settings before, compare after).
<!-- SECTION:NOTES:END -->
