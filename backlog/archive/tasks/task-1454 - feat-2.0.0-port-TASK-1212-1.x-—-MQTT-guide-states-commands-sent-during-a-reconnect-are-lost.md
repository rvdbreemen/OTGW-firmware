---
id: TASK-1454
title: >-
  feat-2.0.0: port TASK-1212 (1.x) — MQTT guide states commands sent during a
  reconnect are lost
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T06:26:00Z'
updated_date: '2026-10-29T06:59:00Z'
labels:
  - port-from-1.x
  - mqtt
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 614000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Docs-only port of 1.x TASK-1212 (1.x commit 548934b85, GH #682).

What 1.x did: it added a section 'Commands sent while the gateway is reconnecting are lost' to docs/guides/MQTT_LWT.md (1.x lines 96-102). The section explains that the firmware connects with a clean session and subscribes to its command topics at QoS 0. The broker therefore keeps nothing for the gateway while it is disconnected. A command that Home Assistant publishes to a set/ topic during a reconnect, even one of a few seconds, is dropped and never reaches the boiler. In HA the entity keeps showing the value the gateway reports, not the requested one, and the user should resend once the gateway is 'online'. The section also records why this is deliberate: with a persistent session plus QoS 1, Mosquitto queues commands without expiry, also during an outage of hours. The gateway would then apply every queued command at once, including a setpoint that is hours old.

Why it applies to 2.0.0: origin/dev has the same client behaviour.
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1328 MQTTclient.setCleanSession(true)
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1523 comment: sessionPresent unused, always clean session
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1568/:1574 subscribe(topic, 0) and subscribe("homeassistant/status", 0) in onMqttConnect()
origin/dev:docs/guides/MQTT_LWT.md has no such section. Its headings run The Problem (5), How LWT Works (9), Complete Lifecycle Diagram (54), How Home Assistant Uses This (77), The Role of Retain (94), Summary Table (103), Related Code (110).

Where: add the section to origin/dev:docs/guides/MQTT_LWT.md, directly before '## How Home Assistant Uses This' (line 77), as a '##' or '###' heading that fits the dev file structure. Adapt the code references to the dev async client: name setCleanSession(true) in the setServer block and subscribe(..., 0) in onMqttConnect() in MQTTstuff.ino. Do not name the 1.x PubSubClient MQTTclient.connect(clientId, ..., 0, true, "offline") call, which does not exist on dev. Refer to function names rather than line numbers so the doc does not go stale. Cite TASK-1212 (1.x) / GH #682 as the origin of the decision.

What NOT to port: the 1.x paragraph above it about MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS is a separate, older change and out of scope. Make no firmware change. Do not switch to a persistent session or QoS 1; the doc records that this is rejected on purpose.

Risk: none to firmware. The only risk is a factual one: the text must match the dev code at the time of writing. Re-check setCleanSession(true) and the QoS 0 subscribe calls before committing.

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1328 MQTTclient.setCleanSession(true) (same clean-session behaviour as 1.x)
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1523 comment 'sessionPresent is unused (we always setCleanSession(true))'
- origin/dev:src/OTGW-firmware/MQTTstuff.ino:1568 and :1574 MQTTclient.subscribe(topic, 0) / subscribe("homeassistant/status", 0); also :308 and :1786 at QoS 0
- origin/dev:docs/guides/MQTT_LWT.md headings at lines 1,5,9,11,39,50,54,77,94,103,110: no section on lost commands; git grep for 'clean session|persistent session|reconnecting are lost|TASK-1212' in origin/dev docs/ finds only unrelated hits (ADR-041:134, RELEASE_GITHUB_1.4.1.md:18)
- origin/dev:backlog/tasks/task-1123 ...md:65 states the same clean-session/QoS0 fact as an aside in the DHW meter task, but no user-facing doc carries it; no dev sibling task for #682 found (git grep '#682|clean.session' in backlog/tasks, backlog/archive, backlog/completed)
- git log origin/dev -- docs/guides/MQTT_LWT.md: last change f7291eefe (#526), predates TASK-1212

## Platform notes

No firmware change. Dev uses an async MQTT client (MQTTclient.connect() with no arguments at MQTTstuff.ino:223; options are set via setCleanSession/setKeepAlive at :1327-1328, and subscriptions happen in onMqttConnect at :1524+) instead of PubSubClient's connect(clientId, ..., willQos, willRetain, willMsg). So the code references in the doc text must name the dev locations (setCleanSession(true) in the setServer block, subscribe(topic, 0) in onMqttConnect), not the 1.x connect(...) call. On 2.0.0 there are more command sources than set/ topics from HA (for example the SAT controller), but the statement is about MQTT-delivered commands only.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict holds. Dev uses the same clean session and QoS 0 subscriptions through the async espMqttClient, so a set/ command published during a reconnect is lost on 2.0.0 as well. No user-facing doc on dev says so. ADR-186:128-129 on dev already links 'lost commands' to #682, but only as a slow-broker risk inside an ADR, so it does not count as the port. One small citation fix: the topic subscribe is at line 1569 (1568 is the strlcat/debug context). One thing the analyst could add: dev's SAT MQTT-fallback feature (docs/api/MQTT.md:955) is a nearby user-facing topic, and the new section can mention it in one line, because on 2.0.0 a long MQTT outage can also hand control to SAT. This is optional and not a correction.

Change the subscribe citation from MQTTstuff.ino:1568 to :1569, or better, cite it by function name only, as the draft already suggests. Placement: the 1.x section is a '###' that sits under the offline-republish text. Dev has no such parent, so on dev make it a '##' section before '## How Home Assistant Uses This' (line 77), or put it after '## The Role of Retain' (line 94). Optional: add 'docs/adr/ADR-186 (mentions #682 lost commands)' as a cross-reference in the task description, so the implementer sees the existing dev mention. Leave the ACs as they are; the docs-only skip of the build gate is consistent with the push policy.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/guides/MQTT_LWT.md on the 2.0.0 line contains a section stating that a command published to a set/ topic while the gateway is disconnected or reconnecting is lost, and that the user should resend it once the availability topic shows 'online'
- [x] #2 The section names the mechanism with references that exist on the 2.0.0 code: clean session (MQTTclient.setCleanSession(true) in MQTTstuff.ino) and QoS 0 subscriptions (MQTTclient.subscribe(..., 0) in onMqttConnect()); a grep of origin/dev MQTTstuff.ino confirms both still hold at commit time
- [x] #3 The section records the rationale (persistent session + QoS 1 would make Mosquitto replay hours-old setpoints after a long outage) and cites TASK-1212 (1.x) and GH #682
- [x] #4 No source files under src/ change (git diff --stat shows only docs/ and backlog/ paths); because this is a docs-only commit, the build and evaluate.py gates may be skipped under the project's docs-only push policy
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 verified on dev a632e8e68+: MQTTstuff.ino:1328 MQTTclient.setCleanSession(true); onMqttConnect() MQTTstuff.ino:1524-1574 subscribe(topic, 0) at :1569 and subscribe("homeassistant/status", 0) at :1574, also :308 and :1787 at QoS 0; vendored espMqttClient default _cleanSession(true) (src/libraries/espMqttClient/src/MqttClient.cpp:32). Dev does not queue or replay inbound set/ commands: with a clean session the broker stores nothing for the client, so the 1.x statement holds unchanged on dev. Only 2.0.0 difference: opt-in SAT MQTT-loss takeover (SATmqttfallback, docs/api/MQTT.md:959), mentioned in one line. Out-of-scope drift seen: MQTT_LWT.md Step 1 still quotes the 1.x PubSubClient connect(clientId, willTopic, 0, true, "offline") at MQTTstuff.ino:790-795 and the intro says ESP8266; dev uses MQTTclient.setWill(MQTTPubNamespace, 0, true, "offline", 7) at MQTTstuff.ino:1347.

2026-10-29 addendum (C5 request): MQTT_LWT.md now describes dev instead of 1.x. LWT via MQTTclient.setWill(MQTTPubNamespace, 0, true, "offline", 7) in the setServer block (MQTTstuff.ino:1347) instead of the PubSubClient connect(...) call; birth via publishBirthOnline() from onMqttConnect() (MQTTstuff.ino:1539, 2052-2062) plus 5-min re-assert timerMQTTbirthreassert (:1293, :1418); ESP8266 -> ESP32-S3; HA avty_t from MQTTHaDiscovery.cpp:2425 instead of the retired data/mqttha.cfg; line numbers replaced by function names; em dashes removed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Docs-only port of 1.x TASK-1212 (GH #682). docs/guides/MQTT_LWT.md gains '## Commands sent while the gateway is reconnecting are lost' before 'How Home Assistant Uses This'. AC1: section states set/ commands during a disconnect or reconnect are lost, resend once availability shows online. AC2: names setCleanSession(true) in the setServer block (MQTTstuff.ino:1328) and subscribe(..., 0) in onMqttConnect() (:1569, :1574); grep at commit time: setCleanSession(true) count 1, QoS-0 subscribe calls present. AC3: rationale (persistent session + QoS 1 replays hours-old setpoints) with TASK-1212 (1.x) and GH #682. AC4: diff touches only docs/guides/MQTT_LWT.md and this task file; no build or evaluator needed (docs-only). Dev behaves as 1.x here: no queue or replay of inbound commands. Not verified on a device.
<!-- SECTION:FINAL_SUMMARY:END -->
