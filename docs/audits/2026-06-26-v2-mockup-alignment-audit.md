# v2 Web UI ↔ PR #649 mockup alignment audit (TASK-933 AC#1)

**Date:** 2026-06-26
**Task:** TASK-933 — align live v2 Web UI with the design mockup (ground truth) on real data
**Ground truth (maintainer directive, 2026-06-26):** the **interactive mockup** `docs/design/boiler-dashboard-concepts.html` (PR #649, TASK-854) is canonical. The 38 screenshot PNGs and the two `.dc.html` specimen files (`OTGW Patterns and Tokens`, `OTGW Mockup Screens`) are *supporting* reference, imported from the claude.ai "Mockups into design system" project.
**Method:** 8 parallel per-surface audits (design vs live `v2.{html,js,css}` + `ds-tokens.css` + firmware REST), each followed by an adversarial verify pass that tried to refute every "open" verdict by finding the implementation in the live code. 833 K agent tokens, 129 tool calls.

## Headline

The shipped v2 is a **faithful, substantially-complete** port of the mockup. `v2.html`/`v2.css` are a 1:1 structural port; the heavy lifting since the original (2026-06-25) TASK-933 audit happened in `v2.js` wiring and ~20 lines of firmware REST. Several items the original audit listed as OPEN (settings labels/categories, OT-Support spec table, two-link connectivity, sortable stats, graph CSV/PNG) are now **done**. The live v2 also *deliberately exceeds* the mockup in places: it binds real `/api/v2` + OT-WebSocket data instead of the prototype's simulated `sim` object, and uses XSS-safe `textContent` DOM construction where the prototype used `innerHTML`.

| Area | done | open (self) | deliberate | hardware-gated |
|---|---|---|---|---|
| Tokens & CSS | 6 | 1 (radii) | 6 | – |
| Settings | ~15 | 3 | 3 corrections | live round-trip |
| Connectivity | 9 | 1 (recency/staleness) | 1 | live transitions |
| OT-Support map | 5 | 2 (trivial) | 2 | live colours |
| Home A (System) | 9 | 0 | 3 | live values |
| Home B/C | 9 | 3 | 3 | live values |
| Monitor Log/Stats/Graph | 16 | 1 (sort glyph) | 2 | live fill |
| Header / a11y | 6 | 1 (aria) | 3 | auth edge |

## OPEN — self-verifiable (actionable without hardware)

### Trivial (mockup-fidelity, low-risk JS/CSS)
1. **OT-Support `sd-hint` footnote missing** — prototype closes every detail render with `<div class="sd-hint">From the OpenTherm spec table + your bus traffic this session.</div>` (`concepts.html:1698`); CSS `.supdetail .sd-hint` exists (`v2.css:300`) but is dead. Fix: append the node at end of `showSupportDetail()` after `v2.js:526`.
2. **Concept B footer clobbered** — `txt('bOut', fmt(model.outside…))` (`v2.js:730`) overwrites the static `"Outside … · OTGW firmware"` (`v2.html:268`) with a bare `"8.4°"`, losing the label. Fix: render `'Outside ' + value + ' · OTGW firmware'` or split into labeled span + value span.
3. **Concept B status headline ignores faults** — B uses a terse inline 4-state string (`v2.js:737`) that never reads `model.fault`; a boiler fault is invisible in Concept B. Concept A already has the rich `statusSentence(isHP)` (`v2.js:612-619`). Fix: call `statusSentence()` for `bStatusTxt` (matches mockup `concepts.html:1386`).
4. **Stats sort has no visual indicator** — headers are click-sortable (`v2.js:1608-1616`) but the active column gets no `aria-sort`/arrow/`.sorted` class. Fix: set `th.setAttribute('aria-sort', …)` + CSS `::after` ▲/▼ on the active `th`.
5. **Theme-toggle a11y** — v2 `#themeToggle` lacks `aria-pressed`/`aria-label` (`v2.html:30`); classic has them (`index.html:59`). Not in the mockup ground truth (its toggle was a bare `<span>`), but cheap. Fix: toggle `aria-pressed` in `applyTheme()` (`v2.js:21-22`).
6. **`ui_graphtimewindow` raw input vs mockup select** — mockup is a `[10min/30min/1h/4h/24h]` select (`concepts.html:2177`); firmware emits int 0-1440 (`restAPI.ino:3288`), v2.js renders a raw number input. Trivial: add `ENUM_OPTS.ui_graphtimewindow=[[10,'10 min'],…,[1440,'24 hours']]` (the select path already handles integer values, `v2.js:1241`).
7. *(optional)* **OT-Support `sd-badge` order** — live is `sd-top→sd-badge→h5→…` (`v2.js:506-526`); mockup is `sd-top→h5→sd-badge→…` (`concepts.html:1685-1697`). Cosmetic only.

### Medium
8. **Concept C ticker is monochrome** — `renderTicker` uses safe `textContent` (`v2.js:818-821`) but emits **no spans**, so the CSS classes `.ticker .t/.b/.ts` (shipped at `v2.css:220`, exact mockup palette) are dead. The mockup colours timestamp/request/answer (`concepts.html:1458-1466`). Fix (the task's own prescription — colour via DOM spans, not innerHTML): store parsed `{ts,dir,body}` in `pushTicker` (currently raw line, `v2.js:233/252-254`); per line append `span.ts` + `span.t`/`span.b` chosen by the direction letter (already extracted at `v2.js:222/267-269`) via `createElement`+`className`+`textContent`.
9. **Settings string-enum selects** — `ntptimezone` (5-zone select, `concepts.html:2075`) and `webhookcontenttype` (3-type select, `concepts.html:2161`) are emitted as string type `"s"` (`restAPI.ino:3270,3308`) and render as text inputs. Not a one-liner: the select value path does `parseInt(curVal)||0` (`v2.js:1241`) and would break on string values — the select branch must first accept string-valued enums. Lower priority (a free-text timezone is arguably more flexible).
10. **SAT long-tail curated labels (~40 keys)** — the mockup specifies curated labels/hints/sub-groups for the full SAT field set via `SAT_GROUPS`/`SAT_EXTRA` (`concepts.html:1832-2025`): `satovershootmargin`, `satexternaltemp`, `satpresetcomfort/eco/away`, `satweatherenable`, `satsolargain`, `satpvboost*`, `satsimulation`, `satautotune`, plus Temperature-Source / Zones / Boiler-Model groups. Firmware exposes all over GET (`restAPI.ino:3327-3464`) but `SET_META` has no entries, so they render with `humanizeKey` fallbacks under a flat SAT category. **Code already flags this as deferred** (`v2.js:1027-1028` "Phase 2: port full SAT metadata"). Shown, not lost — but not the mockup's curated presentation. Fix: extend `SET_META` with the labels/hints + assign `sub:` groups.

## OPEN — needs a small firmware addition
11. **Connectivity recency / OT-bus "stale" (`st-warn`)** — the mockup specifies per-link recency ("data recent" vs "up but stale", `concepts.html:983-984`). `v2.js` scaffolds it (`connRecency()` `v2.js:909`, detail reads `c.seen` `v2.js:1492`) but `c.seen` is **never assigned**, and `st5ok()` is binary (`v2.js:861`), because `/health` exposes only booleans (`restAPI.ino:2887-2888`) with no timestamps. Fix: add per-link last-seen (e.g. `*_age_s`) to `/health` (`sendHealth`), set `CONN.therm.seen`/`.boiler.seen` in `fetchConn`, derive `st-warn` past a freshness budget — or remove the unreachable recency scaffolding if recency is deferred.

## Maintainer-decision items (deliberate, but visible divergences from the mockup)
- **Dark-mode header colour** — mockup darkens the nav to `#383838` in dark mode (`patterns spec :root`); v2 keeps the bright cyan `#00bffe` (`--nav→--brand-cyan`, no dark override). v2.css comments bless "single-brand cyan," but it's single-source-justified and a highly visible call. Veto-or-keep is yours.
- **Dark "off" LED brightness** — reconciling `--tile-off → --status-neutral` (ADR-091, correct) makes the dark idle LED resolve to bright `#b0b0b0` vs the mockup's dim `#54565a`. A light-grey off-LED reads more prominently than intended.
- **Tile/card radii** — v2 uses literal `12/16/22px` instead of the spec's `8px`/`--radius-2xl`. Internally consistent softening, but bypasses the DS radius scale. Adopt token or record as a deliberate v2 choice.
- **Concept C grid** — live dropped the explicit **Flame** cell and split "Setpoint" into ROOM SET + CH SETPOINT (still 11 cells). Reasonable (distinguishes thermostat target from boiler control setpoint), but a reviewer expecting the mockup's Flame ON/OFF cell will find it absent.
- **Concept C pressure bands** — retuned (warn `<1.2` vs mockup `<1.5`, inclusive upper bound). HA-style 5-band; at exactly 3.0 bar mockup=warn, live=alert.

## Hardware-gated (TASK-933 AC#6 — maintainer sign-off on the OTGW32 at 192.168.88.39)
Every live-data binding is code-correct but needs the device to confirm visually: Home A/B/C values under live OT traffic (flame on/off, DHW tap, fault inject, HP vs gas source), OT-Support cell colours, stats fill (non-empty interval/value), graph curves over time, connectivity live state transitions, settings GET-populate + POST round-trip, and PNG export across Chrome/Firefox/Safari.

## Overlap with sibling open tasks (do not double-implement)
- **TASK-932** (mobile review): already owns the header tap-target 44px bump (`v2.css:453-456`) — finding F8/M is its block, no separate work owed.
- **TASK-924** (BLE temp/hum, dhw token, save r.ok, a11y): overlaps the `--dhw` dark token note and the a11y items (#5, theme-toggle aria).
- **TASK-925** (all v2 audit findings + gas/HP source control): the Home-A source toggle, stats sort, log download, conn detail, support hover, graph chips/CSV/PNG it lists are the very items this audit now finds **done** — TASK-925's work is largely landed; the residual open items above are what's left.

## Bottom line
No structural rebuild is warranted. The remaining alignment is: ~7 trivial fidelity fixes (items 1-7), 3 medium ones (8-10), 1 firmware-touch (11), and 5 maintainer design calls. Everything else is done or a deliberate, defensible improvement on the mockup.
