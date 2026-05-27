# ADR-075: MQTT Source-Topic Worldview Routing — Proxy-Answer (no-B) Refinement

## Status

Accepted, 2026-05-19 (four verification gates passed: Completeness, Evidence, Clarity, Consistency). Classification: supersession of ADR-069 (worldview routing table is restated here in full with one added row). Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

ADR-069 ("MQTT Source-Subtopic Worldview Semantics", Accepted 2026-05-07) adopted the worldview model: each source subtopic carries what the named device *sees*. Its routing table fixes the gateway-faked Answer-Thermostat frame (`A`) to `_thermostat` only, and `is_value_valid_for_master_topic()` blocks every `A` from the canonical topic, on the stated rationale "`B` carries canonical for reads".

That rationale embeds an implicit assumption: **every readable MsgID has a `B` frame from the boiler** that delivers the boiler-side worldview to the canonical and `_boiler` topics. The assumption holds when a physical boiler answers. It fails for **OTGW-internally-answered MsgIDs** — the gateway answers the thermostat directly with an `A` frame and **no `B` frame is ever emitted**. This occurs for gateway-served IDs (e.g. **MaxTSet, ID 57**), and more generally whenever no boiler is attached or the attached boiler does not implement the requested ID and the OTGW proxies the answer.

Consequence on current `dev` (`1.5.1-beta.11`) and on the 2.0.0 feature line, for a proxy-answered ID such as MaxTSet:

- canonical `…/MaxTSet` — **empty**: `is_value_valid_for_master_topic` rejects all `A` frames (`OTGW-Core.ino:1276` on dev; `:1293` on the feature line).
- `…/MaxTSet_boiler` — **empty**: `A` routes to `_thermostat` only (`MQTTstuff.ino:1291-1293` dev; `:1658-1660` feature).
- `…/MaxTSet_thermostat` — receives the value, **but only when `bSeparateSources = true`**.

With the default `bSeparateSources = false`, a proxy-answered MaxTSet is **invisible on MQTT entirely** — the same "permanently-empty subtopic / confusing HA sensor" failure class ADR-069 was created to eliminate, reached through a frame pattern ADR-069 did not consider. PR #565 ("MaxTSet /boiler MQTT source topic never updated when OTGW acts as standalone proxy") reported this; two of its three changes (sibling-suffix separator, Write-Ack enum-collision gate) reached `dev` independently, but its core change did not, and the PR is now unmergeable (stale base, `version.h` bumped to `1.5.0-beta.22` while `dev` is at `1.5.1-beta.11`).

The two `A` scenarios are mechanically distinguishable: an **answer-override `A`** is preceded by a `B` for the same id within 500 ms — already detected by the delay machinery at `OTGW-Core.ino:4092-4094` (dev; `:4096` feature). A **proxy `A`** has no preceding `B`. The current code carries no flag forward to publish time recording which kind of `A` is being processed.

## Decision

**Refine worldview routing to distinguish a proxy-answer `A` (no preceding `B`) from an answer-override `A` (preceded by `B`). A proxy `A` is the de-facto boiler-side value — the gateway is the sole responder and no boiler frame exists — so it publishes to `_thermostat`, `_boiler`, and canonical. An answer-override `A` is unchanged from ADR-069: `_thermostat` only.**

Add a per-frame RAM discriminator `bAnswerOverride` (a `byte` on `OpenthermData_t`, default `0`), set `1` only when the existing `(B → A)` override pair is detected, and carried forward so the promoted `A` frame knows its origin at publish time.

Routing table (supersedes ADR-069's table in full; `T`/`R`/`B` rows are unchanged from ADR-069):

| Frame (`OTdata.rsptype`) | `_thermostat` | `_boiler` | canonical |
|---|---|---|---|
| `OTGW_THERMOSTAT` (T) | yes | yes (no gateway-write override) | yes (no `R` follows ≤ 500 ms) |
| `OTGW_REQUEST_BOILER` (R) | no | yes | yes (overrides `T`) |
| `OTGW_BOILER` (B) | yes (no answer-override) | yes | yes |
| `OTGW_ANSWER_THERMOSTAT` (A) — answer-override, `bAnswerOverride == 1` | yes | **no** | **no** |
| `OTGW_ANSWER_THERMOSTAT` (A) — proxy answer, `bAnswerOverride == 0` | yes | **yes** | **yes** |

Conceptual statement, extending ADR-069's "canonical = boiler-side worldview": when the boiler emits no frame, the gateway's proxied answer *is* the boiler-side worldview, because it is the only value the bus ever carried for that id. The answer-override invariant from ADR-069 is preserved exactly: when a genuine `B` exists, that `B` owns `_boiler` and canonical and the faked `A` never overwrites it.

### Implementation primitives — 1.5.x / `dev` line

(Delivered on `claude/review-pr-565-analysis-eFPJE`; line numbers per current `dev`.)

- `OTGW-Core.h` — add `byte bAnswerOverride;` to `OpenthermData_t` immediately after `bGatewaySubstituted` (`:549`), with a comment that it is `1` only on a `(B,A)` answer-override `A` frame and `0` (default) on a proxy `A`.
- `OTGW-Core.ino` frame init (`~:4071`) — `OTdata.bAnswerOverride = false;` alongside the existing `skipthis` / `bGatewaySubstituted` resets.
- `OTGW-Core.ino` detection/reorder (`~:4098-4100`) — after `delayedOTdata = OTdata;`, add `delayedOTdata.bAnswerOverride = bGatewaySubstituted;`. The detected `(B,A)` pair sets `bGatewaySubstituted` for the older `B`; the incoming `A` (now the delayed frame) inherits the override marker and carries it through the next swap to the cycle that publishes it.
- `OTGW-Core.ino:1276` — change `if (OT.rsptype == OTGW_ANSWER_THERMOSTAT) return false;` to `if (OT.rsptype == OTGW_ANSWER_THERMOSTAT && OT.bAnswerOverride) return false;`.
- `MQTTstuff.ino:1291-1293` — `case OTGW_ANSWER_THERMOSTAT:` becomes `toThermostat = true; toBoiler = !OTdata.bAnswerOverride;`. Update the routing comment block (`~:1231-1246`, including the `A : _thermostat only` summary line) to document both `A` sub-cases and cite this ADR.

### Implementation primitives — 2.0.0 feature line (port; separate worktree)

The 2.0.0 worktree (`feature-dev-2.0.0-otgw32-esp32-sat-support`) uses independent ADR numbering (the equivalent of ADR-069 there is **ADR-096**) and is not present in this single-worktree container. The logically identical change applies at: `OTGW-Core.h:559`, `OTGW-Core.ino` detection `~:4096-4109`, master-topic gate `OTGW-Core.ino:1293`, `MQTTstuff.ino:1658-1660`. A coherent sibling ADR is authored in the 2.0.0 `docs/adr/` (next free number there) cross-referencing this one; the decision must stay coherent across both lines.

## Alternatives Considered

### Alternative A — Blanket `A → _boiler` unconditionally (PR #565 as written)

`case OTGW_ANSWER_THERMOSTAT: toThermostat = true; toBoiler = true;` for every `A`.

**Rejected.** It fixes the proxy case but regresses ADR-069's answer-override invariant: in a `(B,A)` override, `B` publishes to `_boiler` first, then the gateway-faked `A` (published ~ms later, last-write-wins) overwrites `_boiler` with the faked value, so `_boiler` no longer reflects what the boiler actually sent. PR #565's own comment concedes this ("acceptable") but it directly contradicts the Accepted ADR-069 Decision table and would also be blocked by ADR-069's `llm_judge` enforcement.

### Alternative B — Status quo (do nothing)

**Rejected.** Proxy-answered IDs (MaxTSet/57 and any gateway-served or boiler-unsupported id) stay invisible on canonical and `_boiler`, and entirely invisible on MQTT for the default `bSeparateSources = false` cohort. This is the exact UX failure class ADR-069 set out to remove; leaving it guarantees recurring field reports.

### Alternative C — Proxy `A → canonical` only (not `_boiler`)

**Rejected.** Under the worldview model `_boiler` and canonical both describe the boiler's view and must agree; a permanently-empty `_boiler` for proxy IDs while canonical is populated reproduces ADR-069's original "empty `boiler_*` sensor" complaint for `bSeparateSources = true` users and splits the mental model.

### Alternative D — Proxy `A → _boiler` only (not canonical)

**Rejected.** Default users (`bSeparateSources = false`) see no subtopics, so they would still get nothing for MaxTSet. Canonical = boiler-side worldview must carry the only boiler-side value the bus produced.

## Consequences

### Positive

- Proxy-answered IDs (MaxTSet/57, and any gateway-served or boiler-unsupported id) become visible on canonical and `_boiler`; default `bSeparateSources = false` users get MaxTSet on the canonical topic again.
- ADR-069's answer-override invariant is preserved **exactly** — a genuine `B` still owns `_boiler`/canonical; the faked `A` never overwrites it.
- Minimal surface: one RAM `byte`, one init line, one assignment, one gate predicate, one routing line, plus comment/ADR updates. `T`/`R`/`B` routing and the ADR-066 eligibility gate are untouched.

### Negative

- One extra `byte` per `OpenthermData_t` instance (a handful of instances; negligible against the ~40 KB budget).
- A future contributor must understand that `A` now has two sub-cases. Mitigated by the updated comment block citing this ADR and by `bAnswerOverride` being set in exactly one place.
- Slight publish-volume increase for proxy IDs under `bSeparateSources = true` (now also `_boiler` + canonical). Bounded by proxy-id frequency; the publish path is non-blocking (ADR-006).

### Neutral

- HA discovery generators are unchanged — `_thermostat`/`_boiler` variants are already emitted; only publish-time routing changes.
- `bSeparateSources = false` users see only the intended canonical change (proxy IDs reappear); no new topics.

### Risks & Mitigation

- **Risk:** the override marker is mis-propagated through the 1-frame delay swap, mislabeling an override `A` as proxy and overwriting genuine `B`. **Mitigation:** the marker is set on the incoming `A` at the same cycle the `(B,A)` pair is detected and rides the existing, well-exercised swap; hardware validation with an active `TT=`/`CS=` answer-override on a read id (AC) confirms `_boiler` keeps the `B` value.
- **Risk:** a proxy `A` is wrongly suppressed from canonical because some unrelated prior frame set the marker. **Mitigation:** `bAnswerOverride` is reset to `false` on every frame init (alongside `skipthis`) and set `true` only inside the `(B,A)` detection branch.

## Verification gates

1. **Completeness:** Status / Context / Decision / Alternatives (4) / Consequences (positive+negative) / Related / References present; filename `ADR-075-mqtt-source-topic-proxy-answer-routing.md` matches the `# ADR-075` heading.
2. **Evidence:** PR #565 field report (MaxTSet/ID 57 standalone-proxy); concrete code paths cited with file:line on both branches (`MQTTstuff.ino:1291`/`1658`, `OTGW-Core.ino:1276`/`1293`, `:4092`/`4096`, `OTGW-Core.h:549`/`559`).
3. **Clarity:** the routing table plus the five named edit sites are the complete implementation spec; single concrete decision (proxy/override split via `bAnswerOverride`), imperative voice, no hedging.
4. **Consistency:** supersedes ADR-069 (status line of ADR-069 to be set to "Superseded by ADR-075" only after this ADR is Accepted); preserves ADR-066's eligibility gate and ADR-068's mutual exclusion; ADR-040/065 unaffected; no duplicate ADR number (075 is the next free after 074).

## Related Decisions

- **ADR-069:** MQTT Source-Subtopic Worldview Semantics. **This ADR supersedes ADR-069.** The full worldview routing table is restated here with one added row distinguishing proxy `A` from answer-override `A`. Upon acceptance, ADR-069's Status line is changed (only) to "Superseded by ADR-075, YYYY-MM-DD"; its body remains the historical record.
- **ADR-066:** MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification. Preserved. The `bSlaveEchoesValue` Write-Ack gate and canonical eligibility contract are unchanged; this ADR only changes which `A` sub-case is eligible for canonical/`_boiler`.
- **ADR-068:** bSeparateSources Mutual Exclusion. Preserved; routing operates within the source-variant path.
- **ADR-040 / ADR-065:** Unaffected (orthogonal namespace / superseded framing).
- **PR #565:** The originating field report. Its blanket approach is rejected (Alternative A); its stale base and `version.h` bump make it unmergeable. This ADR + its task supersede that PR.
- **2.0.0 sibling ADR:** A coherent ADR in the `feature-dev-2.0.0-otgw32-esp32-sat-support` worktree's `docs/adr/` (independent numbering; the local equivalent of ADR-069 there is ADR-096) carries the same decision for the 2.0.0 line.

## References

- `src/OTGW-firmware/MQTTstuff.ino:1247-1308` — `publishToSourceTopic()` (dev); feature-line equivalent `:1620-1680` (`A` case `:1658-1660`).
- `src/OTGW-firmware/OTGW-Core.ino:1272-1284` — `is_value_valid_for_master_topic()` (dev; feature-line gate `:1293`).
- `src/OTGW-firmware/OTGW-Core.ino:4071-4100` — frame init + `(T,R)`/`(B,A)` detection + 1-frame delay swap (dev; feature-line `~:4096`).
- `src/OTGW-firmware/OTGW-Core.h:538-554` — `OpenthermData_t` (dev; feature-line `bGatewaySubstituted` at `:559`).
- PR #565 — https://github.com/rvdbreemen/OTGW-firmware/pull/565
- ADR-069 — `docs/adr/ADR-069-mqtt-source-topic-worldview-semantics.md`

## Enforcement

```json
{
  "llm_judge": true
}
```
