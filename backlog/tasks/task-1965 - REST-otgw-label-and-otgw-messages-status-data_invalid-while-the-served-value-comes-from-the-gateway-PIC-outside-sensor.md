---
id: TASK-1965
title: >-
  REST /otgw/label and /otgw/messages: status data_invalid while the served
  value comes from the gateway (PIC outside sensor)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T10:12:00Z'
updated_date: '2026-11-15T12:31:00Z'
labels:
  - rest
  - bug
  - beta-feedback
milestone: 2.0.0
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
- [ ] #3 openapi.yaml and docs/api/README.md updated; evaluate.py --quick no new FAIL; esp32-combo build
- [ ] #4 Field confirmation by indigo_light
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 choice for AC1 (smallest contract change): keep status as it is (the boiler's own last answer, TASK-1871; clients that read it keep their meaning) and add one field, source: none / boiler / gateway / thermostat / summary / replay, the writer of the value getOTGWValue() serves. Rejected: let status follow the served value (changes the meaning of an existing field for every client, and loses the 'boiler does not support this ID' signal indigo's case needs). Implementation: OTGW-Core.ino boiler-answer block gains otValueSource[], recordValueSource(), otValueSourceOfFrame(rsptype, bLocalAnswer, bReplayed), valueSourceName(); recorded after decodeAndPublishOTValue() under the same store gate the print_* functions use (is_value_valid && is_value_valid_for_master_topic), in the PS=1 summary store (summary) and in OT-Direct OT= (OTDirect.ino, gateway); enum OT_VALSRC_* and prototypes in OTGW-firmware.h; both REST senders emit source.

Evidence: tests/test_ot_value_source.py (REAL block sliced by marker + wiring on the source text): fix 9/9 OK, indigo case 'id=27 status=data_invalid source=gateway'; against origin's sources (OTGW_SRC_DIR, temp copy) FAILED (failures=3, errors=1): the block has no value source, no wiring. tests/test_boiler_answer_status.py (TASK-1871): its harness now also takes the two enums the block uses; 11/11 OK, every 1871 expectation unchanged. Related: test_boiler_support_bitmap_b_only, test_sat_rest_contract_1496, test_rest_* PASS. openapi.yaml: source on /v2/otgw/messages/{id}; /v2/otgw/label/{msglabel} gained status, last_seen_age_s (missing before) and source; yaml parses. docs/api/README.md updated. evaluate --quick 0 FAIL. Build and field confirmation (AC4) pending.

2026-11-15 bench (09 on .63, ffd0a64): the source field is present on otgw/messages and otgw/label; without a PIC outside sensor and without an OT bus it reads none, as expected. The gateway case (PIC DS18B20, boiler without MsgID 27) is the field AC4 with indigo_light.
<!-- SECTION:NOTES:END -->
