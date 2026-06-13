# ADR-131 MQTT Engine on espMqttClient: Async Connect, Single Publish Chokepoint (ADR-123 Phase 2)

## Status

Proposed. Date: 2026-06-13.

This is the **Phase 2** networking step that ADR-123 (the 2.0.0 concurrency-model
decision) anticipated: *"MQTT to espMqttClient (ESP32 path); re-validate the HA
pipeline; supersede ADR-108 on the ESP32 side."* ADR-123 deferred the concrete
engine surface to the ADR that lands the phase. This ADR carries that surface and
**supersedes ADR-108** (MQTT `connect()` accepted as a synchronous loop-blocker),
whose premise no longer holds once `connect()` is asynchronous.

## Status History

status_history:
  - date: 2026-06-13
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-2 MQTT engine swap (PubSubClient to espMqttClient) delivered by TASK-865.7; supersede ADR-108
    changed_via: adr-kit

## Context

The 2.0.0 firmware ran MQTT on **PubSubClient** (vendored, synchronous). Its
`connect()` blocks the cooperative `loop()` for up to the socket-timeout window
while the TCP handshake and CONNECT/CONNACK complete. ADR-108 documented that
stall as an accepted sync-blocker bounded at 5 s, with the explicit caveat that
replacing the client *"would need its own ADR if pursued."* This is that ADR.

ADR-123 (Accepted) chose event-driven async networking for the 2.0.0 ESP32-S3
line and named `espMqttClient` (bertmelis) as the MQTT client, the same one
EMS-ESP32 runs (`other-projects/EMS-ESP32-dev`). espMqttClient's `connect()` is
**non-blocking**: it queues the TCP + CONNECT work and returns immediately, with
the result arriving later via `onConnect` / `onDisconnect` callbacks. That single
property dissolves the whole sync-blocker class ADR-108 accepted: there is no
synchronous connect to bound, so `setSocketTimeout(5)` is removed and ADR-108's
5 s envelope ceases to exist.

The swap is larger than a one-line client substitution because PubSubClient and
espMqttClient differ on three load-bearing axes, each of which forces a design
decision that this ADR records:

1. **Threading.** espMqttClient can run its own internal FreeRTOS task that fires
   callbacks off the main loop. This firmware is single-threaded on the MQTT path
   (static scratch buffers, `doBackgroundTasks()` re-entrancy assumptions, no
   locks around `OTGWState` MQTT fields). An internal task would detonate every
   one of those assumptions.
2. **Publish API.** PubSubClient exposed a streaming
   `beginPublish` / `write` / `endPublish` API; the old code used it with a 128 B
   chunker (an ESP8266-heap workaround) and a TASK-770 "disconnect on truncated
   write" desync guard. espMqttClient has **no** streaming API: `publish()` copies
   `topic + payload` into its Outbox atomically and frames the packet itself.
3. **Pointer lifetime.** espMqttClient **stores** the pointers handed to
   `setClientId` / `setWill` / `setCredentials` (it does not copy them until
   `connect()` builds the CONNECT packet), where PubSubClient copied at call time.

The migration also moots ADR-108's premise and shifts the meaning of the ADR-104
publish-success commit point (see Decision item 6). The HA discovery/retained/LWT/
availability pipeline (ADR-041/052/077/088/096/097/102/104) had to be re-validated
against the new client; this ADR records the points where its behaviour changed.

## Decision

Replace PubSubClient with **espMqttClient** as the 2.0.0 MQTT engine, on the terms
below. (Implemented by TASK-865.7 in `MQTTstuff.ino` / `MQTTstuff.h` /
`MQTTHaDiscovery.cpp`; the `lib_deps` entry is added by the seq4 prerequisite and
the `PubSubClient` entry is removed from `platformio.ini`.)

1. **`UseInternalTask::NO` is load-bearing.** The client is constructed as
   `espMqttClient MQTTclient(espMqttClientTypes::UseInternalTask::NO)`. With `NO`,
   the engine is pumped **only** by our explicit `MQTTclient.loop()` from
   `handleMQTT()` (and the reboot drain), so `onConnect` / `onMessage` /
   `onDisconnect` run on the same cooperative loop as the rest of
   `doBackgroundTasks()`. This preserves the single-threaded static-buffer and
   re-entrancy contract the firmware depends on. `loop()` is now called
   **unconditionally** every tick (not gated on `connected()`), because under
   `NO` it is `loop()` that drives the connect state machine: gating it on
   `connected()` would stall the handshake forever.
   *Forward caveat:* if MQTT later moves onto its own FreeRTOS task
   (ADR-123 Phase 3+, seq9/10/11), that task becomes the sole `loop()` pumper and
   this single-threaded re-entrancy contract must be re-evaluated there.

2. **Async connect, non-blocking state machine.** `MQTT_STATE_TRY_TO_CONNECT`
   calls `MQTTclient.connect()` (queues work, returns immediately) and arms a
   per-attempt backoff timer, then advances to `MQTT_STATE_WAIT_CONNECTION_ATTEMPT`.
   The outcome lands via callbacks: `onConnect` advances to
   `MQTT_STATE_IS_CONNECTED`; `onDisconnect` clears the live flag. The thin
   retry/backoff (3 s, 6 s, 9 s, 12 s; 10-minute fallback after 5 failures) is
   retained. There is **no** synchronous stall, so `setSocketTimeout(5)` is
   removed; `setKeepAlive(60)` and `setCleanSession(true)` carry over.

3. **Post-connect work moves into `onConnect`.** Birth (retained `online`),
   topic subscribe, conditional republish/discovery reset, V2-migration window,
   versioninfo, and PIC-settings publish all run from `onMqttConnect()` (when the
   broker's CONNACK actually lands), not inline after a synchronous `connect()`
   returned true. `state.mqtt.bConnected` is written by `onMqttConnect` /
   `onMqttDisconnect` (authoritative) and mirrored once at the tail of
   `handleMQTT()` from `MQTTclient.connected()` as a missed-edge backstop.

4. **Single publish chokepoint: `mqttPublishRaw`.** Declared in `MQTTstuff.h` as
   `bool mqttPublishRaw(const char* topic, const uint8_t* payload, size_t len, bool retain)`
   and defined in `MQTTstuff.ino`, it is the **only** outbound-publish path in
   the firmware. It wraps `MQTTclient.publish(topic, /*qos=*/0, retain, payload, len)`
   and returns `true` iff the publish was queued (`packetId != 0`). The
   PubSubClient streaming trio (`beginPublish`/`write`/`endPublish`), the 128 B
   chunker, the PROGMEM stage buffer, and the `writeMqttChunk*Ext` forwarders are
   all removed.

5. **Buffer model: single buffered publish, no chunker.** espMqttClient frames
   each publish atomically, so the 128 B ESP8266-only chunker is gone. The HA
   discovery composer keeps its `MqttJsonWriter` two-pass shape but now MEASUREs
   the payload, `malloc`s a transient heap buffer of exactly that size, WRITEs
   into it via `memcpy` / `memcpy_P`, and hands the filled buffer to
   `mqttPublishRaw` as one atomic publish (worst-case discovery payload ~900 B,
   trivial against the ESP32-S3 DRAM budget). Because framing is atomic, the
   **TASK-770 disconnect-on-truncated-write desync guard is removed** from every
   publish helper: there is no on-wire truncation window to defend against.

6. **Pointer-lifetime contract (documented and honoured).** Because espMqttClient
   stores the pointers until `connect()` builds the CONNECT packet, every pointer
   passed to it must reference storage that outlives the connect call:
   `setClientId(MQTTclientId)` and `setWill(MQTTPubNamespace, 0, true, "offline", 7)`
   point at file-static buffers + a `.rodata` literal; `setCredentials(...)` points
   into the persistent `settings.mqtt` struct, never a stack temporary. The **LWT
   must be set before `connect()`** so it is embedded in the CONNECT packet; birth
   is published from `onConnect`. This contract is captured in an in-code comment at
   the call site.

7. **ADR-104 commit semantics shift from "sent" to "queued."** PubSubClient's
   `endPublish()` success meant bytes confirmed on the wire; espMqttClient has no
   synchronous on-wire signal, so `mqttSendSuccessCount` (and therefore the
   ADR-104 / OTPublishGate slot commit-or-discard point) now counts publishes
   **queued into the Outbox**. The slot semantics are unchanged: a queued publish
   is the commit point. The verify-window RX-buffer grow/shrink (ADR-062) becomes a
   no-op (espMqttClient's RX buffer is a fixed `EMC_RX_BUFFER_SIZE`, 1440 B,
   already larger than the old verify target and the ~900 B retained config), so
   `verifyAccessorSetMqttBufferSize` / `…Restore…` always report success.

8. **Inbound via `onMessage` shim.** The library delivers messages as
   `(props, topic, payload, len, index, total)` and may split an oversize payload
   across calls. Every subscribed topic is tiny (a few bytes of `set/<nodeId>/<cmd>`,
   `online`/`offline`, or a topic-name-only verify filter), all far below the RX
   buffer, so a real message always arrives whole. The shim asserts
   `index == 0 && len == total` (dropping pathological continuation chunks) and
   forwards into the existing `handleMQTTcallback(char*, byte*, len)` dispatcher,
   which keeps the PIC/OTDirect command routing and the `copyMQTTPayloadToBuffer`
   guard unchanged.

**ADR-108 is superseded by this ADR.** Its accepted 5 s `setSocketTimeout`
envelope existed only to bound a synchronous `connect()` that no longer exists.
ADR-108's body and Status are left untouched per the immutable-ADR rule; this ADR
records the supersession.

## Alternatives Considered

### Alternative 1: Keep PubSubClient inside a dedicated FreeRTOS task (ADR-123 Alt 3)

Run the existing synchronous PubSubClient unchanged, but inside its own FreeRTOS
task so its blocking `connect()` no longer stalls the cooperative loop.

**Pros:** smallest code delta on the MQTT path; familiar synchronous flow; no new
client API to learn; espMqttClient's own author notes a dual-core ESP32 can simply
publish synchronously from a task.

**Rejected:** it keeps PubSubClient's synchronous connect and the 128 B chunker,
inflates RAM with a per-task stack, and, most importantly, forgoes the maintained
`espMqttClient` ecosystem that ADR-123 chose for the line and that EMS-ESP32 proves
in the field. ADR-123 explicitly identifies this only as a *fallback* if the async
stack's maintenance risk materialises, not as the primary model. Adopting it here
would also leave ADR-108's sync-blocker in place, contradicting ADR-123's Phase-2
goal.

### Alternative 2: AsyncMqttClient (marvinroger)

Use the other well-known async MQTT client for the ESP32.

**Rejected:** EMS-ESP32 (the architectural blueprint ADR-123 follows) migrated
*away* from AsyncMqttClient to espMqttClient (issue #1178) precisely because
espMqttClient is the actively maintained successor with a cleaner buffered-publish
and LWT story. Choosing the client EMS-ESP32 left behind would walk into the
maintenance and correctness issues they already escaped.

### Alternative 3: espMqttClient with `UseInternalTask::YES`

Let the library spawn its own task and fire callbacks asynchronously, the simplest
way to "be async."

**Rejected:** callbacks would run off the cooperative loop, concurrently with
`doBackgroundTasks()`. Every MQTT-path static scratch buffer, the
`doAutoConfigure` re-entrancy guards, and the un-mutexed `OTGWState` MQTT fields
assume single-threaded access. `YES` would introduce exactly the race class
ADR-123 confines to a disciplined queue + mutex design, with none of that
discipline in place on the MQTT path. `NO` keeps the async benefit (non-blocking
connect) without the threading hazard; a future move to a dedicated MQTT task is
deferred to ADR-123 Phase 3+ where the locking contract is designed for it.

## Consequences

### Positive

- **No connect stall.** ADR-108's worst-case 5 s loop freeze on a dead broker is
  gone; the WebUI, graph live feed, and OT log stay responsive during a broker
  outage because `connect()` no longer blocks.
- **Simpler publish path.** One chokepoint (`mqttPublishRaw`), one buffered publish,
  no chunker, no PROGMEM stage buffer, no truncated-write desync guard (TASK-770).
  ~150 fewer lines of fragile streaming-write code.
- **Atomic framing.** The broker can no longer observe a half-written publish, so
  the disconnect-on-short-write recovery the old path needed is unnecessary.
- **Maintained dependency** aligned with EMS-ESP32, the project's async blueprint.

### Negative / Trade-offs

- **`mqttSendSuccessCount` now means "queued," not "sent on the wire."** A publish
  queued into the Outbox is counted as success even if the link drops before the
  bytes leave. For QoS 0 fire-and-forget this matches the old practical guarantee
  (PubSubClient could also lose an "successfully sent" packet to a TCP reset), but
  the ADR-104 slot-commit semantics are now anchored to the queue, not the wire.
- **Pointer-lifetime is a new footgun.** Handing `setWill` / `setClientId` /
  `setCredentials` a stack temporary would queue a dangling pointer into the next
  CONNECT. The contract is documented at the call site and all current arguments
  are file-static or persistent, but contributors must respect it.
- **Per-publish transient `malloc`.** Discovery payloads now allocate a ~900 B
  worst-case heap buffer per config instead of streaming in 128 B slices. Trivial
  on the ESP32-S3 DRAM budget, but it is a heap allocation that did not exist
  before; the `MQTT_DISCOVERY_HEAP_MIN` last-rail guard remains.

### Risks & Mitigation

- **Risk:** a missed `onConnect` / `onDisconnect` edge leaves the rest of the
  firmware reading a stale `state.mqtt.bConnected`.
  **Mitigation:** `handleMQTT()` mirrors `MQTTclient.connected()` into the flag on
  every tick as a backstop, and `MQTT_STATE_WAIT_CONNECTION_ATTEMPT` independently
  syncs to `IS_CONNECTED` if it observes `connected()`.
- **Risk:** a future move of MQTT onto its own task silently breaks the
  single-threaded assumption this ADR relies on.
  **Mitigation:** Decision item 1 records the contract and the explicit "re-evaluate
  in Phase 3+" caveat; the `UseInternalTask::NO` ctor argument is the enforcement
  point (see Enforcement).
- **Risk:** field reconnect behaviour differs from PubSubClient.
  **Mitigation:** field validation gate. Broker kill must keep the WebUI/graph
  responsive (no 5 s stall), HA availability must flip online (birth) / offline
  (LWT within keepalive), and `set/<nodeId>/<cmd>` must still reach the PIC/OTDirect
  queue, before this ADR is accepted.

## Related Decisions

- **Supersedes:** ADR-108 (MQTT `connect()` socket timeout accepted as a synchronous
  loop-blocker). Its 5 s envelope bounded a synchronous `connect()` that this ADR
  makes asynchronous; the premise is gone. ADR-108's body/Status are left untouched.
- **Child of:** ADR-123 (2.0.0 concurrency model), Phase 2. ADR-123 named
  espMqttClient and deferred this engine surface to "the ADR that lands that phase."
- **Sibling phase:** ADR-129 / ADR-130 (ADR-123 Phase 1: OT-frame queue + state
  mutex; PIC-UART dedicated task).
- **Re-validated against (unchanged behaviour, re-anchored to the new client):**
  ADR-041 / ADR-052 / ADR-077 / ADR-088 / ADR-096 / ADR-097 / ADR-102 (HA discovery
  + retained state), ADR-104 (publish-success commit point, semantics shifted to
  "queued"; Decision item 7), ADR-062 (verify-window RX buffer, now a no-op),
  ADR-042 (streaming JSON / no ArduinoJson; composer retained, output buffered),
  ADR-100 (broker-restart discovery reset, runs in `onConnect`),
  ADR-084 (V2 deprecated-topic migration window, runs in `onConnect`),
  ADR-118 (active-override retained refresh).
- **Depends on:** seq4 (TASK-865.4) for the `lib_deps` addition, seq6 (TASK-865.6)
  per the ADR-123 phase ordering.

## References

- TASK-865.7: this migration (Phase 2 / seq7).
- `src/OTGW-firmware/MQTTstuff.ino:229`: espMqttClient instance
  (`UseInternalTask::NO`).
- `src/OTGW-firmware/MQTTstuff.ino:361`: `mqttPublishRaw` definition (single
  publish chokepoint); `:1289` `onMqttConnect` (birth/subscribe/republish/
  versioninfo); `:1122` `setWill(...)` before `connect()` with the
  pointer-lifetime comment; plus `handleMQTT()` async state machine,
  `onMqttDisconnect` / `onMqttMessage`, and `PrintMQTTError` reason mapping.
- `src/OTGW-firmware/MQTTstuff.h:466`: `mqttPublishRaw` declaration; plus
  `MqttJsonWriter` buffered WRITE mode and the streaming-discovery signatures
  with the `PubSubClient&` parameter removed.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp`: discovery composer routed through
  `mqttPublishRaw` / `publishDiscoveryJson`.
- `src/OTGW-firmware/OTGW-firmware.h`: `sendMQTTData` / `mqttSendSuccessCount`
  doc updated to "queued into the Outbox" (ADR-104 commit point).
- `platformio.ini`: `PubSubClient @ 2.8.0` removed from `lib_deps`.
- `docs/adr/ADR-108-mqtt-connect-socket-timeout-as-accepted-sync-blocker.md`:
  superseded by this ADR.
- `docs/adr/ADR-123-2-0-0-concurrency-model-async-modernization.md`: parent
  concurrency-model decision (Phase 2).
- espMqttClient (bertmelis): <https://www.emelis.net/espMqttClient/>
- EMS-ESP32 AsyncMqttClient to espMqttClient migration (#1178):
  <https://github.com/emsesp/EMS-ESP32/issues/1178>

## Enforcement

Declarative `adr-judge` guards cover the two reversions that would re-introduce the
superseded design; the "single publish chokepoint" rule (all outbound publishes go
through `mqttPublishRaw`) needs call-graph reasoning a regex cannot express and is a
PR-review boundary (and the `beginPublish`/`endPublish`/`write` API it would have to
use no longer exists once PubSubClient is gone, so a reversion is self-evident in
the diff).

- `forbid_import` of `<PubSubClient.h>` in the firmware sources: re-adding it means
  the engine swap was reverted.
- `forbid_pattern` on the no-internal-task constructor: the instance must keep
  `espMqttClientTypes::UseInternalTask::NO`. A bare `espMqttClient MQTTclient(` with
  no argument (or `::YES`) spawns the internal task and breaks the single-threaded
  re-entrancy contract in Decision item 1.

```json
{
  "forbid_import": [
    { "pattern": "PubSubClient.h", "path_glob": "src/OTGW-firmware/*.{ino,h,cpp}", "message": "ADR-131: PubSubClient is superseded by espMqttClient on the 2.0.0 line. Do not re-introduce it. To change the MQTT engine, supersede ADR-131 with a new ADR." }
  ],
  "forbid_pattern": [
    { "pattern": "espMqttClient\\s+MQTTclient\\((?!espMqttClientTypes::UseInternalTask::NO)", "path_glob": "src/OTGW-firmware/MQTTstuff.ino", "message": "ADR-131 Decision item 1: the espMqttClient instance MUST be constructed with UseInternalTask::NO. The internal-task ctor fires callbacks off the main loop and breaks the single-threaded static-buffer/re-entrancy contract. Moving MQTT to its own task is deferred to ADR-123 Phase 3+ and needs a superseding ADR." }
  ],
  "require_pattern": [],
  "llm_judge": false
}
```
