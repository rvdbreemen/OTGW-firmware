---
id: TASK-1154
title: >-
  Investigate: MQTT reconnects every 30-60s remain after the malformed-packet
  fix
status: To Do
assignee: []
created_date: '2026-09-22 21:15'
labels:
  - bug
dependencies: []
priority: high
ordinal: 228000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1134, reported by mrfox7688 on GH #682 on 2026-09-22. The malformed-packet defect is confirmed fixed by the field: he found no malformed packet entries in the Mosquitto log over four days on beta.3. What remains is a different symptom that the fix very likely causes.

WHAT HE SEES. After a broker restart on 2026-09-21 19:22 the gateway connected at 19:23 and stayed up for about 15 hours 34 minutes. From 2026-09-22 10:58 onward it reconnects repeatedly, typically every 30 to 60 seconds, sometimes after several stable minutes. The broker logs:

  disconnected: Success
  disconnected: connection closed by client
  session taken over          (twice at 18:14 and 18:15)

HYPOTHESIS, code-backed but not yet confirmed on hardware. These are our own disconnects. Both short-write remedy sites call MQTTclient.disconnect() (MQTTstuff.ino:359 for the header half, :1059 for the payload half). That sends a clean MQTT DISCONNECT packet, which Mosquitto logs as "connection closed by client", and a client-initiated close also explains "disconnected: Success". So every short write now costs a full reconnect rather than a desynchronised stream.

That was the deliberate trade in TASK-769 and TASK-1134, recorded in the code comment at MQTTstuff.ino:356: "a reconnect costs one gap, a desynchronised stream costs every packet after it." The trade is still right. What this report adds is a measurement of how often the remedy fires on a real network, and every 30 to 60 seconds is more often than that reasoning assumed.

"session taken over" follows from the same thing: the firmware reconnects with the same client ID before the broker has reaped the previous socket, so the broker sees two sessions for one ID and evicts the old one.

WHAT THIS TASK IS FOR. Not to undo the disconnect. It is to reduce how often a short write happens at all, and to make the frequency visible. Directions worth weighing, none chosen:

- Count the remedy. There is no counter for "publish aborted and link dropped". Without one, nobody can tell a healthy gateway from one reconnecting every minute, which is exactly the blindness TASK-1148 fixed for the telnet console. This is the cheapest step and probably the first.
- Understand why the send buffer fills on his network specifically. A 15 hour stable run followed by an hour of churn suggests a condition that develops, not a constant.
- Reconsider whether a failed write must always drop the link. A short write that stops cleanly at a packet boundary may be recoverable without a reconnect, whereas one mid-packet is not. Distinguishing the two is more work than it sounds and must not weaken the desync guarantee.

Do not start by weakening the disconnect. The desync it prevents is worse than the reconnect it causes, and that ordering is settled.

OPEN QUESTION FOR THE REPORTER, already asked by him in the other direction: he wants to know whether these clean disconnects are expected from the beta. The honest answer is yes, they are the fix operating, and the remaining question is why the underlying condition is so frequent for him. His Wi-Fi conditions and broker placement are worth knowing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The hypothesis is confirmed or refuted with evidence: are the logged disconnects our own MQTTclient.disconnect() calls from the short-write remedy, or something else
- [ ] #2 A counter exists for publishes aborted by the short-write remedy, readable without telnet, so reconnect frequency is observable instead of inferred from broker logs
- [ ] #3 mrfox7688 is given a straight answer on whether the clean disconnects are expected behaviour from the beta
- [ ] #4 Any change preserves the TASK-769/TASK-1134 guarantee: the broker never sees a truncated or malformed publish
- [ ] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
