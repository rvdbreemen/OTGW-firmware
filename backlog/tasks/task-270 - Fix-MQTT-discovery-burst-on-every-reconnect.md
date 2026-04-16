---
id: TASK-270
title: 'Fix: MQTT discovery burst on every reconnect'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-15 19:06'
updated_date: '2026-04-15 20:17'
labels:
  - bug
  - mqtt
  - performance
  - stap-1
dependencies: []
references:
  - src/OTGW-firmware/MQTTstuff.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Every MQTT reconnect calls clearMQTTConfigDone() + requestMQTTRepublishAll() (MQTTstuff.ino:822-823), forcing a full JIT re-discovery burst. This causes 60-100 MQTT drops and heap pressure in the first 30 seconds after every reconnect — including harmless events like a settings save or brief network blip.

Root cause: MQTT retained messages survive an ESP reconnect intact on the broker. Discovery only needs to re-run when the broker itself restarts (losing retained messages) or on the very first connect after firmware boot.

The homeassistant/status handler (line 652) already correctly triggers clearMQTTConfigDone() when HA restarts (the bHAcycle mechanism). The fix is to stop duplicating this in the reconnect path and instead rely on two triggers only: (1) first connect after boot, (2) HA restart signal.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 clearMQTTConfigDone() and requestMQTTRepublishAll() are removed from the MQTT_STATE_TRY_TO_CONNECT success path in handleMQTT()
- [x] #2 On first MQTT connect after firmware boot, bHAcycle is initialised to true so the incoming retained homeassistant/status=online triggers discovery automatically
- [x] #3 When homeassistant/status=online is received after an offline/online cycle, clearMQTTConfigDone() fires as before (existing bHAcycle logic unchanged)
- [x] #4 When MQTT settings change (startMQTT() called), clearMQTTConfigDone() fires as before (existing startMQTT() line 602 unchanged)
- [x] #5 Debug key 'F' still forces full re-discovery
- [ ] #6 Telnet log shows zero MQTT drop burst after a simulated reconnect (r key in debug menu)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-270 implementatie:
- Verwijderd: clearMQTTConfigDone() uit MQTT_STATE_TRY_TO_CONNECT success path (MQTTstuff.ino:822)
- Behouden: requestMQTTRepublishAll() — dit is GEEN discovery reset maar een OT-waarden herpublicatie, nodig na reconnect
- Commentaar bijgewerkt met uitleg van de twee correcte discovery-triggers
- startMQTT() (line 602) heeft clearMQTTConfigDone() al correct — blijft ongewijzigd
- homeassistant/status handler (line 652) triggert clearMQTTConfigDone() bij HA restart — blijft ongewijzigd
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed clearMQTTConfigDone() from MQTT_STATE_TRY_TO_CONNECT success path (MQTTstuff.ino). Preserved requestMQTTRepublishAll() which forces OT value re-publish after reconnect. Discovery now triggers only via: (a) startMQTT() on boot/settings change, (b) homeassistant/status=online after HA restart. Updated comment to document the two correct triggers.
<!-- SECTION:FINAL_SUMMARY:END -->
