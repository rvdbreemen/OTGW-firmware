# ADR-161: Dedicated REST Endpoint for the SAT BLE Sensor Roster with a Write-Only Per-Slot Secret

## Status

Proposed. Date: 2026-06-29.

**Decision Maker:** User: Robert van den Breemen (maintainer). This ADR documents a decision already built and verified on-device under TASK-935 (commit `3dffd4d6`) and hardened under TASK-946 (commit `5d6ac053`) on the 2.0.0 `dev` line. It is filed as `Proposed` so the maintainer can review the two reusable patterns it establishes (a dedicated structured endpoint for an indexed collection, and a write-only-secret REST contract) before they are treated as binding precedent. It is guideline-level per ADR-080: there is no `evaluate.py` gate; the write-only property was verified by source audit across every emit path rather than by an automated check. Do not flip to Accepted until the maintainer signs off on the two patterns.

## Status History

status_history:
  - date: 2026-06-29
    status: Proposed
    changed_by: Agent
    reason: Initial decision record for the SAT BLE roster REST endpoint and its write-only-secret contract (TASK-935 + TASK-946 input hardening), built and verified on OTGW32 alpha.285
    changed_via: adr-kit

## Context

The SAT (Smart Autonomous Thermostat) subsystem reads room-temperature data from Bluetooth Low Energy (BLE) sensors over the REST (Representational State Transfer) v2 API (Application Programming Interface). The set of sensors the firmware listens to is the **BLE roster**: 8 indexed slots, each carrying a MAC (Media Access Control) address, a human label, and an optional 32-hex-character (16-byte) Xiaomi MiBeacon v4/v5 decryption **bindkey** (a per-sensor secret), plus three roster-wide fields: a name prefix, a name-filter-ingest flag, and a live slot count. The roster lives in `OTGWSettings` and is declared in `SATtypes.h`, where the bindkey storage is `sBleBindkey[8][33]` (8 slots, 32 hex chars + NUL). The roster was introduced under TASK-508 and extended for encrypted MiBeacon under TASK-930 (see ADR-154).

TASK-935 needed this roster to be round-trippable over the REST v2 API so a configuration UI (and scripted provisioning) can read and write it. Two firmware-wide constraints shaped the design:

1. **No ArduinoJson (ADR-146).** The firmware has no JSON-array parser. Inbound JSON arrays cannot be parsed with a vendored library; any array parse would be a hand-rolled scanner, which is error-prone for a structured collection.
2. **Protected-admin auth and secret-handling contract (ADR-056).** Mutating REST methods are gated behind HTTP Basic Auth plus same-origin CSRF enforcement, and secrets must not be casually echoed.

Two firmware-wide constraints also reference ADR-154's AES-CCM (Advanced Encryption Standard, Counter with CBC-MAC) encrypted-MiBeacon work and the CSRF (Cross-Site Request Forgery) protection in ADR-056.

The naive approach is to expose each roster field as a flat per-key setting through the existing generic `/api/v2/settings` surface: `satblemac0..7`, `satblelabel0..7`, `satblebindkey0..7`, plus the three roster-wide keys. That is roughly 27 flat keys (24 indexed + 3 scalar). It has two concrete problems. First, it floods the generic settings form and its `SET_META` enumeration with 24 indexed keys that belong to one logical collection, not 24 independent toggles. Second, and more seriously, it puts the per-slot bindkey **secret** into the same generic read/write path that echoes values back, creating a secret-echo risk: a `GET /api/v2/settings` would hand the 32-hex bindkey back to any authenticated caller, and the value would surface in `SET_META` and debug logs by default.

## Decision

Expose the BLE roster over REST v2 as a **dedicated endpoint** `/api/v2/sat/ble/roster` (GET / PUT / DELETE), not as ~27 flat per-key settings, and make the per-slot MiBeacon bindkey **write-only** over REST: it can be set but never read back.

This establishes two reusable patterns for the codebase:

1. **A dedicated structured endpoint for an indexed collection.** When a settings group is an indexed collection of N slots (rather than a handful of independent scalars), expose it as one structured endpoint instead of expanding it into N flat settings keys that clutter the generic settings form and its metadata enumeration.
2. **A write-only-secret REST contract.** A secret field may be writable over REST while never being emitted back; reads return only a presence boolean (`has_bindkey`), never the value, across every emit path.

### Endpoint shape (as built, TASK-935 + TASK-946)

`GET /api/v2/sat/ble/roster` returns structured JSON built with the streaming `JsonEmit` writer (ADR-146), shaped as:

```
{
  "count": <int>,
  "name_prefix": "<string>",
  "name_filter_ingest": <bool>,
  "slots": [ { "idx": <0..7>, "mac": "<string>", "label": "<string>", "has_bindkey": <bool> }, ... ]
}
```

Labels are JSON-escaped via `jsonEscapeTo`. The bindkey **value is never emitted**: each slot carries only the `has_bindkey` boolean derived from whether `sBleBindkey[idx]` is non-empty.

`PUT /api/v2/sat/ble/roster` writes a **single slot**, addressed by `idx`, with optional `mac`, `label`, and `bindkey` supplied as **flat query/body parameters** (no JSON-array parse, honouring ADR-146). The write reuses the existing `updateSetting()` field dispatch in `settingStuff.ino`, so per-field validation lives in exactly one place rather than being duplicated in the endpoint. The PUT response echoes **no user-supplied text** (it returns status + `idx` + `has_bindkey` only), which sidesteps any response-escaping concern entirely.

`DELETE /api/v2/sat/ble/roster` clears a single slot (addressed by `idx`).

The whole block is gated `#if HAS_SAT_BLE` and returns 404 when the capability is absent, per the platform-abstraction capability-flag rule.

### Auth

Mutating methods pass through the central mutating-method gate in `processAPI`, and `handleSAT` additionally calls its own `checkHttpAuth`. The roster **GET also requires auth** (it is not anonymous), because even the slot MACs and labels are configuration data, and the endpoint must be a single uniformly-protected surface.

### Input hardening (TASK-946)

The initial build had two silent-failure modes that TASK-946 closed:

- **Strict index parsing.** `idx` is parsed with a strict `strtol` check (via `satBleParseRosterIdx`) that rejects non-numeric input. Previously a non-numeric `idx` silently parsed to 0 and wrote slot 0 instead of erroring.
- **400 on malformed payload.** A malformed `mac` or `bindkey` now returns HTTP 400 (via `satBleMacValid` / `satBleBindkeyValid`). Previously these reached `updateSetting()`, hit its silent no-op path, and the endpoint returned 200 as if the write had succeeded.

### Write-only verification

The write-only property of the bindkey was verified by auditing **every** emit path in the firmware: the roster GET (emits `has_bindkey` only), the BLE discovery GET (emits `has_key` only), `GET /api/v2/settings` (does not emit the value), and debug logging (masked). No path returns the bindkey value.

## Alternatives Considered

### Alternative A: Flat per-key settings over the generic `/api/v2/settings`

Expose every roster field as an individual setting key (`satblemac0..7`, `satblelabel0..7`, `satblebindkey0..7`, plus the three roster-wide keys) and let the existing generic settings GET/POST handle it. Rejected on two grounds. First, **clutter**: it injects ~24 indexed keys into the generic settings form and its `SET_META` enumeration, drowning the handful of genuinely independent settings in what is really one indexed collection. Second, and decisive, **secret echo**: the generic settings path reads values back, so a `GET /api/v2/settings` would return the 32-hex bindkey to any authenticated caller and surface it in `SET_META` and default debug output. Keeping a secret out of a read-back-by-default surface is far cheaper than retrofitting per-key suppression onto the generic path.

### Alternative B: Dedicated endpoint that accepts a full JSON-array PUT

Keep the dedicated endpoint but let PUT accept the whole roster as a JSON array of slot objects in one request. Rejected because it requires a **hand-rolled JSON-array parser**: ADR-146 forbids ArduinoJson, and the firmware has no array-parsing primitive. A bespoke array scanner for nested objects with optional fields is exactly the error-prone parsing surface ADR-146 exists to avoid, and it would duplicate the per-field validation that already lives in `updateSetting()`. The marginal convenience of a one-shot array write does not justify a new parser on a memory-constrained device.

### Alternative C: Dedicated endpoint, structured GET + single-slot flat-param PUT/DELETE, bindkey write-only (chosen)

A dedicated `/api/v2/sat/ble/roster` endpoint whose GET returns structured JSON (built with the existing streaming `JsonEmit` writer, no value for the secret), whose PUT writes one slot at a time using flat query/body parameters (no array parse, reusing `updateSetting()` validation), and whose DELETE clears one slot. The bindkey is write-only across every emit path. Chosen: it gives a clean roster surface separate from the settings form, keeps the secret on the device, needs no new parser, and reuses the single existing validation point.

### Alternative D: Do nothing (roster not exposed over REST)

Leave the roster reachable only through the file-system settings editor. Rejected: TASK-935 explicitly needs the roster round-trippable over REST so a future BLE-roster UI and scripted provisioning can manage sensors without hand-editing `settings.ini`. "Do nothing" fails the stated requirement.

## Consequences

**Benefits**

- A clean, self-contained roster surface that is decoupled from the generic settings form: the indexed collection lives behind one endpoint instead of ~27 flat keys polluting `SET_META` (a roughly 96% reduction in settings-form keys for this collection, from 27 to 1).
- The bindkey **secret never leaves the device over REST**: reads return `has_bindkey` only, verified across the roster GET, the BLE discovery GET, `/api/v2/settings`, and debug logs.
- **Parser-free writes**: single-slot flat-param PUT/DELETE means no hand-rolled JSON-array parser, honouring ADR-146 with zero new parsing code.
- **Single validation point**: the PUT reuses `updateSetting()`'s field dispatch, so MAC/label/bindkey validation is not duplicated in the endpoint.
- A future BLE-roster Web UI consumes exactly one endpoint for read, write, and delete.
- After TASK-946, malformed input fails loudly (strict `idx`, 400 on bad MAC/bindkey) instead of silently writing the wrong slot or returning a false 200.

**Trade-offs**

- PUT is one-slot-at-a-time, so provisioning all 8 slots is up to 8 requests rather than one array PUT. This is the deliberate price of avoiding a JSON-array parser; for an 8-slot roster configured rarely, the request count is negligible.
- The roster now has its own endpoint contract to maintain alongside the generic settings surface (two surfaces touch the same underlying fields). The shared `updateSetting()` dispatch keeps validation single-sourced, which bounds the divergence risk.
- The bindkey remains **plaintext at rest** in `/settings.ini`. This is not a regression: it matches the existing posture of `httppasswd`, `mqttpasswd`, and `weatherapikey`, all of which sit plaintext behind the same auth-gated file-system surface. The write-only REST contract narrows the exposure (the secret is no longer readable over the API) without changing the at-rest storage, which is out of scope for this decision.

**Risks and mitigations**

- *Risk*: a later contributor adds the bindkey to a GET response (the "complete the round-trip" instinct), breaking the write-only contract. *Mitigation*: this ADR records the write-only property as a named, reusable pattern, and the verification audit lists every emit path that must keep emitting only the presence boolean.
- *Risk*: someone re-exposes the roster as flat keys on `/api/v2/settings` for convenience, reintroducing the secret-echo and clutter problems. *Mitigation*: Alternative A documents exactly why that path was rejected.
- *Risk*: the single-validation-point assumption erodes if the endpoint starts validating inline instead of delegating to `updateSetting()`. *Mitigation*: the Decision pins validation to `updateSetting()`; the endpoint's own validators (`satBleMacValid`, `satBleBindkeyValid`) are pre-checks that return 400, not a second validation authority.

## Related Decisions

- **ADR-056 (Protected admin endpoint security and secret-handling contract):** the roster endpoint's mutating-method auth gate and its write-only treatment of the bindkey are a direct application of this contract; the write-only-secret pattern extends ADR-056's secret-handling rule to a new field. Builds on it.
- **ADR-146 (No ArduinoJson; hand scanner + JsonEmit):** the structured GET is emitted with the `JsonEmit` streaming writer, and the flat-param PUT exists specifically to avoid an inbound JSON-array parse this ADR forbids. Conforms to it.
- **ADR-050 (Centralized API route dispatch):** the `/api/v2/sat/ble/roster` routes are dispatched through the central v2 route mechanism and the `processAPI` mutating-method gate. Builds on it.
- **ADR-051 (Settings/State architecture; `updateSetting`):** the PUT reuses the `updateSetting()` field dispatch so roster field validation lives in the same single place as all other settings. Depends on it.
- **ADR-154 (Encrypted MiBeacon, AES-CCM per-slot bindkey):** defines the per-slot bindkey that this endpoint stores and treats as write-only; this ADR is the REST surface for the secret ADR-154 introduced. Depends on it.

## References

- TASK-935 (BLE roster round-trippable over REST): commit `3dffd4d6`.
- TASK-946 (input hardening: strict `idx`, 400 on malformed MAC/bindkey): commit `5d6ac053`.
- Endpoint implementation: `src/OTGW-firmware/restAPI.ino`, `handleSAT()` `/api/v2/sat/ble/roster` block (the 400-on-bad-input guards at `src/OTGW-firmware/restAPI.ino:1109`, `:1116`, `:1122`, `:1138`); validators `satBleParseRosterIdx` (strict `strtol` index, `src/OTGW-firmware/restAPI.ino:1007`), `satBleMacValid` (`src/OTGW-firmware/restAPI.ino:1015`), `satBleBindkeyValid` (`src/OTGW-firmware/restAPI.ino:1024`).
- Field dispatch / validation reuse: `src/OTGW-firmware/settingStuff.ino`, `updateSetting()` `SATblemac` / `SATblelabel` / `SATblebindkey` cases.
- Secret storage: `src/OTGW-firmware/SATtypes.h:532`, `sBleBindkey[SAT_BLE_MAX_ROSTER][33]` (8 slots, 32 hex chars + NUL, 16 bytes of key) — the per-slot secret, masked in REST GET and logs like `httppasswd`.
- On-device verification: OTGW32 at `192.168.88.39`, firmware `alpha.285`. GET / PUT / DELETE round-trip verified; the bindkey value is absent from the GET output (only `has_bindkey` present).
- Write-only audit paths: roster GET (`has_bindkey`), BLE discovery GET (`has_key`), `GET /api/v2/settings` (value not emitted), debug logs (masked).
- Xiaomi MiBeacon bindkey background (per-sensor encryption key): https://github.com/custom-components/ble_monitor

This ADR has **no `Enforcement` block**. The decision is a structural REST-surface choice (a dedicated endpoint for an indexed collection) plus a semantic security property (the bindkey is write-only across all emit paths). The write-only property is not a single mechanically-expressible code boundary: it is an invariant over every present and future emit path, which a regex cannot reliably enforce (a new GET that emits the secret would not match a fixed forbidden pattern). There is no `evaluate.py` gate for it. This matches ADR-080's structural-versus-pattern split and the same call made in ADR-155 and ADR-156. Drift is caught at PR review against this ADR (the write-only audit path list) and in-session via `/adr-kit:judge`. If the maintainer wants the pre-commit judge to actively surface future changes to the roster endpoint or its secret handling for semantic review, this can be upgraded to an `llm_judge: true` Enforcement block.
