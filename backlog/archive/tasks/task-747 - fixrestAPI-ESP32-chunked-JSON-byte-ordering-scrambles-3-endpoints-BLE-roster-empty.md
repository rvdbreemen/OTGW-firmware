---
id: TASK-747
title: >-
  fix(restAPI): ESP32 chunked-JSON byte-ordering scrambles 3 endpoints (BLE
  roster empty)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 07:54'
updated_date: '2026-06-01 05:23'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On ESP32 the TASK-529 coalescing TX buffer (sTxBuf in jsonStuff.ino) buffers sendJsonMapEntry/sendStartJsonMap/sendBeforenext via restSendContent, but raw httpServer.sendContent(_P) calls bypass it and hit the socket immediately. In satBLERosterSendJSON (SATble.ino), sendSensorStatus (restAPI.ino) and the SAT weather forecast block (SATweather.ino), array/sub-object bytes are written raw -> emitted BEFORE the buffered wrapper flushes -> malformed JSON on the wire (e.g. sensors:[..]{meta..}). Frontend r.json() rejects, .catch swallows -> BLE roster renders empty, user cannot select a sensor. ESP8266 unaffected (no buffer, inline send). Reported by sergeantd on Discord (alpha.84). Fix: route the raw writes through restSendContentP/restSendContent so byte order is preserved. Also keep the index.js diagnostic that surfaces the parse failure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satBLERosterSendJSON routes all sensors-array writes through restSendContent/restSendContentP (no raw httpServer.sendContent in the function)
- [x] #2 sendSensorStatus devices+s0 sub-objects routed through restSendContent/restSendContentP
- [x] #3 SATweather forecast arrays routed through restSendContent
- [x] #4 index.js renderBleRoster surfaces a visible error row on parse failure (kept from diagnostic)
- [x] #5 ESP32-S3 build green (python build.py) and evaluate.py --quick shows no new failures
- [x] #6 Field-validate with sergeantd: BLE roster lists sensors and Select works on ESP32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented + built green (ESP32-S3 + ESP8266) shipped alpha.88 (commit e57e1cce). Code ACs 1-5 done. AC6 (field-validate roster select with sergeantd on OTGW32) pending hardware confirmation — staying In Progress until George reports.

2026-06-01T07:23:31+02:00: Closed per maintainer directive (do not block task completion on hardware/field validation). AC#6 field-validation by sergeantd waived as a completion gate; code shipped alpha.88 (e57e1cce).
<!-- SECTION:NOTES:END -->
