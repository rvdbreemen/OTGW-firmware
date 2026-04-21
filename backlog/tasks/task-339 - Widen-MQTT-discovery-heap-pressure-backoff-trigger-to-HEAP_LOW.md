---
id: TASK-339
title: Widen MQTT discovery heap-pressure backoff trigger to HEAP_LOW
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-19 21:04'
updated_date: '2026-04-19 21:13'
labels:
  - mqtt
  - heap
  - discovery
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Change the adaptive drip backoff trigger from HEAP_WARNING (<4KB) to HEAP_LOW (<6KB) so the slow-drip mode kicks in BEFORE the throttle gate starts dropping messages.

**Background**
In loopMQTTDiscovery() the drip interval switches from 1s to 10s when the heap reaches WARNING level (<4096 bytes). But the publish throttle-gate (canPublishMQTT) starts dropping messages already at HEAP_LOW (<6144 bytes). This means drops begin BEFORE the drip backs off — by the time the slow-mode triggers, the heap is already in WARNING territory.

Moving the trigger to HEAP_LOW (<6KB) means drip throttles itself BEFORE the publish gate engages.

**Considerations**
- Pro: one-line change (MQTTstuff.ino:980)
- Pro: prevents the system ever reaching WARNING band under normal load
- Pro: eliminates most drops at source rather than mitigating them at the gate
- Con: slower boot discovery more often (10s drip whenever heap <6KB, which happens briefly during almost every Status-burst)
- Con: may cause visible UI lag in HA entity registration during boot if heap constantly dips below 6KB. Measure first.
- Alternative considered: introduce a new HEAP_CAUTION tier at 7KB specifically for drip-slowdown — extra complexity without clear benefit over reusing HEAP_LOW.

**Impact estimate**
Combined with slower normal cadence (sibling task): expected further 30-40% reduction in throttle events.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 heapPressure condition uses HEAP_LOW instead of HEAP_WARNING in loopMQTTDiscovery
- [ ] #2 Comment updated to explain why HEAP_LOW is the correct threshold (drip must back off before publish gate)
- [ ] #3 Build passes for esp8266 environment
- [ ] #4 Manual verification: boot the firmware and confirm [drip] slowed to 10s messages when heap dips below 6KB
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit src/OTGW-firmware/MQTTstuff.ino:980 — change `getHeapHealth() >= HEAP_WARNING` to `getHeapHealth() >= HEAP_LOW`
2. Update the comment block (lines 968-970) to explain that drip must back off BEFORE the publish gate engages — i.e. HEAP_LOW is correct, not HEAP_WARNING
3. Build esp8266
4. Commit together with TASK-338 (same file, same scope)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Changed heapPressure trigger from HEAP_WARNING to HEAP_LOW (MQTTstuff.ino). Rationale comment added: drip MUST back off before the publish gate engages.
<!-- SECTION:NOTES:END -->
