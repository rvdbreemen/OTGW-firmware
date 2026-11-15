---
id: TASK-1961
title: 'OT-Direct: OH= default 1 like the PIC'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T07:58:00Z'
updated_date: '2026-11-15T12:14:00Z'
labels:
  - otdirect
dependencies: []
priority: low
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1750. The PIC factory default sets OverrideHigh (gateway.asm 6.8 SavedSettings :5459, IgnoreErr1 and OverrideHigh; OverrideHigh = settings bit 6 :512), so a PIC answers a thermostat MsgID 100 read with the override flags in both bytes (MessageID100 :3293-3311) and PR=T shows 11 (tests/serial-2.stc). OT-Direct defaults OH to 0 (settings.otd.bOverrideHB = false, OTDirecttypes.h:131). Robert chose A by DM 1571432519373557801 (on the coordinator's recommendation DM 1571432372916850780): default 1 like the PIC, only for new installs and units without a stored OH; a stored OH=0 stays. Finding before the build (session 95): writeSettings() writes every key including OTDoverridehb (settingStuff.ino:499, since TASK-1836, first in v2.0.0-beta.203) and boot writes settings.ini only when it is missing, so 'not stored' is only a new install or a settings.ini last written before beta.203; any unit on beta.203+ that saved a setting since has false stored and keeps 0. The coordinator put that nuance back to Robert (A as stated or B a one-time migration with a marker): HOLD the implementation until that answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert's answer on the nuance (A as stated / B one-time migration) recorded with the DM id
- [x] #2 Settings default changed as decided; host test on the real settings load: a new install (no settings.ini) gives OH=1, a stored OTDoverridehb=false gives 0 (and, for B, the migration runs once)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision on the nuance (relayed by the coordinator): Robert chose B, DM 1571441135526875227 (reply to the coordinator's question DM 1571433984439750759): a one-time migration. Every unit goes to OH=1 once, with a persistent marker so it never repeats; a user who sets OH=0 afterwards keeps it. Plus a release-notes line and a CHANGELOG entry. Tests: (a) old settings.ini with OH=0 and no marker -> 1 + marker, (b) marker + OH=0 -> stays 0, (c) new install -> 1. Pattern reused: the key-presence sentinel + deferred settingsDirty persist of readSettings() (ui_onboarded TASK-997, SATbleenable TASK-1693, SATfollowthermostat TASK-1259); no new mechanism. needs-maintainer-ok: not set on this task.

Implemented (lane-95-next): OTDirecttypes.h bOverrideHB default true; settingStuff.ino writeSettings() writes the marker OTDohpicdefault (kOtdOhMarkerValue), updateSetting() sets the sentinel g_sawOtdOhMarkerKey on it, readSettings() resets the sentinel per parse and after the parse, in the [otd-oh-migrate] block, sets OH=1 + settingsDirty when the marker was absent (deferred flush stores the marker, so it runs once). A fresh install writes the defaults with the marker, so the re-read keeps OH=1 without a migration. Evidence: new tests/test_otd_oh_default_migration.py on the real setter/writer lines, the migration block and the struct default: (a) OH=0 file without marker -> boot1 oh=1 dirty=1, boot2 oh=1 marker=1 dirty=0; (b) marker + OH=0 -> 0; (c) new install -> 1; (d) OH=0 after the migration -> stays 0. 4/4 OK; on HEAD's settingStuff.ino + OTDirecttypes.h (a)(c)(d) FAIL. tests/test_otd_persist_pic_eeprom.py now starts its OH step from a stored 0 (OH_PRESET) because OH=1 equals the new default. evaluate.py --quick 86 passed / 0 failed (a literal 'true' in writeJsonBoolKV tripped check_settings_writer_separator, hence the named constant). Settings tests (test_settings_* 4, ble_nopsram, sat_follow, fs_update_keep, boot_restore_order) green. CHANGELOG [Unreleased] Changed line added. Release-notes line for the next public beta (to carry into its notes file): 'OT-Direct: OH= (copy the MsgID 100 override flags into the high byte) is now on by default like the PIC; every OT-Direct board switches to OH=1 once at its first boot on this version. Send OH=0 again if your thermostat needs it; that choice then stays.' NOT verified: a real boot on the bench (09) with an old settings.ini.

Docs (Robert asked 'is de documentatie op orde?', 2026-11-15): manual ch05/h05 (MsgID 100 flags 0x03/0x01, OH default 1, master TT=0), ch09/h09 (PIC-parity list: bare XX=, OT=/SW=/SH= relay, MsgID 99, TT/OH flags + one-time OH migration with the OTDohpicdefault marker, master TT=0; PR=T row), docs/c4/c4-code-otdirect.md, docs/c4/c4-code-settings.md (migration list, OTD key count 21, struct with the TASK-1836 fields), docs/api/MQTT.md (overridehb default; PR=A/O/L/T/P/R rows that were stale since TASK-1797/1848). Covers TASK-1957, 1847, 1906, 1803, 1750, 1751 and this task. Checks: manual OTGW32 count 0 in the touched chapters, test_manual_sat_commands / test_sat_mqtt_contract_1485 / test_sat_pm25_poll OK, evaluate --quick 86/0.

Known edge (09, bench 2026-11-15, accepted and documented, no code): a firmware older than beta.301 drops the unknown OTDohpicdefault key when it saves settings.ini (seen with d3's beta.297 on .61). After upgrade -> downgrade -> upgrade the migration therefore runs again and sets OH=1 over a user's OH=0. A guard 'migrate only when OTDoverridehb is missing' was rejected: every unit since beta.203 has OTDoverridehb=false stored, so the migration Robert chose (B) would never run. Downgrades within the beta are rare; the CHANGELOG line says to send OH=0 again after one.

Downgrade edge CONFIRMED on the bench (09, .61, 2026-11-15): after beta.297 (marker gone, OH=0) beta.301 migrated again (PR: T=11, OTDoverridehb true + marker). Accepted as documented behaviour; Known issues line for the next public release notes sent to 92: 'After going back to a build older than beta.301 and up again, OH= is set to 1 once more; set OH=0 again if you had chosen that.'
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
beta.301. AC1: Robert B (DM 1571441135526875227). AC2: tests/test_otd_oh_default_migration.py (real setter/writer lines, migration block, struct default) 4/4, (a)(c)(d) FAIL on the old code; bench 09 on .61 beta.301: all steps PASS (OH migrated to 1 with the OTDohpicdefault marker, stays 1 after reboot, OH=0 + reboot stays 0). CHANGELOG + docs (ch05/h05, ch09/h09, C4, MQTT.md) updated. Pushed origin/dev 2e4ca9c05..3e29faa63; zip beta.301+2b3d9a2 posted (Discord 1571471374038470711); final build exit 0 'Build completed successfully', RAM 130572 B, Flash 2157483 B; sweep 88/88, evaluate --quick 86/0; bench 09 on .61 beta.301+2b3d9a2 PASS (smoke + re-check).
<!-- SECTION:FINAL_SUMMARY:END -->
