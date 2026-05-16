---
id: TASK-610
title: >-
  investigate(mqtt): JIT discovery after GW=R / PIC-only reset — RAM slot-state
  + done-bitmap survive because ESP doesn't restart
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 08:50'
updated_date: '2026-05-16 09:28'
labels:
  - mqtt
  - investigation
  - needs-decision
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Refined handover (supersedes the first one) claims JIT discovery rarely re-triggers because per-slot value state persists across reboots, and that JIT hangs on first=true.

Static analysis corrects and refines this:

STORAGE: mqttlastsent[MQTT_TRACKED_SLOT_COUNT] (OTGW-Core.ino:255) is pure RAM, statically init {0}. No RTC, no LittleFS. RTC user memory is only used for the WiFi-portal-reset flag (slot 96), not slot tracking.

SENTINEL: TRACKED_TIME_UNSEEN = 0xFFFF (OTGW-Core.ino:316). firstSeen = (packed & 0xFFFF) == 0xFFFF. Note {0} static-init reads as firstSeen=FALSE; the proper UNSEEN sentinel is written by resetMqttTrackedState().

RESET PATHS for slot-state: resetMqttTrackedState() is called from exactly two sites:
 1. gTrackingStateInitializer — a C++ static-init object (OTGW-Core.ino:440-448). Constructor runs once per ESP program start, before setup(). Cold boot, ESP.restart(), watchdog, exception, OTA all hit this.
 2. requestMQTTRepublishAll() (OTGW-Core.ino:1426) — only from the broker-restart heuristic (MQTT offline > 5 min) in handleMQTT().
 NOT called on: GW=R, PIC physical reset, normal MQTT reconnect (<5min), settings change, debug F.

REBOOT PATH ASYMMETRY:
 - UI/FSexplorer/nightly reboot -> doRestart() -> ESP.restart() (helperStuff.ino:632-656). Full restart: static-init runs (slots=UNSEEN) AND startMQTT() runs (clearMQTTConfigDone + clearMQTTConfigPending + publishNonOTDiscoveryConfigs). Clean JIT cycle. WORKS.
 - GW=R / REST / serial -> resetOTGW() (OTGW-Core.ino:538) -> OTGWSerial.resetPic() ONLY. PIC restarts, ESP keeps running. mqttlastsent[] and MQTTautoConfigMap (done-bitmap) retain prior state. No static-init, no startMQTT().
 - Physical reset button on NodeShop OTGW resets the PIC, not the ESP — same as GW=R.

PREMISE CORRECTION: The handover says JIT hangs on first=true. The JIT trigger (processOT() OTGW-Core.ino:4112-4126, post-TASK-601) gate is is_value_valid && settings.mqtt.bEnable && hasConfig && !getMQTTConfigDone(id). There is NO firstSeen term. firstSeen only gates value-topic throttling in shouldPublishMQTTForID(). After GW=R the real suppressor is the done-bitmap (getMQTTConfigDone true for already-published IDs), not the slot state.

OPEN PRODUCT QUESTION: After a PIC-only reset the MQTT session is NOT dropped, so the broker still retains all discovery configs and HA keeps its entities. Per ADR-073 there is nothing to republish. If users report missing entities specifically after GW=R, the likely real trigger is either (a) they also wiped retained broker topics / removed the HA device while the done-bitmap stayed set, or (b) the symptom is actually cold-boot/fresh-flash and is the phantom-ID drip stall already fixed in PR #572 / TASK-601.

This task captures the diagnosis. Implementation deferred until the maintainer confirms desired first/discovery semantics (handover step 3, options 1/2/3).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Confirm storage location of slot-state (RAM vs RTC vs LittleFS) — DONE: RAM-only
- [x] #2 Confirm sentinel + firstSeen computation — DONE: 0xFFFF, time-bits == sentinel
- [x] #3 Enumerate every reset path for resetMqttTrackedState + done-bitmap — DONE
- [x] #4 Confirm UI-reboot vs GW=R asymmetry mechanism — DONE: ESP.restart vs PIC-only
- [x] #5 Maintainer confirms desired semantics (option 1 RAM-only republish-on-restart / option 3 reset-all-paths / no-change) before any implementation
- [x] #6 If implementation approved: test-matrix (cold/UI/GW=R/physical/F/broker-reconnect) results recorded in PR
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Maintainer decision: keep ADR-073 semantics as-is. GW=R / physical PIC reset stays PIC-only; no rediscovery is needed because the MQTT session survives a PIC-only reset and the broker retains all discovery configs. No firmware change for this task. The user-visible 'missing entities' symptom is addressed by the phantom-ID drip-stall fix (TASK-601, PR #572) plus documented recovery guidance.

Maintainer reconfirmed scope (2026-05-16): PIC resets (GW=R, physical button, internal resetOTGW) are NEVER a discovery trigger. Only ESP-level restart / cold boot / OTA / watchdog / exception are. This is exactly the existing behaviour — gTrackingStateInitializer + startMQTT() clear the done/pending bitmaps on every ESP program start, and the JIT trigger then instantly re-marks on incoming OT bus traffic. No code change, no ADR supersession. The visible-entities outcome is delivered by the TASK-601 phantom-ID drip-stall fix in PR #572 (the marking was always instant; the drip uitlevering was the broken link).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Investigation only — no firmware change (maintainer chose to keep ADR-073 as-is).

Findings (static analysis, corrects the refined handover's premise):
- Slot-state mqttlastsent[] is pure RAM (OTGW-Core.ino:255), not RTC/LittleFS.
- Sentinel TRACKED_TIME_UNSEEN=0xFFFF; the {0} static init is NOT 'unseen' — resetMqttTrackedState() writes the real sentinel.
- resetMqttTrackedState() runs from a C++ static-init object (gTrackingStateInitializer) on every ESP program start (cold boot, ESP.restart, watchdog, OTA), and from requestMQTTRepublishAll() on the >5min broker-restart heuristic. NOT on GW=R, physical reset, normal reconnect, settings change, or F.
- UI/FSexplorer/nightly reboot -> doRestart() -> ESP.restart(): full restart re-runs static-init AND startMQTT() (clears done+pending bitmaps). Clean JIT cycle.
- GW=R / physical button -> resetOTGW() -> OTGWSerial.resetPic() only: ESP keeps running, mqttlastsent[] and the MQTTautoConfigMap done-bitmap survive.
- Premise correction: the JIT trigger (processOT() ~4112, post-TASK-601) does NOT depend on firstSeen. Its gate is is_value_valid && bEnable && hasConfig && !getMQTTConfigDone(id). firstSeen only throttles value-topic publishing. After GW=R the real suppressor (correctly) is the done-bitmap, and since the MQTT session is unaffected the broker still retains every config — HA loses nothing, so there is nothing to republish.

Decision: keep ADR-073 semantics. The 'missing entities' field symptom is the cold-boot/fresh-flash phantom-ID drip stall, already fixed in TASK-601 / PR #572. Added user-facing recovery documentation (CHANGELOG + docs/guides/MQTT_STALE_TOPICS_CLEANUP.md 'Recovering missing HA entities' section) covering progressive JIT appearance, PIC-only-reset semantics, Unknown-Data-Id boiler behaviour, and escalating recovery steps.

Outcome: docs-only deliverable on branch claude/fix-jit-mqtt-discovery-N7Yos (rides PR #572). No code, no ADR change.
<!-- SECTION:FINAL_SUMMARY:END -->
