---
id: TASK-696
title: >-
  fix(esp32-port): trim flash by ~4 KB so PR #641 fits in the OTGW32 app
  partition
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 09:35'
updated_date: '2026-05-24 09:35'
labels:
  - fix
  - port-from-dev
  - esp32
  - flash-budget
dependencies: []
priority: high
ordinal: 64000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PR #641 ESP32 build fails because the ported feature set pushes the firmware to 1,969,847 B vs the 1,966,080 B (1.875 MB) app partition limit on OTGW32 ESP32-S3. Overrun = 3,767 B.\n\nThree targeted shrinks (low risk):\n1. REST endpoint format strings: drop the dual first/non-first PSTR variants; emit the leading comma via sendContent(F(",")) outside the snprintf_P call. ~200 B saved per endpoint x 2 endpoints + the inner duplicate label format = ~600 B.\n2. JSON persistence file keys: shorten 'sent_read'/'sent_write'/'acked_read'/'acked_write'/'unsupported_read'/'unsupported_write' to 'sr'/'sw'/'ar'/'aw'/'ur'/'uw'. Affects both reader (key patterns in OTGW-Core.ino loadOtSupportFiles) and writer (writeOtThermoFile / writeOtBoilerFile). ~600 B saved across the 6 format strings + the matching reader keys.\n3. MQTT CSV: combine the two PSTR format variants 'pos==0 ? PSTR("%dR") : PSTR(",%dR")' into a single 'PSTR("%s%dR")' with a sep-tracking variable. Minor (~50 B) but easy.\n\nNo behaviour change beyond the file format keys, which only the firmware reads/writes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 REST handlers /api/v2/otgw/boiler-support and /api/v2/otgw/ot-support use a single snprintf_P format per row; the leading comma is emitted via sendContent before the row when needed.
- [ ] #2 Persistence file format uses short keys (sr/sw/ar/aw/ur/uw). Both the reader (fileFindToken target strings) and writer (writeOt*File format strings) updated to match. Magic 'v':1 unchanged.
- [ ] #3 MQTT publishBoilerUnsupportedMsgids() uses a single format string with a sep-prepend pattern.
- [ ] #4 python build.py --firmware --target esp8266 exits 0 (local check).
- [ ] #5 CI pio run -e esp32 on PR #641 passes (firmware <1,966,080 B).
<!-- AC:END -->
