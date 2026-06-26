# ADR-155: v2 Web UI Connectivity Model — Two-Link OT Bus, MODE Separated from HEALTH, Five-State Vocabulary

## Status

Accepted. Date: 2026-06-26.

**Decision Maker:** User: Robert van den Breemen (maintainer). The model originated in the v2 Web UI design exploration (the `boiler-dashboard-concepts.html` mockup, PR #649) and was implemented and validated this session; the maintainer directed that it be recorded and accepted.

## Status History

status_history:
  - date: 2026-06-26
    status: Accepted
    changed_by: User
    reason: Initial decision record; model implemented in alpha.268 and merged at alpha.274 on origin/dev, live-verified on a real OTGW32
    changed_via: adr-kit

## Context

The OpenTherm (OT) bus connects two devices to the gateway: a thermostat (the OT master, which asks) and a boiler (the OT slave, which answers). Until this decision the firmware exposed bus liveness as a single boolean `state.otBus.bOnline`, defined as `bBoilerState || bThermostatState` (OTGW-Core.ino). The v2 Web UI rendered that one flag as a single "OT bus down" pill.

That single flag cannot distinguish two materially different fault conditions: "the boiler is not answering" (a wiring or boiler-power fault) from "the thermostat is not asking" (the room unit is offline or in a quiet window). A user staring at "OT bus down" cannot tell which side to check.

Two further defects compounded this. First, the gateway operating MODE (gateway vs monitor) was conflated with health: the v2 map chip labelled "GATEWAY MODE" was fed `h.networkmode` (Wi-Fi vs Ethernet), so it rendered "WiFi" where the OT mode belongs. Second, connectivity state used an ad-hoc `up/weak/down` vocabulary that disagreed with the on-page legend and offered no distinct state for "disabled in settings" (which read as a red error) or "not yet determined".

The underlying state already carried the richer data: `OTBustypes.h:26-29` declares `bGatewayMode`/`bGatewayModeKnown`, `bBoilerState`, and `bThermostatState` as fields distinct from `bOnline`, and `/api/v2/device/info` already emitted all three (restAPI.ino:2778-2783). The gap was that `/api/v2/health` — the endpoint the v2 UI polls — emitted only `mqttconnected` and `otgwconnected` (=`bOnline`), and the UI had no model to consume the richer state.

## Decision

The v2 Web UI models the OT bus as two independent links (thermostat and boiler), renders the gateway MODE as a separate non-health chip, and uses one five-state-plus-mode vocabulary everywhere; the firmware `/api/v2/health` endpoint is extended additively to carry the data this requires.

Concretely:

1. **Two independent OT links.** Thermostat and boiler are separate connectivity entities, each with its own state, map node, strip pill, and detail row. On PIC firmware they read the independent `thermostatconnected`/`boilerconnected` flags; on OT-Direct hardware, which does not populate the OT sub-states, both fall back to `otgwconnected` (`bOnline`) so the UI stays honest rather than inventing two flags from one.
2. **MODE separated from HEALTH.** Gateway-vs-monitor mode is a setting, never a green/red health light. It renders as a distinct blue `st-mode` chip driven by `otgwmode` (`gateway`/`monitor`/`detecting`/`N/A`), never by the network mode.
3. **One five-state vocabulary.** Every connectivity surface uses `connected` / `degraded` / `disconnected` / `off` / `unknown` plus `mode`, expressed as colour + icon + text (v2.js:99-101 `STICON`/`STLABEL`/`STV`). `off` is reserved for an integration disabled in settings (grey, not a red fault); `unknown` is the pre-fetch state.
4. **Additive REST contract.** `sendHealth()` (restAPI.ino:2857) emits `thermostatconnected`, `boilerconnected`, `otcommandinterface`, `otgwmode`, and `ntpenable` (restAPI.ino:2887-2896), mirroring the proven `/api/v2/device/info` emitter (restAPI.ino:2774-2783). The fields are additive; existing `/health` consumers are unaffected.

## Alternatives Considered

### Alternative A: Keep the single `bOnline` "OT bus" flag

The status quo: one pill, one state. Rejected because it is exactly the ambiguity that motivated the change — it cannot tell "boiler silent" from "thermostat silent", which are different repairs, and it leaves the maintainer-facing complaint ("I see OT bus down but the gateway is clearly working") unaddressed.

### Alternative B: Split the two OT links but leave MODE folded into health

Add thermostat/boiler split but keep showing gateway mode as a coloured health light. Rejected because a gateway running in monitor mode is not "unhealthy" — painting a legitimate mode amber/red trains the user to ignore the colour. Separating MODE from HEALTH is what keeps the five-state colours meaningful.

### Alternative C: Mirror the Home Assistant per-entity availability model end to end

Drive every UI element from a full HA-style availability topology. Rejected as over-scoped for a LAN status panel: it would couple the UI to the MQTT discovery topology (ADR-140) and add per-entity plumbing the page does not need. The five-state vocabulary captures the same user-facing distinctions with a fraction of the surface.

### Alternative D: Do nothing

Leave the connectivity panel as built. Rejected: the adversarial design review identified the single-flag model, the wrong MODE datum, and the dead `off`/`unknown` states as concrete, reproducible defects against the approved mockup.

## Consequences

**Benefits**

- A bus fault is now locatable: a down thermostat link and a down boiler link render as distinct rows with distinct fix hints, so the user knows which side to check.
- MODE is legible and never misread as a fault; the gateway/monitor chip shows the real OT mode instead of "WiFi".
- One vocabulary across strip pills, the chain map, and the detail list removes the legend-vs-rows mismatch; a disabled integration reads grey "Off" instead of a red "Disconnected".
- The REST change is ~6 lines and purely additive (restAPI.ino:2887-2896), reusing an already-shipped pattern from `/api/v2/device/info`; no existing consumer breaks.

**Trade-offs**

- The UI now branches on `otcommandinterface`: on OT-Direct it deliberately drives both links from `bOnline` (v2.js:867-872), so on that hardware the two-link split is presentational, not a genuine two-source signal.
- Two more boolean fields on every `/health` response (a hot, frequently-polled endpoint), a negligible payload increase.

**Risks and mitigations**

- *Risk*: on a real OTGW32 (OT-Direct) with a live boiler, `bThermostatState`/`bBoilerState` stay at their `false` init because only the OTGW-Core PIC/OT-frame parser (OTGW-Core.ino:4599/4606) populates them; OTDirect.ino sets only `bOnline`. *Mitigation*: the UI already falls back to `bOnline` for both links when `otcommandinterface == "OT-Direct"`, so the panel is correct on that hardware today; populating the OTDirect sub-states is a separate, optional firmware follow-up, not a prerequisite for this model.
- *Risk*: `off`-vs-`down` for MQTT depends on the settings being loaded; if `setData` is empty the UI could mis-render. *Mitigation*: `fetchConn` renders `unknown` (grey) rather than `down` (red) when the enabled-state is not yet known, and `init()` prefetches settings.

## Related Decisions

- **ADR-152 (Coexisting v2 Web UI selected by a device-wide setting)**: this connectivity model lives inside the v2 Web UI that ADR-152 introduces; complements it.
- **ADR-084 (Generic OT-bus state MQTT topics)**: the MQTT-side representation of the same `bThermostatState`/`bBoilerState`/`bGatewayMode` signals; this ADR is the REST/UI-side counterpart and uses the same state fields.
- **ADR-031 (Two-Microcontroller Coordination Architecture)**: the PIC-vs-ESP split that produces the OT sub-states this model consumes; depends on it.
- **ADR-019 (REST API Versioning Strategy)**: the additive `/api/v2/health` field extension follows the additive-change rule defined there.

## References

- Implementation: alpha.268 (connectivity rebuild) and alpha.274 merge `b69d8743` on `origin/dev` (TASK-933). Live round-trip verified on a real OTGW32.
- Firmware: `src/OTGW-firmware/restAPI.ino:2857` (`sendHealth`), `:2887-2896` (two-link + mode + ntp emit), `:2774-2783` (the `/device/info` emitter it mirrors).
- State: `src/OTGW-firmware/OTBustypes.h:26-29` (`bGatewayMode`, `bBoilerState`, `bThermostatState`); producers `src/OTGW-firmware/OTGW-Core.ino:4599,4606` (PIC path), `src/OTGW-firmware/OTDirect.ino` (sets only `bOnline`).
- Web UI: `src/OTGW-firmware/data/v2.js:99-101` (`STICON`/`STLABEL`/`STV`), `:106` (`CONN` model), `:867-872` (PIC-vs-OT-Direct two-link fallback), `:935` (mode dot), `:1488` (detail badge).
- Design source: `docs/design/boiler-dashboard-concepts.html` and `docs/design/README.md` (PR #649), section "Connectivity: separate MODE from HEALTH".

## Enforcement

No `Enforcement` block. This is a structural / UI-architecture decision with no single mechanically-expressible code boundary: the model spans a JS render layer plus an additive REST emitter, and there is no `evaluate.py` gate for it (consistent with ADR-080's structural-vs-pattern distinction). Drift is caught at PR review against this ADR and against the design mockup, not by a pre-commit rule.
