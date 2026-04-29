---
id: TASK-483
title: 'fix(webui): apply ADR-066 master-topic filter to log decode and REST state'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-29 22:20'
labels:
  - webui
  - ADR-066
  - follow-up
  - rest-api
  - ot-log
  - beta.4
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port van TASK-481 (feature branch commit c694fbdf) naar dev / 1.5.0-beta.4. TASK-478 fixed Tier 4 (MQTT base topic); deze task fixt Tier 1 (log decode) en Tier 2 (REST state via OTcurrentSystemState). Zelfde edit in print_f88/s16/s8s8/u16: validForMaster cache, gate AddLogf en state-write.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 print_f88, print_s16, print_s8s8, print_u16 each compute validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem) once per call
- [ ] #2 AddLogf with decoded value is gated on validForMaster; non-valid messages log label only
- [ ] #3 State-write is gated on validForMaster
- [ ] #4 Tier 3 (publishToSourceTopic) and Tier 4 (sendMQTTData base topic) call sites unchanged
- [ ] #5 evaluate.py passes (no new violations beyond pre-existing baseline)
- [ ] #6 ESP8266 build clean (python build.py --firmware)
- [ ] #7 Hardware verification deferred to tester: WebUI stats stable, OT-log shows one decoded value per WRITE-pair
<!-- AC:END -->
