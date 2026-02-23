# ADR-040: MQTT Source-Specific Topics for OpenTherm Values

**Status:** Accepted  
**Date:** 2026-02-16  
**Updated:** 2026-02-23 (Adapted to nested source topics and opt-in default)
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)

## Context

OpenTherm values can originate from multiple sources:
- Thermostat request (`T`)
- Boiler response (`B`)
- Gateway override request/answer (`R` / `A`)

Publishing all values to a single MQTT topic hides source-specific behavior. Example: the thermostat may request one setpoint while the boiler responds with a limited value, but only the last value remains visible on the shared topic.

This affects:
- Diagnostics (request vs actual behavior)
- Troubleshooting (gateway override visibility)
- Automation (different actions for thermostat/boiler/gateway values)
- Home Assistant observability (source-specific entities)

The firmware already detects source prefixes and stores source type in `OTdata.rsptype` during `processOT()`.

### Constraints

- **Backward compatibility:** Existing MQTT topics and HA integrations must continue to work.
- **Memory limits:** ESP8266 RAM is constrained (ADR-004); solution must avoid excessive heap churn.
- **MQTT reliability:** Additional publishing must respect backpressure and existing MQTT patterns (ADR-006, ADR-030).
- **HA discovery size:** Discovery generation must stay within bounded buffers and streaming limits.

## Decision

**Implement additive source-specific MQTT topics and Home Assistant discovery entries, controlled by an opt-in setting (`settingMQTTSeparateSources`, default `false`).**

When enabled, the firmware publishes:
1. **Legacy/base topic** (always): `<prefix>/value/<node-id>/<metric>`
2. **Source-specific topic** (new, additive): `<prefix>/value/<node-id>/<metric>/<source>`

Where `<source>` is:
- `thermostat`
- `boiler`
- `gateway` (for `R` and `A`)

### Topic Shape (Current Implementation)

Examples for `TSet`:

```text
# Always published (backward compatibility)
otgw-firmware/value/otgw/TSet

# Published only when settingMQTTSeparateSources=true
otgw-firmware/value/otgw/TSet/thermostat
otgw-firmware/value/otgw/TSet/boiler
otgw-firmware/value/otgw/TSet/gateway
```

### Home Assistant Discovery

Home Assistant discovery is also additive:
- Existing unsuffixed entities remain.
- Source-specific entities are emitted from template entries in `mqttha.cfg`.

Discovery templates use placeholders:
- `%source_suffix%` -> `_thermostat`, `_boiler`, `_gateway` (for unique IDs / names)
- `%source_name%` -> `Thermostat`, `Boiler`, `Gateway`
- `%source_topic_segment%` -> `thermostat`, `boiler`, `gateway` (for nested MQTT topic paths)

## Why This Choice

1. **No breaking change:** Base topics continue to work unchanged.
2. **User control:** Feature can be enabled only when source separation is needed.
3. **Clear semantics:** Nested `<metric>/<source>` is explicit and groups all source variants per metric.
4. **HA-friendly:** Source-specific discovery entries are generated automatically when enabled.
5. **Memory-safe implementation:** Uses static buffers, streaming publish, and a scoped lock for discovery generation.

## Alternatives Considered

### Alternative 1: Replace existing topics with source-specific topics only

**Pros:**
- Simpler topic model
- No duplicate publishes

**Cons:**
- Breaking change for all existing MQTT and HA integrations
- Migration complexity and retained-topic cleanup burden

**Why not chosen:** Backward compatibility is required.

### Alternative 2: Suffix-based source topics (`<metric>_thermostat`)

**Pros:**
- Easy to append
- Familiar pattern

**Cons:**
- Less consistent grouping than nested paths
- Harder to share HA discovery templates that already compose path segments
- Does not match current branch implementation

**Why not chosen:** Current implementation uses nested source paths and corresponding HA discovery templates.

### Alternative 3: JSON payload with all source values on one topic

**Pros:**
- Fewer MQTT topics
- Richer payload model

**Cons:**
- Breaking change for existing consumers
- More parsing complexity in HA
- Larger payloads and JSON overhead

**Why not chosen:** Backward compatibility and low-overhead MQTT publishing are higher priority.

### Alternative 4: Always enabled source-specific publishing

**Pros:**
- Feature is always available
- Simpler UX (no setting)

**Cons:**
- Increases topic/entity count for all users
- Increases publish volume even when unnecessary

**Why not chosen:** Feature is useful but optional; opt-in reduces noise and resource usage for default installs.

## Consequences

### Positive

- **Better diagnostics:** Users can compare thermostat request vs boiler response.
- **Gateway visibility:** Overrides can be observed separately.
- **HA automation flexibility:** Automations can target source-specific values.
- **Backward compatibility preserved:** Existing entities/topics remain valid.
- **Opt-in behavior:** No added topic/entity clutter unless enabled.

### Negative

- **More MQTT publishes when enabled:** Base + source-specific topics increase traffic.
  - Mitigation: Feature is opt-in.
- **More HA entities when enabled:** Discovery emits additional entities.
  - Mitigation: Users can keep feature disabled or hide unused entities.
- **More code paths in discovery generation:** Template expansion adds complexity.
  - Mitigation: Shared helper functions and bounded buffers.

### Risks & Mitigation

- **Risk:** Memory pressure during HA discovery generation.
  - **Mitigation:** Static shared autoconfig buffers, streaming MQTT publish, bounded sizes (ADR-004/ADR-006).
- **Risk:** Re-entry clobbers shared discovery buffers.
  - **Mitigation:** Scoped `MQTTAutoConfigSessionLock` guard prevents overlapping autoconfig sessions.
- **Risk:** Retained HA discovery topics from older builds remain visible.
  - **Mitigation:** Document retained-topic cleanup in release notes / migration guidance.
- **Risk:** Template and runtime topic paths drift.
  - **Mitigation:** Use shared placeholder schema and automated audit checks for key entities.

## Implementation Notes

### Key Implementation Elements

- `publishToSourceTopic()` publishes nested source-specific value topics (additive to base topic).
- `doAutoConfigure()` and `doAutoConfigureMsgid()` detect source placeholders in `mqttha.cfg`.
- `expandAndPublishSourceTemplates()` expands one template line into three source-specific discovery entries.
- `settingMQTTSeparateSources` controls both source-specific publishing and source-template HA discovery emission.

### Settings Behavior

- **Setting:** `MQTTseparatesources`
- **Default:** `false` (opt-in)
- **Persistence:** stored in settings file (`settings.json` via existing settings persistence flow, ADR-008)

### Home Assistant Discovery Template Pattern

Example source-specific discovery line (conceptual):

```text
... /sensor/%node_id%/TSet/%source_topic_segment%/config
...
"stat_t": "%mqtt_pub_topic%/TSet/%source_topic_segment%"
```

## Related Decisions

- **ADR-004:** Static Buffer Allocation Strategy
- **ADR-006:** MQTT Integration Pattern
- **ADR-008:** LittleFS for Configuration Persistence
- **ADR-009:** PROGMEM Usage for String Literals
- **ADR-030:** Heap Memory Monitoring and Emergency Recovery
- **ADR-038:** OpenTherm Message Data Flow Pipeline

## References

- Source-specific MQTT publishing: `src/OTGW-firmware/MQTTstuff.ino`
- OpenTherm decode/publish paths: `src/OTGW-firmware/OTGW-Core.ino`
- HA discovery templates: `src/OTGW-firmware/data/mqttha.cfg`
- Settings flag declaration: `src/OTGW-firmware/OTGW-firmware.h`
- Source-separation analysis doc: `docs/reviews/2026-02-20_issue-143-source-separation/ISSUE_143_OPTIONS_ANALYSIS.md`
