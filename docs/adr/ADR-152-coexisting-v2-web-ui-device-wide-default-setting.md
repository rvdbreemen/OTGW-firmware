# ADR-152: Coexisting v2 Web UI selected by a device-wide setting

- **Status:** Accepted
- **Accepted:** 2026-06-24 (maintainer approval)
- **Date:** 2026-06-24
- **Deciders:** Robert van den Breemen (maintainer), Claude
- **Type:** Structural (no automated CI gate; reviewed at PR per ADR-080)
- **Related:** ADR-051 (settings architecture), ADR-079/081 (per-component type headers),
  ADR-091 (design-system drift gate), ADR-139 (static-file shell serving), ADR-132 (async HTTP stack)

## Context

The firmware ships a single on-device Web UI (`index.html` + `index.js` + `components.css`).
A full multi-page redesign was prepared as a self-contained mockup (Home with three concepts,
Monitor, Settings, connectivity strip). The maintainer wants the redesign delivered **alongside**
the existing UI, switchable both ways, rather than as a hard replacement — so the proven UI keeps
working while the new one is validated in the field.

Constraints:

- ESP32-S3 LittleFS partition is 1.5 MB (combo: 1.875 MB). Two full UIs must fit. Measured: current
  `data/` ~851 KB, new bundle ~150 KB ⇒ ~500 KB margin. Coexistence fits; gzip (`webSendFile`
  already supports `.gz`) is held in reserve.
- The redesign must not fork the design system (ADR-091): tokens fold into `ds-tokens.css`.
- The new UI must show live data without duplicating the data layer.

## Decision

1. **Two bundles, one served root.** Ship the redesign as its own LittleFS assets
   (`data/v2.html`, `data/v2.css`, `data/v2.js`) next to the untouched classic assets. v2-only
   component/layout CSS lives in `v2.css`; only shared tokens go in `ds-tokens.css`.

2. **Device-wide default via a persisted setting.** Add `settings.ui.bUseV2` (ADR-051: bool in the
   existing `UISection`, key `ui_usev2`, default `false`). `sendIndex()` serves `/v2.html` when the
   flag is set, else `/index.html`. Both shells stay individually reachable at their own paths so a
   toggle can navigate explicitly regardless of the flag.

3. **Reuse the existing contract for the toggle and for data.** The toggle flips `ui_usev2` through
   the existing `POST /api/v2/settings` path (no new REST route). The v2 renderer consumes the same
   `/api/v2/*` REST endpoints and OT-log WebSocket the classic UI exposes; it does not import
   `index.js`.

The choice of a **device-wide setting** (vs per-browser `localStorage`) is deliberate: the default
should follow the device, survive reflash, and be the same for every client/household member during
field validation. Per-browser preferences (`localStorage`) remain for the in-page concept-chip
(A/B/C) and theme selections only.

## Consequences

- **Positive.** Old UI is untouched and remains the default; the redesign is opt-in and reversible.
  Minimal firmware surface: one bool field, one `if` in `sendIndex`, three asset routes — no schema
  migration, no new endpoint. New UI inherits all server-side decoding/state for free.
- **Negative / costs.** Two UIs to keep working until a cutover decision (deferred to epic end).
  LittleFS carries both bundles (~500 KB margin consumed partially). A future page that balloons the
  budget forces the gzip lever or a legacy trim.
- **Neutral.** `bUseV2` is a normal `ui_*` setting; it serializes, round-trips, and is exposed in the
  settings API like its siblings.

## Alternatives considered

- **Hard replace.** Rejected: removes the fallback during validation; higher field risk.
- **Per-browser localStorage default.** Rejected by the maintainer: the default should be device-wide
  and survive reflash, not per-browser.
- **Fresh standalone data layer for v2.** Rejected: ~doubles JS flash and risks drift between the two
  UIs; reusing the REST/WS contract is the budget-safe, consistent path.

## Verification

Verified on the live OTGW32 (192.168.88.39, 2.0.0-alpha.241) by uploading the v2 assets to LittleFS
and loading `/v2.html`: the shell renders, navigation/sub-tabs/theme work, and there are no page
console errors. Firmware (`esp32` target) builds green; the settings field round-trips through the
existing serializer.


## Amendment 2026-06-24 (TASK-922): per-user choice overrides the device default

Field feedback: the UI choice should be remembered **per browser/user**, with the
device default staying classic. So a per-user override is layered on top of the
device-wide `bUseV2`:

- `localStorage['otgw-ui']` = `'v2'` | `'classic'` (unset = follow device default).
- `index.html` redirects to `/v2.html` when `otgw-ui==='v2'`; `v2.html` redirects to
  `/index.html` when `otgw-ui==='classic'`. Redirects target **explicit files**
  (`/v2.html`, `/index.html`), never `/`, so they never loop with the `sendIndex`
  device default regardless of `bUseV2`.
- The in-page toggle buttons set `localStorage` only (they no longer POST the
  device-wide `ui_usev2`). The device-wide `bUseV2` remains as the admin default
  (Settings), default false (classic).

Net precedence: per-user `localStorage` > device `bUseV2` > classic. Because the
redirects are pure client-side, the per-user switch works even before the firmware
carrying `bUseV2` is flashed. Verified on 192.168.88.39: v2-choice bounces
`/index.html`->`/v2.html`, classic-choice bounces `/v2.html`->`/index.html`, default unset stays classic.


## Amendment 2026-06-24 (TASK-923): preference persisted in settings.ini (supersedes TASK-922 per-user)

Maintainer decision: the UI preference must persist **device-side in `settings.ini`**,
not per-browser. This supersedes the TASK-922 localStorage approach:

- The toggle buttons POST `ui_usev2` to `/api/v2/settings`, which serializes to
  `settings.ui.bUseV2` in `/settings.ini` (the existing TASK-910 plumbing), then reload.
- The per-user `localStorage['otgw-ui']` redirects in `index.html`/`v2.html` are removed.
- Serving is driven solely by `sendIndex()` reading `settings.ui.bUseV2` (default false =
  classic). The choice is therefore **device-wide** (shared across all browsers/clients)
  and survives reboot/reflash because it lives in `settings.ini` on LittleFS.

Consequence: unlike the localStorage layer, this requires the firmware carrying the
`bUseV2`/`sendIndex` switch (TASK-910) to be flashed before the toggle takes effect; on
older firmware the `ui_usev2` POST is ignored and the UI stays classic.
