# ADR-069: MQTT Source-Subtopic Worldview Semantics

## Status

Superseded by ADR-075, 2026-05-19. Original status: Accepted, 2026-05-07 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Classification: structural amendment to ADR-040 and ADR-066. Decision Maker: User: Rob van den Breemen (rvdbreemen). Reason for supersession: the worldview routing table assumed every readable MsgID has a boiler `B` frame; for OTGW-proxied IDs (e.g. MaxTSet/57, no-boiler, boiler-unsupported) the gateway answers `A` with no `B`, starving canonical and `_boiler`. ADR-075 restates the full routing table with a proxy-answer vs answer-override split that preserves this ADR's override invariant.

## Context

ADR-040 introduced opt-in source-separated MQTT subtopics under each metric (`/thermostat`, `/boiler`, `/gateway`) so that consumers can distinguish where an OpenTherm value came from. ADR-066 then constrained the legacy "base" topic to thermostat-side intent (Read-Ack and Write-Data only, no Write-Ack), and ADR-068 made `bSeparateSources` mutually exclusive between base and source-variant entities for source-templated MsgIDs. Across these three ADRs the original framing of the subtopic semantics was **source-of-publication**: which side of the OT bus emitted this frame.

The implementation has drifted from that framing in two places:

1. **`/gateway` subtopic was retired**: `mqttSourceKeys[]` in `MQTTstuff.ino:442-446` now only contains `thermostat` and `boiler`. Gateway-substituted Request-Boiler (`R`) frames land on the canonical topic only (`MQTTstuff.ino:1190-1193`, `resolveSourceIndex` default branch returns false).
2. **Thermostat-side values are dropped during gateway override**: when `R` follows `T` within 500 ms for the same MsgID, `OTGW-Core.ino:4046-4051` marks the `T` frame as `skipthis = true`. The frame is logged as `<ignored>` and never published — neither to the canonical topic nor to `/thermostat`. Same logic mirrors for `B` followed by `A`.

A field report from beta-tester Andre on 2026-05-07 demonstrated the consequence: with `bSeparateSources = true` and an active `CS=27.37` setpoint override, the thermostat raised its setpoint from 20 to 23 °C. The user expected the new value on `…/TSet/thermostat`. Instead `…/TSet/thermostat` stayed empty and `…/TSet/boiler` showed the override value (27.37). The user's mental model: "each subtopic shows what *that side* sees", which is **not** the source-of-publication framing — it is a different, equally coherent semantic that can be called the **worldview** model.

The two models, illustrated for `TSet` with override `CS=27.37` active and the thermostat asking 23 °C:

| Subtopic | Model A (source-of-publication) | Model B (worldview) |
|---|---|---|
| `…/TSet/thermostat` | 23 (T frame originated here) | 23 (this is what the thermostat sent) |
| `…/TSet/boiler` | empty (no B frame for this write) | 27.37 (this is what the boiler received) |
| `…/TSet/gateway` | 27.37 (R frame originated at gateway) | not used |
| canonical `…/TSet` | 27.37 (today: gateway-substituted) | 27.37 (boiler-side worldview) |

Model A's `/boiler` subtopic is empty for write-only metrics because the boiler does not echo the write back. Power users have to consult a third subtopic (`/gateway`) or the canonical topic to discover what the boiler actually received. New HA users naming a sensor `boiler_setpoint` from `…/TSet/boiler` find it permanently empty — a confusing UX outcome already complained about on Discord and present in TASK-549's bug report.

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

### Constraints

- **Backward compatibility for default users (`bSeparateSources = false`)**: the canonical topic must keep producing useful values for HA installations that have not opted in to source separation. Most HA users in this category have automations bound to canonical topics like `…/value/<id>/TSet`.
- **Spec-correct behavior preservation**: the per-MsgID `bSlaveEchoesValue` gate from ADR-066 remains in force for Write-Ack publications to `/boiler`. This ADR re-routes frames between subtopics; it does not loosen the publish-eligibility contract.
- **No HA discovery `unique_id` churn within a mode**: per ADR-068, toggling `bSeparateSources` is an explicit one-way migration. Users who keep the setting fixed must not see entity churn from this change alone.
- **Memory limits per ADR-004**: routing decisions remain O(1); no per-call lookup table growth beyond what already exists.
- **`/gateway` subtopic stays retired**: TASK-531 already removed it for backward compat reasons (HA entities for gateway-source metrics keep their bare canonical topic). This ADR ratifies that removal as part of the worldview design.

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

1. **Thermostat-side `T` frame is no longer suppressed during gateway-write override.** Remove the `skipthis = true` assignment for the `(R after T, same id, < 500 ms)` case in `OTGW-Core.ino:4046-4051` from the publish path. The frame still gets the `<ignored>` log marker for OT-bus diagnostic purposes (it tells the user the gateway substituted the value), but it must reach `publishToSourceTopic` so that `/thermostat` is updated.
2. **Boiler-side `B` frame is no longer suppressed during gateway-answer override.** Symmetric to the above for the `(A after B, same id, < 500 ms)` case. `B` reaches `/boiler` and canonical; `A` reaches `/thermostat` only.
3. **`A` frame routes to `/thermostat`, not `/boiler`.** Update `resolveSourceIndex` in `MQTTstuff.ino:1185-1196`: case `OTGW_ANSWER_THERMOSTAT` returns `sourceIndex = 0` (thermostat) instead of the current `1` (boiler). The misleading comment ("OTGW answers as boiler — value is boiler-side") is removed.

`R` continues to map to `sourceIndex = 1` (boiler) with the worldview rationale "this is the value that reached the boiler". The current default-branch handling (R → canonical only, no subtopic) is changed: R must publish to the `/boiler` subtopic too. This is a single-line edit to `resolveSourceIndex` adding `case OTGW_REQUEST_BOILER: sourceIndex = 1; return true;`.

The publish-eligibility contract from ADR-066 (`bSlaveEchoesValue` gate, base-topic Write-Ack suppression) remains intact. Worldview routing is orthogonal to publish eligibility: routing decides *which subtopic*, eligibility decides *whether to publish at all*.

### Implementation primitives

- `resolveSourceIndex` in `MQTTstuff.ino` gains explicit cases for `OTGW_REQUEST_BOILER` (returns 1, boiler) and reroutes `OTGW_ANSWER_THERMOSTAT` (returns 0, thermostat). Default branch returns false unchanged.
- `OTGW-Core.ino:4046-4051` `skipthis` assignment is replaced with a logging-only flag (e.g. `OTdata.bGatewaySubstituted = true`) that marks the frame for log decoration without removing it from the publish path. The `<ignored>` log marker is preserved for backward-compatible OT-log readability.
- `is_value_valid` and `is_value_valid_for_master_topic` (ADR-066) require no signature change. The base-topic eligibility gate continues to skip Write-Ack as before.
- HA discovery (`composeSensorPayload`, `composeBinSensorPayload`, `expandAndStreamSensorSources`) requires no change to the *generation* code. The `mqttHaSensors[]` table entries flagged `MQTT_HA_FLAG_ANY_SOURCE` already produce `_thermostat` and `_boiler` variants; under ADR-068 those replace the base entity when `bSeparateSources = true`. The semantic re-interpretation of which value lands where happens at publish time, not discovery time.

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

**Rejected** because (1) it requires a third subtopic (`/gateway`) which has already been removed from the implementation by TASK-531 for backward-compat reasons, and (2) it produces empty subtopics for normal use cases. A user creating `sensor.boiler_setpoint` from `…/TSet/boiler` finds it permanently blank because the boiler does not echo write commands. The model is internally coherent ("who put this on the bus") but the practical UX outcome — empty subtopics for the most common HA sensor names — is poor. Power users wanting override visibility need to cross-reference three subtopics instead of two.

### Alternative B (chosen): Worldview semantics

Each subtopic carries the value that side of the bus saw, regardless of which frame type carried it. Override behavior is implicit in the divergence between `/thermostat` and `/boiler`.

**Trade-off accepted**: in pass-through (no override active), the same value publishes to both `/thermostat` and `/boiler`. The two MQTT publishes carry identical payloads. This duplicates wire traffic for source-separated installations. Mitigation: the publish path already handles deduplication via the per-topic last-value tracking that `is_value_valid` consults; consecutive identical publishes to the same topic are coalesced. The cross-topic publish is intentional because the two subtopics document distinct semantic claims (the thermostat asked X *and* the boiler received X), and a future debugging session must be able to verify both independently.

A second, smaller trade-off: canonical semantics shift from "thermostat-side intent" (the strict reading of ADR-066) to "boiler-side worldview". For Reads with answer-override active this changes the canonical value from `A` (gateway-faked) to `B` (boiler-actual). For Writes with gateway-write override active the canonical continues to publish `R` (override) — no change relative to today, because the existing `skipthis` logic already produces this outcome. The shift documented here is "make the de-facto behavior the documented invariant".

### Alternative C: Hybrid — keep canonical = thermostat-side intent (strict ADR-066), make subtopics worldview

Canonical follows ADR-066 unchanged: `T` wins canonical for writes (currently `R` wins, so this would be a behavior change for the default-user majority). Subtopics use the worldview model from Alternative B.

**Rejected** because it splits the mental model. Canonical and `/boiler` would carry different values during gateway-write override (canonical = `T` thermostat intent; `/boiler` = `R` override actual), which contradicts the principle "canonical and `/boiler` both describe the boiler's worldview". It also forces a behavioral change on default users, who today see the override value on canonical and would suddenly see the pre-override thermostat value instead. ADR-068 already established that canonical is suppressed for source-templated MsgIDs when `bSeparateSources = true`, so the only audience affected by the canonical-side question is the default-user cohort. For them, "the actual current setpoint" is more useful than "the value the thermostat originally asked for". Worldview-aligned canonical wins.

### Alternative D: Status quo (Model A residue + the `<ignored>` bug)

Keep current routing; fix only the one immediate bug Andre reported.

**Rejected** because the routing inconsistency for `A` (currently to `/boiler` per the misleading "OTGW answers as boiler" comment) is symmetric to the inconsistency that produced Andre's bug. Fixing only the write-side leaves the read-side in the same conceptually-confused state, guaranteeing a future bug report from the next user who cares about answer-overrides. One ADR + one task is cheaper than a series of point fixes followed by a unifying ADR later.

## Consequences

### Positive

- **Override visibility is symmetric and obvious.** A user reading both `…/TSet/thermostat` and `…/TSet/boiler` immediately knows whether the gateway is intervening: equal values mean pass-through, divergence means override active. No need to consult a third topic, log line, or status flag.
- **Sensor names match intuition.** `sensor.boiler_setpoint` from `…/TSet/boiler` always reflects what the boiler is being asked to do. `sensor.thermostat_setpoint_request` from `…/TSet/thermostat` always reflects what the thermostat asked for. The HA dashboard becomes self-explanatory.
- **The `<ignored>` data-loss bug closes.** The thermostat's request always reaches `/thermostat` and the canonical (when no override) regardless of override state. No more silent data drops during override.
- **`/gateway` subtopic stays retired with explicit rationale.** TASK-531 removed it without an ADR; this ADR ratifies the removal and explains why no replacement is needed (override = subtopic divergence, not a third subtopic).
- **Documentation becomes simpler.** One sentence: "each subtopic shows what that device sees". The existing ADR-040/ADR-066 framing required three sentences plus a footnote about which frame type maps to which subtopic.

### Negative

- **Canonical semantics for Reads change under answer-override.** When the gateway is faking an answer to the thermostat (via `TT=`, `TC=` for setpoint, etc.), the canonical topic switches from publishing `A` (the faked answer) to publishing `B` (the boiler's actual response). Default users who relied on canonical to show "what HA sees as the room temperature the thermostat is using" will see the boiler's real value instead. Migration: explained in release notes; users wanting the faked value can subscribe to `/thermostat` (which now carries `A`).
- **More wire traffic in pass-through.** With `bSeparateSources = true` and no override, every value publishes to both `/thermostat` and `/boiler`. Roughly doubles MQTT publish volume for source-templated MsgIDs in this configuration. Mitigation: ESP8266 publish path is non-blocking (ADR-006); broker bandwidth is typically not a constraint for LAN-only MQTT brokers. If field reports surface heap pressure from the increased volume, a future ADR can introduce per-(topic, value) deduplication at publish time.
- **Subtle log decoration change.** The `<ignored>` marker in the OT-bus log currently means "this frame did not affect any MQTT publish". After this ADR it means "the gateway substituted this frame on the OT bus, but we still published it to the corresponding worldview subtopic". Existing log readers must internalize this. Mitigation: changelog note + the marker text remains the same so screen-scraping log parsers continue to work.

### Neutral

- **Discovery generators are unchanged.** `mqttHaSensors[]` flag-driven expansion under ADR-068 already emits `_thermostat` and `_boiler` variants. The flag table stays as-is; only the publish-time routing changes.
- **`bSeparateSources = false` users see canonical-only behavior.** No new subtopics appear for them. The only behavior change visible to this cohort is the canonical-on-read shift (B replaces A under answer-override).
- **The publish-eligibility contract from ADR-066 is preserved.** Write-Ack still does not reach the base topic. The per-MsgID `bSlaveEchoesValue` gate still applies to Write-Ack publications on `/boiler`. Worldview routing operates within these constraints, not against them.

### Risks & Mitigation

- **Risk:** users with custom HA YAML bound to the canonical topic during answer-override scenarios see a value change after upgrading.
  - **Mitigation:** release notes call out the canonical-on-read shift. Users who specifically need the faked-answer value subscribe to `/thermostat` (requires `bSeparateSources = true`).
- **Risk:** doubled publish volume in pass-through causes heap pressure on heavily-loaded gateways.
  - **Mitigation:** monitor field reports for the first beta. The publish path is already heap-aware (ADR-030); per-topic value coalescing already exists. If a user reports degraded behavior, add per-(topic, value) deduplication at the source-subtopic publish call.
- **Risk:** future contributor reads `OTGW-Core.ino:4046-4051` and reintroduces `skipthis = true` for the override case, regressing the bug.
  - **Mitigation:** the comment block at that location is updated to cite this ADR explicitly and explain the worldview rationale. The `bGatewaySubstituted` flag retains the diagnostic intent without the data-loss side effect.

## Verification gates

1. **Completeness:** Status / Context / Decision / Alternatives / Consequences / Related sections populated. Cross-references to ADR-040 (amends source-of-publication framing), ADR-066 (canonical semantics shift; publish-eligibility preserved), ADR-068 (mutually-exclusive base/source-variant publication preserved), TASK-531 (`/gateway` retirement ratified), TASK-549 (implementation tracking task).
2. **Evidence:** field report from beta-tester Andre on Discord `#beta-testing`, 2026-05-07, with full OT-log capture demonstrating the `<ignored>` data-loss bug under `CS=27.37` override. Implementation references: `MQTTstuff.ino:442-446` (`mqttSourceKeys`), `MQTTstuff.ino:1185-1196` (`resolveSourceIndex`), `OTGW-Core.ino:4046-4051` (`skipthis` logic). Existing per-source publish path: `MQTTstuff.ino:1209-1234` (`publishToSourceTopic`).
3. **Clarity:** the routing table in the Decision section is the entire implementation specification. Three concrete code-edit sites named with file:line. The `bGatewaySubstituted` flag suggested name is illustrative, not normative — the implementation may use a different name as long as the data-loss behavior of the current `skipthis` is removed from the publish path.
4. **Consistency:** amends ADR-040's source semantics (publication-source → worldview); refines ADR-066's canonical interpretation (thermostat-side → boiler-side worldview) while preserving its publish-eligibility gate; preserves ADR-068's mutual-exclusion property; ratifies TASK-531's `/gateway` retirement. No contradiction with ADR-051 (settings encapsulation), ADR-052 (publish-eligibility umbrella), ADR-065 (`otgw-pic/` subtree — orthogonal namespace), ADR-067 (wipe-on-OTA — companion mechanism).

## Related Decisions

- **ADR-040:** MQTT Source-Specific Topics for OpenTherm Values. **This ADR amends ADR-040** by changing the subtopic semantic model from source-of-publication to worldview. ADR-040's "source = which side originated the frame" interpretation is replaced by "subtopic = which side saw the value". Per project amendment convention, ADR-040's status line should be updated to note "amended by ADR-069".
- **ADR-066:** MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification. **This ADR refines ADR-066** by shifting the canonical-topic interpretation from "thermostat-side intent" to "boiler-side worldview" while preserving the per-MsgID `bSlaveEchoesValue` gate for `/boiler` Write-Ack publications. Reads under answer-override are the only default-user behavior change.
- **ADR-068:** bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive. Companion. The mutual-exclusion property is preserved. Worldview routing operates entirely within the source-variant publishing path; default users (`bSeparateSources = false`) see only the canonical change described above.
- **ADR-065:** otgw-pic/ MQTT Subtree as Stable Public Topic API. Orthogonal namespace. Not affected by this ADR.
- **ADR-067:** HA Discovery Wipe-on-OTA. Companion mechanism. The shift in subtopic semantics will benefit from the wipe-on-OTA cleanup if any consumers had cached the old `A → /boiler` routing in retained-state assumptions.
- **TASK-531:** Restored backward-compatible bare topic for gateway-source HA entities. **This ADR ratifies the `/gateway` subtopic retirement** that TASK-531 implemented without a formal ADR. Worldview semantics make `/gateway` redundant by design (override = subtopic divergence).
- **TASK-549:** Implementation tracking task. Carries the seven acceptance criteria for the worldview-routing change and the HA-discovery verification work.

## References

- Routing implementation: `src/OTGW-firmware/MQTTstuff.ino:1185-1196` (`resolveSourceIndex`).
- Source-key table: `src/OTGW-firmware/MQTTstuff.ino:442-446` (`mqttSourceKeys`).
- Per-source publish: `src/OTGW-firmware/MQTTstuff.ino:1209-1234` (`publishToSourceTopic`).
- OT-frame skip logic to be reworked: `src/OTGW-firmware/OTGW-Core.ino:4046-4051`.
- Decode-and-publish entry point: `src/OTGW-firmware/OTGW-Core.ino:4132-4143`.
- User-facing contract documentation: `docs/api/MQTT.md` "Source-Separated Topics" section (updated as part of TASK-549).
- Field report: Discord `#beta-testing`, beta-tester Andre, 2026-05-07. OT-log capture preserved in TASK-549 description.
- HA discovery generators (unchanged by this ADR): `src/OTGW-firmware/mqtt_configuratie.cpp:1896-1911` (`composeSensorPayload`), `:1989-2000` (`composeBinSensorPayload`), `:2284-2291+` (`expandAndStreamSensorSources`).

## Enforcement

```json
{
  "llm_judge": true
}
```
