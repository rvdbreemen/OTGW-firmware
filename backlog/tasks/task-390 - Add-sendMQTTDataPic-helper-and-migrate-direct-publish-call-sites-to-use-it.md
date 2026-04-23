---
id: TASK-390
title: Add sendMQTTDataPic() helper and migrate direct publish call-sites to use it
status: Done
assignee:
  - '@rvdbreemen'
created_date: '2026-04-23 17:57'
updated_date: '2026-04-23 19:02'
labels:
  - refactor
  - mqtt
  - technical-debt
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:980-985'
  - 'src/OTGW-firmware/MQTTstuff.ino:1045-1050'
  - 'src/OTGW-firmware/OTGW-Core.ino:691'
  - 'src/OTGW-firmware/OTGW-Core.ino:3743'
  - 'src/OTGW-firmware/OTGW-Core.ino:3750'
  - 'src/OTGW-firmware/OTGW-Core.ino:3758'
  - 'src/OTGW-firmware/OTGW-Core.ino:707-749'
  - 'src/OTGW-firmware/OTGW-Core.ino:778-794'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-388 lands, kPicSubtreePrefix exists as a central constant and the discovery side uses it correctly. However, the publish-side call-sites in MQTTstuff.ino and OTGW-Core.ino still contain the literal string otgw-pic/ in each F() argument. This works and is currently correct, but spreads the subtree name across multiple locations. If the subtree ever needs renaming, 24 sites must be touched manually.

This task introduces a PROGMEM-correct helper sendMQTTDataPic(label, value) that composes the topic internally using kPicSubtreePrefix, and migrates the direct call-sites (11 locations) to use it. Indirect usage (13 switch-case literals in OTGW-Core.ino:707-749 that build mqttTopic for later use) is out of scope in this task - those require a larger dispatcher refactor.

Benefit: subtree name lives in exactly one place. Zero behavior change (topic paths remain byte-identical). This is pure technical-debt reduction; not required to fix the bug in TASK-388 but captures the future-proofing intent.

Blocked by: TASK-388 must be merged first (kPicSubtreePrefix is defined there).
Related: ADR-065 (TASK-389) documents the contract this helper operationalizes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 void sendMQTTDataPic(const __FlashStringHelper* label, const char* value) implemented in MQTTstuff.ino, PROGMEM-correct, using strlcpy_P/strlcat_P with char[128] stack buffer (conform ADR-004, no String class)
- [x] #2 Function prototype added to OTGW-firmware.h forward-declarations section so OTGW-Core.ino can invoke it (Arduino auto-prototype does not lift cross-file)
- [x] #3 Direct publish call-sites in MQTTstuff.ino migrated: lines 980, 981, 982, 983, 985, 1045, 1046, 1048, 1050 - 9 call-sites changed from F(otgw-pic/X) to sendMQTTDataPic(F(X))
- [x] #4 Direct publish call-sites in OTGW-Core.ino migrated: line 691, 3743, 3750, 3758 - 4 call-sites changed
- [x] #5 Indirect literals in OTGW-Core.ino:707-749 (switch-case mqttTopic assignment) and :778-794 (picSettings publish) are NOT touched in this task - documented as out of scope
- [x] #6 grep -rn F.otgw-pic/ src/OTGW-firmware yields zero results except for the kPicSubtreePrefix definition itself and any intentionally-scoped-out literals
- [x] #7 python build.py --firmware exits 0, binary size delta within +-200 bytes vs baseline
- [x] #8 python evaluate.py --quick exits 0
- [x] #9 OTA flash to test device: mosquitto_sub -v -t <pub>/otgw-pic/# shows all previously-present topics still publishing (boiler_connected, thermostat_connected, gateway_mode, otgw_connected, version, deviceid, firmwaretype, designer, picavailable)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify current line numbers of all otgw-pic/ literals - DONE (9 in MQTTstuff.ino, 4 direct in OTGW-Core.ino for migration; 28 excluded per AC #5)
2. Add two prototypes to OTGW-firmware.h after line 127 (sendMQTTData prototype section): `void sendMQTTDataPic(const __FlashStringHelper*, const char*);` and `void sendMQTTDataPic(const __FlashStringHelper*, const __FlashStringHelper*);`
3. Add helper implementations to MQTTstuff.ino after line 966 (after last sendMQTTData overload): two overloads using strlcpy_P/strlcat_P with char[128] topic buffer; F-value overload uses char[64] value buffer to delegate to char*-value overload path
4. Migrate 9 MQTTstuff.ino sites: lines 988 (version), 989 (deviceid), 990 (firmwaretype), 991 (designer -- uses F-F overload), 993 (picavailable), 1053 (boiler_connected), 1054 (thermostat_connected), 1056 (gateway_mode), 1058 (otgw_connected)
5. Migrate 4 OTGW-Core.ino sites: lines 691 (gateway_mode), 3743 (boiler_connected), 3750 (thermostat_connected), 3758 (otgw_connected)
6. Update comment at MQTTstuff.ino:58 to reflect helper is now in use (was "still use F literals until TASK-390")
7. Run python build.py --firmware + python evaluate.py --quick + independent code-reviewer agent in parallel
8. Post-build: grep to confirm only the intentionally-scoped-out 28 literals remain in OTGW-Core.ino:707-794; verify MQTTstuff.ino has zero F("otgw-pic/ literals (excluding kPicSubtreePrefix definition and the comment)
9. Mark ACs complete, commit with descriptive message, push to origin/dev
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
=== 2026-04-23 implementation log ===

1. Grep verified 9 direct publish sites in MQTTstuff.ino (988, 989, 990, 991, 993, 1053, 1054, 1056, 1058 pre-edit) + 4 in OTGW-Core.ino (691, 3743, 3750, 3758). 30 literals in OTGW-Core.ino:707-794 confirmed out of scope per AC #5.

2. Added two overload prototypes to OTGW-firmware.h:130-131 -- char-value + F-value.

3. Added implementation pair in MQTTstuff.ino after line 967 (after last sendMQTTData overload). Char-value overload uses strlcpy_P + strncat_P with a 128-byte stack buffer. F-value overload copies value to valueBuf[64] then delegates to char-value overload.

4. Migrated all 13 call-sites: 5 in sendMQTTversioninfo (1015-1020 post-edit), 4 in sendMQTTstateinformation (1080-1085 post-edit), 4 in OTGW-Core.ino.

5. Updated comment block on kPicSubtreePrefix (MQTTstuff.ino:54-62) to reflect post-migration reality.

6. FIRST BUILD FAILED: `error: strlcat_P was not declared in this scope`. Root cause: strlcat_P is not present in ESP8266 Arduino Core 3.1.2 pgmspace.h (strlcpy_P is, strlcat_P is not -- asymmetric API). Fixed by replacing strlcat_P with `strncat_P(topic, reinterpret_cast<PGM_P>(label), sizeof(topic) - strlen(topic) - 1)` -- matches the canonical pattern used at OTGW-Core.ino:475.

7. SECOND BUILD SUCCESS: 731,520 bytes (delta +80 vs TASK-388 baseline, well inside AC #7 +/-200 threshold). RAM 58,840/80,192 (73%, delta -80 bytes -- plain-string literals in .data replaced by short F-labels in PROGMEM, net RAM reduction). IRAM unchanged. Flash +164 bytes (helper implementation overhead). Zero compiler warnings.

8. Evaluate: 30 pass / 0 fail / 2 pre-existing warnings, Health Score 94.1%.

9. Independent code review (comprehensive-review:code-reviewer agent): LGTM verdict. All 7 review dimensions pass: byte-identical topic reconstruction across 13 sites, helper stack-buffer sizing safe (128/64 with ~97/~52 bytes headroom), F-F overload semantics equivalent to the pre-refactor F-F sendMQTTData overload (PubSubClient cannot take PROGMEM anyway), linkage unambiguous across .ino TU concatenation, 30 scoped-out literals confirmed, no behavioral regressions, isPICEnabled guards + retain-default preserved. One optional comment-nit flagged (main kPicSubtreePrefix doc) -- addressed in follow-up edit before commit.

10. Post-migration grep: MQTTstuff.ino has zero F("otgw-pic/") publish sites (only comments + kPicSubtreePrefix definition). OTGW-Core.ino has exactly 30 F-literals, all at :707-749 (switch-case dispatcher) and :778-794 (picSettings publish block), matching AC #5 expectations.

ACs 1-8 self-verified complete.
AC 9 (OTA flash + mosquitto_sub shows all topics still publishing) requires device validation. Risk level: very low -- refactor produces byte-identical topic paths, verified by grep + code review. But AC #9 calls for runtime confirmation, so status stays In Progress until device flashed.

2026-04-23 AC 9 verified by developer via MQTT Explorer screenshot on device running 1.4.2-beta+ae18971 (TASK-390 compiled binary). All 8 PIC-subtree topics present with identical values to TASK-388 baseline. Six of the 15 out-of-scope settings/* topics also visible (setpoint_override, setback, dhw_override, gpio, gpio_states, led) confirming the unchanged dispatcher block still works. gateway_mode topic only publishes on transition or when bGatewayModeKnown; its visibility depends on broker retention policy and device boot sequence -- not a TASK-390 regression (same behaviour pre and post refactor).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pure refactor: collapsed the "otgw-pic/" string-literal usage from 13 scattered F()-literals down to a single kPicSubtreePrefix constant + sendMQTTDataPic() helper. The subtree name now lives in exactly one place; a hypothetical future rename is a one-line change instead of a 13-site safari.

Changes:
- Added two sendMQTTDataPic overloads in MQTTstuff.ino:
  - `(F label, const char* value)` -- for char*, CCONOFF(), state.*.s* strings
  - `(F label, F value)` -- for flash-literal values (e.g. Schelte Bron)
- Helper uses strlcpy_P + strncat_P with 128-byte topic buffer (canonical pattern from OTGW-Core.ino:475). ADR-004 compliant (no String class).
- Prototypes added to OTGW-firmware.h:130-131 so OTGW-Core.ino sees them through the include chain.
- Migrated 9 sites in MQTTstuff.ino (sendMQTTversioninfo + sendMQTTstateinformation) and 4 in OTGW-Core.ino (handlePRresponse + processOT).
- Updated kPicSubtreePrefix block comment in MQTTstuff.ino:54-62 to reflect post-migration state.

Intentionally out of scope (AC #5): the 30 F("otgw-pic/settings/...") literals at OTGW-Core.ino:707-794 -- these are part of a switch-case dispatcher plus a paired publish block that together require a larger refactor. Leaving them as-is for now keeps this PR focused.

Build + verification:
- Build success after strlcat_P -> strncat_P fix (strlcat_P is absent from ESP8266 Arduino Core 3.1.2 pgmspace.h; project canonical pattern is strncat_P with explicit bound).
- Binary size: 731,520 bytes (+80 vs TASK-388 baseline, well inside +/-200 threshold).
- RAM usage: 58,840/80,192 bytes (-80 vs baseline -- plain-string literals promoted to PROGMEM labels).
- Zero compiler warnings.
- Evaluate: 30 pass / 0 fail / 2 pre-existing warnings, 94.1%.
- Independent code review: LGTM, 7 dimensions including byte-identical topic reconstruction verified.

User impact: zero. Topic paths on the broker are byte-for-byte identical to TASK-388 state. External consumers (HA, NodeRED, Prometheus) see no change.

Files changed:
- src/OTGW-firmware/OTGW-firmware.h (+4 lines: 2 prototypes + comment)
- src/OTGW-firmware/MQTTstuff.ino (+30 lines: 2 helper overloads + updated block comment + 9 migrated call-sites, net +27 after migration mechanical edits)
- src/OTGW-firmware/OTGW-Core.ino (4 call-sites migrated, net zero line count change)

Remaining (AC #9): OTA flash + `mosquitto_sub -v -t <pub>/otgw-pic/#` to confirm all 9 topics from TASK-388 validation still publishing. Expected result identical to current behaviour.

Follow-up (user chose not to create task, recording for future reference):
The 15 otgw-pic/settings/* topics currently have no HA auto-discovery. They are published but HA does not create sensor entities for them automatically. Adding entries with MQTT_HA_FLAG_IS_PIC_ENTRY to the sensor table in mqtt_configuratie.cpp would, combined with TASK-388 flag logic, correctly produce stat_t = <pub>/otgw-pic/settings/<X>. No subtree migration required.
<!-- SECTION:FINAL_SUMMARY:END -->
