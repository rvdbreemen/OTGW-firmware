---
id: TASK-338
title: Slow MQTT discovery drip interval from 1s to 2s
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 21:03'
updated_date: '2026-04-19 21:17'
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
Double the normal drip interval in loopMQTTDiscovery() to give heap more recovery time between discovery publishes.

**Background**
Crashevans tester log (v1.4.0-beta+0d6942a, 2026-04-17) shows 15 heap-pressure throttle events in 3 minutes. Analysis shows drip publishes fire every 1s while a Status-frame (msgid 0) fans out 9 sub-topic MQTT publishes within ~20ms. Two drip bursts within 1 second consume the heap faster than it releases.

**Considerations**
- Pro: one-line change (MQTTstuff.ino:972), zero runtime risk
- Pro: doubles recovery window between drip bursts
- Pro: no breaking change, no user-visible topic changes
- Con: HA discovery completion time goes from ~1.5min to ~3min at boot. Acceptable for one-shot boot discovery.
- Alternative considered: keep at 1s but add inter-drip yield — less effective because PubSubClient buffer still in-flight.

**Impact estimate**
Combined with widening the pressure backoff (sibling task): expected 60-70% reduction in throttle events on memory-constrained setups.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DISCOVERY_INTERVAL_NORMAL set to 2 in MQTTstuff.ino
- [x] #2 Comment updated to reflect 2s cadence rationale
- [x] #3 No change to DISCOVERY_INTERVAL_SLOW (10s remains correct pressure fallback)
- [x] #4 Build passes for esp8266 environment
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit src/OTGW-firmware/MQTTstuff.ino:972 — change DISCOVERY_INTERVAL_NORMAL from 1 to 2
2. Update the block comment above (lines 962-971) to reflect the 2s rationale (heap recovery between discovery bursts)
3. Build with pio esp8266 env to confirm no regressions
4. Commit with descriptive title referencing the heap-pressure reduction goal
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Changed DISCOVERY_INTERVAL_NORMAL from 1 to 2 (MQTTstuff.ino). Updated block comment to reflect that 2s cadence gives heap time to recover between publishes. Build pending.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed DISCOVERY_INTERVAL_NORMAL from 1s to 2s in MQTTstuff.ino. Updated the block comment to explain the new rationale: 2s gives heap time to recover between discovery-drip allocation bursts, and prevents collision with Status-frame sub-topic fanout (which can fan out 8-9 publishes in ~20ms).

DISCOVERY_INTERVAL_SLOW (10s) unchanged — correct as pressure fallback.

Build verified on esp8266 (Python 3.12 build.py path): clean compile, 0.69MB firmware artifact produced.

Impact: HA discovery completion time doubles from ~1.5min to ~3min at boot. Acceptable because discovery is one-shot per session. In exchange, the per-second burst rate halves — which combined with TASK-339 keeps the system out of the LOW heap band under normal load.
<!-- SECTION:FINAL_SUMMARY:END -->
