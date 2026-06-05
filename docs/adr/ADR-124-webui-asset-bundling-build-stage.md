---
id: ADR-124
title: WebUI Asset Bundling at Build Time — 8 Static Assets → 3 Bundles via a Staged LittleFS Image
status: Proposed
date: 2026-06-05
tags: [webui, build-tooling, littlefs, http, performance, cold-load, esp32, esp8266]
supersedes: []
superseded_by: []
related: [ADR-123, ADR-080, ADR-004]
deciders: [Robert van den Breemen]
---

# ADR-124: WebUI Asset Bundling at Build Time — 8 Static Assets → 3 Bundles via a Staged LittleFS Image

## Status

Proposed — drafted 2026-06-05, direction chosen by the maintainer (Robert) the same day.

**Guideline-level** per ADR-080: there is no `evaluate.py` runtime gate. The
build-time invariant is instead enforced by `build.py::prepare_bundle_staging()`,
which **fails the build** (`sys.exit(1)`) if any bundle source is missing or if an
`index.html` asset-tag rewrite pattern no longer matches. That fail-closed build
step is the enforcement; a duplicated runtime check would add no coverage.

## Context

### Problem

The 2.0.0 OTGW32 web UI loads **8 local static assets** plus one external CDN
script. Measured on an OTGW32 (alpha.162) over WiFi (RSSI −64…−74), with the
device's synchronous single-connection `WebServer` (Arduino-ESP32 core
`WebServer.cpp`: one accepted `_currentClient` served to completion before the
next is accepted):

- `index.html` (now batched, ADR-pending TASK-823) and the two CSS files
  complete in ~0.8–2 s.
- The **six parallel JS requests never complete** (`index.js` is 346 KB, `sat.js`
  58 KB, `graph.js` 47 KB, plus three tiny defer scripts). Reproduced across
  multiple captures: not a reboot (device stays up, serves other REST in
  10–100 ms), not heap (HEALTHY), not Nagle (the ESP32 core already sets
  `TCP_NODELAY` on every accepted client — `WebServer.cpp` + `NetworkServer.cpp`
  `setsockopt`). It is **connection/accept starvation**: the serial server cannot
  service six concurrent new connections on a variable link, and the browser-side
  requests stall.

The structural answer to single-connection serving is the async networking model
in [[ADR-123]] (event-driven server, no per-request loop blocking). That is a
large rewrite. This ADR is the **tactical, build-time mitigation** that ships now
and remains valuable even after ADR-123 lands (fewer requests is good regardless
of server model).

### Forces

- Fewer parallel requests directly reduces accept-starvation on the serial server.
- Concat does **not** shrink bytes (the 530 KB total is unchanged); gzip would,
  but gzip is disabled (TASK-433 double-encoding) and is an orthogonal follow-up.
  Bundling is request-count only.
- `index.js`/`graph.js`/`sat.js` are blocking scripts (run during parse, in order);
  `echarts-theme.js`/`theme-toggle.js`/`sat-slider.js` are `defer` because they
  hook DOM nodes — they MUST stay deferred. So the JS cannot collapse to one
  bundle; it must be two (blocking + defer).
- Source files must stay individually editable, and `webui_launcher.py` local
  preview must keep working without a build step.

## Decision

Bundle the 8 static assets into **3** at build time, from a **staged copy** of
`data/`, leaving `data/` untouched:

| Bundle | Sources (concat order) | Load |
|---|---|---|
| `styles.css` | `ds-tokens.css`, `components.css` | `<link>` (tokens before use) |
| `app.js` | `index.js`, `graph.js`, `sat.js` | blocking `<script>` |
| `deferred.js` | `echarts-theme.js`, `theme-toggle.js`, `sat-slider.js` | `<script defer>` |

The external `echarts` CDN script is unchanged (integrity-pinned, not bundlable).

`build.py::prepare_bundle_staging()` (called from `build_filesystem`):

1. copies `data/` → `build/fs-staging`,
2. concatenates each bundle's sources (newline-joined so a missing trailing
   newline cannot merge two files), deletes the sources from the staging dir,
3. rewrites the staged `index.html`'s 8 asset tags into the 3 bundle tags,
4. `mklittlefs` builds the image from the staging dir, not `data/`.

`data/` keeps all 8 sources and the 8-tag `index.html`, so source editing and
`webui_launcher.py` local preview are unaffected.

`FSexplorer.ino` is updated to match the served (staged) `index.html`: the
`sendIndex` `?v=<fsHash>` injection token list and the `serveVersionedAsset`
handlers reference the **3 bundle names** (`styles.css`, `app.js`, `deferred.js`).

`BUNDLE_MANIFEST` / `BUNDLE_INDEX_REWRITES` in `build.py` and the token
list/handlers in `FSexplorer.ino` are the three coupled points; all carry a
TASK-825 comment pointing at each other.

## Consequences

**Positive**

- Cold-load local requests drop 8 → 3, removing most of the accept-starvation
  surface on the serial server.
- `data/` and local dev are untouched; only the image differs.
- Composes with the TASK-822 versioned-cache (`?v=`) and TASK-823 index.html
  batching already shipped; orthogonal to a future gzip re-enable (size) and to
  ADR-123 (concurrency).

**Negative / risks**

- Three coupled definitions (manifest, index.html rewrite, FSexplorer tokens)
  must stay in sync; drift fails the build loudly (manifest/rewrite) or yields a
  missing `?v=`/404 (FSexplorer) caught by the field-validation AC.
- Concat changes nothing about total bytes; `index.js` at 346 KB still dominates
  transfer time — gzip is the complementary lever, tracked separately.
- A stale browser holding a pre-bundle `index.html` would request `/index.js`
  etc., now 404. Mitigated: `index.html` is `no-cache`+ETag, so it revalidates to
  the bundled version on the next load after a reflash.
- Frontend behaviour (theme toggle, sliders, ECharts) depends on the concat order
  preserving execution semantics; this requires on-device browser validation, not
  just a clean compile.

## Related

- [[ADR-123]] — async concurrency model (the structural fix this mitigates around).
- [[ADR-080]] — binding-vs-guideline rule (this is guideline-level; build-step gate).
- [[ADR-004]] — no `String` in hot paths (bundling is build-time, unaffected).
- TASK-822 (versioned `?v=` caching), TASK-823 (index.html chunk batching),
  TASK-825 (this implementation), TASK-433 (gzip disabled — the size follow-up).
