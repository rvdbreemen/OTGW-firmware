---
id: TASK-1190
title: 'Announce the DHW water total just in time, as on the 1.x line'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-01 18:26'
updated_date: '2026-10-01 18:26'
labels:
  - 2.0.0
  - mqtt
  - water-meter
dependencies: []
priority: medium
ordinal: 312000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer decision 2026-10-01 (asked while closing TASK-1123's open questions): announce dhw_water_total (faux id 241) only once a MsgID 19 sample was taken on this boot, as the 1.x line does, instead of at boot (ADR-176 Must #7). After ADR-181 a PIC gateway in PS=1 mode never samples, so a boot announce left it with a permanently unknown water sensor in Home Assistant.

Design: queueNonOTDiscoveryIds() queues 241 only when dhwWaterMeterHasData(); markAllMQTTConfigPending() skips 241 in its table walk so the helper alone decides; publishDHWWaterMeter() queues 241 while it is not yet done (the 1.x pattern), behind an MQTT-enabled check. Needs an ADR amending ADR-176 Must #7.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An ADR, authored through adr-kit and Accepted by the maintainer, amends ADR-176 Must #7: 241 is announced just in time, after the first MsgID 19 sample of a boot
- [ ] #2 Without a sample on this boot, neither the boot path nor markAll queues 241; after a sample the 60 s publish queues it once, both republish paths queue it again, and a done config is not re-queued every minute
- [ ] #3 Old-vs-fix on test/host/test_dhw_water_meter.py (discovery cases): the old code fails exactly the flipped cases (D4, D5, D6, D7), the fix passes all, and every 241 mutant is re-targeted with a stated reason
- [ ] #4 docs/api/MQTT.md, c4-code-mqtt.md and c4-code-otgw-core.md describe the just-in-time announce, and the 'Announce timing' difference from 1.x is gone
- [ ] #5 Prerelease bump; build.bat green with fresh binaries; python evaluate.py exit 0; the host-test files green
<!-- AC:END -->
