# Design: coexisting v2 Web UI (new redesign, user-switchable)

- **Date:** 2026-06-24
- **Branch:** `dev` (2.0.0, ESP32-S3-only async + FreeRTOS)
- **Source of the design:** claude.ai design project "Mockups into design system"
  (`eb26ec05-912a-40f0-97cc-d63e770218e0`), files `docs/design/boiler-dashboard-concepts.html`
  (full multi-page mockup) and `OTGW Patterns and Tokens.dc.html` (token reconciliation).
- **Status:** approved approach, pending spec review.

## 1. Goal

Implement the full multi-page Web UI redesign from the mockup, shipped **alongside**
the existing UI. The two UIs coexist on the device; a header toggle switches between
them. The new UI shows **live data** by reusing the firmware's existing REST/WebSocket
contract, not by re-implementing a data layer.

Non-goals (this epic): removing the old UI, HTTPS/WSS, ArduinoJson, new fonts,
runtime hardware detection.

## 2. What the mockup contains

A single self-contained file with three top-level pages and a global connectivity strip:

- **Home** with three *mutually-exclusive* concepts, switchable via concept chips:
  - **A — System view:** animated boiler/heat-pump schematic. Flame or lightning-bolt
    burner whose height = modulation; red flow pipe + temp chip, blue return pipe + temp
    chip, ΔT chip, DHW branch + faucet, pump, radiator, pressure mini-gauge, room &
    outside temperature.
  - **B — At a glance:** mobile-first hero dial + stat cards (Nest/Tado style), zone bar,
    modulation bar.
  - **C — Mission control:** dark data-dense live strip chart (flow/return/setpoint/mod)
    + OT-frame ticker + metric cells.
- **Monitor** with sub-tabs: Log (console), Stats (per-msgid table), Support (msgid
  matrix + detail panel), Graph, Connection (node map).
- **Settings:** left rail of categories + masonry of grouped cards, dirty-field
  tracking, sticky save/discard bar, toast, search, BLE sensor roster.
- **Connectivity strip:** always-visible pills (one per link: WiFi/Eth, MQTT, OT bus,
  NTP, ...) with a click-through detail map.

The mockup is 100% simulated (random-walk JS). Visuals/CSS/SVG are done; the work is
**re-wiring it to live data** and **folding its tokens into the design system**.

## 3. Constraints (measured, not assumed)

- **LittleFS budget — the gate.** `dev` has no ESP8266 target; all three builds
  (esp32, esp32-classic, esp32-combo) are ESP32-S3 sharing the **same 1.5 MB**
  (`1,572,864 B`) LittleFS partition (`fs_size` in `build.py`, `partitions_otgw_esp32.csv`).
  Current `data/` is ~851 KB raw (web assets 653 KB + fonts 68 KB + PIC blobs 113 KB).
  Headroom ~720 KB. New bundle estimate ~150–200 KB ⇒ projected ~1.05 MB, **~500 KB
  margin**. Coexistence fits without gzip. `webSendFile()` already supports gzip
  (`.gz` sibling + `Content-Encoding`) and is held in reserve if a future page balloons.
- **Design-system drift gate (ADR-091).** New tokens MUST fold into `ds-tokens.css`;
  bespoke `zone-*`/`tile-*` map onto the official status palette per the Patterns
  reconciliation doc. No parallel token set.
- **Firmware coding rules.** Any firmware-side change obeys PROGMEM (`F()`/`PSTR()`),
  no ArduinoJson (manual `snprintf_P`/`JsonEmit`), no `String` in hot paths, ADR-051
  settings shape, ESP platform-abstraction boundary.
- **Versioning.** Every commit touching `src/OTGW-firmware/**` (incl. `data/`) bumps
  `_VERSION_PRERELEASE` via `bin/bump-prerelease.sh` (pre-commit hook enforces).

## 4. Architecture

### 4.1 Coexistence + device-wide toggle (decided: firmware setting)

- New UI is its **own bundle on LittleFS**: `data/v2.html` + `data/v2.js`, served next
  to the untouched `index.html`/`index.js`. New tokens/components reuse the existing
  `ds-tokens.css` + `components.css` (extended, not forked).
- **New persisted setting** `settings.ui.bUseV2` (ADR-051: new two-level `ui` sub-section,
  Hungarian `b` prefix, default `false`). This is the **device-wide default** — applies
  to every browser/client.
- **Server-side default switch.** `sendIndex()` (`FSexplorer.ino`) reads
  `settings.ui.bUseV2`; serves `/v2.html` when true, else `/index.html`. New routes
  `server.on("/v2.html", ...)` and `server.on("/v2.js", ...)` registered exactly like
  the existing versioned assets (`serveVersionedAsset`). `/index.html` and `/v2.html`
  remain individually reachable regardless of the flag, so the toggle can always
  navigate explicitly.
- **Toggle control.** A header button in each UI POSTs the new flag to the REST API
  (extend `handleSettings` / a small `v2/ui` sub-route under an existing handler — pick
  the smallest change at implementation time), then reloads `/`. Because the default is
  a persisted setting, the choice survives reboot and is shared across clients.
- A per-browser `localStorage` override is explicitly **out of scope** (user chose
  device-wide). The concept-chip choice (Home A/B/C) and theme remain browser-side
  `localStorage`, matching the mockup.

### 4.2 Data via the existing REST/WS contract

`v2.js` is a fresh, focused renderer that consumes the **same** server contract the old
UI already exposes — it does NOT import `index.js`:

- **OT-log WebSocket** (the AsyncWebSocket live-log endpoint): decoded OT frames →
  feeds Home schematic/dial/strip-chart, Monitor log + stats + support matrix + ticker.
- **`/api/v2/*` REST:** `device/info` (snapshot), `settings` (GET/POST), `sensors`,
  `network` (link/WiFi), `health`, `otgw` (command + status), `sat` incl. `sat/ble/*`
  (roster discovery/select/label/forget/rescan), `simulate`.
- Reuse `graph.js`' data path for Monitor/Graph rather than re-deriving it.

**Field-mapping pass (per page, before that page's task starts).** Map every live
element to a concrete endpoint/WS field. Any value with no current source becomes its
own **firmware task** (new/extended endpoint, subject to §3 rules) — a different risk
class than client JS, tracked separately so it can't silently block a UI task.

### 4.3 Tokens / design system

Fold into `ds-tokens.css` per the Patterns reconciliation doc:
- Genuinely new tokens to adopt: `--hot`, `--cold`, `--dhw`, `--gauge-track`.
- Reconciled aliases: the mockup's `zone-ok/warn/alert`, `tile-on/off/fault` map onto the
  existing `--status-ok/--status-warn/--status-error/...` palette (aliases, not new colors).
- Light + dark variants both defined, matching the existing theme model + `theme-toggle.js`.

## 5. Decomposition (one epic, phased)

Epic: **feat(webui): coexisting v2 Web UI redesign (switchable, live data)**.

- **P0 — Foundation**
  - T-a: Token + pattern reconciliation into `ds-tokens.css` / `components.css`
    (no layout change to live pages; drift gate green).
  - T-b: `settings.ui.bUseV2` (ADR-051) + `sendIndex` default switch + `/v2.html`,`/v2.js`
    routes + REST flag write + toggle button injected into BOTH UIs. ADR for the
    coexistence + settings decision. Ships an empty/skeleton `v2.html` that loads,
    themes, and toggles back. Proves the model end-to-end.
- **P1 — Home (showpiece)**
  - T-c: Concept A (system schematic) wired to OT-log WS + `device/info`.
  - T-d: Concept B (at-a-glance dial + stat cards) wired.
  - T-e: Concept C (mission control strip chart + ticker) wired.
  - Concept chips switch among A/B/C (browser `localStorage`).
- **P2 — Monitor**
  - T-f: Log console (reuse WS log pipeline + filters/search/timestamps).
  - T-g: Stats table + Support msgid matrix/detail.
  - T-h: Graph (reuse `graph.js`) + Connection node map.
- **P3 — Settings**
  - T-i: Rail + grouped cards + dirty tracking + save/discard + search, wired to
    `v2/settings`; BLE roster wired to `v2/sat/ble/*`.
- **P4 — Connectivity**
  - T-j: Always-visible connectivity strip + detail map (both pages), wired to
    `network`/`health`/`otgw`/MQTT status.
- **P5 — Cutover**
  - T-k: a11y pass (aria-live, focus, tap targets per mockup's 44px minimums),
    field-validation gates, decide whether to flip the device default to v2.

**Per-task DoD:** prerelease bump (`bin/bump-prerelease.sh`); esp32 + esp32-classic
builds green; `python evaluate.py --quick` green (incl. ADR-091 drift gate);
field-validation ACs flagged where hardware is required.

## 6. Risks

- **Field gaps (medium).** Some Support/Connectivity values are computed client-side in
  the mockup; a few may need new firmware endpoints. Mitigated by the per-page mapping
  pass (§4.2) turning each gap into a tracked firmware task.
- **Flash creep (low).** ~500 KB margin; gzip lever in reserve; `du` check in each P*
  task's verification.
- **Drift gate (low).** Addressed by §4.3 (tokens fold in, no fork).
- **Two UIs to keep working (low/ongoing).** Old UI is untouched by design; v2 is
  additive. Default stays old until P5.

## 7. Execution mode

Spec + implementation plan written and approved once; then **autonomous drain** of all
phases (implement → build/eval → adversarial review → Proposed ADR where needed →
commit/push → report), advancing task-to-task without per-task prompting. Phase
boundaries are reporting checkpoints, not approval gates.
