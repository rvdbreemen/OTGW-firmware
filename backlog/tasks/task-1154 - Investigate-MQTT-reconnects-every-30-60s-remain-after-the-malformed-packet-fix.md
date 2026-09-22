---
id: TASK-1154
title: >-
  Investigate: MQTT reconnects every 30-60s remain after the malformed-packet
  fix
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-22 21:15'
updated_date: '2026-09-22 21:53'
labels:
  - bug
dependencies:
  - TASK-1134
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
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
- [x] #1 The hypothesis is confirmed or refuted with evidence: are the logged disconnects our own MQTTclient.disconnect() calls from the short-write remedy, or something else
- [x] #2 A counter exists for publishes aborted by the short-write remedy, readable without telnet, so reconnect frequency is observable instead of inferred from broker logs
- [x] #3 mrfox7688 is given a straight answer on whether the clean disconnects are expected behaviour from the beta
- [x] #4 Any change preserves the TASK-769/TASK-1134 guarantee: the broker never sees a truncated or malformed publish
- [x] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
RESEARCH: can a half-written PUBLISH be closed off without dropping the connection?

Short answer: not at the protocol level. MQTT 3.1.1 has no way to abort a packet. The fixed header carries a Remaining Length, and from the moment those bytes are on the wire the broker will consume exactly that many more bytes as this packet. There is no cancel, no escape sequence and no resynchronisation marker in the protocol. Once a partial PUBLISH is out, the only correct outcomes are: finish the declared byte count, or destroy the transport. That is a property of MQTT, not of PubSubClient or of this firmware, and no amount of local cleverness changes it.

So the useful question is not how to abort, but how never to arrive there.

WHAT THE STACK ACTUALLY DOES, verified in Core 2.7.4.

- WiFiClient sets _timeout = 5000 in its constructor (WiFiClient.cpp:81 and :88) and applies it with _client->setTimeout(_timeout) before every write (WiFiClient.cpp:169).
- ClientContext::_write_from_source loops: push what fits with _write_some(), then block in a 1 ms delay loop until the peer ACKs free up space, and give up when _is_timeout().
- _is_timeout() is millis() - _op_start_time > _timeout_ms, and _op_start_time is reset on every _write_some() that makes progress (ClientContext.h:441-444).

That last detail matters and corrects the loose wording in our own CHANGELOG. The 5 seconds is a NO-PROGRESS timeout, not a total duration. A short write therefore does not mean "the buffer was briefly full". It means the socket accepted zero bytes for five consecutive seconds. That is a severe condition.

Two consequences follow immediately:
1. For mrfox7688, reconnecting every 30 to 60 seconds means his TCP socket stalls completely for five seconds, over and over. That is a network or Wi-Fi problem surfacing as an MQTT symptom, not a tuning parameter we can turn.
2. Every one of those events also blocks the cooperative loop for five seconds inside write(). That cost is invisible today and nobody has accounted for it. It is arguably worse than the reconnect it produces.

WHERE THE ASYMMETRY IS.

- Payload half: writeMqttChunk (MQTTstuff.ino:285-306) already retries, MQTT_WRITE_MAX_RETRIES = 10, with feedWatchDog() and yield() between attempts. It only gives up, and disconnects, after those retries.
- Header half: beginPublish (PubSubClient.cpp:526-541) writes the fixed header and topic in ONE _client->write() and returns true only if the full count went out. There is no retry, and no way to add one from outside: PubSubClient does not tell us how many bytes went out, and its assembly buffer is internal. endPublish() is a no-op that returns 1 (PubSubClient.cpp:543-545), so there is no framing state to interrogate either.

So the header is the half that cannot be recovered, and it is also the half with no retry.

OPTIONS EVALUATED.

A. Prevention: refuse to start a packet that cannot be framed.
   WiFiClient::availableForWrite() exists on 2.7.4 and returns tcp_sndbuf(_pcb) (WiFiClient.cpp:209-211, ClientContext.h:162-165), which is exactly the quantity that decides whether a write will be short. wifiClient is a global in OTGW-firmware.h:525, so it is directly reachable without a PubSubClient getter.
   Checking that the header plus topic fits before calling beginPublish() costs one comparison and removes the unrecoverable case entirely. A publish that cannot be framed is deferred and counted, never started.
   Caveat: TCP_SND_BUF is 2 * TCP_MSS (lwipopts.h:1326), so requiring the WHOLE packet to fit would refuse large discovery configs that would otherwise succeed, because write() legitimately completes them across several ACK rounds. Requiring only the header plus topic to fit avoids that: it is small, almost always available, and it is the only part with no recovery path.
   VERDICT: best option. Strictly reduces disconnects, and it also avoids the hidden five-second loop stall for the refused case.

B. Write the header ourselves, with the same retry the payload has.
   Build the PUBLISH fixed header, the Remaining Length varint and the topic by hand, then write them through MQTTclient.write(buf, len), which is a thin passthrough (PubSubClient.cpp:552-555), using the writeMqttChunk retry loop. That removes the asymmetry completely rather than avoiding it.
   Costs: reimplementing varint Remaining Length encoding correctly, and taking ownership of a piece of protocol framing PubSubClient currently owns. A bug here produces exactly the malformed packets we are trying to prevent.
   VERDICT: viable, more surface, only worth doing if A proves insufficient.

C. Pad the packet to its declared length.
   If K of N declared bytes are out, write N-K filler bytes so the framing stays valid and only one payload is corrupt.
   This does not work, and the reason is decisive: the situation only arises because the socket refuses bytes. The padding needs the same socket. If we could write the padding we could have written the real payload. It trades a guaranteed gap for a corrupted value that probably cannot be delivered either.
   VERDICT: reject.

D. Current behaviour: MQTTclient.disconnect() at MQTTstuff.ino:359 and :1059.
   Correct as a last resort and must stay. It is also the direct cause of the "connection closed by client" lines in mrfox7688's broker log, because disconnect() sends a clean MQTT DISCONNECT.

RECOMMENDED SHAPE.

1. Pre-flight the header plus topic against availableForWrite(). Defer and count when it does not fit. This is where the unrecoverable case is eliminated.
2. Keep the existing payload retry.
3. Keep the disconnect as the final fallback, because nothing else is correct once a partial packet is on the wire.
4. Count both the deferrals and the remedy firings, and expose them over REST. Right now the only way to know how often this happens is to read the broker's log, which is the same blindness TASK-1148 removed for the telnet console.

WHAT TO TELL THE REPORTER.
The clean disconnects are the fix working as designed. The thing worth chasing on his side is why the socket stalls for five seconds at a time, because that is what each disconnect proves happened.

UNVERIFIED, flagged rather than asserted: the numeric value of TCP_MSS on this build was not found in lwipopts.h, so the exact sndbuf figure (2 * MSS) is not pinned down. It does not change any conclusion above, since option A deliberately only requires the small header to fit, but anyone tuning a threshold should measure it first.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stops the firmware starting a PUBLISH it cannot frame, which is the only point where the reconnects reported on GH #682 could be prevented rather than cured.

## The protocol answer to the question asked

There is no way to close off a half-written MQTT packet without breaking the transport. MQTT 3.1.1 has no abort: once a PUBLISH fixed header carrying a Remaining Length is on the wire, the broker will consume exactly that many following bytes as this packet. Finish the declared count, or destroy the connection. That is a property of the protocol, not of PubSubClient or this firmware.

So the fix is prevention, not recovery.

## What changed

beginMqttPublish() pre-flights wifiClient.availableForWrite() against the part that has no recovery path: the fixed header byte, the Remaining Length varint, the two-byte topic length and the topic. beginPublish() emits exactly that in one write() and reports only whether the full count went out, PubSubClient does not say how many bytes left, and endPublish() is a no-op, so nothing can resume it. The payload half already retries in writeMqttChunk().

Only the header is checked, deliberately. TCP_SND_BUF is 2 * TCP_MSS, so demanding room for the whole packet would refuse large discovery configs that legitimately complete across several ACK rounds.

Counters mqtt_sndbuf_skips and mqtt_desync_drops are exposed in /api/v2/device/info. The second one counts both remedy sites, header and payload.

## A wrong first version, and how it was caught

The first implementation deferred as soon as availableForWrite() came up short. On a post-OTA boot that skipped 35 to 40 publishes, because the 766 KB upload had just saturated the radio and the buffer was transiently full. Those publishes would have succeeded after a wait of milliseconds, so the change traded a brief delay for a dropped message, which is worse than the defect.

It was caught by measuring the counter on a healthy device rather than only under the fault. The check now gives lwIP the same bounded drain chance writeMqttChunk() already used, and defers only when the room never appears.

## Measured

| scenario | sndbuf_skips | desync_drops |
|---|---|---|
| clean reboot, healthy broker | 0 | 0 |
| post-OTA boot, defer-on-first-look | 35-40 | 0 |
| post-OTA boot, with the drain wait | 0 | 0 |
| broker stops reading, 150 s | 38 | 0 |

The last row is the proof. Against a broker that completes the handshake and then never reads again, 38 publishes were deferred and the desync remedy never fired. The link did close eventually, on keepalive, which is correct for a dead broker and is not counted as a desync drop.

## What this does not do

It lowers how often the disconnect fires; it cannot remove it. availableForWrite() is a snapshot and nothing reserves that space between the check and beginPublish()'s write. The drop therefore stays as the last resort, because once a partial packet is out nothing else is correct.

## Rejected alternatives, with reasons

- Padding the packet to its declared length to keep framing valid. Rejected: the situation only exists because the socket refuses bytes, and the padding needs that same socket. If the padding could be written, the real payload could have been.
- Writing the header by hand so it can be retried like the payload. Viable but it moves protocol framing out of the library, and a mistake there produces exactly the malformed packets this is meant to prevent. Kept in reserve if the pre-flight proves insufficient.
- Temporising the discovery burst. Not needed: the drip timer, DISCOVERY_INTERVAL_SLOW under heap pressure and the status-burst window already pace it, and a clean boot on a calm network produces no deferrals at all.

## Correction carried into the record

Our own notes and CHANGELOG described the short write as happening "until the 5 second socket timeout", which reads as a total duration. WiFiClient sets _timeout = 5000 and ClientContext::_is_timeout() resets its clock on every byte of progress (ClientContext.h:441-444), so it is a no-progress timeout. A short write proves the socket accepted zero bytes for five consecutive seconds, and also blocks the cooperative loop for that time. That severity is the real story behind mrfox7688's reconnect frequency.

## Cost and verification

4 bytes of static RAM (52728 to 52732). Build green, evaluator 38 checks and 0 failures, validated on the bench gateway at 192.168.88.68 with a purpose-built stalling broker. Broker settings restored afterwards; the stored MQTT password was never touched, because the settings POST takes one field per request.
<!-- SECTION:FINAL_SUMMARY:END -->
