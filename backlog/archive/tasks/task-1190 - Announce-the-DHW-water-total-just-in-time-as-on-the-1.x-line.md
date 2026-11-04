---
id: TASK-1190
title: 'Announce the DHW water total just in time, as on the 1.x line'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-01 18:26'
updated_date: '2026-10-01 21:42'
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
- [x] #1 An ADR, authored through adr-kit and Accepted by the maintainer, amends ADR-176 Must #7: 241 is announced just in time, after the first MsgID 19 sample of a boot
- [x] #2 Without a sample on this boot, neither the boot path nor markAll queues 241; after a sample the 60 s publish queues it once, both republish paths queue it again, and a done config is not re-queued every minute
- [x] #3 Old-vs-fix on test/host/test_dhw_water_meter.py (discovery cases): the old code fails exactly the flipped cases (D4, D5, D6, D7), the fix passes all, and every 241 mutant is re-targeted with a stated reason
- [x] #4 docs/api/MQTT.md, c4-code-mqtt.md and c4-code-otgw-core.md describe the just-in-time announce, and the 'Announce timing' difference from 1.x is gone
- [x] #5 Prerelease bump; build.bat green with fresh binaries; python evaluate.py exit 0; the host-test files green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-01, commit 0fdb1e594 (alpha.397).

AC#1 (ADR-182):
- Written through adr-kit and related to ADR-176 on both sides.
- adr-lint: every gate PASS.
- Accepted by the maintainer in this session after the acceptance packet.
- It amends ADR-176's boot-publish clause (line 167).

AC#2 (src/OTGW-firmware/MQTTstuff.ino):
- queueNonOTDiscoveryIds(): 'if (dhwWaterMeterHasData()) setMQTTConfigPending(OTGWdhwmeterid);'.
- markAllMQTTConfigPending(): its table walk skips 241, so the helper alone decides.
- publishDHWWaterMeter(): returns while MQTT is disabled, waits for a sample, then queues 241 while getMQTTConfigDone(241) is false, and sends the state.

AC#3 (test/host/test_dhw_water_meter.py, discovery cases on the real code, sliced by anchor):
- Red: the new expectations against the unchanged firmware fail exactly D4, D5, D6 and D7:
  - D4: pending(241)=1 at boot without a sample;
  - D5: queued by markAll without a sample;
  - D6: a restored total alone announced;
  - D7: the 60 s publish after a sample did not queue 241.
- New case D12 (a published config is not queued again by the 60 s publish) passes on both sides; mutant M16 shows it bites.
- Green: every case passes, RESULT PASS, exit 0.
- 241 mutants re-targeted to the new code:
  - M4 (boot announce restored): fails D4, D5, D6;
  - M5 (walk skip removed): fails D5;
  - M15 (no just-in-time announce): fails D7;
  - M16 (re-queue although done): fails D12;
  - M17 (state gate removed): fails D6, plus D4 and X3;
  - M27 (MQTT check removed): fails D9.
- The --old-rev mode against 4bbceed61^ (before the meter existed) still passes: OLD fails exactly W1, W4, W5; 123 checks yes.

AC#4 (docs):
- docs/api/MQTT.md:
  - 'When it is published' now describes the just-in-time announce, that Home Assistant can drop a first value sent ahead of the config, and that a reset value sent before the announce is ignored;
  - the faux id 241 discovery section is rewritten, including that a retained config from an earlier alpha stays until it is removed in Home Assistant;
  - the 'Announce timing' difference from 1.x is removed.
- c4-code-mqtt.md (markAll and publishDHWWaterMeter entries) and c4-code-otgw-core.md (dhwWaterMeterHasData) updated.
- CHANGELOG [Unreleased]: the TASK-1123 entry now says just in time.
- ADR index: ADR-182 entry, and ADR-176 marked as amended.

AC#5:
- bin/bump-prerelease.sh alpha.396 -> alpha.397 in the same commit.
- build.bat --target all:
  - esp32, esp32-classic and esp32-combo each SUCCESS for firmware and filesystem;
  - 'Build completed successfully!', exit 0;
  - images OTGW-firmware-{esp32-otgw32,esp32-classic,esp32-combo}-2.0.0-alpha.397+0fdb1e5 .ino.bin / .littlefs.bin, written 23:23:59-23:29:57 after the 23:20:59 start.
- python evaluate.py: exit 0, 81 passed / 4 warnings / 0 failed; the ADR-171 'Non-OT Discovery Single Source' gate accepts the conditional entry.
- Host tests: all 19 host-test files pass after the alpha.397 build (test/host x10, tests x6, scripts/tests extract_json_field, heap_soak_driver, run_coverage_test). The live-device probes in scripts/tests were left out (they need a gateway).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Announces dhw_water_total (faux id 241) just in time, after the first MsgID 19 sample of a boot, as on the 1.x line. A gateway that never samples MsgID 19, including a PIC gateway in PS=1 mode since ADR-181, no longer shows a permanently unknown water sensor. ADR-182 (Accepted) amends ADR-176's boot-publish clause.

What changed in MQTTstuff.ino:
- queueNonOTDiscoveryIds() gates 241 on dhwWaterMeterHasData();
- the markAllMQTTConfigPending() walk skips 241;
- publishDHWWaterMeter() queues 241 while it is not done, behind the MQTT check.

Commit 0fdb1e594, alpha.397.

Evidence:
- Red: the old code fails exactly D4, D5, D6 and D7.
- Green: all cases pass; D12 guards against a re-queue every minute.
- Mutants M4, M5, M15, M16, M17 and M27 each fail their case; the old-rev mode passes.
- build.bat --target all [SUCCESS] x3, alpha.397+0fdb1e5.
- evaluate.py exit 0 (81/4/0); the ADR-171 gate passes.
- 19/19 host-test files pass.

Known effect: an entity announced by a 2.0.0 alpha before this change stays in Home Assistant until it is removed there.
<!-- SECTION:FINAL_SUMMARY:END -->
