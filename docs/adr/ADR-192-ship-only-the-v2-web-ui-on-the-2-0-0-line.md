---
id: "ADR-192"
title: "Ship only the v2 web UI on the 2.0.0 line"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-152"
superseded_by: null
related:
  - "ADR-152"
  - "ADR-172"
  - "ADR-173"
  - "ADR-190"
  - "ADR-191"
topics:
  - "web-ui"
  - "v2"
  - "classic-ui"
  - "littlefs"
aliases:
  - "v2 only"
  - "remove classic ui"
  - "retire classic web ui"
components:
  - "v2 web UI as the only device web UI"
symbols:
  - "sendIndex"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-192 Ship only the v2 web UI on the 2.0.0 line

## Status

Accepted, 2026-10-15.

**Decision Maker:** User: Robert van den Breemen (maintainer). On 2026-10-15
he chose "v2 interface only" with the safeboot OTA layout. He approved the 12
dropped features, asked for them to be documented in detail, and chose to
remove the classic UI before every ported view was proven on PIC hardware.

## Status History

```yaml
status_history:
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-191
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-190
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-152
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: The 2.0.0 line ships only the v2 web UI
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-172
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-173
    changed_via: adr-kit lifecycle
```

## Context

Since ADR-152 the 2.0.0 firmware ships two web UIs (Web User Interfaces) on
LittleFS (Little File System): the classic bundle (`index.html`, `index.js`,
`graph.js`, `sat.js` and helpers) and the v2 bundle (`v2.html`, `v2.js`). A
persisted setting, `settings.ui.bUseV2`, picks which one the root path serves.

WiFi firmware updates on the 4MB boards need the safeboot layout of ADR-191.
That layout leaves 768K for LittleFS.

- Both UIs, minified (ADR-190), need 1104 KiB, which does not fit.
- v2 alone needs about 668 KiB plus the 96 KiB runtime reserve that build.py
  enforces (TASK-1223.6).

A gap audit (`docs/research/2026-10-15-classic-vs-v2-gap-audit.md`) compared
60 classic features with v2 and found 31 gaps. Of those, 19 are ported to v2
and 12 are dropped. The core ports were done first (TASK-1223.14 to .18); the
others are follow-ups (TASK-1226 to TASK-1232).

## Decision

The 2.0.0 line ships the v2 web UI as its only device web UI. The classic
bundle, the `bUseV2` switch and the classic-only `ui_*` preference keys are
removed, and the root path always serves `v2.html`.

- `sendIndex()` serves `/v2.html`. When `v2.html` is missing it falls back to
  FSexplorer, which carries its own stylesheet (TASK-1223.13). This lets a
  broken UI be repaired over WiFi.
- Settings files that still hold `ui_usev2` or the removed `ui_*` keys load
  normally; the unknown keys are ignored.
- The REST endpoints and WebSocket messages that only the classic UI used stay
  in place. They are documented API (`docs/api/openapi.yaml`), and Home
  Assistant or scripts may call them.
- The 12 dropped features, and how each was built, are recorded in
  `docs/history/2026-10-15-retired-classic-web-ui-features.md`. Each source
  file can be recovered from commit `3aa1ef927`.
- `ds-tokens.css` stays in the source tree, because it is the input of
  `v2-bundle.css`, but it is no longer shipped in the image.

## Decision Contract

### Must

- Serve `v2.html` at `/`, `/index` and `/index.html`. Serve FSexplorer when
  `v2.html` is missing.
- Keep FSexplorer independent of the v2 assets (its own `fsexplorer.css`).
- Ignore removed settings keys on read, without failing the settings load.

### Must Not

- Reintroduce a second web UI bundle in the 4MB filesystem image.
- Remove a documented REST endpoint only because no UI calls it any more.

### Exceptions

- None.

### Verification

- The build log of the v2-only image shows the LittleFS budget gate passing
  (`build.py` `enforce_littlefs_budget`).
- Bench: `/` serves v2, and an old `settings.ini` loads with 0 lost keys
  (TASK-1223.12 acceptance criteria).

## Alternatives Considered

- **Keep both UIs and gzip the browser assets.** This fits, but the maintainer
  forbids build-time gzip (`src/OTGW-firmware/FSexplorer.ino:83`, reaffirmed
  2026-10-14).
- **Keep both UIs and stay on USB-only firmware updates (ADR-166).** This
  gives up WiFi firmware updates on every 4MB board.
- **Drop the classic UI only after every port is proven on PIC hardware.**
  Safer for PIC users, but OTA would wait for the hardware. The maintainer
  chose to remove it now; the code is recoverable from git.
- **Keep `bUseV2` as a no-op.** API compatibility for scripts that set it,
  but it keeps dead weight. Rejected by the maintainer.

## Consequences

**Positive:**

- The filesystem fits the 768K safeboot layout, which makes WiFi firmware
  updates possible on 4MB boards (ADR-191).
- One UI to build, test and document; about 450 KiB less to ship.

**Negative:**

- **Users lose the 12 dropped features.** WiFi scan and the OpenWeatherMap
  key wizard are the most visible.
  Mitigation: the history document records how each was built. The release
  notes list them.
- **Some ported views are not yet proven on a PIC board** (PIC settings, PS=1
  polling) **or with a 1-Wire sensor** (Dallas labels).
  Mitigation: the bench check stays open in TASK-1223.14, .15 and .17. A
  regression on a PIC board has no fallback UI until it is fixed.
- **Several firmware paths now have no UI consumer**, for example the flash
  status endpoint and the WebSocket flash progress. They stay as API.

## Related Decisions

- **Supersedes ADR-152 (coexisting v2 web UI selected by a device-wide setting).**
- **ADR-191 (safeboot OTA on 4MB boards)**: the reason for this decision.
- **ADR-190 (minified LittleFS staging)**: supplies the image size.
- **ADR-184 (2-in-flight API queue in the classic bundle)**: the classic queue
  goes with the classic bundle; v2 carries the same queue (TASK-1210).

## References

- TASK-1223.11 (gap audit), TASK-1223.12 (removal), TASK-1223.13 to .18 (ports), TASK-1226 to TASK-1233.
- `docs/research/2026-10-15-classic-vs-v2-gap-audit.md`, `docs/history/2026-10-15-retired-classic-web-ui-features.md`.
- `src/OTGW-firmware/FSexplorer.ino` (`sendIndex`, root routes).
