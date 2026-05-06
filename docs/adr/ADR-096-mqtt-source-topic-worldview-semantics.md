# ADR-096: MQTT Source-Subtopic Worldview Semantics

## Status

Accepted, 2026-05-07 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Classification: structural amendment to ADR-040 and ADR-066 (publish-gating). Decision Maker: User: Rob van den Breemen (rvdbreemen).

This ADR is the 2.0.0 (ESP32 + SAT) port of dev-line ADR-069. The decision content is identical; the differences are: (a) the dev-side ADR-068 reference becomes ADR-095 in this branch, and (b) the file rename from `mqtt_configuratie.cpp` (dev) to `MQTTHaDiscovery.cpp` (2.0.0) shifts the discovery-generator references.

## Context

ADR-040 introduced opt-in source-separated MQTT subtopics under each metric (`/thermostat`, `/boiler`, `/gateway`) so that consumers can distinguish where an OpenTherm value came from. ADR-066 (publish-gating-by-source) then constrained the legacy "base" topic to thermostat-side intent (Read-Ack and Write-Data only, no Write-Ack), and ADR-095 made `bSeparateSources` mutually exclusive between base and source-variant entities for source-templated MsgIDs. Across these three ADRs the original framing of the subtopic semantics was **source-of-publication**: which side of the OT bus emitted this frame.

The implementation has drifted from that framing in two places:

1. **`/gateway` subtopic was retired**: `mqttSourceKeys[]` in `MQTTstuff.ino:444-450` now only contains `thermostat` and `boiler`. Gateway-substituted Request-Boiler (`R`) frames land on the canonical topic only (`MQTTstuff.ino:1538-1549`, `resolveSourceIndex` default branch returns false).
2. **Thermostat-side values are dropped during gateway override**: when `R` follows `T` within 500 ms for the same MsgID, `OTGW-Core.ino:4051-4056` marks the `T` frame as `skipthis = true`. The frame is logged as `<ignored>` and never published — neither to the canonical topic nor to `/thermostat`. Same logic mirrors for `B` followed by `A`.

A field report from beta-tester Andre on 2026-05-07 (against the dev branch, but the same code pattern exists in 2.0.0) demonstrated the consequence: with `bSeparateSources = true` and an active `CS=27.37` setpoint override, the thermostat raised its setpoint from 20 to 23 °C. The user expected the new value on `…/TSet/thermostat`. Instead `…/TSet/thermostat` stayed empty and `…/TSet/boiler` showed the override value (27.37). The user's mental model: "each subtopic shows what *that side* sees", which is **not** the source-of-publication framing — it is a different, equally coherent semantic that can be called the **worldview** model.

The two models, illustrated for `TSet` with override `CS=27.37` active and the thermostat asking 23 °C:

| Subtopic | Model A (source-of-publication) | Model B (worldview) |
|---|---|---|
| `…/TSet/thermostat` | 23 (T frame originated here) | 23 (this is what the thermostat sent) |
| `…/TSet/boiler` | empty (no B frame for this write) | 27.37 (this is what the boiler received) |
| `…/TSet/gateway` | 27.37 (R frame originated at gateway) | not used |
| canonical `…/TSet` | 27.37 (today: gateway-substituted) | 27.37 (boiler-side worldview) |

Model A's `/boiler` subtopic is empty for write-only metrics because the boiler does not echo the write back. Power users have to consult a third subtopic (`/gateway`) or the canonical topic to discover what the boiler actually received. New HA users naming a sensor `boiler_setpoint` from `…/TSet/boiler` find it permanently empty — a confusing UX outcome already complained about on Discord and present in the dev-side TASK-549 bug report.

Model B routes `T → /thermostat` (always), `R → /boiler` (always), `B → /boiler` (always), `A → /thermostat` (always). Every subtopic reflects the perspective of the named device, regardless of which OT frame carried the value. Override behavior becomes immediately visible by comparing the two subtopics. A third subtopic (`/gateway`) is unnecessary because the override is implicit in the divergence.

The OpenTherm protocol cleanly maps to worldview routing because each frame type has an unambiguous "who sees this" answer:

| Frame | Direction | Thermostat sees | Boiler sees |
|---|---|---|---|
| `T` (thermostat-write) | M→S, no override | yes (it sent it) | yes (it received it) |
| `T` (thermostat-write) | M→S, override active | yes (it sent it) | no — boiler receives `R` instead |
| `R` (gateway-substituted write) | M→S, override active | no — thermostat sent `T`, not `R` | yes (it received it) |
| `B` (boiler-response) | S→M, no override | yes (it received it) | yes (it sent it) |
| `B` (boiler-response) | S→M, answer-override active | no — thermostat receives `A` | yes (it sent it) |
| `A` (gateway-faked answer) | S→M, answer-override active | yes (it received it) | no — boiler sent `B`, not `A` |

This table is the entire decision content. Model B routes each frame to the subtopic(s) where that frame's value would actually arrive at the named device.

### 2.0.0-specific notes

The 2.0.0 branch carries the same `OTGW-Core.ino`/`MQTTstuff.ino` pattern as dev for the OT-frame substitution path; the SAT-side publish path (`SATstuff.ino`, `SATBLE.cpp`) does not consult `OTdata.rsptype` for worldview routing because SAT acts as the OT master itself and never produces gateway-substituted frames. This ADR therefore touches only the OT-bus path and leaves SAT publication unchanged. The HA-discovery generator was renamed in 2.0.0 from `mqtt_configuratie.cpp` (dev) to `MQTTHaDiscovery.cpp`; the comment-block reference site shifts from dev's L2367 to 2.0.0's L2555+.

### Constraints

- **Backward compatibility for default users (`bSeparateSources = false`)**: the canonical topic must keep producing useful values for HA installations that have not opted in to source separation. Most HA users in this category have automations bound to canonical topics like `…/value/<id>/TSet`.
- **Spec-correct behavior preservation**: the per-MsgID `bSlaveEchoesValue` gate from ADR-066 remains in force for Write-Ack publications to `/boiler`. This ADR re-routes frames between subtopics; it does not loosen the publish-eligibility contract.
- **No HA discovery `unique_id` churn within a mode**: per ADR-095, toggling `bSeparateSources` is an explicit one-way migration. Users who keep the setting fixed must not see entity churn from this change alone.
- **Memory limits per ADR-004**: routing decisions remain O(1); no per-call lookup table growth beyond what already exists. ESP32-S3 has more heap than ESP8266, so the dedup mitigation noted in dev's ADR-069 is even less of a concern here.
- **`/gateway` subtopic stays retired**: TASK-532 (port from dev's TASK-531) already removed the `/gateway` subtopic for backward compat reasons (HA entities for gateway-source metrics keep their bare canonical topic). This ADR ratifies that removal as part of the worldview design.

## Decision

**Adopt the worldview model for source-separated MQTT subtopics. Each subtopic represents what the named device sees, not which side originated the OT frame.**

Routing rules for OpenTherm frames carrying a publishable value:

| Frame type (`OTdata.rsptype`) | Publishes to `/thermostat`? | Publishes to `/boiler`? | Publishes to canonical? |
|---|---|---|---|
| `OTGW_THERMOSTAT` (T) | yes | yes (when no override active for this MsgID-window) | yes (when no R follows within 500 ms) |
| `OTGW_REQUEST_BOILER` (R) | no | yes | yes (overrides T's canonical publish) |
| `OTGW_BOILER` (B) | yes (when no answer-override active for this MsgID-window) | yes | yes |
| `OTGW_ANSWER_THERMOSTAT` (A) | yes | no | no (B carries canonical for reads) |

Conceptual statement: **canonical = boiler-side worldview.** It carries the value the boiler actually transmitted (`B`) or received (`R` if override, `T` if pass-through). This is what most users mean when they ask "what is the actual setpoint?" and matches what the firmware effectively publishes today (the existing `skipthis` logic already lets `R` win the canonical for writes; this ADR makes that an explicit invariant rather than an emergent property).

The three implementation deltas relative to today's behavior:

1. **Thermostat-side `T` frame is no longer suppressed during gateway-write override.** Remove the `skipthis = true` assignment for the `(R after T, same id, < 500 ms)` case in `OTGW-Core.ino:4051-4056` from the publish path. The frame still gets the `<ignored>` log marker for OT-bus diagnostic purposes (it tells the user the gateway substituted the value), but it must reach `publishToSourceTopic` so that `/thermostat` is updated.
2. **Boiler-side `B` frame is no longer suppressed during gateway-answer override.** Symmetric to the above for the `(A after B, same id, < 500 ms)` case. `B` reaches `/boiler` and canonical; `A` reaches `/thermostat` only.
3. **`A` frame routes to `/thermostat`, not `/boiler`.** The current `resolveSourceIndex` table-based dispatch (`MQTTstuff.ino:1538-1549`) is replaced with an explicit per-frame switch inside `publishToSourceTopic`. Case `OTGW_ANSWER_THERMOSTAT` routes to `/thermostat` (was `/boiler` per the misleading "OTGW answers as boiler" comment). Case `OTGW_REQUEST_BOILER` routes to `/boiler` (was canonical-only).

`R` continues to map to the boiler subtopic with the worldview rationale "this is the value that reached the boiler". The publish-eligibility contract from ADR-066 (`bSlaveEchoesValue` gate, base-topic Write-Ack suppression) remains intact. Worldview routing is orthogonal to publish eligibility: routing decides *which subtopic*, eligibility decides *whether to publish at all*.

### Implementation primitives

- `publishToSourceTopic` in `MQTTstuff.ino` is rewritten to drive a worldview switch on `rsptype` directly, with two booleans (`toThermostat`, `toBoiler`) deciding the per-call routing. The intermediate helpers `resolveSourceIndex`, `copySourceTableEntry`, the PROGMEM table `mqttSourceKeys[]`, and the per-source key strings (`s_mqtt_src_key_thermostat`, `s_mqtt_src_key_boiler`) become dead code and are removed. The `MQTT_SOURCE_KEY_COUNT` constant goes with them.
- `OTGW-Core.ino:4051-4066` `skipthis` assignment for the gateway-substitution case is replaced with a logging-only flag `OTdata.bGatewaySubstituted = true` that marks the older frame in the (T,R) / (B,A) sequence for log decoration without removing it from the publish path. The `<ignored>` log marker (`OTGW-Core.ino:4140`) is preserved for backward-compatible OT-log readability and now fires on `skipthis || bGatewaySubstituted`. `skipthis` itself remains in the data structure and is still set by parity errors.
- `is_value_valid` and `is_value_valid_for_master_topic` (ADR-066) gain two ADR-096 worldview gates in the latter: `OTGW_ANSWER_THERMOSTAT` and `(OTGW_THERMOSTAT && bGatewaySubstituted)` both return `false` for the master topic, so canonical follows the boiler-side worldview. `is_value_valid` (used by source-separated subtopics) is unchanged; the worldview routing inside `publishToSourceTopic` does the per-subtopic decision.
- HA discovery (`composeSensorPayload`, `composeBinSensorPayload`, `expandAndStreamSensorSources` in `MQTTHaDiscovery.cpp`) requires no change to the *generation* code. The `mqttHaSensors[]` table entries flagged for source expansion already produce `_thermostat` and `_boiler` variants; under ADR-095 those replace the base entity when `bSeparateSources = true`. The semantic re-interpretation of which value lands where happens at publish time, not discovery time. Only the comment block at `MQTTHaDiscovery.cpp:2555+` is updated to cite this ADR.

### Per-MsgID applicability

The worldview model applies to every MsgID with both a master and slave side. For read-only MsgIDs (Class 4 sensor data sent S→M only) `T` is a Read-Data request without payload; only `B` (or `A` under override) carries a value. The routing table still applies cleanly:

- `B` → `/boiler` and canonical (boiler sent it)
- `B` → `/thermostat` (no answer-override case)
- `A` → `/thermostat` only (answer-override case)

For write-only MsgIDs (Class 8 control of special applications, M→S only) the symmetric reduction applies.

## Alternatives Considered

### Alternative A: Source-of-publication (the original ADR-040 framing)

Each subtopic carries values for frames that originated on that side of the bus.

- `/thermostat` ← `T` only
- `/boiler` ← `B` only
- `/gateway` ← `R` and `A`

**Rejected** because (1) it requires a third subtopic (`/gateway`) which has already been removed from the implementation by TASK-532 (port of dev's TASK-531) for backward-compat reasons, and (2) it produces empty subtopics for normal use cases. A user creating `sensor.boiler_setpoint` from `…/TSet/boiler` finds it permanently blank because the boiler does not echo write commands. The model is internally coherent ("who put this on the bus") but the practical UX outcome — empty subtopics for the most common HA sensor names — is poor. Power users wanting override visibility need to cross-reference three subtopics instead of two.

### Alternative B (chosen): Worldview semantics

Each subtopic carries the value that side of the bus saw, regardless of which frame type carried it. Override behavior is implicit in the divergence between `/thermostat` and `/boiler`.

**Trade-off accepted**: in pass-through (no override active), the same value publishes to both `/thermostat` and `/boiler`. The two MQTT publishes carry identical payloads. This duplicates wire traffic for source-separated installations. Mitigation: the publish path already handles deduplication via the per-topic last-value tracking that `is_value_valid` consults; consecutive identical publishes to the same topic are coalesced. ESP32-S3 has substantially more heap than ESP8266, so the publish-volume concern noted in dev's ADR-069 is less acute on this branch. The cross-topic publish is intentional because the two subtopics document distinct semantic claims (the thermostat asked X *and* the boiler received X), and a future debugging session must be able to verify both independently.

A second, smaller trade-off: canonical semantics shift from "thermostat-side intent" (the strict reading of ADR-066) to "boiler-side worldview". For Reads with answer-override active this changes the canonical value from `A` (gateway-faked) to `B` (boiler-actual). For Writes with gateway-write override active the canonical continues to publish `R` (override) — no change relative to today, because the existing `skipthis` logic already produces this outcome. The shift documented here is "make the de-facto behavior the documented invariant".

### Alternative C: Hybrid — keep canonical = thermostat-side intent (strict ADR-066), make subtopics worldview

Canonical follows ADR-066 unchanged: `T` wins canonical for writes (currently `R` wins, so this would be a behavior change for the default-user majority). Subtopics use the worldview model from Alternative B.

**Rejected** because it splits the mental model. Canonical and `/boiler` would carry different values during gateway-write override (canonical = `T` thermostat intent; `/boiler` = `R` override actual), which contradicts the principle "canonical and `/boiler` both describe the boiler's worldview". It also forces a behavioral change on default users, who today see the override value on canonical and would suddenly see the pre-override thermostat value instead. ADR-095 already established that canonical is suppressed for source-templated MsgIDs when `bSeparateSources = true`, so the only audience affected by the canonical-side question is the default-user cohort. For them, "the actual current setpoint" is more useful than "the value the thermostat originally asked for". Worldview-aligned canonical wins.

### Alternative D: Status quo (Model A residue + the `<ignored>` bug)

Keep current routing; fix only the one immediate bug Andre reported.

**Rejected** because the routing inconsistency for `A` (currently to `/boiler` per the misleading "OTGW answers as boiler" comment) is symmetric to the inconsistency that produced Andre's bug. Fixing only the write-side leaves the read-side in the same conceptually-confused state, guaranteeing a future bug report from the next user who cares about answer-overrides. One ADR + one task is cheaper than a series of point fixes followed by a unifying ADR later.

## Consequences

### Positive

- **Override visibility is symmetric and obvious.** A user reading both `…/TSet/thermostat` and `…/TSet/boiler` immediately knows whether the gateway is intervening: equal values mean pass-through, divergence means override active. No need to consult a third topic, log line, or status flag.
- **Sensor names match intuition.** `sensor.boiler_setpoint` from `…/TSet/boiler` always reflects what the boiler is being asked to do. `sensor.thermostat_setpoint_request` from `…/TSet/thermostat` always reflects what the thermostat asked for. The HA dashboard becomes self-explanatory.
- **The `<ignored>` data-loss bug closes.** The thermostat's request always reaches `/thermostat` and the canonical (when no override) regardless of override state. No more silent data drops during override.
- **`/gateway` subtopic stays retired with explicit rationale.** TASK-532 (port from TASK-531) removed it without an ADR; this ADR ratifies the removal and explains why no replacement is needed (override = subtopic divergence, not a third subtopic).
- **Documentation becomes simpler.** One sentence: "each subtopic shows what that device sees". The existing ADR-040/ADR-066 framing required three sentences plus a footnote about which frame type maps to which subtopic.
- **Dead code removed.** The table-driven `resolveSourceIndex` / `copySourceTableEntry` / `mqttSourceKeys` indirection collapses into an explicit switch, which is shorter, has no PROGMEM table, and is easier to reason about.

### Negative

- **Canonical semantics for Reads change under answer-override.** When the gateway is faking an answer to the thermostat (via `TT=`, `TC=` for setpoint, etc.), the canonical topic switches from publishing `A` (the faked answer) to publishing `B` (the boiler's actual response). Default users who relied on canonical to show "what HA sees as the room temperature the thermostat is using" will see the boiler's real value instead. Migration: explained in release notes; users wanting the faked value can subscribe to `/thermostat` (which now carries `A`).
- **More wire traffic in pass-through.** With `bSeparateSources = true` and no override, every value publishes to both `/thermostat` and `/boiler`. Roughly doubles MQTT publish volume for source-templated MsgIDs in this configuration. Mitigation: ESP32-S3 publish path has ample heap headroom (ADR-089 tier machine); broker bandwidth is typically not a constraint for LAN-only MQTT brokers. If field reports surface heap pressure from the increased volume, a future ADR can introduce per-(topic, value) deduplication at publish time.
- **Subtle log decoration change.** The `<ignored>` marker in the OT-bus log currently means "this frame did not affect any MQTT publish". After this ADR it means "the gateway substituted this frame on the OT bus, but we still published it to the corresponding worldview subtopic". Existing log readers must internalize this. Mitigation: changelog note + the marker text remains the same so screen-scraping log parsers continue to work.

### Neutral

- **Discovery generators are unchanged.** `mqttHaSensors[]` flag-driven expansion under ADR-095 already emits `_thermostat` and `_boiler` variants. The flag table stays as-is; only the publish-time routing changes.
- **`bSeparateSources = false` users see canonical-only behavior.** No new subtopics appear for them. The only behavior change visible to this cohort is the canonical-on-read shift (B replaces A under answer-override).
- **The publish-eligibility contract from ADR-066 is preserved.** Write-Ack still does not reach the base topic. The per-MsgID `bSlaveEchoesValue` gate still applies to Write-Ack publications on `/boiler`. Worldview routing operates within these constraints, not against them.
- **SAT path unaffected.** SAT acts as the OT master and does not generate gateway-substituted frames; its publish path is independent of `OTdata.rsptype` worldview routing.

### Risks & Mitigation

- **Risk:** users with custom HA YAML bound to the canonical topic during answer-override scenarios see a value change after upgrading.
  - **Mitigation:** release notes call out the canonical-on-read shift. Users who specifically need the faked-answer value subscribe to `/thermostat` (requires `bSeparateSources = true`).
- **Risk:** doubled publish volume in pass-through causes heap pressure on heavily-loaded gateways.
  - **Mitigation:** monitor field reports for the first 2.0.0 alpha. The publish path is heap-tier-aware (ADR-089); per-topic value coalescing already exists. If a user reports degraded behavior, add per-(topic, value) deduplication at the source-subtopic publish call.
- **Risk:** future contributor reads `OTGW-Core.ino:4051-4066` and reintroduces `skipthis = true` for the override case, regressing the bug.
  - **Mitigation:** the comment block at that location is updated to cite this ADR explicitly and explain the worldview rationale. The `bGatewaySubstituted` flag retains the diagnostic intent without the data-loss side effect.

## Verification gates

1. **Completeness:** Status / Context / Decision / Alternatives / Consequences / Related sections populated. Cross-references to ADR-040 (amends source-of-publication framing), ADR-066 (canonical semantics shift; publish-eligibility preserved), ADR-095 (mutually-exclusive base/source-variant publication preserved), TASK-532 (`/gateway` retirement ratified), the new 2.0.0 backlog task created alongside this ADR.
2. **Evidence:** field report from beta-tester Andre on Discord `#beta-testing`, 2026-05-07, with full OT-log capture demonstrating the `<ignored>` data-loss bug under `CS=27.37` override. Implementation references: `MQTTstuff.ino:444-450` (`mqttSourceKeys`), `MQTTstuff.ino:1538-1549` (`resolveSourceIndex`), `OTGW-Core.ino:4051-4056` (`skipthis` logic), `MQTTstuff.ino:1561-1585` (`publishToSourceTopic`).
3. **Clarity:** the routing table in the Decision section is the entire implementation specification. Three concrete code-edit sites named with file:line. The `bGatewaySubstituted` flag suggested name is illustrative but used in this ADR's implementation primitives section to align with the dev-side port.
4. **Consistency:** amends ADR-040's source semantics (publication-source → worldview); refines ADR-066's canonical interpretation (thermostat-side → boiler-side worldview) while preserving its publish-eligibility gate; preserves ADR-095's mutual-exclusion property; ratifies TASK-532's `/gateway` retirement. No contradiction with ADR-051 (settings encapsulation), ADR-079/ADR-081 (per-component types), ADR-089 (heap tier machine), ADR-090 (re-entrancy guard — preserved by keeping the `inUse` static in `publishToSourceTopic`), ADR-094 (HA discovery state reconciliation — orthogonal).

## Related Decisions

- **ADR-040:** MQTT Source-Specific Topics for OpenTherm Values. **This ADR amends ADR-040** by changing the subtopic semantic model from source-of-publication to worldview. ADR-040's "source = which side originated the frame" interpretation is replaced by "subtopic = which side saw the value". ADR-040's status line is appended with "Amended by: ADR-096".
- **ADR-066 (publish-gating-by-source):** MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification. **This ADR refines ADR-066** by shifting the canonical-topic interpretation from "thermostat-side intent" to "boiler-side worldview" while preserving the per-MsgID `bSlaveEchoesValue` gate for `/boiler` Write-Ack publications. Reads under answer-override are the only default-user behavior change. ADR-066's status line is appended with "Refined by: ADR-096".
- **ADR-095:** bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive. Companion. The mutual-exclusion property is preserved. Worldview routing operates entirely within the source-variant publishing path; default users (`bSeparateSources = false`) see only the canonical change described above.
- **ADR-094:** HA Discovery State Reconciliation on OTA Upgrade. Companion mechanism. The shift in subtopic semantics will benefit from the on-OTA reconciliation if any consumers had cached the old `A → /boiler` routing in retained-state assumptions.
- **ADR-089:** Heap Tier Machine Contract. The doubled publish volume in pass-through is bounded by the heap tier machine's existing throttling; no new gates needed.
- **ADR-090:** Re-entrancy Guard Pattern for Shared Scratch Buffers. The `inUse` static guard inside `publishToSourceTopic` is preserved by the rewrite; the static `sourceTopic` buffer ownership is unchanged.
- **TASK-532:** Restored backward-compatible bare topic for gateway-source HA entities (port from dev's TASK-531). **This ADR ratifies the `/gateway` subtopic retirement** that TASK-532 implemented without a formal ADR. Worldview semantics make `/gateway` redundant by design (override = subtopic divergence).
- **dev-line ADR-069:** This ADR is the 2.0.0 port. The decision content is identical; only ADR-068↔ADR-095 numbering and the `mqtt_configuratie.cpp`↔`MQTTHaDiscovery.cpp` rename differ.

## References

- Routing implementation: `src/OTGW-firmware/MQTTstuff.ino:1538-1549` (`resolveSourceIndex` — to be removed).
- Source-key table: `src/OTGW-firmware/MQTTstuff.ino:444-450` (`mqttSourceKeys` — to be removed).
- Per-source publish: `src/OTGW-firmware/MQTTstuff.ino:1561-1585` (`publishToSourceTopic` — to be rewritten).
- OT-frame skip logic to be reworked: `src/OTGW-firmware/OTGW-Core.ino:4051-4066`.
- Master-topic gate to be extended with worldview suppression: `src/OTGW-firmware/OTGW-Core.ino:1276-1285` (`is_value_valid_for_master_topic`).
- OT data struct (gains `bGatewaySubstituted` field): `src/OTGW-firmware/OTGW-Core.h:548-589` (`OpenthermData_t`).
- User-facing contract documentation: `docs/api/MQTT.md` "Source-Separated Topics" section (updated as part of this ADR).
- Field report: Discord `#beta-testing`, beta-tester Andre, 2026-05-07. OT-log capture preserved in dev's TASK-549 description.
- HA discovery generators (unchanged generation logic, comment block updated): `src/OTGW-firmware/MQTTHaDiscovery.cpp:2555-2618` (`expandAndStreamSensorSources`).

## Enforcement

```json
{
  "llm_judge": true
}
```
