---
id: TASK-883
title: >-
  fix(async): mitigate Task Watchdog reboot under heavy concurrent REST load
  (TASK-867 AC#8 / TASK-879)
status: To Do
assignee: []
created_date: '2026-06-18 09:24'
updated_date: '2026-06-18 12:16'
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
- [x] #4 A/B causation check: flash pre-migration build (b54c0890~1), run identical 8-worker test_load.py; record whether the TWDT threshold differs from the migrated build (proves/refutes migration involvement). Until done, the migration is NOT implicated.
<!-- AC:END -->



## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
A/B HARDWARE RESULT (same-session, same-conditions, 192.168.88.39). Arm A = 0a3e5eee alpha.204 (pre-migration, hand-rolled JSON, OLD partition table, littlefs 0x1f0000). Arm B = f27030c alpha.211 (full ArduinoJson migration, NEW partition table, littlefs 0x270000). BOTH fresh-flashed via per-region no-erase esptool (NVS/WiFi preserved; LittleFS wiped on both -> neither provisioned MQTT -> equal heap baseline, fair A/B). test_load.py params identical per arm.
REALISTIC (4 workers @ --delay 1.0, 3 min): Arm A PASS (553 req 100%, 0 reboot); Arm B PASS (491 req 100%, 0 reboot). Real-world dashboard load is fine on BOTH builds.
FLOOD (8 workers, --delay 0): Arm A survived 1 min AND 2 min, and even survived a 16-worker/2-min flood, with ZERO reboots (graceful degradation: timeouts/low success, but bootcount never moved, uptime monotonic). Arm B REBOOTED TWICE under the 8-worker/1-min flood (bootcount 1->3, uptime reset).
VERDICT: the ArduinoJson v7 migration MEASURABLY LOWERED the flood/abuse TWDT threshold. This OVERTURNS the earlier 'causation unmeasured / pre-existing only' framing and the static CPU-delta prediction (which expected 'marginal, both arms behave the same'). The migration IS implicated for the flood case. Mechanism: two-pass build-then-serialize JsonDocument + heap-pool alloc/free churn holds core 1 (async_tcp pri 10) longer per request -> starves loopTask (pri 1, core 1) past the 30s TWDT under sustained 8-way concurrency. Heap axis itself stayed clean on both (no leak, 0 ADR-089 tier entries, frag recovers). TASK-879's web-dead TWDT (alpha.199) was a DISTINCT pre-migration manifestation; what the migration shifted is the flood threshold. AC#1 (8-worker survive) now has a concrete repro + a confirmed pre-migration baseline. Recommended fix #1 (async_tcp -> core 0, amend ADR-139) would address BOTH the pre-existing web-dead TWDT and this migration-worsened flood case. Note: test_load.py hardened to retry the cooldown snapshot and use bootcount (not a failed snapshot) as the only reboot signal (the 16w arm-A run initially false-positived 'rebooted' from a saturated snapshot).
<!-- SECTION:NOTES:END -->
