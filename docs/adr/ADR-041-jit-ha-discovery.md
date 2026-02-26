# ADR-041: Just-In-Time Home Assistant MQTT Discovery

**Status:** Accepted
**Date:** 2026-02-24
**Updated:** 2026-02-24 (Gap 2 resolved; `bForceAll` parameter removed; Serial `m` removed)
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)

## Context

Home Assistant MQTT auto-discovery works by publishing a JSON config payload to a well-known
retained topic (`homeassistant/<domain>/<node_id>/<object_id>/config`). HA subscribes to
`homeassistant/#` and registers entities from those retained payloads.

The firmware's discovery configuration lives in `/mqttha.cfg` on LittleFS, containing one entry
per OpenTherm message ID (≈200 entries covering the full OpenTherm spec).

### Prior Design: Two-Path Discovery

The original implementation had two paths:

**Path A — Bulk (startup sweep):**
On every successful MQTT connection, `doAutoConfigure()` iterated all entries in `mqttha.cfg`
and published discovery configs for every message ID unconditionally.
_Trigger:_ `handleMQTT()` connection-success branch, `MQTTstuff.ino`.

**Path B — Just-In-Time (per message):**
Inside `processOT()`, after decoding a valid OpenTherm frame, the firmware checks
`getMQTTConfigDone(id)`. If the bit is not set, it calls `doAutoConfigureMsgid()` to publish
the discovery config for that specific message ID, then sets the bit.
_Trigger:_ `OTGW-Core.ino`, every decoded OT message.

A 256-bit array (`MQTTautoConfigMap[8]`) tracks which message IDs have had their discovery
config published. Both paths use `setMQTTConfigDone()` / `getMQTTConfigDone()` to read/write
this state.

### Problem With Path A

Path A publishes discovery configs for **all ~200 entries in `mqttha.cfg`**, regardless of
whether a specific message ID has ever been seen on the OpenTherm bus. This causes:

- **Entity noise in HA:** Entities appear immediately at `unknown` state and may never receive
  a value if the boiler/thermostat combination does not transmit that message ID.
- **Unnecessary flash I/O:** Reads all of `mqttha.cfg` on every MQTT connect, including after
  transient reconnects.
- **Misaligned semantics:** Configuring topics for messages that are never seen contradicts the
  intent of discovery ("tell HA what this device actually reports").

Path B already handles every case correctly for message IDs that are actually seen. The only
scenario Path A addressed that Path B did not was: "publish discovery for message IDs that will
never appear on the bus" — which is not desirable.

## Decision

**Drop Path A from all automatic triggers. Rely exclusively on Path B (JIT) for discovery.**

Retain `doAutoConfigure()` as an explicit force-all utility, callable via:
- Serial debug command `F` (existing, unchanged)
- REST endpoints `POST /api/v1/otgw/autoconfigure`, `POST /api/v2/otgw/discovery`,
  `POST /api/v2/otgw/autoconfigure` (updated to always pass `bForceAll=true`)

### Recovery Mechanism: Clear Bitfield on Every MQTT Connect

Because Path B only fires when a message ID's bit is **not** set, the bitfield must be cleared
whenever the MQTT broker may have lost its retained messages (e.g. broker restart). The chosen
mechanism is: **call `clearMQTTConfigDone()` on every successful MQTT connection**, in the
`MQTT_STATE_TRY_TO_CONNECT` success branch of `handleMQTT()`.

This ensures that after any reconnect — including broker restart recovery — Path B re-publishes
discovery configs as OpenTherm messages arrive (typically within seconds for frequently-polled
values like room temperature and setpoints).

### Desired Behaviour (Target State)

The intended trigger table is:

| Trigger | Intended Action |
|---|---|
| ESP boot | Bitfield initialises to 0; Path B handles everything as messages arrive |
| MQTT reconnect, `sessionPresent=1` | Broker retained messages intact; **do not** reset bitfield |
| MQTT reconnect, `sessionPresent=0` | Broker lost state; clear bitfield → Path B re-publishes on next messages |
| HA restart (`homeassistant/status` → `online`) | Clear bitfield only (no reconnect needed); Path B re-publishes on next messages |

### Current Implementation Gap

#### Gap 1: `sessionPresent` Not Exposed by PubSubClient v2.8

The MQTT 3.1.1 CONNACK packet includes a `sessionPresent` flag:
- `1` = broker retained the client session (subscriptions, queued messages survived)
- `0` = no prior session (broker restarted, or clean-session connection)

**PubSubClient v2.8 does not expose this flag.** `MQTTclient.connect()` returns only a `bool`.
There is no API to read the CONNACK `sessionPresent` value.

As a result, the firmware cannot distinguish a reconnect where retained messages are intact
(`sessionPresent=1`) from one where the broker lost state (`sessionPresent=0`). The current
implementation **always calls `clearMQTTConfigDone()` on connect**, regardless of broker state.

This is functionally correct — Path B harmlessly re-publishes the same values as discovery
configs are already retained on the broker — but it causes unnecessary re-publishing after
transient reconnects where the broker is fine.

#### Gap 2: HA Restart Triggers a Full MQTT Reconnect — **Resolved**

~~When `homeassistant/status` → `online` is received, the handler calls `startMQTT()`, which
disconnects and reconnects to the MQTT broker unnecessarily.~~

**Resolved:** The `homeassistant/status` → `online` handler now calls only `clearMQTTConfigDone()`
(`MQTTstuff.ino`). The MQTT broker is unaffected by an HA restart, so no reconnect is needed.
Path B re-publishes discovery configs as OpenTherm messages arrive.

### Future: Implementing the Full Target Table

When a MQTT client library available for ESP8266/Arduino exposes `sessionPresent` from CONNACK,
the implementation should be updated as follows:

**In the `MQTT_STATE_TRY_TO_CONNECT` success branch of `handleMQTT()`:**

```cpp
// Pseudo-code — adapt to actual library API
bool sessionPresent = MQTTclient.sessionPresent(); // hypothetical API
if (!sessionPresent) {
  // Broker lost state (restarted or clean session); retained messages may be gone.
  // Clear bitfield so JIT re-publishes discovery as OT messages arrive.
  clearMQTTConfigDone();
  MQTTDebugTln(F("MQTT: session not present, discovery state reset for JIT re-publish"));
} else {
  // Broker retained our session; retained discovery messages are intact.
  // No action needed — bitfield remains valid.
  MQTTDebugTln(F("MQTT: session present, retained discovery messages intact"));
}
```

**In the `homeassistant/status` → `online` handler (`handleMQTTcallback()`):**

Already implemented. The handler calls `clearMQTTConfigDone()` directly; no MQTT
disconnect/reconnect is performed.

**Libraries to evaluate when upgrading:**

- [`async-mqtt-client`](https://github.com/marvinroger/async-mqtt-client): exposes `sessionPresent`
  in the connect event callback — suitable for ESP8266, but requires `ESPAsyncTCP`.
- A patched PubSubClient that surfaces the CONNACK `sessionPresent` byte (byte 2 of the
  CONNACK variable header, bit 0).

## Alternatives Considered

### Alternative 1: Keep Path A (bulk sweep at startup)

**Pros:** All HA entities appear immediately at boot, even before OT messages arrive.

**Cons:** Publishes configs for message IDs that may never be seen, creating permanent `unknown`
entities in HA. Unnecessary flash I/O on every reconnect.

**Why not chosen:** The "only configure what's actually seen" goal is more important than
immediate entity appearance. Entities appearing progressively as messages arrive is correct
and desirable behaviour.

### Alternative 2: Persist bitfield to flash (survive reboots)

Store `MQTTautoConfigMap` in LittleFS or EEPROM so message IDs known from previous sessions
are not re-published after a reboot.

**Pros:** After first boot, subsequent reboots produce no discovery publishing at all (broker
retains everything, bitfield is pre-populated).

**Cons:** Adds flash write cycles for a 32-byte structure. Stale if `mqttha.cfg` changes or
settings (node ID, topic prefix) change — would require invalidation logic. Adds complexity
without meaningful benefit since Path B is fast.

**Why not chosen:** Unnecessary complexity; the cost of re-publishing retained messages after
reboot is negligible.

### Alternative 3: Subscribe to own discovery topics to detect broker state

On MQTT connect, subscribe to `homeassistant/+/<node_id>/+/config` with a short timeout.
If no messages come back, assume the broker lost retained messages and clear the bitfield.

**Pros:** Works with PubSubClient without library changes.

**Cons:** Requires a timed window (non-trivial on cooperative ESP8266), generates extra
subscribe/receive traffic, fragile timing, complex state machine addition.

**Why not chosen:** Overly complex workaround. The always-clear-on-connect simplification
achieves the same result with trivial code.

### Alternative 4: Remove `bForceAll` parameter; always force in `doAutoConfigure()` — **Implemented**

Since Path A is no longer called automatically, the `bForceAll=false` path (used only by
Serial `m`) was effectively a no-op in normal operation — all bits are cleared on connect and
set by Path B as messages arrive.

**Implemented:** `doAutoConfigure()` no longer takes a `bForceAll` parameter; it always
force-publishes all configured message IDs. Serial `m` command removed; Serial `F` and all
REST endpoints now call the parameterless `doAutoConfigure()`.

## Consequences

### Positive

- **No entity noise:** HA only receives discovery configs for message IDs actually transmitted
  by the connected boiler/thermostat.
- **Lower flash I/O:** `mqttha.cfg` is no longer read on every MQTT connect.
- **Self-healing:** After broker restart, discovery repopulates naturally as OT messages flow
  in — no manual intervention required.
- **Simpler connect handler:** One `clearMQTTConfigDone()` call replaces the bulk file-parse
  loop at connect time.
- **Force command preserved:** `doAutoConfigure(true)` via Serial `F` / REST endpoints gives
  operators a way to force a full rediscovery on demand.

### Negative / Risks

- **Progressive entity appearance:** After boot or reconnect, HA entities appear one-by-one
  as messages arrive rather than all at once. For frequently-polled values this is seconds;
  for rare message IDs it may be minutes or never.
- **Rare message IDs:** Fault codes or diagnostic message IDs that fire only during error
  conditions will not have HA entities until the condition occurs. This is arguably correct
  behaviour — the entity appears precisely when it becomes relevant.
- **Always-clear on reconnect:** Until `sessionPresent` is available, every transient MQTT
  reconnect triggers a round of Path B re-publishing (harmless, but unnecessary when broker
  is fine).

## Related ADRs

- [ADR-004](ADR-004-static-buffer-allocation.md) — Static buffer allocation (scratch buffers
  used by `doAutoConfigure` and `doAutoConfigureMsgid`)
- [ADR-006](ADR-006-mqtt-integration-pattern.md) — MQTT integration pattern
- [ADR-040](ADR-040-mqtt-source-specific-topics.md) — Source-specific topics; source-template
  expansion in `doAutoConfigureMsgid` / `expandAndPublishSourceTemplates`
