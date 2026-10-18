---
id: TASK-1250
title: >-
  Fix MQTT staying down for the rest of the boot when the broker DNS lookup
  fails after a reconnect
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 14:26'
updated_date: '2026-10-18 15:05'
labels:
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 246000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1248 fast-path review (workflow wf_4dc93850-258, upheld 2/2 by two refuters; not yet reproduced on hardware). In MQTTstuff.ino the MQTT state machine resolves the broker name only in MQTT_STATE_INIT. After a WiFi reconnect (refreshServicesAfterWifiReconnect -> doMqttDisconnect + startMQTT), a DNS lookup that fails overwrites the buffer PubSubClient connects to (MQTTbrokerIPchar) before the result is validated, and the ERROR / WAIT_FOR_RECONNECT states never return to INIT, so the broker is not re-resolved and MQTT stays down until a reboot. The comments call the ERROR wait '10 minutes' while the timer is 42 s. Exposure grew with TASK-1248, whose forced reconnects run this path more often, typically right after a router outage when DNS may not answer yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on the old code: broker configured by hostname, DNS unreachable at the moment of a WiFi reconnect, MQTT never recovers without a reboot (bench or a host-side simulation that calls the real state machine)
- [ ] #2 With the fix, MQTT reconnects once DNS answers again, without a reboot
- [ ] #3 A failed lookup never overwrites the last good broker address
- [ ] #4 The wait comments state the real durations
- [ ] #5 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
