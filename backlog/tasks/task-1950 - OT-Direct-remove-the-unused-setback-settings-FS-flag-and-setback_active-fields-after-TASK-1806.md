---
id: TASK-1950
title: >-
  OT-Direct: remove the unused setback settings, FS= flag and setback_active
  fields after TASK-1806
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T22:21:00Z'
updated_date: '2026-11-15T11:39:00Z'
labels:
  - otdirect
  - cleanup
dependencies: []
priority: low
ordinal: 997000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1806 (lane-1924, d3). Since 1806 OT-Direct no longer applies a setback on thermostat loss, but OTDsetbacktemp, OTDfailsafe (FS=), the REST fields setback_temp / setback_active / otdsetback, the MQTT topic otgw-otdirect/setback_active, the web UI rows (v2.js otd_row_setback, otdsetbacktemp) and the RAM state otSetbackEngaged / state.otd.bSetbackActive / otFailSafeEnabled stay, so the API did not change. Removing them is an API contract change (REST, MQTT, settings.ini migration): needs Robert's decision. Also: the OT-Direct SB= handler stores otSetbackTemp (OTDirect.ino SB= branch), which has no consumer any more; the PIC's SB= is the GPIO away room setpoint (gateway.asm 6.6 :5390-5398).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decided which fields go and how a stored value migrates
- [ ] #2 The removed fields are gone from firmware, REST, MQTT, web UI and the manual EN/NL; a settings.ini with the old keys still loads
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 Robert decided: remove the unused setback settings, FS= and setback_active ('2. OK' in Discord DM 1571454228763779093, answer to the coordinator's list 1571452589231439943, item 1950 'remove the unused OT-Direct setback settings / FS= / setback_active (rec. yes, PIC parity after 1806)'; acknowledged in 1571454958447099976). Migration (the recommendation he accepted, recorded as our reading): a settings.ini with the old keys keeps loading, the removed keys are ignored on load and dropped at the next save; the REST fields and MQTT topic go. Added scope (d3, 2026-11-15): since TASK-1802 bug 9 (commit 103fedacf on lane-1954) OTDsetbacktimeout / iSetbackTimeout is no longer read either (fixed 3 s loss detection), so it joins the removal list: settings key OTDsetbacktimeout, REST otdsetbacktimeout and setback_timeout, v2.js otdsetbacktimeout row, settingsHelp entry. Queue (coordinator): after 1954, 1812 and bug 9.

2026-11-15 Scope decided by the coordinator under Robert's delegation (CLI 2026-11-15), option A. The release text 'remove FS=' was based on a wrong reading: FS= is a PIC command (gateway.asm 6.6 SetFailSafety :5252-5259, saved with SaveConfig; FailSafeMode = monitor-mode start after a watchdog reset, :1067-1070, :1167-1172, :4713), and SB= is the PIC's AWAY-input setback (SetSetBack :4124, PR=S). Option C (give FS= the PIC meaning on OT-Direct) is not a task now.

2026-11-15 Implemented (d3, lane-1954, placeholder beta number). Removed: state.otd.bSetbackActive, otSetbackEngaged and its reset block, the REST fields setback_active (otdirect_status), state.otd.setback_active, otdsetback (device info), setback_timeout (otdirect_settings), settings.otd.setback_timeout, the otdsetbacktimeout settings row and POST form parameter setbacktimeout, settings key OTDsetbacktimeout (write, read, debug), settingsHelp entry, telnet debug lines, the web UI graph row otd_row_setback and the otdsetbacktimeout label/meta; docs api README, openapi.yaml (YAML valid), MQTT.md, manuals EN/NL ch09/h09, C4 settings/otdirect. MQTT: otgw-otdirect/setback_active was no longer published (all otgw-otdirect/* topics left in 2.0.0, MQTTstuff.ino:2328) and had no HA discovery entry: only the stale doc rows were removed. Kept like the PIC: SB= (otSetbackTemp, OTDsetbacktemp, PR=S) and FS= (otFailSafeEnabled, OTDfailsafe); help text and comments now give the PIC meaning and 'no effect on OT-Direct'. Settings migration: updateSetting() has no OTDsetbacktimeout branch, so an old key leaves the struct unchanged and returns through platformSettingsNoopUnchanged; the next save does not write it.

2026-11-15 Evidence: new tests/test_otd_setback_fields_removed.py (8 tests): FIX OK; OLD (OTGW_SRC_REV=HEAD) FAILED 15 (every removed symbol found, the loader branch for OTDsetbacktimeout found). Related tests green via run_tests -j 2 (13 files): otd_controller_removed, otd_no_setback, otd_thermostat_lines, otd_thermostat_loss_time, otd_mode_change_keeps_state, otd_mode_switch_queue_reset, otd_oh_default_migration, otd_mode_live_apply, sat_rest_contract_1496, telnet_console_setting, wifi_skip_portal, otd_settings_hidden_in_pic_mode (OTD_KEYS 14 -> 13, the removed otdsetbacktimeout). Web UI: node --test tests/webui/otd-hidden-in-pic-mode.test.mjs 2/2 pass; v2.js parses. evaluate --quick 86/0. CHANGELOG: Removed section with a BREAKING (REST) line. AC2 stays open until the bench boots a unit whose settings.ini has OTDsetbacktimeout (.61 has it) on this build: no error, the key gone after the next save.

2026-11-15 Follow-up: the nl/de/el web UI packs still had graph.otd_row_setback and settings otdsetbacktimeout; tests/webui/i18n-lang.test.mjs failed on 'de.json has graph.otd_row_setback, English does not'. Removed from the three packs (i18n/*.json), JSON valid.
<!-- SECTION:NOTES:END -->
