# Coexisting v2 Web UI Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement
> this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the full mockup redesign (Home A/B/C, Monitor, Settings, connectivity strip)
as a second on-device UI that coexists with the existing UI, selectable via a device-wide
firmware setting, showing live data over the existing REST/WS contract.

**Architecture:** New bundle `data/v2.html` + `data/v2.css` + `data/v2.js` lives next to the
untouched `index.html`/`index.js`. `settings.ui.bUseV2` picks which the root path serves. The
new renderer consumes the same `/api/v2/*` REST + OT-log WebSocket; tokens fold into shared
`ds-tokens.css`, v2-only component CSS stays in `v2.css` (old `components.css` untouched).

**Tech Stack:** Arduino C/C++ firmware (ESP32-S3, async, FreeRTOS), vanilla HTML/CSS/JS
(no libraries, no ArduinoJson, no String in hot paths), LittleFS asset image.

## Global Constraints

- No ArduinoJson; JSON out via `snprintf_P`/JsonEmit, in via hand scanner. (ADR-146)
- PROGMEM: all string literals `F()`/`PSTR()`; `snprintf_P`, `strcasecmp_P`.
- No `String` in hot paths; firmware additions use `char[]`.
- Settings shape: ADR-051 (sub-section `settings.ui`, Hungarian prefix `b`).
- ESP platform abstraction: no raw `#ifdef ESP8266/ESP32` outside allowlist.
- Design-system drift gate ADR-091: new tokens fold into `ds-tokens.css`; no parallel set.
- Versioning: every commit touching `src/OTGW-firmware/**` (incl. `data/`) bumps
  `_VERSION_PRERELEASE` via `bin/bump-prerelease.sh` (pre-commit hook enforces).
- Build green for `esp32` AND `esp32-classic`; `python evaluate.py --quick` green.
- Branch `dev`; push allowed to `origin/dev` once build+eval green (docs-only skips gates).
- Verification cycle for this project is **build + evaluate + browser-visual**, not unit tests
  (no JS test harness exists). "Test" steps below mean: build, run evaluate, and load the page
  in a browser pointed at a live device or the static file to confirm render + data binding.

## File Structure

| File | Responsibility | New/Mod |
|---|---|---|
| `src/OTGW-firmware/UItypes.h` | add `bUseV2` to `UISection` | Mod |
| `src/OTGW-firmware/settingStuff.ino` | save + load `ui_usev2` | Mod |
| `src/OTGW-firmware/restAPI.ino` | emit `ui_usev2`; allow-key list | Mod |
| `src/OTGW-firmware/FSexplorer.ino` | `sendIndex` default switch; `/v2.*` routes | Mod |
| `src/OTGW-firmware/data/v2.html` | new UI shell (header, nav, pages, strip) | New |
| `src/OTGW-firmware/data/v2.css` | v2-only component/layout CSS (from mockup) | New |
| `src/OTGW-firmware/data/v2.js` | renderer + REST/WS data client + toggle | New |
| `src/OTGW-firmware/data/ds-tokens.css` | fold in `--hot/--cold/--dhw/--gauge-track` + aliases | Mod |
| `docs/adr/ADR-1XX-coexisting-v2-webui.md` | record coexistence + setting decision | New |

Source mockup (read-only, in the design project, copy into repo at P0b for reference):
`docs/design/boiler-dashboard-concepts.html` (markup/CSS/sim-JS), `OTGW Patterns and Tokens.dc.html` (tokens).

---

## P0 — Foundation

### Task P0a: Token reconciliation into ds-tokens.css

**Files:** Modify `src/OTGW-firmware/data/ds-tokens.css`.

**Interfaces — Produces:** CSS custom properties available to both UIs:
`--hot`, `--cold`, `--dhw`, `--gauge-track` (light + dark), and confirmation that
`--zone-ok/--zone-warn/--zone-alert`, `--tile-on/--tile-off/--tile-fault` resolve to the
existing status palette (aliases, not new colors).

- [ ] Step 1: Read the reconciliation section of `OTGW Patterns and Tokens.dc.html` (design project) to get exact alias mapping.
- [ ] Step 2: Add the four genuinely-new tokens to both `html[data-theme="light"]/:root` and `html[data-theme="dark"]` blocks in `ds-tokens.css`, values from the mockup (`--hot:#ff5a3c/#ff6a4d`, `--cold:#2f8fe8/#4da3ff`, `--dhw:#ff9b3c`, `--gauge-track:#d7eef6/#3a3a3d`). Add `--zone-*`/`--tile-*` as `var(--status-*)` aliases.
- [ ] Step 3: `python evaluate.py --quick` — expect green incl. `check_design_system_drift`.
- [ ] Step 4: Bump + commit (`bin/bump-prerelease.sh`; `git add` ds-tokens.css + version files + task; message `feat(webui): add v2 dashboard tokens to design system (TASK-9xx)`).

### Task P0b: Setting + server switch + bundle skeleton + toggle

**Files:** Modify `UItypes.h`, `settingStuff.ino`, `restAPI.ino`, `FSexplorer.ino`;
Create `data/v2.html`, `data/v2.css`, `data/v2.js`.

**Interfaces:**
- Consumes: `settings.ui` (existing sub-section), `serveVersionedAsset()`, `writeJsonBoolKV`,
  `EVALBOOLEAN`, `addBool`, the settings POST path.
- Produces: persisted bool `settings.ui.bUseV2` (key `ui_usev2`, default false); root path
  serves `v2.html` when true; `/v2.html`,`/v2.css`,`/v2.js` reachable; a working toggle in
  both UIs that POSTs `ui_usev2` and reloads.

- [ ] Step 1 — UItypes.h: add `bool bUseV2 = false;` to `UISection` (after `bAutoExport`).
- [ ] Step 2 — settingStuff.ino: save — after the `ui_autoexport` line (~271) add
  `writeJsonBoolKV(file, F("ui_usev2"), settings.ui.bUseV2, true);`
- [ ] Step 3 — settingStuff.ino: load — after the `ui_autoexport` parse (~800) add
  `else if (strcasecmp_P(field, PSTR("ui_usev2"))==0) settings.ui.bUseV2 = EVALBOOLEAN(newValue);`
- [ ] Step 4 — restAPI.ino: emit — after line 3241 add
  `addBool(F("ui_usev2"), settings.ui.bUseV2, "b");`
- [ ] Step 5 — restAPI.ino: add `"ui_usev2"` to the allowed-keys array (~3523, keep alpha order).
- [ ] Step 6 — FSexplorer.ino `sendIndex` (113): serve v2 when flag set —
  `serveVersionedAsset(settings.ui.bUseV2 ? "/v2.html" : "/index.html", F("text/html; charset=UTF-8"));`
- [ ] Step 7 — FSexplorer.ino registerHandlers (~150): add three routes mirroring the css/js ones:
  `server.on("/v2.html", HTTP_GET, [](AsyncWebServerRequest *r){ webBeginRequest(r); serveVersionedAsset("/v2.html", F("text/html; charset=UTF-8")); });`
  and likewise `/v2.js` (`application/javascript`), `/v2.css` (`text/css`).
- [ ] Step 8 — Create `data/v2.html`: minimal shell — header with title, clock, theme toggle
  (reuse `theme-toggle.js`), a "Classic UI" toggle button (`id="uiToggle"`), an empty
  `<main id="app">`, links to `ds-tokens.css`, `v2.css`, `v2.js`, fonts. `data-theme` pre-paint
  script copied from index.html head.
- [ ] Step 9 — Create `data/v2.css`: paste the mockup's `<style>` block (component/layout rules),
  strip the token `:root`/`html[data-theme]` blocks (those went to ds-tokens.css in P0a).
- [ ] Step 10 — Create `data/v2.js`: APIGW helper (copy the 2 const lines from index.js),
  a `setUseV2(false)` that POSTs `ui_usev2=0` to the settings endpoint then `location='/'`,
  wire `#uiToggle`. Render a "v2 shell OK" placeholder in `#app`.
- [ ] Step 11 — In `data/index.html`: add a "Try new UI" button in the header that POSTs
  `ui_usev2=1` and reloads. (Old UI is otherwise untouched.)
- [ ] Step 12 — Build: `python build.py --target esp32` then `--target esp32-classic`; grep log for per-env SUCCESS. evaluate `--quick` green.
- [ ] Step 13 — Write ADR (Proposed) for coexisting-UI + bUseV2 setting; reference in commit.
- [ ] Step 14 — Bump + commit + push. Verify on a live device: toggle Classic→v2→Classic round-trips and the choice persists across reload.

---

## P1 — Home (showpiece)

Each concept: extract its `<section>` markup from the mockup into `v2.html`, keep classes
(CSS already in v2.css), replace the simulation JS with a render function in `v2.js` driven
by live data. Concept chips switch A/B/C via `localStorage('otgwHomeConcept')`.

**Live data source (shared by all three):** OT-log WebSocket decoded frames give flow temp
(MsgID 25), return temp (28), modulation (17), CH setpoint (1), DHW temp (26), pressure (18),
room temp (24/16), outside temp (27); flame/CH/DHW status bits (MsgID 0). `device/info` gives
gateway mode + device identity. `v2.js` maintains a `model{}` object updated on each WS frame;
render functions read `model`.

### Task P1a: Home Concept A (system schematic) wired
**Files:** Modify `data/v2.html` (add section-a markup), `data/v2.js` (model + renderA).
- [ ] Extract `#design-a` section markup into v2.html under the Home page container.
- [ ] In v2.js: open OT-log WS (copy connect/reconnect/watchdog shape from index.js), parse
  decoded frames into `model` (flow/return/mod/dhw/pressure/room/outside/flame/ch/dhw bits).
- [ ] `renderA(model)`: set chip texts, toggle `svg.flame-on/.ch-on/.dhw-on`, set burner
  `--fy` from modulation, pressure needle rotation, ΔT. Call on each frame (throttled rAF).
- [ ] Build (both targets) + evaluate green; browser-verify the schematic animates from live frames.
- [ ] Bump + commit + push.

### Task P1b: Home Concept B (at a glance) wired
- [ ] Extract `#design-b` markup; `renderB(model)`: hero dial arc + big number (room temp/
  setpoint), stat cards (flow/return/mod/DHW), zone bar dot, modulation bar height, flame badge.
- [ ] Build + evaluate + browser-verify; bump + commit + push.

### Task P1c: Home Concept C (mission control) + concept chips
- [ ] Extract `#design-c` markup; `renderC(model)`: push samples to a ring buffer, redraw the
  flow/return/setpoint/mod strip-chart polylines, append OT frames to the ticker, fill metric cells.
- [ ] Wire concept chips (A/B/C) with `localStorage('otgwHomeConcept')`; default A.
- [ ] Build + evaluate + browser-verify all three switch; bump + commit + push.

---

## P2 — Monitor

### Task P2a: Log console
- [ ] Extract Monitor page + subtabs; Log panel. Reuse the WS log stream; implement
  search / SAT-only / timestamps / autoscroll / freeze (mirror index.js log logic, trimmed).
- [ ] Build + evaluate + browser-verify; bump + commit + push.

### Task P2b: Stats table + Support matrix
- [ ] Maintain per-MsgID counters (T/B/dir) from WS frames; render the stats table (sortable)
  and the 16-wide support matrix + detail panel (msgid → decoded meaning from a static map).
- [ ] Build + evaluate + browser-verify; bump + commit + push.

### Task P2c: Graph + Connection map
- [ ] Graph: reuse `graph.js`/ECharts data path (load `graph.js`, `echarts-theme.js`) bound to
  the same series; honor `ui_graphtimewindow`.
- [ ] Connection node map: render nodes (ESP/PIC-or-OTDirect/MQTT/WiFi-or-Eth/HA) with link
  states from `device/info` + `network` + MQTT status; animate active links.
- [ ] Build + evaluate + browser-verify; bump + commit + push.

## P3 — Settings

### Task P3a: Settings rail + grouped cards + save/discard
- [ ] Extract Settings layout; GET `v2/settings`, render grouped cards from the field set
  (reuse the field metadata index.js uses at lines ~5876+). Dirty tracking, sticky save/discard
  bar, toast, search filter. Save POSTs changed fields to `v2/settings`.
- [ ] Build + evaluate + browser-verify save round-trips; bump + commit + push.

### Task P3b: BLE sensor roster
- [ ] Roster from `v2/sat/ble/discovery`; select/label/forget/rescan via the `v2/sat/ble/*`
  POST sub-actions. Render rows (name/mac/tags/value/controls).
- [ ] Build + evaluate + browser-verify; bump + commit + push.

## P4 — Connectivity strip

### Task P4a: Always-visible connectivity strip + detail
- [ ] Render the global `#connStrip` pills (WiFi/Eth, MQTT, OT bus, NTP) on both pages from
  `network`/`health`/`device/info`/MQTT status; click a pill → detail map (reuse P2c node map).
- [ ] Build + evaluate + browser-verify; bump + commit + push.

## P5 — Cutover

### Task P5a: a11y + field validation + default decision
- [ ] a11y pass: aria-live on save feedback + log, focus order, 44px tap targets, color-contrast
  check (reuse ui-design:accessibility-audit guidance).
- [ ] Field-validation ACs: confirm on real hardware (flagged hardware-only).
- [ ] Decide whether to flip device default (`bUseV2` default) — maintainer call, leave false
  unless instructed.
- [ ] Final build + evaluate + push; mark epic TASK-908 ACs.

---

## Self-Review

- **Spec coverage:** P0a↔tokens; P0b↔coexistence+setting+toggle (spec §4.1); P1↔Home A/B/C
  (§2); P2↔Monitor; P3↔Settings; P4↔connectivity strip; P5↔a11y+cutover (§5). All spec
  sections map to a task. ✓
- **Placeholder scan:** firmware steps carry exact code/line anchors; UI steps specify
  extract-from-mockup + named render fns + data sources. No TBD. ✓
- **Type consistency:** setting key `ui_usev2` and field `settings.ui.bUseV2` used uniformly;
  render fns `renderA/B/C(model)` and shared `model` object consistent across P1. ✓
