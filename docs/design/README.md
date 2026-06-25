# OTGW Web UI — Design Exploration

> Status: **exploration / design-review artifacts** (no firmware changes).
> Branch: `claude/webui-gauge-widget-design-s5t9s` · PR #649.
> These are self-contained HTML mockups plus the reasoning behind them, kept so
> the design decisions and lessons learned survive beyond the chat that produced
> them. They are **not** shipped to LittleFS.

This folder explores how the OTGW Web UI could evolve: glanceable dashboards,
gauges, an animated boiler/heat-pump schematic, redesigned monitor sub-tabs, a
connectivity model, and a fully-audited settings page. Everything is hand-rolled
SVG/CSS/JS (or inlined ECharts) — **no build step, no network** — so any mockup
opens directly in a browser and animates with simulated data.

---

## How to view

- **Download** an `.html` file from this folder and double-click it (works
  offline; everything is self-contained).
- **Or** use the htmlpreview proxy over the raw branch file:
  `https://htmlpreview.github.io/?https://raw.githubusercontent.com/rvdbreemen/OTGW-firmware/claude/webui-gauge-widget-design-s5t9s/docs/design/<file>.html`
  - Concepts: `boiler-dashboard-concepts.html` (the main, current prototype)
  - Gauges: `gauge-dashboard-mockup.html`
  - The proxy caches aggressively — append `?v=N` to the raw URL or hard-refresh
    (Ctrl/Cmd+Shift+R) if you see a stale version.

---

## Artifacts

### 1. `boiler-dashboard-concepts.html` — the main prototype
A Home / Monitor / Settings app shell with a persistent connectivity strip.

- **Home** — three switchable opening-page concepts:
  - **A · System view** *(recommended hero)* — animated appliance schematic:
    a gas **flame** *or* an electric **lightning bolt** (heat-pump) whose
    **height tracks modulation**, a red **flow** arrow out with temperature and
    a blue **return** arrow back with temperature, DHW branch with droplets,
    spinning pump, temperature-tinted radiator, 5-zone pressure mini-gauge,
    room/outside chips, a prominent modulation %, and a plain-language status
    line. Toggle 🔥 Boiler / ⚡ Heat pump.
  - **B · At a glance** — phone-first hero room dial with setpoint steppers and
    a `TT=` command preview; four stat cards (boiler ↑/↓, pressure zone strip,
    gradient modulation bar, DHW).
  - **C · Mission control** — always-dark live strip chart
    (flow/return/setpoint/modulation) + mono value grid + scrolling OT-frame
    ticker.
- **Monitor** — redesigned sub-tabs: **Log** (filterable console),
  **Statistics** (scannable table with `T → / ← B / T ⇄ B` direction badges),
  **OT Support** (128-cell support **map**, hover tooltip + click-to-pin msgID
  detail), **Graph** (strip chart), **Connection** (signal-flow map).
- **Settings** — category rail → grouped cards, masonry flow, live search,
  changed-field markers, sticky save bar, REBOOT badges; BLE Sensors roster.

### 2. `gauge-dashboard-mockup.html` — gauge & thermostat study
ECharts gauges (inlined, offline): CH pressure with 5-band severity, a
blue→yellow→red modulation bar, five temperature-visualisation styles side by
side (needle gauge, progress ring, thermometer, trend area, bar-vs-setpoint),
and a turnable SVG thermostat dial that previews the `TT=`/`TC=` command.

### Screenshots (reference, both themes / desktop + mobile)
`concept-A-desktop`, `concept-A-mobile-dark`, `concept-B-mobile`,
`concept-C-desktop`, `appliance-gas-high`, `appliance-hp-low`,
`mon-log-desktop`, `mon-stats-desktop`, `mon-support-desktop`,
`support-detail-desktop`, `conn-desktop`, `conn-mobile-dark`,
`set3-sat-desktop` / `-dark` / `-mobile`, `set2-otdirect-desktop`,
`set2-system-desktop`, `mockup-light` / `-dark` / `-v2-light`.

---

## Design decisions & rationale (the lessons)

**Boiler as a living system (Concept A).** Driven by user feedback: show the
boiler itself, not a number list. Flame **height = modulation** (a short bolt at
34 %, a tall flame at 82 %) gives load at a glance; the red flow / blue return
arrows carry their temperatures inline. A heat pump is **electric**, so it gets a
lightning bolt with a cyan glow, the LCD reads `HP` not `CH`, and the first tile
reads *Compressor* not *Flame*. This maps to the real SAT "Heating System"
setting (Gas / Heat Pump / Hybrid / Underfloor).

**Pressure severity is a 5-band model.** Red < 1.0 bar, yellow, **green around
the 2.0 optimum**, yellow, red > 3.0, with min/optimum/max markers — following
the Home-Assistant gauge severity idea. Thresholds are data, not draw code.
(Open caveat: the *temperature* gauge zones are still more decorative than
domain-correct — a boiler legitimately running 75–80 °C should not read amber.)

**OT Support as a map, not a table.** The 128 msgIDs become a colour-coded grid
(green = both sides, blue = thermostat-only, orange = boiler-only, grey = never
seen). Hover for a quick label; click to pin full detail — decimal, hex, spec
name, human name, data type, direction, and a plain-language conclusion. All
metadata is extracted verbatim from the firmware `OTmap` table
(`OTGW-Core.h`), so it is authentic.

**Connectivity: separate MODE from HEALTH.** Gateway/Monitor is a *mode*
(blue chip), never a green/red light; a disabled integration reads *off/grey*,
not an error; an explicit *unknown* state exists for "not determined yet". One
five-state vocabulary (connected / degraded / disconnected / off / unknown) is
used everywhere with colour **+ icon + text**. The detail view is a **chain
map** (thermostat & boiler → gateway → wifi/router → broker→HA and this browser)
so a break is *locatable*, with a fix hint per signal. The OT bus is modelled as
**two independent links** (`bThermostatState` vs `bBoilerState`), because
`bOnline` alone can't tell "boiler not answering" from "thermostat not asking".

**Settings: cluster, then make scannable.** Built on the 2.0.0 grouping idea but
taken further: a sticky **category rail** (per-category field counts) → grouped
cards in a **masonry column flow** (NOT a CSS grid — a grid row-aligns to the
tallest card and leaves huge whitespace; `columns` + `break-inside:avoid` packs
tightly). One category visible at a time; live search filters across all
categories. **BLE Sensors is a roster**: each discovered sensor shows its type
(temperature / humidity / both) and the room it maps to (`satsensorarea0-3`),
because the room mapping is conceptually a per-sensor setting.

**Self-contained is a hard requirement.** A LAN device may have no internet, and
preview proxies sandbox network. The gauge mockup originally used a CDN ECharts
and showed blank gauges on mobile/proxy; the fix was to **inline ECharts** and
make boot resilient (gauge failure must not block the rest). Verified by
rendering with **all network blocked**: zero page errors.

---

## Settings audit (basis for the mockup)

The settings mockup is based on a full audit of the new `dev` (2.0.0) line, not
guesswork.

- **Source of truth:** the keys serialised by `writeSettings()` in
  `src/OTGW-firmware/settingStuff.ino` (`writeJson*KV(file, F("Key"), …)`) =
  **185 persisted keys**. SAT field metadata (labels/types/options) is taken
  verbatim from `SAT_SETTINGS_GROUPS` in `data/index.js`.
- **Coverage:** the mockup reproduces **all 185 keys = 100 %**, across **13
  categories / 172 base fields + the BLE roster**.
- **Cluster proposal (top-level categories):** System · Network (Wi-Fi +
  Ethernet) · MQTT · Time/NTP · OpenTherm Gateway · **OT-Direct** (Mode, PID &
  curve, Bypass, Ventilation, Setback) · GPIO Sensors · GPIO Outputs · S0 Pulse
  Counter · Webhook · Behavior · User Interface · **SAT** (the real 14 groups:
  Core Control, PID, PWM, Temperature Sources, Presets, DHW, Pressure, Smart
  Features, Safety, Energy, PV Surplus Boost, Weather, Sync, Advanced — plus
  supplementary Temperature Source, BLE Sensors, Zones, Boiler Model).
- **Webhook** corrected to the real keys: enable, trigger bit, **URL on / URL
  off**, content type, payload template.
- Deliberately surfaced internal/panel-managed keys for completeness
  (`MQTTlastPublishedLegacy` readonly; BLE roster / zone mapping) but flagged
  them so they don't read as ordinary knobs.

To re-run the audit:
```bash
# 1) persisted firmware keys
git show origin/dev:src/OTGW-firmware/settingStuff.ino \
 | grep -oE 'writeJson(String|Bool|Int|Float)KV\(file, F\("[A-Za-z0-9_]+"' \
 | grep -oE '"[A-Za-z0-9_]+"' | tr -d '"' | sort -u > /tmp/fw_keys.txt
# 2) keys present in the mockup
grep -oE "key: ?'[A-Za-z0-9_]+'" docs/design/boiler-dashboard-concepts.html \
 | grep -oE "'[A-Za-z0-9_]+'" | tr -d "'" | sort -u > /tmp/mock_keys.txt
# 3) gaps (case-insensitive)
comm -23 <(tr 'A-Z' 'a-z' </tmp/fw_keys.txt|sort -u) \
         <(tr 'A-Z' 'a-z' </tmp/mock_keys.txt|sort -u)
```

---

## Competitive analysis

- **[Laxilef/OTGateway](https://github.com/Laxilef/OTGateway)** — a directly
  comparable OTGW firmware with a polished built-in dashboard, but it is pure
  **forms and checklists**: setpoint steppers, a ✓-list of states, label→value
  sensor rows. No diagram, no gauges, no charts. The one thing it does well —
  heating/DHW **control on the front page** — was adopted into Concept B.
  Everything visual here (animated schematic, support map, connectivity map) is
  beyond it.
- **Home Assistant** — gauge-card *severity zones* (adopted for pressure), the
  round thermostat dial (Concept B), and community **animated-flow** cards
  (which require hand-written YAML — these concepts are zero-config).
- **tclcode OTmonitor** — validates "most-interesting parameters on top" (the
  hero band) and inspired Concept C's frame ticker.

---

## Frontend design review (per view)

| View | Verdict | Notes |
|---|---|---|
| Home A — System view | ⭐ strongest, make it the hero | flame/bolt height = modulation reads instantly; add a minimum burner floor at very low modulation; room/radiator cluster is busy |
| Home B — At a glance | good, phone-right | primary action (setpoint) should dominate the uniform stat cards more |
| Home C — Mission control | right idea, needs hierarchy | ticker + grid + chart compete; pick one hero |
| Monitor — Statistics | shippable | direction badges are excellent |
| Monitor — OT Support map | best single idea | clickable + spec-backed |
| Monitor — Connection map | strong concept, fragile build | hand-placed SVG coords; should be component-driven |
| Monitor — Graph | placeholder | |
| Settings | big step up (now fixed) | rail itself deserves sections; show units inline consistently; save-bar should name what changed |
| Cross-cutting | — | inconsistent hit areas; a few colours don't map to real tokens; temp-gauge zones decorative; some color-only signals |

---

## Recommendation: converge on the design system

The most important finding: **`dev` already ships a design system** —
`data/ds-tokens.css` (tokens: `--bg-panel`, `--fg-1..5`, `--accent-primary`,
`--radius-*`, `--fs-*`, `--hit-min`, `--dur-*`), `data/components.css`
(`.ds-card`, `.ds-switch`, `.ds-pill`, `.btn-*`), and a `data/design.html`
reference page with 14 sections. The mockups here invented a *parallel* token /
component set — which is the root cause of any "looks slightly off vs the
device" feeling.

Recommended path, in order:

1. **Rebase the mockup onto the real `ds-tokens.css` + `components.css`** so it
   previews exactly what the firmware can render.
2. **Run the `firmware-design-handoff` skill** to produce the full design-system
   package (tokens + component library + `design.html` sections + page templates
   with patch blocks + `handoff.md`). This is the path from "nice mockups" to
   "implementable UI with a single source of truth".
3. **Promote the winning patterns into `design.html`** (OT-support map,
   connectivity map, boiler schematic, settings rail, BLE roster).

---

## Progressive disclosure & live signals

The guiding principle is **simple → deeply technical, with all data reachable** —
the *opposite* of the current UI, which presents everything at one flat technical
level (tables and panels) regardless of who is looking.

Depth ladder (every level is one tap deeper, nothing is hidden, only deferred):

| Level | View | Audience | Shows |
|---|---|---|---|
| L0 glance | Home B — hero dial | anyone | is it warm? room temp + target |
| L1 system | Home A — schematic | homeowner | flow/return/modulation/pressure/DHW, flame, the whole loop |
| L2 detail | Monitor → **All data** | curious → technical | every value (OT bus + Dallas + BLE + overrides) with source provenance; a **Simple⇄Technical** toggle shows the decoded essentials or *everything* (counters, diagnostics, raw IDs) |
| L3 technical | Monitor: Statistics / OT-Support map / Graph | tinkerer | per-msgID intervals, support map, trends |
| L4 raw | Monitor: Log / Mission control | developer | raw `T…/B…` frames |

**Override signals** (gateway is injecting a value — `TT=`/`TC=`/`OT=`/`SW=`/`MM=`…):
the boiler/thermostat see the injected value, not the original, so it must be
visible at *every* level, not buried. In the mockup:
- L0/L1: a `⛓` marker sits on each overridden value (room setpoint, outside
  temp) on the schematic, and an amber **"⛓ Overrides N"** pill rides in the
  persistent header strip.
- click → an **Active gateway overrides** panel: per override the command
  (`TT=21.0`), the value now in effect, and what it replaced.
- This elevates what the current UI hides in a Statistics-tab table
  ("Active gateway overrides (injected values…)").

**Keeping the power-user "everything" view** — progressive disclosure must *defer*
detail, never *delete* it. The original UI's strength was that it showed all
information; the **All data** view preserves that completely: a dense, searchable
table of every value with its source (`T → / ← B / T ⇄ B`, `1-Wire`, `BLE`) and
override marks. The **Simple** filter shows ~17 decoded essentials a homeowner can
read; **Technical** reveals all 33+ (counters, diagnostics, bounds, versions). A
starting user understands the data available; a technical user reaches everything
in one tap.

**Sensor-availability signals** (new capability coming online): when a BLE or
DS18B20 (1-Wire) sensor is discovered, a dismissible **discovery card** announces
it with the address/reading and a one-tap **Assign to room / Name & use** action.
This turns the current silent behaviour (Dallas sensors just appear as new rows
in the otmonitor table; BLE lives only inside SAT) into an explicit, actionable
signal — the moment deeper sensor capability becomes available.

## Old interface vs new — the difference

| Dimension | Current `dev` UI | New design |
|---|---|---|
| Information model | flat: one technical level for all | layered L0→L4 progressive disclosure |
| First impression | otmonitor table of ~30 rows | a glanceable hero / living schematic |
| Boiler state | numbers in a table | animated schematic (flame/bolt height = modulation, flow/return arrows) |
| Overrides | a table buried in the Statistics tab + an OT-Direct panel | `⛓` on the value + header pill + drill-down panel, at every level |
| New sensors | Dallas appear as silent new rows; BLE only in SAT | explicit discovery cards with assign/label actions |
| OT support | scrollable table | colour-coded 128-cell map, click for spec |
| Connectivity | one WS dot + gateway-mode dot | five-state vocabulary + signal-flow chain map |
| Settings | flat list / partial grouping | category rail + masonry cards + search, 100% coverage |
| Consistency | real `ds-tokens`/`components`/`design.html` | mockup uses parallel tokens (to be reconciled) |

## Open follow-ups / decisions pending

- Pick a winning Home direction (recommendation: **A as hero + B-style controls
  beneath it**).
- Fix temperature-gauge severity zones to be domain-correct (or make them
  neutral) — current zones are decorative.
- If the gauge direction ships: **self-host ECharts in LittleFS** (the CDN
  breaks on offline LANs and would take the Graph + gauges down).
- ADR for the thermostat/setpoint **write path** (Gateway-mode gating, debounce,
  optimistic→confirmed update).
- Make the Connection map component-driven rather than hand-placed coordinates.

---

## CI note

This PR's `pio run -e esp32*` checks fail on a pre-existing **`other-projects`
submodule clone error** on `dev` (the submodule repo is not reachable by
Actions). Checkout aborts before any compile; it is unrelated to these docs-only
changes and fails on every PR until fixed maintainer-side.
