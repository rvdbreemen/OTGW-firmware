---
id: TASK-347
title: Post-Status-burst cooldown window for MQTT discovery drip
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 09:34'
updated_date: '2026-04-20 09:41'
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
After endStatusBurst fires, hold the discovery drip for an extra window (default 10s) so lwIP pbufs from the Status-frame fanout fully drain before we allocate for the next discovery publish.

**Design**
- Add burstCooldownUntilMs (static in MQTTstuff.ino)
- Track statusBurstPublishCount during each burst; increment in publishStatusBitMQTT when an actual MQTT send happens
- At endStatusBurst(): if publishCount > 0, arm cooldown = millis() + STATUS_BURST_COOLDOWN_MS. Empty bursts skip the cooldown.
- New helper isDripDeferred() = isStatusBurstActive() || (millis() < burstCooldownUntilMs)
- loopMQTTDiscovery uses isDripDeferred() instead of isStatusBurstActive()
- iDripQuiescedCount covers both skip-paths (existing telemetry field)

**Cooldown value**
STATUS_BURST_COOLDOWN_MS = 10000 (user-chosen). CAUTION: Status-frames typically arrive every ~3s under normal boiler traffic; a 10s cooldown overlaps consecutive bursts and can stall discovery during heavy Status traffic. Monitor via iDripQuiescedCount spiking without iDripSlowModeCount accompanying it.

**Tuning**
One #define in MQTTstuff.ino to lower the cooldown if boot-discovery proves too slow. Candidate values: 2500ms (fits between bursts), 5000ms (partial overlap), 10000ms (chosen).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 STATUS_BURST_COOLDOWN_MS constant defined (value=10000)
- [x] #2 burstCooldownUntilMs static added to MQTTstuff.ino
- [x] #3 statusBurstPublishCount tracks real sends during a burst
- [x] #4 publishStatusBitMQTT increments the counter on actual MQTT publish
- [x] #5 endStatusBurst arms cooldown only when publishCount > 0
- [x] #6 isDripDeferred() helper added to header
- [x] #7 loopMQTTDiscovery uses isDripDeferred()
- [x] #8 iDripQuiescedCount increments for both active-burst skip and cooldown skip
- [x] #9 Build passes esp8266
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. MQTTstuff.ino: add cooldown state, empty-burst guard, helper isDripDeferred, drop new guard into loopMQTTDiscovery
2. OTGW-Core.ino: publishStatusBitMQTT increments counter on real send
3. OTGW-firmware.h: add isDripDeferred() forward decl
4. Build + commit + push + close
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
User confirmed: 1=10s cooldown, 2=empty-burst guard enabled, 3=split counter. Plan adjusted: rename iDripQuiescedCount -> iDripActiveBurstSkipCount (struct + JSON + REST + UI) and add iDripCooldownSkipCount. Two MQTT JSON fields: drip_burst_skip, drip_cooldown_skip. Two REST fields: hd_drip_burst_skip, hd_drip_cooldown_skip. Safe rename since TASK-346 just landed today and there are no external consumers yet.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added post-Status-burst cooldown window for MQTT discovery drip on branch 1.4.1.

**Cooldown mechanism**
- STATUS_BURST_COOLDOWN_MS = 10000 (user-chosen)
- Armed at endStatusBurst() ONLY if statusBurstPublishCount > 0 (empty-burst guard)
- statusBurstPublishCount incremented by incrementStatusBurstPublishCount() called from publishStatusBitMQTT (when allowPublish) and publishMasterStatusState/publishSlaveStatusState (when publishCombined)

**New helper**
isDripDeferred() = isStatusBurstActive() || (millis() < burstCooldownUntilMs)

**Telemetry split (per user request)**
- iDripQuiescedCount renamed to iDripActiveBurstSkipCount
- New iDripCooldownSkipCount for cooldown-window skips
- MQTT JSON: drip_burst_skip + drip_cooldown_skip (was drip_quiesced)
- REST: hd_drip_burst_skip + hd_drip_cooldown_skip
- UI labels: "Discovery Drip Skipped (active burst)" + "Discovery Drip Skipped (cooldown)"

**Known trade-off documented in code**
Status-frames arrive ~3s apart in normal boiler traffic. 10s cooldown will overlap consecutive bursts under heavy traffic, effectively stalling discovery. Visible in telemetry as iDripCooldownSkipCount growing without iDripSlowModeCount. Tunable via STATUS_BURST_COOLDOWN_MS (2500ms = fits between bursts, 5000ms = partial overlap).

Build verified, commit pushed to origin/1.4.1.
<!-- SECTION:FINAL_SUMMARY:END -->
