---
id: TASK-360
title: >-
  docs(mqtt): fix stale comments on heapdiag call site, drip interval, and
  ADR-077 reference
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:34'
updated_date: '2026-04-21 17:19'
labels:
  - code-review
  - docs
  - cleanup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A HIGH#4 plus LOW items: sendMQTTheapdiag header comment still claims doTaskEvery60s call path but actually runs via doTaskMinuteChanged under hourFlag (ADR-086). Drip loop comment at OTGW-firmware.ino:409 says 3s interval but is 2s/10s. MQTTstuff.ino:46 references non-existent ADR-077. (void)yearFlag cast at OTGW-firmware.ino:324 is a no-op since yearFlag is already consumed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sendMQTTheapdiag header comment corrected to reflect doTaskMinuteChanged + hourFlag (ADR-086)
- [x] #2 loopMQTTDiscovery comment in main loop reflects 2s normal / 10s slow intervals
- [x] #3 ADR-077 reference replaced with an existing ADR (e.g. ADR-044) or removed
- [x] #4 (void)yearFlag dead cast at OTGW-firmware.ino:324 removed; comment clarified
- [x] #5 Four VERIFICATION_* constants (MQTTstuff.ino:186-189) get per-line rationale trailer matching the ADR-062 tuning table
- [x] #6 STATUS_BURST_COOLDOWN_MS constant (MQTTstuff.ino:107) gets trailing comment explaining the shipped default rationale (or updates to reflect TASK-353's 2000ms change)
- [x] #7 HeapDiagSection struct comment (OTGW-firmware.h:267-277) notes the retained MQTT blob emits 17 keys not 9 and lists the additional live/derived fields
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. MQTTstuff.ino sendMQTTheapdiag header comment: update call-path description to doTaskMinuteChanged+hourFlag (ADR-086).
2. OTGW-firmware.ino loopMQTTDiscovery call site comment: change "3s interval" -> "2s normal / 10s slow".
3. MQTTstuff.ino:46 ADR-077 reference: does not exist; replace with ADR-042 (streaming JSON) since that is what the stated ~200B/chunk context actually describes.
4. MQTTstuff.ino VERIFICATION_* constants: add per-line rationale trailer (already partially present; refine tuning rationale).
5. MQTTstuff.ino STATUS_BURST_COOLDOWN_MS comment: value is already 2000ms (TASK-353 applied); trailing comment mentions "Crashevans cadence fit"; expand to note the 3s Status cadence.
6. OTGW-firmware.h HeapDiagSection struct: add leading comment noting 17 keys in retained stats/heap blob, mixing struct + state.discovery + live ESP heap calls. Not authoritative for wire format.
Skip AC #4 here (dead-cast removal is TASK-362 #5).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
All seven ACs satisfied.
- sendMQTTheapdiag header now reads doTaskMinuteChanged + hourFlag (ADR-086).
- loopMQTTDiscovery call-site comment: "2s normal / 10s slow".
- ADR-077 replaced with ADR-042 (streaming JSON, which is the genuinely relevant decision).
- VERIFICATION_* constants now have per-line rationale aligned with ADR-062 tuning table.
- STATUS_BURST_COOLDOWN_MS trailing comment now explains the ~3s Status cadence.
- HeapDiagSection got a leading comment explaining the 17-key wire format (8 struct + 3 live + 6 from state.discovery).
- (void)yearFlag removal actually executed by TASK-362 #5 (same line, shared fix); AC #4 here satisfied by that change.
Build: python build.py --firmware passed (1.4.1-beta+deaddd8).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cleaned up stale comments and added missing inline rationale across three files.

Changes:
- MQTTstuff.ino sendMQTTheapdiag header: corrected call-path to doTaskMinuteChanged under hourFlag (ADR-086). The old "doTaskEvery60s gated by hourChanged" wording was pre-ADR-086.
- OTGW-firmware.ino loopMQTTDiscovery trailing comment: "3s interval" -> "2s normal / 10s slow" matching DISCOVERY_INTERVAL_NORMAL / DISCOVERY_INTERVAL_SLOW in MQTTstuff.ino.
- MQTTstuff.ino MQTT_DISCOVERY_HEAP_MIN comment: replaced ghost ADR-077 citation with ADR-042 (streaming JSON, no ArduinoJson) — that is the genuinely relevant streaming-discovery decision.
- MQTTstuff.ino VERIFICATION_* constants (window, buffer, heap start/abort): added per-line rationale trailer tied to the ADR-062 tuning table and to HEAP_LOW=5120 / HEAP_WARNING=3072.
- MQTTstuff.ino STATUS_BURST_COOLDOWN_MS: trailing comment now names the ~3s Status cadence as the reason for the 2000ms default so a future tuner understands the bound.
- OTGW-firmware.h HeapDiagSection: added leading comment warning that the retained stats/heap blob emits 17 JSON keys (8 struct + 3 live + 6 discovery) and that the struct is NOT authoritative for the wire format.
- (void)yearFlag cast removal fulfilled by TASK-362 #5 — same line, shared fix.

Build: python build.py --firmware passed (1.4.1-beta+deaddd8).
<!-- SECTION:FINAL_SUMMARY:END -->
