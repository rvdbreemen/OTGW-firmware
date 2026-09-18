---
id: "ADR-096"
title: "Serve one REST API version at a time and remove superseded ones"
status: "Proposed"
date: "2026-09-18"
binding: false
gate: null
documents_shipped: true
verified_in:
  - "src/OTGW-firmware/restAPI.ino"
supersedes:
  - "ADR-019"
superseded_by: null
format: "madr"
topics:
  - "rest-api-versioning"
  - "api-lifecycle"
  - "breaking-changes"
aliases:
  - "api version removal"
  - "410 Gone for old API versions"
  - "single API version"
components:
  - "restAPI"
symbols:
  - "kV2Routes"
  - "sendApiError"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-096 Serve one REST API version at a time and remove superseded ones

## Status

Proposed, 2026-09-18.

## Status History

```yaml
status_history:
  - date: 2026-09-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: ADR-019 promised indefinite version coexistence; the code removed v0 and v1 in PR #459 and nothing recorded the reversal
    changed_via: adr-kit
```

## Context and Problem Statement

ADR-019 decided to "adopt URL path-based API (Application Programming Interface)
versioning with indefinite version support (no deprecation)", and spelled that
out as "all versions remain active indefinitely" and "no forced migration: old
clients continue working".

The firmware has not done that since v1.3.5. `src/OTGW-firmware/restAPI.ino:1068-1069`
matches `v0` and `v1` in the request path and answers
`sendApiError(410, F("API version removed; use /api/v2"))`. Only `/api/v2/` is
served.

The removal was deliberate and its cause is recorded in the history rather than
in the decision log. Commit `783f8242f` (PR #459) is titled "feat: Webhook support
+ remove ArduinoJson (streaming JSON I/O) + map-based JSON APIs only (v0 and v1
removed)". It dropped the ArduinoJson library in favour of hand-written streaming
JSON (JavaScript Object Notation) output, which ADR-042 records. v0 and v1 emitted
an array shape; v2 emits a map shape. Once every response is assembled by hand with
`snprintf_P` and `sendJsonMapEntry`, each additional shape is code that a person has
to write and keep correct, on a device with roughly 40 KB of usable RAM (Random
Access Memory).

So the problem is not the behaviour. The problem is that ADR-019 is still
`Accepted` with `superseded_by: null`, which means the decision log states a
commitment the firmware contradicts, and has done for several releases without
anything flagging it. `CLAUDE.md` repeated ADR-019's version list until it was
corrected on 2026-09-18, so an agent reading the project instructions would
reasonably have added an endpoint to a version that answers nothing.

## Decision Drivers

* The decision log must not contradict shipped behaviour; a stale `Accepted` ADR is
  worse than no ADR because it is trusted.
* Hand-written serialization (ADR-042) makes each retained API shape a recurring
  maintenance cost rather than a one-time one.
* Flash and RAM on the ESP8266 are the binding constraint on this branch.
* A client calling a removed version deserves an answer that says what happened.

## Considered Options

* Option A: serve exactly one version, remove superseded ones, answer 410 Gone.
* Option B: keep ADR-019 as written and restore v0 and v1.
* Option C: deprecate but keep serving old versions read-only.

## Decision Outcome

Chosen option: **Option A**, because it is what the firmware already does, the
maintainer confirms it was intended, and the alternative that preserves ADR-019's
promise would require re-adding two hand-written serialization shapes that ADR-042
deliberately removed the machinery for.

Concretely: `/api/v2/` is the current version. When a version is superseded it is
removed rather than frozen, and its path answers `410 Gone` with a message naming
the current version. New endpoints are added only to the v2 dispatch table
`kV2Routes[]` in `src/OTGW-firmware/restAPI.ino`, per ADR-050.

This is a breaking-change policy, stated as such. It trades the compatibility
promise ADR-019 made for a maintenance cost the project is not willing to carry on
this hardware.

### Confirmation

Two checks, both cheap:

* `curl -s -o /dev/null -w '%{http_code}' http://<gateway>/api/v1/otgw/data` returns
  `410`, and the body names `/api/v2`.
* The `Enforcement` block below fails any commit that adds an `/api/v0/` or
  `/api/v1/` path literal under `src/`, which is the regression that went unnoticed
  in the other direction for several releases.

## Decision Contract

### Must

* Serve exactly one REST API version at a time; `/api/v2/` is current.
* Answer a request for a removed version with `410` and a message naming the
  current version, via `sendApiError`.
* Register new endpoints in `kV2Routes[]` only.
* When a future version supersedes v2, write a superseding ADR before removing v2.

### Must Not

* Reintroduce `/api/v0/` or `/api/v1/` route handling.
* Answer a removed version with `404`, which is indistinguishable from a typo in
  the path and tells the client nothing.

### Exceptions

* None.

## Consequences

### Positive

* The decision log matches the firmware. An agent or contributor reading either one
  reaches the same conclusion.
* One serialization shape to maintain by hand instead of three, which is the
  property ADR-042 was adopted for.
* A client on a removed version gets a diagnosable answer rather than a silent 404.

### Negative

* Clients written against v1 break, by design. This is a real loss: ADR-019 promised
  they would keep working, and anyone who relied on that promise was entitled to.
  Mitigation is limited to the 410 body naming the replacement, plus the release
  notes for v1.3.5. There is no shim and none is planned.
* The project gives up the option of a gradual migration window for future versions.
  Mitigation: the Must clause above requires a superseding ADR before any future
  removal, so the next break is a deliberate decision rather than a side effect of
  another change.
* An integration that is only exercised occasionally may still be on v1 and nobody
  would know. Mitigation: none available from the firmware side; the 410 is the
  signal and it only fires when the client calls.

## Pros and Cons of the Options

### Option A: serve one version, remove superseded ones

* Good, because it matches shipped behaviour, so no code changes and no risk.
* Good, because it keeps exactly one hand-written JSON shape, consistent with ADR-042.
* Good, because 410 is the specific status for a resource that is intentionally gone.
* Bad, because it breaks old clients with no migration window.

### Option B: keep ADR-019 and restore v0 and v1

* Good, because it honours the compatibility promise that was actually made.
* Bad, because it requires re-adding two array-shaped serializers by hand, which is
  the work ADR-042 removed the library for.
* Bad, because it costs flash and RAM on a 40 KB-RAM device for paths with no known
  users.
* Bad, because it would be a behaviour regression against shipped releases from
  v1.3.5 onward.

### Option C: deprecate but keep serving old versions read-only

* Good, because old clients keep working while being warned.
* Bad, because read-only still means carrying both serializers, so it pays most of
  Option B's cost for part of its benefit.
* Bad, because it needs a deprecation signal the current API has no place for, which
  is new surface for a path being retired.

## Open Questions

* None.

## Related Decisions

* **ADR-019 (REST API Versioning Strategy)**: superseded by this ADR. Its decision to
  keep all versions active indefinitely is the commitment being withdrawn.
* **ADR-042 (Streaming JSON I/O, No ArduinoJson)**: the reason one shape is cheap and
  three are not. The removal of v0 and v1 shipped in the same commit as that change.
* **ADR-050 (Centralized API route dispatch)**: defines `kV2Routes[]`, the single
  table this decision points every new endpoint at.
* **ADR-035 (RESTful API compliance strategy)**: the broader API conventions this
  version policy sits inside.

## References

* `src/OTGW-firmware/restAPI.ino:1068-1069` — the v0/v1 branch returning 410.
* Commit `783f8242f`, PR #459 — "map-based JSON APIs only (v0 and v1 removed)".
  First released in tag `v1.3.5`.
* `docs/adr/ADR-019-rest-api-versioning-strategy.md` — the superseded decision.
* `CLAUDE.md`, REST API Versioning section — corrected on 2026-09-18 in commit
  `f045a97d`, which is what surfaced this gap.
* RFC 9110 section 15.5.11, `410 Gone`:
  <https://www.rfc-editor.org/rfc/rfc9110#name-410-gone>

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "[\"'`]/api/v[01]/",
      "path_glob": "src/**/*.{ino,cpp,h,js}",
      "message": "ADR-096: only /api/v2/ is served. v0 and v1 answer 410 (restAPI.ino:1068-1069). Add the endpoint to kV2Routes[] instead."
    }
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false,
  "llm_judge_reason": "The decision is fully expressible as a path literal check, so a model call would add cost without adding judgement. The project also disables the per-commit LLM pass globally (docs/adr/.adr-kit.json judge.llm_enabled false)."
}
```
