---
id: TASK-388
title: 'Fix: MQTT binary_sensor discovery via flag-driven otgw-pic/ prefix'
status: Done
assignee:
  - '@rvdbreemen'
created_date: '2026-04-23 16:59'
updated_date: '2026-04-23 18:40'
labels:
  - bug
  - mqtt
  - ha-discovery
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.h:171'
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:1035-1036'
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:1896-1900'
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:1985-1992'
  - 'src/OTGW-firmware/mqtt_configuratie.cpp:2404'
  - 'src/OTGW-firmware/MQTTstuff.ino:1045-1046'
  - 'src/OTGW-firmware/OTGW-Core.ino:3743'
  - 'src/OTGW-firmware/OTGW-Core.ino:3750'
  - 'src/OTGW-firmware/MQTTstuff.ino:1365'
  - 'src/OTGW-firmware/MQTTstuff.ino:1385'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Root cause confirmed via code inspection: composeBinSensorPayload() in mqtt_configuratie.cpp does not honor MQTT_HA_FLAG_IS_PIC_ENTRY (0x08) when writing stat_t. The flag is set on boiler_connected and thermostat_connected entries (mqtt_configuratie.cpp:1035-1036) but is only consumed as a skip-filter in MQTTstuff.ino:1365/1385, never as a topic-prefix driver.

Result: HA listens on <pub>/boiler_connected while firmware publishes to <pub>/otgw-pic/boiler_connected. Broken since the mqttha.cfg -> mqtt_configuratie.cpp takeover. In v1.3.5 the mqttha.cfg entries explicitly set stat_t to <pub>/otgw-pic/boiler_connected; the 1.4.x generator must produce the same path.

Fix approach: introduce PROGMEM constant kPicSubtreePrefix = "otgw-pic/" in MQTTstuff.h as single source of truth for the PIC subtree, and honor MQTT_HA_FLAG_IS_PIC_ENTRY in both composeBinSensorPayload and composeSensorPayload. The climate payload at mqtt_configuratie.cpp:2404 (currently hardcodes /otgw-pic/thermostat_connected) adopts the same constant for consistency.

Publish-side code (24 call-sites across MQTTstuff.ino and OTGW-Core.ino) is NOT modified in this task - existing consumers on the otgw-pic/ subtree keep working. The otgw-pic/ subtree is the original legacy location (since v1.3.0) and is treated as a stable public topic API. See ADR-065 (proposed alongside this task).

Discord #nederlandse-ondersteuning: the_royal_fortune, 2026-04-23 08:15Z.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 kPicSubtreePrefix PROGMEM constant added to MQTTstuff.h directly below MQTT_HA_FLAG_IS_PIC_ENTRY definition with value otgw-pic/ and a comment referencing ADR-065
- [x] #2 composeBinSensorPayload() in mqtt_configuratie.cpp (around line 1985-1992) writes kPicSubtreePrefix between mqttPubTopic/ and label when cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY is set
- [x] #3 composeSensorPayload() in mqtt_configuratie.cpp (around line 1896-1900) applies the same flag check on stat_t, with sourceSuffix handling (line 1901+) still appended after label
- [x] #4 Climate payload in mqtt_configuratie.cpp:2404 uses kPicSubtreePrefix instead of the literal string /otgw-pic/thermostat_connected
- [x] #5 python build.py --firmware exits 0 with no warnings related to string handling or PROGMEM
- [x] #6 python evaluate.py --quick exits 0
- [x] #7 mosquitto_sub -v -t <haprefix>/binary_sensor/<nodeid>/boiler_connected/config shows stat_t ending in /otgw-pic/boiler_connected
- [x] #8 mosquitto_sub -v -t <pub>/otgw-pic/boiler_connected and /thermostat_connected show ON/OFF values; HA entities binary_sensor.<hostname>_boiler_connected and _thermostat_connected transition from unavailable to active state
- [x] #9 climate.<hostname>_thermostat entity remains functional (regression check: mode_stat_t still resolves to <pub>/otgw-pic/thermostat_connected, composed via the constant)
- [x] #10 Publish-side call-sites in MQTTstuff.ino:1045-1046 and OTGW-Core.ino:3743/3750 are unchanged - grep confirms otgw-pic/boiler_connected still present as F() literal
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify current state of each target location (MQTTstuff.h:171, mqtt_configuratie.cpp:1896-1902, 1985-1992, 2404) — DONE
2. Add `extern const char kPicSubtreePrefix[] PROGMEM;` declaration to MQTTstuff.h after the flag block (after line 173 #endif)
3. Add `const char kPicSubtreePrefix[] PROGMEM = "otgw-pic/";` definition to MQTTstuff.ino after line 52 (MQTT_DISCOVERY_HEAP_MIN) — single source of truth
4. Modify composeSensorPayload stat_t write (mqtt_configuratie.cpp:1896-1902): insert flag-check + writeProgmem(kPicSubtreePrefix) between `/` and label
5. Modify composeBinSensorPayload stat_t write (mqtt_configuratie.cpp:1985-1991): same pattern
6. Modify climate payload (mqtt_configuratie.cpp:2404): replace literal `/otgw-pic/thermostat_connected` with writeChar(/) + writeProgmem(kPicSubtreePrefix) + writeProgmem(thermostat_connected)
7. Run python build.py --firmware in background; run python evaluate.py --quick in background
8. Spawn code-reviewer agent in parallel to independently verify changes
9. Post-build: check binary size delta, verify grep for otgw-pic/ publish literals still present (publish-side unchanged)
10. Mark ACs 1-6 and 10 complete (self-verifiable); leave 7-9 for user device validation
11. Write Final Summary and update status (In Progress with device-validation note)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
=== 2026-04-23 implementation log ===

1. Verified current code state at all target locations (MQTTstuff.h:171, mqtt_configuratie.cpp:1896-1902/1985-1991/2404) -- matched mental model.

2. MQTTstuff.h: added extern declaration for kPicSubtreePrefix after MQTT_HA_FLAGS_DEFINED guard block (lines 175-182).

3. MQTTstuff.ino: added definition `const char kPicSubtreePrefix[] PROGMEM = "otgw-pic/";` at line 60, with block comment explaining purpose and forward-reference to TASK-390.

4. mqtt_configuratie.cpp:1896-1906 (composeSensorPayload): inserted MQTT_HA_FLAG_IS_PIC_ENTRY check + writeProgmem(kPicSubtreePrefix) between `/` and label; sourceTopicSegment handling preserved after label.

5. mqtt_configuratie.cpp:1989-2000 (composeBinSensorPayload): same pattern, correct for binary_sensor entries.

6. mqtt_configuratie.cpp:2411-2415 (climate mode_stat_t): replaced literal `/otgw-pic/thermostat_connected` with writeChar(/) + writeProgmem(kPicSubtreePrefix) + writeProgmem(PSTR("thermostat_connected\"")).

7. Ran python build.py --firmware in parallel with python evaluate.py --quick and an independent code-reviewer agent.

8. evaluate.py initially FAILED with "5 unresolved ADR reference(s)" -- the new ADR-065 comments pointed to a file that did not yet exist. Authored docs/adr/ADR-065-otgw-pic-mqtt-subtree.md as Proposed (this also covers 7 of 8 ACs of TASK-389; the 8th -- status->Accepted -- requires explicit user approval per CLAUDE.md ADR workflow).

9. After ADR-065 creation: evaluate.py reports 30 pass / 0 fail / 2 pre-existing warnings / 2 info; Health Score 94.1%.

10. Build artifact: build/OTGW-firmware-1.4.2-beta+e27d7bd.ino.bin (731,440 bytes). RAM 58920/80192 (73%), IRAM 61983/65536 (94%), Flash 680116/1048576 (64%). Zero compiler warnings.

11. Build exit status was 1 due to `tee` failing on non-existent `.tmp/` directory (pipeline exit = last command = tee=1). Build itself succeeded -- verified via artifact presence and "Build completed successfully!" in log. Not a regression.

12. Independent code review (comprehensive-review:code-reviewer agent) reported: verdict "correct and safe to merge". Byte-for-byte verification of climate path. No missed stat_t sites. Linkage safe across TUs. One optional comment-nit flagged for TASK-390 scope awareness -- not blocking.

13. Publish-side verification: grep confirms 40 F("otgw-pic/...") literals remain in MQTTstuff.ino/OTGW-Core.ino (unchanged). The 4 boiler_connected/thermostat_connected publish sites at MQTTstuff.ino:1053-1054 and OTGW-Core.ino:3743/3750 are intact.

ACs 1-6 and 10 are self-verified complete.
ACs 7-9 (mosquitto_sub output, HA entity state, climate regression) require OTA flash + HA broker + HA UI verification -- user action needed.

2026-04-23: AC 7, 8, 9 verified by developer. Publish-side MQTT browser shows otgw-pic/ subtree intact with boiler_connected, thermostat_connected, otgw_connected all present. HA UI confirms entities leave 'unavailable' state; climate entity regression clean.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Restored 1.3.x HA-discovery contract for binary_sensor.boiler_connected and binary_sensor.thermostat_connected by activating the dormant MQTT_HA_FLAG_IS_PIC_ENTRY flag in the discovery payload generators.

Changes:
- Added `extern const char kPicSubtreePrefix[] PROGMEM;` in MQTTstuff.h as the single source of truth for the otgw-pic/ subtree string (see ADR-065). Defined once in MQTTstuff.ino.
- composeSensorPayload() and composeBinSensorPayload() in mqtt_configuratie.cpp now emit `<mqttPubTopic>/otgw-pic/<label>` in stat_t when cfg.flags & 0x08 is set, restoring symmetry with the publish side.
- Climate mode_stat_t generator now uses kPicSubtreePrefix instead of hardcoded literal /otgw-pic/thermostat_connected, eliminating a second magic-string location.
- Authored docs/adr/ADR-065-otgw-pic-mqtt-subtree.md (Status: Proposed) to record the subtree as stable public topic API and document the migration strategy for any future rename. This also discharges 7 of 8 TASK-389 ACs; the 8th (Status -> Accepted) awaits developer review per the CLAUDE.md ADR workflow.

Why:
Regression reported by the_royal_fortune on Discord #nederlandse-ondersteuning (2026-04-23). Root cause: commits bc9bd6a2 / 3e1872ce (mqttha.cfg -> mqtt_configuratie.cpp takeover) introduced the 0x08 flag with the intent of driving the subtree prefix, but the discovery payload generators never read it. v1.3.x mqttha.cfg entries explicitly encoded `stat_t = %mqtt_pub_topic%/otgw-pic/<label>`; the 1.4.x generator now produces the same path.

User impact:
HA entities binary_sensor.<hostname>_boiler_connected and binary_sensor.<hostname>_thermostat_connected transition from permanently "unavailable" to actually reporting the PIC connectivity state after this change is flashed.

Publish side unchanged:
All 24 F("otgw-pic/...") literals in MQTTstuff.ino/OTGW-Core.ino preserved. Existing external consumers (HA YAML snippets, NodeRED flows, Prometheus rules that already read from otgw-pic/...) are unaffected. TASK-390 tracks the publish-side helper refactor as a separate, optional follow-up.

Tests run:
- python build.py --firmware: success. Binary 731,440 bytes; RAM 73%, IRAM 94%, Flash 64%. Zero compiler warnings.
- python evaluate.py --quick: 30 pass / 0 fail / 2 pre-existing warnings; Health Score 94.1%.
- Independent code review by comprehensive-review:code-reviewer agent: verdict "correct and safe to merge". Byte-for-byte verification of climate path equivalence. No missed stat_t write sites.

Remaining work (user action):
- AC #7: mosquitto_sub -v -t `<haprefix>/binary_sensor/<nodeid>/boiler_connected/config` to confirm stat_t payload ends in /otgw-pic/boiler_connected.
- AC #8: mosquitto_sub on the publish topics + HA UI check that the two entities leave "unavailable" state.
- AC #9: regression check that climate.<hostname>_thermostat still operates (mode_stat_t still resolves correctly via the constant).

Files changed:
- src/OTGW-firmware/MQTTstuff.h (+8 lines: extern declaration + block comment)
- src/OTGW-firmware/MQTTstuff.ino (+8 lines: definition + block comment)
- src/OTGW-firmware/mqtt_configuratie.cpp (+14 lines: flag-checks in compose*Payload + climate refactor)
- docs/adr/ADR-065-otgw-pic-mqtt-subtree.md (new, 97 lines, Status: Proposed)

Risks / follow-ups:
- Users who in v1.4.0 or v1.4.1 manually worked around the regression by subscribing HA to <pub>/boiler_connected would need to revert that workaround. Expected population very small (the bug makes the entity useless, so workarounds are unlikely). Recommend calling out in v1.4.2 RELEASE_NOTES.
- ADR-065 is Proposed; promote to Accepted only after developer review per CLAUDE.md rule (never self-approved).
<!-- SECTION:FINAL_SUMMARY:END -->
