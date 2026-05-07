---
id: TASK-567
title: 'fix(sat): emit null instead of nan in /api/v2/sat/status JSON'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 19:21'
updated_date: '2026-05-07 21:56'
labels:
  - sat
  - rest-api
  - json
  - 2.0.0
dependencies: []
priority: high
ordinal: 30000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
satSendJsonFloat() at SATcontrol.ino:1610 emits float values via dtostrf() without checking isnan/isinf. When room_temp / outside_temp / etc. are NaN (e.g. SergeantD's cold device with no thermostat / no BLE temp / no MQTT temp), the JSON payload contains the literal token nan which is not valid JSON. Result: client-side JSON.parse() in data/sat.js:156 throws SyntaxError, the SAT page can't render any data, the live UI looks frozen. Field evidence: SergeantD's alpha.8 browser console (2026-05-07) shows repeated [SAT] fetch error: SyntaxError: Unexpected token 'a', ...m_temp: nan, ou... — every fetch fails. Fix: in satSendJsonFloat, if isnan(fValue) || isinf(fValue), emit the literal null token instead of dtostrf output. ~5 line change, no behaviour change for finite values.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satSendJsonFloat() in src/OTGW-firmware/SATcontrol.ino emits the literal JSON null token (not 'null' as a string) when fValue is NaN or Inf; finite values continue to emit via dtostrf with the requested precision unchanged
- [x] #2 GET /api/v2/sat/status response is parseable by JSON.parse() in all states (cold device with no temp source, normal operation, edge transitions). Verified via curl + jq on the running device
- [x] #3 data/sat.js:156 [SAT] fetch error console messages disappear in the same scenario that produced them on alpha.8
- [x] #4 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [x] #5 python3 evaluate.py --quick — no new failures
- [x] #6 Prerelease bump committed alongside the firmware change (alpha.11 -> alpha.12) per project versioning policy
- [ ] #7 Field validation on alpha.12+: SergeantD or another tester confirms the SAT page no longer shows fetch errors with no thermostat / no BLE source
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. satSendJsonFloat NaN/Inf -> null; 2. verify /api/v2/sat/status JSON.parse; 3. fetch error gone; 4/5/6/7 verify.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped as alpha.12. satSendJsonFloat now emits the literal JSON null token when fValue is NaN or Inf, while finite values continue through dtostrf with requested precision. /api/v2/sat/status verified parseable by JSON.parse() in cold-device / no-source / normal / edge-transition states. data/sat.js [SAT] fetch errors gone in the same scenario. ESP8266 + ESP32-S3 builds exit 0; evaluator clean baseline; alpha.11 -> alpha.12 prerelease bump committed. AC #7 (SergeantD confirms SAT page no longer shows fetch errors with no thermostat / no BLE source) gated on field validation.
<!-- SECTION:FINAL_SUMMARY:END -->
