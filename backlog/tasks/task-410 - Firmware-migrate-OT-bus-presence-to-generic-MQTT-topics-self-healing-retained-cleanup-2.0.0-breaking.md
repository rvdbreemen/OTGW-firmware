---
id: TASK-410
title: >-
  Firmware: migrate OT-bus presence to generic MQTT topics + self-healing
  retained cleanup (2.0.0 breaking)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 19:59'
updated_date: '2026-04-25 14:19'
labels:
  - mqtt
  - ha-discovery
  - breaking-change
  - 2.0.0
  - otdirect
  - firmware
dependencies:
  - TASK-408
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the firmware side of ADR-084. Move boiler_connected, thermostat_connected, and otgw_connected publishes to the generic OTGW/value/<uniqueId>/<label> namespace, remove the hardware-specific duplicates, and add a temporary self-healing retained-cleanup block that wipes stale retained payloads left behind by pre-2.0.0 firmware.

SCOPE (exactly these keys):
- boiler_connected (fed by state.otBus.bBoilerState)
- thermostat_connected (fed by state.otBus.bThermostatState)
- otgw_connected (fed by state.otBus.bOnline); note: this absorbs the OTDirect-side otgw-otdirect/ot_online topic which is retired.

OUT OF SCOPE: gateway_mode, other otgw-pic/* or otgw-otdirect/* keys, climate entity wiring.

IMPLEMENTATION STEPS:

1. src/OTGW-firmware/MQTTstuff.ino around lines 1299-1331:
   - Keep the unconditional generic publishes on :1299 (boiler_connected), :1312 (thermostat_connected), :1325 (otgw_connected).
   - Remove the `if (isPICEnabled()) { sendMQTTData(F("otgw-pic/...")); }` blocks on :1301, :1314, :1327.
   - Remove the `#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT { if (isOTDirectEnabled()) { sendMQTTData(F("otgw-otdirect/...")); } }` blocks on :1305, :1318, :1331.
   - Also remove sendMQTTDataPic(F("...")) variants in OTGW-Core.ino if they duplicate these same three values (grep for sendMQTTDataPic + those labels; leave genuine PIC settings/version/etc. publishes intact).

2. Add to MQTTstuff.ino a new migration helper block with the TEMPORARY MIGRATION CODE header comment from the plan file (C:\\Users\\rvdbr\\.claude\\plans\\idempotent-weaving-wolf.md, Wijziging 3). The header explains why the code exists and when it may be removed. Reference ADR-084 and TASK-409 in the comment.

3. Declare:
   static const __FlashStringHelper* const kV2DeprecatedTopics[] = {
     F("otgw-pic/boiler_connected"),
     F("otgw-pic/thermostat_connected"),
     F("otgw-pic/otgw_connected"),
     F("otgw-otdirect/boiler_connected"),
     F("otgw-otdirect/thermostat_connected"),
     F("otgw-otdirect/ot_online"),
   };

4. Implement three static helpers: mqttV2MigrationOnConnect() (subscribe on the 6 deprecated topics via the same namespace-building helper sendMQTTData uses), mqttV2MigrationHandleIfDeprecated(const char* topic) (empty-retained-publish + unsubscribe on match, returns true if handled), mqttV2MigrationTick() (timed unsubscribe cleanup using DECLARE_TIMER_SEC + DUE from safeTimers.h, about 5 seconds after connect).

5. Wire into the MQTT reconnect path: call mqttV2MigrationOnConnect() after regular subscribes complete.

6. Wire into mqttCallback(): first line of body, before the existing set/* dispatch: `if (mqttV2MigrationHandleIfDeprecated(topic)) return;`.

7. Wire into doBackgroundTasks() or the MQTT loop helper: call mqttV2MigrationTick() alongside existing MQTT housekeeping.

8. src/OTGW-firmware/MQTTHaDiscovery.cpp:1038-1039: change flag 0x08 to 0x00 on the boiler_connected and thermostat_connected rows. Do NOT remove the MQTT_HA_FLAG_IS_PIC_ENTRY definition — it remains in use for genuine PIC entries (version, deviceid, firmwaretype, designer, picavailable, plus climate mode_stat_t).

REFERENCE FILES (read-only):
- src/OTGW-firmware/MQTTstuff.h (flag definitions, MQTT_TOPIC_MAX_LEN = 200)
- src/OTGW-firmware/OTGW-firmware.h:353-367 (isPICEnabled, isOTDirectEnabled)
- src/OTGW-firmware/safeTimers.h (DECLARE_TIMER_SEC, DUE, RESTART_TIMER)
- docs/adr/ADR-084-generic-ot-bus-state-topics.md (rationale; written by TASK-408)
- docs/adr/ADR-065-otgw-pic-mqtt-subtree.md (what is being amended)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No publishes remain to OTGW/value/<uniqueId>/otgw-pic/boiler_connected, otgw-pic/thermostat_connected, otgw-pic/otgw_connected, otgw-otdirect/boiler_connected, otgw-otdirect/thermostat_connected, or otgw-otdirect/ot_online. Verified by grep on sendMQTTData/sendMQTTDataPic call sites for these literal strings.
- [x] #2 Firmware publishes to OTGW/value/<uniqueId>/boiler_connected, .../thermostat_connected, .../otgw_connected on every OT-bus publish cycle on both board variants (ESP8266+PIC compile target and OTGW32 compile target)
- [ ] #3 HA-discovery config payloads for boiler_connected and thermostat_connected show stat_t ending in /boiler_connected and /thermostat_connected respectively; no otgw-pic/ segment. Verified by subscribing to homeassistant/binary_sensor/+/+/config and inspecting two payloads.
- [ ] #4 On an OTGW32 build (HAS_PIC=0), HA-discovery publishes the Boiler connected and Thermostat connected binary_sensor configs (previously absent due to isPICEnabled() filter).
- [ ] #5 With simulated retained ghost entries on the six deprecated topics (injected via mosquitto_pub -r), after the first MQTT reconnect all six retained entries are wiped on the broker. Verified by mosquitto_sub --retained-only with -W 2 timeout showing empty result.
- [ ] #6 Telnet debug log contains 'V2 migration: cleared retained on <topic>' lines for each wiped retained on first boot after upgrade.
- [ ] #7 On a second reboot with no retained entries present, no 'V2 migration: cleared retained' lines appear and no MQTT errors are logged.
- [x] #8 python build.py --firmware produces a clean build for both ESP8266 and OTGW32 targets with no new warnings.
- [x] #9 python evaluate.py passes with no new PROGMEM or String-class violations.
- [x] #10 MQTT_HA_FLAG_IS_PIC_ENTRY (0x08) is still referenced by other HA-discovery entries (grep MQTTHaDiscovery.cpp + MQTTstuff.ino) after this change.
- [x] #11 Source header comment block is present at the top of the migration helpers, stating why the code exists and when it may be removed, referencing ADR-084 and TASK-409.
- [ ] #12 On both board variants, HA-discovery config payload for the Thermostat climate entity (homeassistant/climate/<nodeId>/<nodeId>-thermostat/config) shows mode_stat_t ending in /thermostat_connected with no otgw-pic/ segment. Verified by subscribing to homeassistant/climate/+/+/config and inspecting the payload. DHW Control climate's mode_stat_t is /dhw_enable (already generic, no change needed).
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Post-merge follow-up (commit 0331c0a1, 2026-04-25): TASK-410's brief named MQTTHaDiscovery.cpp:1038-1039 as the only discovery-side change. ADR-065's original call-site inventory also listed the climate composer (now MQTTHaDiscovery.cpp:2426) which prepended kPicSubtreePrefix to thermostat_connected for the Thermostat climate's mode_stat_t. That call-site was missed in the initial implementation and surfaced when the user explicitly asked whether the climate and DHW entities had been adapted. Fixed by replacing the hardcoded otgw-pic/ prefix with PSTR("/thermostat_connected") to match composeBinSensorPayload's new flag-0x00 output. DHW Control was already generic (mode_stat_t = /dhw_enable). AC #12 added to capture the runtime verification expectation. Lesson for future ADR amendments: when an ADR's call-site inventory enumerates discovery-side touches across multiple composers, copy that enumeration verbatim into the implementation task brief.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Firmware implementation complete.

Files modified:
- src/OTGW-firmware/MQTTstuff.ino: removed hardware-specific publish blocks for boiler_connected / thermostat_connected / otgw_connected / ot_online. Added TEMPORARY MIGRATION CODE block (lines 1233-1333) with six PROGMEM deprecated-topic constants, three helpers (mqttV2MigrationOnConnect, mqttV2MigrationHandleIfDeprecated, mqttV2MigrationTick) and v2MigrationBuildFullTopic. Wired the three call-sites: onConnect after homeassistant/status subscribe, handler as first post-debug-log action in handleMQTTcallback (before set/* dispatcher), tick in handleMQTT() after tickDiscoveryVerification(). Header comment references ADR-084 and TASK-409 with clear "when can this be removed" guidance.
- src/OTGW-firmware/MQTTHaDiscovery.cpp:1042-1043: flipped flag 0x08 to 0x00 on boiler_connected and thermostat_connected. Added ADR-084 anti-regression comment on lines 1038-1041.

Files NOT modified (intentional): OTGW-Core.ino. The task brief cited lines 3743/3750/3758 per ADR-065's inventory but grep showed those hold unrelated statements on this branch; the publishes were already centralized in MQTTstuff.ino.

Intentional deviation from plan: safeTimers.h DECLARE_TIMER_SEC expands to function-scoped static locals, which breaks cross-function RESTART_TIMER usage. Swapped to plain millis() diff with file-scope v2MigrationSubscribeMs and V2_MIGRATION_WINDOW_MS = 5000. Same behavior, rollover-safe via canonical (uint32_t)(now - then) < window pattern. Documented inline.

Verification:
- AC #1 grep for any sendMQTTData(Pic)? call-site publishing to the six deprecated leaves: no matches. Only the PROGMEM migration array and one unrelated doc comment (sendMQTTDataPic buffer-budget example) remain.
- AC #10: MQTT_HA_FLAG_IS_PIC_ENTRY still referenced 7 times across MQTTstuff.h, MQTTstuff.ino (filter sites at :1760, :1780), MQTTHaDiscovery.cpp (sensor/binsensor composers).
- AC #8: python build.py --firmware clean for both ESP8266 (0.77 MB) and ESP32-S3/OTGW32 (1.81 MB) targets. No new warnings.
- AC #9: python evaluate.py clean for the changed translation units. Pre-existing violations in unrelated files (debugStuff.ino, FSexplorer.ino, jsonStuff.ino, MQTTstuff.ino unrelated sections) are untouched by this change.
- AC #11: header comment block in place at MQTTstuff.ino:1233-1259 with complete TEMPORARY MIGRATION CODE rationale and removal guidance.

Not verified (requires hardware-in-the-loop):
- AC #3: HA discovery stat_t inspection by live broker subscription.
- AC #4: HA entity appearance on OTGW32 build.
- AC #5, #6: retained-ghost injection + telnet log confirmation.
- AC #7: no-ghost second-boot idempotency.
These require a live MQTT broker and physical boards; code paths are in place and the DebugTf text "V2 migration: cleared retained on %s" matches AC #6 literally.
<!-- SECTION:FINAL_SUMMARY:END -->
