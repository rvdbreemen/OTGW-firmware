---
id: TASK-1250
title: >-
  Fix MQTT staying down for the rest of the boot when the broker DNS lookup
  fails after a reconnect
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 14:26'
updated_date: '2026-10-18 19:52'
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
- [x] #1 Reproduced on the old code: broker configured by hostname, DNS unreachable at the moment of a WiFi reconnect, MQTT never recovers without a reboot (bench or a host-side simulation that calls the real state machine)
- [x] #2 With the fix, MQTT reconnects once DNS answers again, without a reboot
- [x] #3 A failed lookup never overwrites the last good broker address
- [x] #4 The wait comments state the real durations
- [x] #5 esp8266 build and evaluate.py --quick green
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-18 implementation: 41608bb07 (decisions moved into mqttBrokerAddress.h, behaviour unchanged), 01025ea68 (first fix + host test), 07ffbebd0 (review fixes), 4b3e950d2 (lookup timeout back to the core default, CHANGELOG).
Review of 01025ea68 (workflow wf_87178900-b4b, 2 lenses + 2 refuters): [major 4/4] the held address was not tied to the broker name, so a rename with a failed lookup silently used the previous broker (a regression of 01025ea68); [minor 2/2] a held but stale address was never looked up again. Fixed in 07ffbebd0: MQTTbrokerResolvedFor ties the address to its name; WAIT_FOR_RECONNECT always goes back to INIT (MQTT is down whenever that wait runs out); lookup target made static. The 3 s lookup timeout added there was reverted in 4b3e950d2 (lwIP retries ~15 s per server; a slow DNS would always fail).
Deviation from the approved plan: the plan re-resolved only when no address was held and put that decision in the header; the review changed it to always re-resolve after the wait, and that transition now lives in MQTTstuff.ino only.
AC evidence and deviations:
- AC1 (deviation, protocol in docs/guides/backlog-cli.md): the lookup decision is real code and was proven red then green: 7/13 failures on 41608bb07 (logs/hosttest_1250_old.log), 3/15 failures on a copy of the header with the 01025ea68 name-blind semantics (logs/hosttest_1250_rename_red.log). The state transitions around it (INIT/ERROR/WAIT/TRY, WAIT back to INIT) are modelled in the test, not compiled from handleMQTT(); the re-resolve after the wait is therefore covered by the model, not by firmware code under test.
- AC2: host scenarios 'DNS back after boot outage' and 'broker moved' pass on the final code (logs/hosttest_1250_final.log), on the modelled transitions above. Not tested on hardware.
- AC3: decideMqttBrokerLookup never takes a failed result (unit checks + 'last good address survives' scenario, 15/15).
- AC4: the four '10 minutes' texts say 42 seconds (01025ea68).
- AC5: build.bat --firmware fresh 773760 B with 'Build completed successfully!' (logs/build_1250d.log), evaluate.py --quick 37/37, host tests 6/6 suites.

2026-10-18 hardware run on the COM3 unit .88.68 (ESP8266 + PIC 6.8), rig: own Mosquitto 2.1.2 on the PC :1885, broker names advertised over mDNS by python-zeroconf (host_ttl 10 s), router MikroTik.
- Positive control: the unit resolved otgwtest-a.local from the PC responder and connected (mosq.log 21:29:49).
- OLD (1.7.7-beta.1+ed84e49, same lookup code as before 41608bb07): broker set to otgwtest-b.local while unadvertised at 21:30:48, advertised from 21:31:55 (PC control lookup resolves it). After a broker restart at 21:34:11 dropped the leftover session, the unit stayed disconnected until the end of the observation at 21:40:42 (6.5 min, about 9 wait cycles): no new connection in mosq.log, mqttconnected=False in every 15 s poll, telnet 'MQTT server is [otgwtest-b.local], IP[255.255.255.255]'. Defect reproduced.
- REOPENED: a regression in the committed fix found while setting this up. startMQTT() never closes an open session, and when the lookup for a renamed broker fails the fix keeps MQTTbrokerIP (the previous broker's address) instead of writing INADDR_NONE over it, so both publish guards in sendMQTTData() pass and the unit keeps publishing to the PREVIOUS broker over the old session while it shows as connected. The old code refused those publishes ('MQTT broker IP not valid'). The host test missed it because its model closed the session in startMQTT(). Fix planned: doMqttDisconnect() when the name has no usable address; model corrected. To be shown on hardware: committed fix (v1) publishes to the old broker after the rename, v2 does not.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTT now recovers by itself after a failed broker name lookup (1.x, otgw-1.x.x, not yet in a beta).

Cause: MQTT_STATE_INIT was the only state that looked the broker name up, and it wrote a failed result (INADDR_NONE) over the broker address and the text PubSubClient points to before checking it; the state machine then never returned to INIT. One failed lookup (DNS not answering after a router reboot, at boot or after a WiFi reconnect) kept MQTT down until a WiFi reconnect, a settings save or a restart. Brokers set by IP address were not affected.

Fix: a failed lookup keeps the last good address for the same name (MQTTbrokerResolvedFor ties address and name, so a renamed broker never falls back to the old one); after every 42 s reconnect wait the name is looked up again, which also finds a broker that moved; the lookup keeps the core's 10 s timeout; the '10 minutes' texts now say 42 seconds.

Evidence: host test test/host/test_mqttBrokerAddress.cpp against the real mqttBrokerAddress.h, red then green (7/13 failing on the old decision, 3/15 on the name-blind variant of the first fix, 15/15 on the final code); firmware build green; evaluate.py --quick 37/37; all six host suites green. Deviation on AC1/AC2: the state transitions around the decision, including the re-resolve after the wait, are modelled in the test rather than compiled from handleMQTT(). Not tested on hardware. Commits 41608bb07, 01025ea68, 07ffbebd0, 4b3e950d2.
<!-- SECTION:FINAL_SUMMARY:END -->
