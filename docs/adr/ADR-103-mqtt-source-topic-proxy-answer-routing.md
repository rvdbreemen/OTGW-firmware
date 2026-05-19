# ADR-103: MQTT Source-Topic Worldview Routing — Proxy-Answer (no-B) Refinement

## Status

Accepted, 2026-05-19 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Classification: supersession of ADR-096 (worldview routing table is restated here in full with one added row). Decision Maker: User: Robert van den Breemen (rvdbreemen). 2.0.0 feature-line sibling of dev-line ADR-075 — the decision is coherent across both lines.

## Context

ADR-096 ("MQTT Source-Subtopic Worldview Semantics", Accepted 2026-05-07) is the 2.0.0 feature-line equivalent of dev-line ADR-069. It adopted the worldview model: each source subtopic carries what the named device *sees*. Its routing table fixes the gateway-faked Answer-Thermostat frame (`A`) to `_thermostat` only, and `is_value_valid_for_master_topic()` blocks every `A` from the canonical topic on the rationale "`B` carries canonical for reads".

That rationale embeds an implicit assumption: **every readable MsgID has a `B` frame from the boiler** delivering the boiler-side worldview to canonical and `_boiler`. It fails for **OTGW-internally-answered MsgIDs** — the gateway answers the thermostat directly with an `A` frame and **no `B` frame is ever emitted**. This occurs for gateway-served IDs (e.g. **MaxTSet, ID 57**) and whenever no boiler is attached or the attached boiler does not implement the requested ID and the OTGW proxies the answer. On the 2.0.0 line this also covers OTGW32/ESP32 standalone-proxy bring-up where a boiler may be absent during bench testing.

Consequence on the feature line, for a proxy-answered ID such as MaxTSet:

- canonical `…/MaxTSet` — **empty**: `is_value_valid_for_master_topic` rejects all `A` frames (`OTGW-Core.ino:1293`).
- `…/MaxTSet_boiler` — **empty**: `A` routes to `_thermostat` only (`MQTTstuff.ino:1658-1660`).
- `…/MaxTSet_thermostat` — receives the value, **but only when `bSeparateSources = true`**.

With the default `bSeparateSources = false`, a proxy-answered MaxTSet is **invisible on MQTT entirely** — the same "permanently-empty subtopic / confusing HA sensor" failure class ADR-096 was created to eliminate, reached through a frame pattern ADR-096 did not consider. Originating field report: PR #565 (1.5.x line). Dev-line ADR-075 fixes the same gap on `dev`; this ADR carries the identical decision to the 2.0.0 line.

The two `A` scenarios are mechanically distinguishable: an **answer-override `A`** is preceded by a `B` for the same id within 500 ms — already detected by the delay machinery at `OTGW-Core.ino:4096-4098`. A **proxy `A`** has no preceding `B`. The current code carries no flag forward to publish time recording which kind of `A` is being processed.

## Decision

**Refine worldview routing to distinguish a proxy-answer `A` (no preceding `B`) from an answer-override `A` (preceded by `B`). A proxy `A` is the de-facto boiler-side value — the gateway is the sole responder and no boiler frame exists — so it publishes to `_thermostat`, `_boiler`, and canonical. An answer-override `A` is unchanged from ADR-096: `_thermostat` only.**

Add a per-frame RAM discriminator `bAnswerOverride` (a `byte` on `OpenthermData_t`, default `0`), set `1` only when the existing `(B → A)` override pair is detected, and carried forward so the promoted `A` frame knows its origin at publish time.

Routing table (supersedes ADR-096's table in full; `T`/`R`/`B` rows are unchanged from ADR-096):

| Frame (`OTdata.rsptype`) | `_thermostat` | `_boiler` | canonical |
|---|---|---|---|
| `OTGW_THERMOSTAT` (T) | yes | yes (no gateway-write override) | yes (no `R` follows ≤ 500 ms) |
| `OTGW_REQUEST_BOILER` (R) | no | yes | yes (overrides `T`) |
| `OTGW_BOILER` (B) | yes (no answer-override) | yes | yes |
| `OTGW_ANSWER_THERMOSTAT` (A) — answer-override, `bAnswerOverride == 1` | yes | **no** | **no** |
| `OTGW_ANSWER_THERMOSTAT` (A) — proxy answer, `bAnswerOverride == 0` | yes | **yes** | **yes** |

Conceptual statement, extending ADR-096's "canonical = boiler-side worldview": when the boiler emits no frame, the gateway's proxied answer *is* the boiler-side worldview, because it is the only value the bus ever carried for that id. ADR-096's answer-override invariant is preserved exactly: when a genuine `B` exists, that `B` owns `_boiler` and canonical and the faked `A` never overwrites it.

### Implementation primitives — 2.0.0 feature line

(Delivered on a sibling branch of `feature-dev-2.0.0-otgw32-esp32-sat-support`; line numbers per the current feature branch.)

- `OTGW-Core.h:559+` — add `byte bAnswerOverride;` to `OpenthermData_t` immediately after `bGatewaySubstituted`, with a comment that it is `1` only on a `(B,A)` answer-override `A` and `0` (default) on a proxy `A`.
- `OTGW-Core.ino:4075` — `OTdata.bAnswerOverride = false;` alongside the existing `skipthis` / `bGatewaySubstituted` resets.
- `OTGW-Core.ino:4102` — after `delayedOTdata = OTdata;`, add `delayedOTdata.bAnswerOverride = bGatewaySubstituted && (delayedOTdata.rsptype == OTGW_ANSWER_THERMOSTAT);`. The detected `(B,A)` pair sets `bGatewaySubstituted` for the older `B`; the incoming `A` (now the delayed frame) inherits the override marker and carries it through the next swap to the cycle that publishes it.
- `OTGW-Core.ino:1293` — change `if (OT.rsptype == OTGW_ANSWER_THERMOSTAT) return false;` to `if (OT.rsptype == OTGW_ANSWER_THERMOSTAT && OT.bAnswerOverride) return false;`.
- `MQTTstuff.ino:1658-1660` — `case OTGW_ANSWER_THERMOSTAT:` becomes `toThermostat = true; toBoiler = !OTdata.bAnswerOverride;`. Update the routing comment block (`~:1597-1609`, including the `A : _thermostat only` summary line) to document both `A` sub-cases and cite this ADR.

Per-platform note (ESP32-S3 / OTGW32): the change is platform-independent — it operates on the decoded OT frame stream, not on board-specific I/O. No conditional compilation, deadband, or pin-map considerations apply. The single extra `byte` is negligible on the ESP32-S3 RAM budget.

### Dev-line coherence

The 1.5.x/dev sibling is **ADR-075** (`docs/adr/ADR-075-mqtt-source-topic-proxy-answer-routing.md` on `dev`), which supersedes dev-line ADR-069. The decision, routing table, and discriminator design are identical; only the ADR numbers (096/103 here vs 069/075 on dev) and source line offsets differ.

## Alternatives Considered

### Alternative A — Blanket `A → _boiler` unconditionally (PR #565 as written)

`case OTGW_ANSWER_THERMOSTAT: toThermostat = true; toBoiler = true;` for every `A`.

**Rejected.** It fixes the proxy case but regresses ADR-096's answer-override invariant: in a `(B,A)` override, `B` publishes to `_boiler` first, then the gateway-faked `A` (published ~ms later, last-write-wins) overwrites `_boiler` with the faked value, so `_boiler` no longer reflects what the boiler actually sent. This directly contradicts the Accepted ADR-096 Decision table and would be blocked by ADR-096's `llm_judge` enforcement.

### Alternative B — Status quo (do nothing)

**Rejected.** Proxy-answered IDs (MaxTSet/57 and any gateway-served or boiler-unsupported id) stay invisible on canonical and `_boiler`, and entirely invisible on MQTT for the default `bSeparateSources = false` cohort — the exact UX failure class ADR-096 set out to remove. On the 2.0.0 OTGW32 line, bench bring-up without a boiler attached makes this especially visible.

### Alternative C — Proxy `A → canonical` only (not `_boiler`)

**Rejected.** Under the worldview model `_boiler` and canonical both describe the boiler's view and must agree; a permanently-empty `_boiler` for proxy IDs while canonical is populated reproduces ADR-096's original "empty `boiler_*` sensor" complaint for `bSeparateSources = true` users and splits the mental model.

### Alternative D — Proxy `A → _boiler` only (not canonical)

**Rejected.** Default users (`bSeparateSources = false`) see no subtopics, so they would still get nothing for MaxTSet. Canonical = boiler-side worldview must carry the only boiler-side value the bus produced.

## Consequences

### Positive

- Proxy-answered IDs (MaxTSet/57, and any gateway-served or boiler-unsupported id) become visible on canonical and `_boiler`; default `bSeparateSources = false` users get MaxTSet on the canonical topic again.
- ADR-096's answer-override invariant is preserved **exactly** — a genuine `B` still owns `_boiler`/canonical; the faked `A` never overwrites it.
- Minimal surface: one RAM `byte`, one init line, one assignment, one gate predicate, one routing line, plus comment/ADR updates. `T`/`R`/`B` routing and the ADR-066 eligibility gate are untouched. Platform-independent — applies unchanged to ESP8266 and ESP32-S3/OTGW32.

### Negative

- One extra `byte` per `OpenthermData_t` instance (negligible on the ESP32-S3 budget).
- A future contributor must understand that `A` now has two sub-cases. Mitigated by the updated comment block citing this ADR and by `bAnswerOverride` being set in exactly one place.
- Slight publish-volume increase for proxy IDs under `bSeparateSources = true` (now also `_boiler` + canonical). Bounded by proxy-id frequency; the publish path is non-blocking.

### Neutral

- HA discovery generators are unchanged — `_thermostat`/`_boiler` variants are already emitted; only publish-time routing changes.
- `bSeparateSources = false` users see only the intended canonical change (proxy IDs reappear); no new topics.

### Risks & Mitigation

- **Risk:** the override marker is mis-propagated through the 1-frame delay swap, mislabeling an override `A` as proxy and overwriting genuine `B`. **Mitigation:** the marker is set on the incoming `A` at the same cycle the `(B,A)` pair is detected and rides the existing, well-exercised swap; hardware validation with an active `TT=`/`CS=` answer-override on a read id (AC) confirms `_boiler` keeps the `B` value.
- **Risk:** a proxy `A` is wrongly suppressed from canonical because some unrelated prior frame set the marker. **Mitigation:** `bAnswerOverride` is reset to `false` on every frame init (alongside `skipthis`) and set `true` only inside the `(B,A)` detection branch.

## Verification gates

1. **Completeness:** Status / Context / Decision / Alternatives (4) / Consequences (positive+negative) / Related / References present; filename `ADR-103-mqtt-source-topic-proxy-answer-routing.md` matches the `# ADR-103` heading.
2. **Evidence:** PR #565 field report (MaxTSet/ID 57 standalone-proxy); concrete feature-line code paths cited with file:line (`MQTTstuff.ino:1658`, `OTGW-Core.ino:1293`, `:4096`/`4102`, `OTGW-Core.h:559`).
3. **Clarity:** the routing table plus the five named edit sites are the complete implementation spec; single concrete decision (proxy/override split via `bAnswerOverride`), imperative voice, no hedging.
4. **Consistency:** supersedes ADR-096 (its Status line is set to "Superseded by ADR-103" — body unchanged); preserves ADR-066's eligibility gate and ADR-040; coherent with dev-line ADR-075; ADR-103 is the next free number after 102.

## Related Decisions

- **ADR-096:** MQTT Source-Subtopic Worldview Semantics (2.0.0 line). **This ADR supersedes ADR-096.** The full worldview routing table is restated here with one added row distinguishing proxy `A` from answer-override `A`. Upon acceptance, ADR-096's Status line is changed (only) to "Superseded by ADR-103, 2026-05-19"; its body remains the historical record.
- **ADR-075 (dev line):** The 1.5.x sibling of this decision. Identical routing table and discriminator; the decision is coherent across both lines per the cross-worktree convention.
- **ADR-066:** MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification. Preserved; only which `A` sub-case is eligible for canonical/`_boiler` changes.
- **ADR-040:** MQTT Source-Specific Topics. Unaffected.
- **PR #565:** The originating field report. Its blanket approach is rejected (Alternative A).

## References

- `src/OTGW-firmware/MQTTstuff.ino:1614-1675` — `publishToSourceTopic()` (`A` case `:1658-1660`; routing comment `:1597-1609`).
- `src/OTGW-firmware/OTGW-Core.ino:~1290-1294` — `is_value_valid_for_master_topic()` (`A` gate `:1293`).
- `src/OTGW-firmware/OTGW-Core.ino:4073-4104` — frame init + `(T,R)`/`(B,A)` detection + 1-frame delay swap.
- `src/OTGW-firmware/OTGW-Core.h:559` — `OpenthermData_t` (`bGatewaySubstituted` / new `bAnswerOverride`).
- PR #565 — https://github.com/rvdbreemen/OTGW-firmware/pull/565
- ADR-096 — `docs/adr/ADR-096-mqtt-source-topic-worldview-semantics.md`
- ADR-075 (dev line) — `docs/adr/ADR-075-mqtt-source-topic-proxy-answer-routing.md` on branch `dev`

## Enforcement

```json
{
  "llm_judge": true
}
```
