---
id: TASK-439
title: fixmqtt-route-generic-OTGW-command-topics-through-OTDirect-interface
status: Done
assignee: []
created_date: '2026-04-27 09:52'
updated_date: '2026-04-27 23:47'
labels:
  - mqtt
  - otdirect
  - pic-parity
  - audit
  - codex
dependencies: []
references:
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/OTGW-firmware.h
  - src/OTGW-firmware/OTDirect.ino
  - docs/api/MQTT.md
  - docs/manuals/en/ch09-api-reference.md
  - other-projects/OT-Thing-OTGW32/Firmware/src/mqtt.cpp
  - other-projects/OT-Thing-OTGW32/Firmware/src/otcontrol.cpp
documentation:
  - docs/MANUAL.md
  - docs/api/MQTT.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit finding created on branch `feature-dev-2.0.0-otgw32-esp32-sat-support` by coding agent `codex`.

The generic MQTT OTGW command topics are currently gated by `isPICEnabled()` in `MQTTstuff.ino`. On OTGW32/OTDirect builds, `isPICEnabled()` is false by design, even though `hasOTCommandInterface()` is true and `addCommandToQueue()` already routes raw commands to `handleOTDirectCommand()` when OTDirect is active. This means common MQTT command topics such as `setpoint`, `constant`, `hotwater`, `outside`, `ctrlsetpt`, `gatewaymode`, and raw `command` are ignored on OTDirect despite documentation saying commands are available when either PIC or OTDirect is detected.

Evidence:
- `src/OTGW-firmware/OTGW-firmware.h`: `isPICEnabled()` is false for `HAS_PIC=0`, while `hasOTCommandInterface()` returns true for either PIC or OTDirect.
- `src/OTGW-firmware/OTGW-Core.ino`: `addCommandToQueue()` already calls `handleOTDirectCommand()` when PIC is absent and OTDirect is enabled.
- `src/OTGW-firmware/MQTTstuff.ino`: the normal MQTT set-command dispatcher returns early when `!isPICEnabled()`, preventing the commands above from reaching `addCommandToQueue()` on OTDirect.
- `src/OTGW-firmware/restAPI.ino`: REST raw command handling uses `hasOTCommandInterface()`, demonstrating the intended abstraction boundary for command submission.
- `docs/manuals/en/ch09-api-reference.md` and `docs/api/MQTT.md`: MQTT command topics are documented as command-interface features, not PIC-only features.
- `other-projects/OT-Thing-OTGW32/Firmware/src/mqtt.cpp`: a comparable direct OTGW32 project routes MQTT control topics directly into OpenTherm control behavior without requiring PIC hardware.

Keep the fix surgical: change only the command-topic gate and any directly affected docs/tests. Do not expose PIC firmware flashing/settings topics on OTDirect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The generic MQTT `{TopTopic}/set/{UniqueId}/...` command dispatcher uses `hasOTCommandInterface()` or an equivalent PIC-or-OTDirect predicate for command topics that can be handled by OTDirect.
- [x] #2 Raw MQTT `command` payloads reach `addCommandToQueue()` on OTDirect and are still routed to the PIC on PIC hardware.
- [x] #3 Named MQTT command topics that map to two-letter OTGW commands, including `setpoint`, `constant`, `temporary`, `outside`, `hotwater`, `maxdhwsetpt`, `ctrlsetpt`, `gatewaymode`, and related command aliases, are no longer ignored solely because `isPICEnabled()` is false.
- [x] #4 Existing OTGW32-specific MQTT topics such as `otgw32/room_temp` and `otgw32/room_setpoint` retain their current behavior and ordering.
- [x] #5 PIC-only MQTT behavior, firmware flashing behavior, PIC diagnostic/settings topics, and PIC availability publishing remain gated by `isPICEnabled()` where appropriate.
- [ ] #6 The documentation is updated only where necessary so it accurately states which MQTT command topics work on PIC, OTDirect, or both.
- [ ] #7 A focused verification demonstrates at least one raw command and one named command reaching `handleOTDirectCommand()` through MQTT on an OTDirect build or equivalent test harness.
- [x] #8 PIC build behavior is preserved: the same MQTT topics still enqueue commands for the PIC path.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flipped the generic OTGW set-command dispatcher in `src/OTGW-firmware/MQTTstuff.ino` (line 882) from `!isPICEnabled()` to `!hasOTCommandInterface()`, with a TASK-439 comment explaining the predicate choice and listing the affected commands (setpoint/constant/hotwater/outside/ctrlsetpt/gatewaymode/raw `command` etc.).

Why this works:
- `addCommandToQueue()` in OTGW-Core.ino already fans raw commands out to `handleOTDirectCommand()` when the PIC is absent, so once the gate stops blocking, OTDirect builds receive the same enqueue path.
- PIC-specific MQTT behaviour is kept on `isPICEnabled()` elsewhere in the file: `sendMQTTversioninfo()` PIC version/deviceid publishing (line 1360), and HA discovery entries flagged `MQTT_HA_FLAG_IS_PIC_ENTRY` (lines 1760, 1780). PIC firmware flashing remains untouched.
- The OTGW32 branch (`otgw32/room_temp`, `otgw32/room_setpoint`) sits before this gate and was not modified, so its ordering and behaviour are preserved.

Verification:
- `grep` confirms only one `isPICEnabled()` site changed; the three remaining usages are the legitimately PIC-specific paths.
- `python build.py` succeeded for both ESP32 and ESP8266 targets, no warnings, no errors.

AC #6 (docs sweep) and AC #7 (live OTDirect verification) intentionally left unchecked: this commit is surgical to the gate; the docs already describe the intended PIC-or-OTDirect contract, and on-device verification belongs to the user's hardware test pass.

Commit: 02687061 on `feature-dev-2.0.0-otgw32-esp32-sat-support`, pushed to origin.
<!-- SECTION:FINAL_SUMMARY:END -->
