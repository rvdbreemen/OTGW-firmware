---
id: TASK-1250
title: >-
  Fix MQTT staying down for the rest of the boot when the broker DNS lookup
  fails after a reconnect
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 14:26'
updated_date: '2026-10-18 15:07'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Confirmed by reading (core 2.7.4 + MQTTstuff.ino):
- handleMQTT MQTT_STATE_INIT (MQTTstuff.ino:900-931) calls WiFi.hostByName(sBroker, MQTTbrokerIP). On failure the core sets the result to INADDR_NONE (255.255.255.255, ESP8266WiFiGeneric.cpp:621) and the firmware writes it into MQTTbrokerIPchar before validating. PubSubClient keeps that buffer as a pointer (setServer(const char*)).
- INIT is the only place that resolves. ERROR -> WAIT_FOR_RECONNECT -> TRY_TO_CONNECT (1085-1090, 1072-1083) never returns to INIT, so after one failed lookup MQTT connects to a bad address forever, until startMQTT() runs again (a WiFi reconnect, a settings change or a reboot).
- Affects brokers configured by host name only; an IP literal resolves through fromString without DNS.
- The four '10 minutes' comments describe timerMQTTwaitforconnect, which is 42 s.
Fix (MQTTstuff.ino only):
1. Resolve into a local IPAddress; update MQTTbrokerIP / MQTTbrokerIPchar only on a valid result. On a failed lookup keep the last good address if there is one (log it), else go to ERROR.
2. WAIT_FOR_RECONNECT goes back to INIT (re-resolve) when there is no valid broker address, otherwise to TRY_TO_CONNECT as today. Re-resolving only in that case limits the blocking hostByName (up to 10 s) to a DNS outage in which MQTT is down anyway.
3. Correct the '10 minutes' comments to 42 s.
Evidence (no ESP8266 on the bench): move the two decisions (apply a lookup result; next state after the wait) into a small header used by handleMQTT, first as a behaviour-preserving refactor, add a host test in test/host (run_tests.bat, real code compiled against the shim) that drives a DNS outage then recovery: red on the refactored old logic (never re-resolves, keeps 255.255.255.255), green on the fix. Build, evaluate, host tests.
<!-- SECTION:PLAN:END -->
