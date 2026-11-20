---
id: TASK-1965
title: >-
  REST /otgw/label and /otgw/messages: status data_invalid while the served
  value comes from the gateway (PIC outside sensor)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T10:12:00Z'
updated_date: '2026-11-20T10:23:00Z'
labels:
  - rest
  - bug
  - beta-feedback
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 1005000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Beta feedback, reporter indigo_light (Discord 301077214374658048), #beta-testing https://discord.com/channels/812969634638725140/914498730001072149/1571465125280350249; Robert: 'Nee gaan we fixen' (reply 1571466554011553823, relayed by the coordinator). beta.250, Classic + S3 Mini, PIC 6.8, Vitodens 100-W, DS18B20 on the PIC, boiler without MsgID 27. GET /api/v2/otgw/label/Toutside returns value 6.31 (the PIC sensor, which SAT uses as outside_temp) together with status data_invalid. Cause (coordinator, from code): sendOTValue/sendOTLabel (restAPI.ino ~2885/~2922) emit status = boilerAnswerStatus(id), the last BOILER answer (TASK-1871), and value = getOTGWValue(), what the gateway serves; with a PIC substitute the two disagree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The client can tell whether the value is valid and where it comes from: status follows the served value, or a new field source (boiler/gateway); smallest contract change, the choice recorded in the task
- [x] #2 Host test on the real sendOTValue/sendOTLabel code: boiler DATA_INVALID plus a gateway substitute is red on the old code and green on the fix; the TASK-1871 cases stay green
- [x] #3 openapi.yaml and docs/api/README.md updated; evaluate.py --quick no new FAIL; esp32-combo build
- [x] #4 Field confirmation by indigo_light
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 choice for AC1 (smallest contract change): keep status as it is (the boiler's own last answer, TASK-1871; clients that read it keep their meaning) and add one field, source: none / boiler / gateway / thermostat / summary / replay, the writer of the value getOTGWValue() serves. Rejected: let status follow the served value (changes the meaning of an existing field for every client, and loses the 'boiler does not support this ID' signal indigo's case needs). Implementation: OTGW-Core.ino boiler-answer block gains otValueSource[], recordValueSource(), otValueSourceOfFrame(rsptype, bLocalAnswer, bReplayed), valueSourceName(); recorded after decodeAndPublishOTValue() under the same store gate the print_* functions use (is_value_valid && is_value_valid_for_master_topic), in the PS=1 summary store (summary) and in OT-Direct OT= (OTDirect.ino, gateway); enum OT_VALSRC_* and prototypes in OTGW-firmware.h; both REST senders emit source.

Evidence: tests/test_ot_value_source.py (REAL block sliced by marker + wiring on the source text): fix 9/9 OK, indigo case 'id=27 status=data_invalid source=gateway'; against origin's sources (OTGW_SRC_DIR, temp copy) FAILED (failures=3, errors=1): the block has no value source, no wiring. tests/test_boiler_answer_status.py (TASK-1871): its harness now also takes the two enums the block uses; 11/11 OK, every 1871 expectation unchanged. Related: test_boiler_support_bitmap_b_only, test_sat_rest_contract_1496, test_rest_* PASS. openapi.yaml: source on /v2/otgw/messages/{id}; /v2/otgw/label/{msglabel} gained status, last_seen_age_s (missing before) and source; yaml parses. docs/api/README.md updated. evaluate --quick 0 FAIL. Build and field confirmation (AC4) pending.

2026-11-15 bench (09 on .63, ffd0a64): the source field is present on otgw/messages and otgw/label; without a PIC outside sensor and without an OT bus it reads none, as expected. The gateway case (PIC DS18B20, boiler without MsgID 27) is the field AC4 with indigo_light.

indigo_light capture 2026-11-15 10:50-13:26 (beta.250+aed38bf, PIC gateway 6.8, Vitodens + Honeywell Round, sent to Robert by DM, kept local in session 95 scratchpad indigo1115/): 28020 frames. The Round never reads MsgID 27 (no T 27 frame at all), so the capture cannot show how the PIC answers 27 to a thermostat; that last check stays open. What it does show: the PIC writes the DS18B20 outside temperature to the boiler itself, R WRITE-DATA 27 3020 times (~19/min, values 6.19-8.8 C), and the boiler answers Data-Invalid every time (B 601B0000); the firmware sent no OT= (no OT= in the telnet command log). Source: PIC 6.6 gateway.asm MessageTable :1820-1830 lists MSG_OUTSIDETEMP among the alternatives, MessageID27 :3050-3067 turns the request into T_WRITE with outside1/outside2 when OutsideTemp is set (6.8 source not in the repo; 6.6 checked).

2026-11-16 (95) AC3: commit 2e49979dc updates docs/api/openapi.yaml (+22 lines, source field with boiler/gateway/thermostat/summary/replay/none, the 'data_invalid with source gateway' example at :1631-1638) and docs/api/README.md (:429-434); on origin since beta.339. evaluate --quick 0 failed today on the 2.0.0-beta.344 lane; esp32-combo build 2.0.0-beta.344+873f06b exit 0, 'Build completed successfully', contains it (09 also benched ffd0a64 on .63: the source field is present). AC4 stays open: field confirmation by indigo_light on beta.339 or later.

2026-11-20 (ba, coordinator decision): AC4 via a tester ask. 92 posted in #beta-testing, message 1573143895154626665: indigo_light runs GET /api/v2/otgw/label/Toutside on beta.339 or newer; expect status data_invalid + source gateway. Field archive checked first: no upload holds it (the capture sweep excludes otgw/label/*; device-state has no source field).

2026-11-20 milestone 2.0.0 -> 2.1.0 (otgw-docs, coordinator's assignment). Robert DM 2026-11-20 01:15 (1573144814600462457), relayed by the coordinator: "C — field/hardware ACs outside the 2.0.0 scope, to 2.1.0". Status, assignee and ACs unchanged.

2026-11-20 AC4 field confirmation (relayed by 92, BlueFlame): indigo_light, #beta-testing 2026-11-20 08:45 UTC, msg 1573257919783501948 (reply to 1573143895154626665), firmware beta.392, verbatim: GET /api/v2/otgw/label/Toutside -> {"label":"Toutside","value":8.88,"unit":"°C","status":"data_invalid","last_seen_age_s":0,"source":"gateway"}: the boiler answers MsgID 27 DATA_INVALID and the served value is the gateway substitute, exactly the expected status data_invalid + source gateway. Archived: OTGW-field-data/indigo_light/2026-11-19_2.0.0-beta.382+224e30c/macos-20261119-091712/_analysis/tester-answers.txt (checked present, line 1).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: status keeps following the boiler's verdict and a new field source (boiler/gateway) tells where the served value comes from (smallest contract change, recorded in the notes). AC2: host test on the real sendOTValue/sendOTLabel code red on the old code, green on the fix; TASK-1871 cases green (notes). AC3: openapi.yaml and docs/api/README.md updated, evaluate --quick no new FAIL, esp32-combo build (notes). AC4: field confirmation by indigo_light on beta.392, 2026-11-20 08:45 UTC, msg 1573257919783501948: Toutside status data_invalid, source gateway; archived in OTGW-field-data (tester-answers.txt).
<!-- SECTION:FINAL_SUMMARY:END -->
