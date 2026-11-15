---
id: TASK-1965
title: >-
  REST /otgw/label and /otgw/messages: status data_invalid while the served
  value comes from the gateway (PIC outside sensor)
status: To Do
assignee: []
created_date: '2026-11-15T10:12:00Z'
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
- [ ] #1 The client can tell whether the value is valid and where it comes from: status follows the served value, or a new field source (boiler/gateway); smallest contract change, the choice recorded in the task
- [ ] #2 Host test on the real sendOTValue/sendOTLabel code: boiler DATA_INVALID plus a gateway substitute is red on the old code and green on the fix; the TASK-1871 cases stay green
- [ ] #3 openapi.yaml and docs/api/README.md updated; evaluate.py --quick no new FAIL; esp32-combo build
- [ ] #4 Field confirmation by indigo_light
<!-- AC:END -->
