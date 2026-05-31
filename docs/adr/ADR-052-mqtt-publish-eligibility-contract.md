# ADR-052: MQTT Publish Eligibility and Reconnect Refresh Contract

## Status

Accepted, 2026-03-17.

## Context

ADR-006 established MQTT as the primary Home Assistant integration path and later added configurable publish gating for OpenTherm values and status bits. That ADR captures the overall MQTT architecture, but it does not define a single precise contract for when a value is eligible to publish, how stale-value refresh behaves, or how MQTT reconnect should reset publish state.

Recent troubleshooting around `msgid 0` status topics, especially binary sensors such as `flame`, showed that developers and users were reasoning from slightly different interpretations of the intended behavior:

- Some read the interval as a minimum spacing between publishes.
- Some read it as a maximum stale age that forces a refresh.
- Some expected first observation after MQTT reconnect to behave like a new first sighting for every tracked topic.
- Some expected the combined `status_master` / `status_slave` topics and the per-bit topics to follow identical triggering rules.

That ambiguity is dangerous because MQTT state topics are part of an external contract with Home Assistant and other subscribers. The firmware needs one explicit, testable rule set for:

- normal OpenTherm message IDs,
- combined status-byte topics derived from `msgid 0`,
- per-bit status topics derived from `msgid 0`, and
- reconnect behavior when retained broker state may have been lost.

### Constraints

- **ESP8266 memory limits:** publish tracking must remain bounded and static-buffer friendly (ADR-004, ADR-044).
- **Home Assistant expectations:** retained state topics must be restored after broker reconnects without requiring a full device reboot (ADR-006).
- **OpenTherm fan-out model:** publish decisions must fit the synchronous OT processing path and PS=1 summary translation path (ADR-038, ADR-046).
- **Reconnect behavior:** MQTT reconnects are routine and must not require manual republishing by the user (ADR-047).

## Decision

**Define MQTT publish eligibility as a first-seen OR value-changed OR stale-refresh contract, with MQTT reconnect resetting first-seen state for all tracked message IDs and all tracked status-bit topics.**

### Contract for normal message IDs

For any tracked non-status OpenTherm message ID, a publish is eligible when any of the following is true:

1. The topic is seen for the first time.
2. The value changed since the last successful publish.
3. The topic has not been published for at least `XX` seconds.

`XX` is the configured publish interval setting and is interpreted as a **maximum stale age**, not as a minimum spacing that suppresses real changes.

In formula form:

$$
publish = firstSeen \lor valueChanged \lor (now - lastPublished \ge XX)
$$

### Contract for MQTT reconnect

When MQTT reconnects successfully, the firmware resets publish state for all tracked OpenTherm message IDs and all tracked `msgid 0` per-bit topics.

This means the next observed value for each tracked topic is treated as `firstSeen` again, even if the value has not changed since before the disconnect.

This reset applies to:

- tracked normal message IDs,
- `status_master`,
- `status_slave`,
- each master status bit topic, and
- each slave status bit topic.

### Contract for `msgid 0` combined status topics

`msgid 0` represents two independent status bytes:

- master status byte,
- slave status byte.

These publish to the combined topics:

- `status_master`,
- `status_slave`.

Each combined topic is eligible to publish when:

1. It is first seen.
2. Any bit within that byte changed.
3. It has not been published for at least `XX` seconds.
4. MQTT has reconnected and the next observed byte is being treated as first seen.

### Contract for `msgid 0` per-bit topics

`msgid 0` also expands into independent per-bit topics. Each bit topic has its own publish state and is evaluated independently.

Each per-bit topic is eligible to publish when:

1. That bit topic is first seen.
2. That bit changed state.
3. That bit topic has not been published for at least `XX` seconds.
4. MQTT has reconnected and the next observed bit state is being treated as first seen.

If multiple bits change in the same status frame, all affected per-bit topics publish from that one frame.

Master and slave bits both follow the same rules, but each topic keeps its own independent publish history.

### Interpretation of `XX = 0`

When the configured publish interval is `0`, the firmware stays in legacy always-publish mode for compatibility.

In that mode:

- every observed value is eligible to publish immediately,
- every observed status byte is eligible to publish immediately,
- every observed status bit is eligible to publish immediately.

## Alternatives Considered

### Alternative 1: Treat `XX` as a minimum spacing between any two publishes
**Pros:**
- Simple rate-limiting model
- Predictable upper bound on broker traffic

**Cons:**
- Suppresses real changes that happen faster than `XX`
- Breaks the expected semantics for binary sensors such as `flame`
- Makes automation state lag behind actual boiler state

**Why not chosen:** Real state changes must always be publishable immediately. `XX` is a stale-refresh deadline, not a debounce window.

### Alternative 2: Publish only on change and never force refresh
**Pros:**
- Lowest broker traffic
- Very simple state tracking

**Cons:**
- Subscribers that reconnect later may miss stable retained values if broker state was lost
- Harder to recover after broker restarts or reconnect churn
- Leaves long-lived stable binary sensors looking stale from the subscriber perspective

**Why not chosen:** The firmware must be able to restore MQTT state over time even when values stay stable.

### Alternative 3: Re-publish everything immediately on MQTT reconnect
**Pros:**
- Restores broker state quickly
- Easy to reason about

**Cons:**
- Can create burst traffic for many never-used or rarely-used topics
- Conflicts with the existing just-in-time discovery and observed-value flow
- Adds unnecessary traffic on routine reconnects

**Why not chosen:** Resetting publish eligibility and republishing on the next observation preserves correctness while keeping traffic bounded by actual observed data flow.

### Alternative 4: Track `msgid 0` only as combined bytes and derive per-bit topics from the combined publish result
**Pros:**
- Less tracking state
- Fewer publish decisions

**Cons:**
- Cannot independently refresh stale per-bit topics
- Prevents correct per-bit first-seen and reconnect semantics
- Makes binary sensor behavior dependent on unrelated bits in the same byte

**Why not chosen:** Per-bit MQTT topics are externally visible state topics and need independent publish eligibility.

## Consequences

### Positive
- **Clear contract:** developers and users can reason about MQTT behavior from one rule set.
- **Correct binary sensor semantics:** status bits such as `flame` and `centralheating` publish immediately on change.
- **Reconnect recovery:** broker state can be restored after reconnect without requiring manual intervention.
- **Bounded refresh traffic:** stable values are refreshed periodically instead of every loop iteration.
- **Testability:** the contract can be verified with deterministic publish-gate tests.

### Negative
- **More state bookkeeping:** combined status bytes and per-bit topics both need tracked publish state.
- **Reconnect burst potential:** the first observation after reconnect can trigger multiple refresh publishes across active topics.
- **Implementation care required:** handlers must avoid re-entrancy or out-of-order state updates around yield points.

### Risks & Mitigation
- **Risk:** Ambiguous implementation could still suppress binary sensor publishes even with a clear ADR.  
  **Mitigation:** Keep the publish rule explicit in helper functions and add targeted tests for status-bit transitions and reconnect refresh.

- **Risk:** Combined status topics and per-bit topics could drift into different semantics.  
  **Mitigation:** Define both behaviors here and require both to use the same first-seen/change/stale-refresh model.

- **Risk:** MQTT reconnect handling could restore only normal message IDs but forget per-bit topics.  
  **Mitigation:** Reconnect reset must clear tracking for all tracked message IDs and all tracked status-bit slots.

## Related Decisions

- **ADR-004:** Static Buffer Allocation Strategy
- **ADR-006:** MQTT Integration Pattern
- **ADR-038:** OpenTherm Message Data Flow Pipeline
- **ADR-041:** JIT HA Discovery
- **ADR-046:** PS=1 Summary Translation with Shared Publish Helpers
- **ADR-047:** Non-Blocking WiFi Reconnect State Machine
- **ADR-044:** Global State — extern Declaration in Header, Definition in .ino

## References

- Implementation area: `src/OTGW-firmware/OTGW-Core.ino`
- Implementation area: `src/OTGW-firmware/MQTTstuff.ino`
- Current publish tracking state: `mqttlastsent[256]`, `mqttlastsentstatusbit[16]`
- MQTT interval setting: `settings.mqtt.iInterval`

## Enforcement

```json
{
  "llm_judge": true
}
```
