---
id: TASK-883
title: >-
  fix(async): mitigate Task Watchdog reboot under heavy concurrent REST load
  (TASK-867 AC#8 / TASK-879)
status: To Do
assignee: []
created_date: '2026-06-18 09:24'
updated_date: '2026-06-18 09:34'
labels: []
dependencies: []
ordinal: 99000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
scripts/tests/test_load.py (the AC#8 heap-under-load tool) surfaced a real edge: 8 concurrent UNTHROTTLED clients hammering the heaviest ArduinoJson endpoints (settings ~8.6KB, debug ~7.6KB, sat/status 132 fields) reboot the OTGW32 with lastreset='Task watchdog' (heap healthy ~94KB, no panic/crashlog -> NOT heap exhaustion, it is CPU starvation). Root: AsyncTCP service task pinned to core 1 (CONFIG_ASYNC_TCP_RUNNING_CORE=1, ADR-139), same core as the Arduino loop; under saturation the async_tcp task hogs core 1 and starves idle1 -> IDF Task Watchdog fires. This is the SAME TWDT mechanism as TASK-879, which was observed at alpha.199 BEFORE the ArduinoJson migration -> the failure mode is pre-existing, not introduced by the migration. Whether the migration shifted the threshold (heavier per-request CPU: build+serialize a JsonDocument vs the old small-buffer stream) is UNMEASURED; the only honest discriminator is an A/B run: flash the pre-migration build (b54c0890~1) and run the identical 8-worker test. Do NOT claim the migration caused or worsened this without that A/B. Realistic load is fine: at <=4 concurrent workers (870 req/3min) the device serves 99.4%, heap fully recovers (no leak), fragmentation peaks then recovers, no ADR-089 tier breach, zero reboots. Also note the test itself is near-DoS (unthrottled N-worker flood); a throttled realistic-rate run should gauge real-world urgency. Repro: python scripts/tests/test_load.py --workers 8 --minutes 1 --host <ip>.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Device survives the 8-worker test_load.py run (>=1 min) with bootcount delta 0 (no TWDT reboot)
- [ ] #2 Mitigation does not regress single-request latency or the <=4-worker heap-recovery behaviour
- [ ] #3 evaluate.py green; esp32 build + flash-fit
- [ ] #4 A/B causation check: flash pre-migration build (b54c0890~1), run identical 8-worker test_load.py; record whether the TWDT threshold differs from the migrated build (proves/refutes migration involvement). Until done, the migration is NOT implicated.
<!-- AC:END -->
