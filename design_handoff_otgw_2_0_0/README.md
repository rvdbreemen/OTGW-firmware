# OTGW-firmware 2.0.0 — design handoff

Five small patches against `data/` in the OTGW-firmware repo. Apply
them in order. Each patch is a folder with a `PATCH.md` (what to do)
and the file(s) to copy in. They are independent enough that you can
land them in separate PRs, but **the order matters for verification**:
later patches assume the tokens from earlier ones exist.

## Patches

| #  | Folder                          | What it does                                          | Files touched                                                                  |
|----|---------------------------------|-------------------------------------------------------|--------------------------------------------------------------------------------|
| 01 | `01_status_color_tokens/`       | Consolidate 3 greens / 2 reds into `--status-*` tokens | `data/ds-tokens.css`, `data/index.css`, `data/index_dark.css`                  |
| 02 | `02_sat_theme/`                 | New `sat.css` — neutral cards, fixes Target/heading/contrast issues | `data/sat.css` (new), `data/index.html`, SAT partial markup        |
| 03 | `03_otmonitor_refresh/`         | New `otmonitor.css` — home page cards match SAT vocabulary | `data/otmonitor.css` (new), `data/index.html`, OTMonitor markup            |
| 04 | `04_echarts_selfhost/`          | Drop CDN, ship a 180 KB custom ECharts build, theme tokens for charts | `data/echarts.min.js` (new), `data/echarts-theme.js` (new), `data/index.html` |
| 05 | `05_dhw_slider_live_fill/`      | DHW setpoint slider track fills as you drag          | `data/sat-slider.js` (new), `data/index.html`, `data/sat.js`                   |

## Apply order

```sh
cd <repo>/src/OTGW-firmware

# Patch 01 — tokens. Read PATCH.md for the find/replace list.
cp .../01_status_color_tokens/ds-tokens.css data/

# Patch 02 — SAT panel.
cp .../02_sat_theme/sat.css data/
# Edit data/index.html: add <link href="sat.css">.
# Edit the SAT partial: rename .sat-card -> .sat-tile, etc. (see PATCH.md).

# Patch 03 — home page.
cp .../03_otmonitor_refresh/otmonitor.css data/
# Edit data/index.html: add <link href="otmonitor.css">.
# Edit OTMonitor partial: add data-state attribute on bool rows.

# Patch 04 — ECharts.
# Build echarts.min.js per PATCH.md Step 1, then:
cp .../04_echarts_selfhost/echarts.VERSION data/
cp dist/echarts.min.js data/                            # from your custom build
# Add data/echarts-theme.js (snippet in PATCH.md).
# Edit data/index.html: swap the <script src="…cdn…"> line.

# Patch 05 — slider.
cp .../05_dhw_slider_live_fill/sat-slider.js data/
# Edit data/index.html: add <script src="sat-slider.js">.
# Edit data/sat.js: dispatch 'sat:rendered' after each render.
```

## Single-PR or split?

- **Single PR**: easier to verify end-to-end, easier to revert.
- **Split**: 01 + 04 are infrastructure (no visible change on their
  own); 02 + 03 + 05 are the visible UX fixes. If reviewers want a
  smaller blast radius, land 01 first as prep, then 02–05 together.

I'd land it as one PR titled `2.0.0 design pass: SAT theme + tokens
+ self-host ECharts`.

## Smoke test (post-flash)

After flashing the LittleFS image to a test device:

1. **Tokens** — DevTools console:
   ```js
   getComputedStyle(document.body).getPropertyValue('--status-ok').trim()
   ```
   Returns `#2e7d32` in light, `#66bb6a` in dark. Toggle theme,
   re-evaluate — should swap. (Patch 01)

2. **SAT page** — Navigate to `/sat`. Verify:
   - Four temperature tiles read at the same visual weight; only
     "Target" carries the blue accent.
   - Section headings ("DHW", "Temperature History", "Controls") all
     use the same neutral uppercase treatment — no orphan cyan or
     white heading.
   - "Disabled" pill is legible against the body background.
   - Toggle theme → every SAT element swaps together.
   - (Patch 02)

3. **Home page** — Cards are white (light) or `#2c2c2e` (dark), no
   light-blue tint. Flame indicator on → green dot with glow; toggle
   theme → glow color follows. (Patch 03)

4. **ECharts** —
   ```sh
   curl -I http://<device>/echarts.min.js
   ```
   Returns 200, content-length ~180 KB. Pull device offline, reload
   `/sat` — Temperature History chart still renders. Gridlines and
   axis labels are visible in dark mode. (Patch 04)

5. **DHW slider** — Drag it. Blue fill grows/shrinks in real time;
   °C readout updates with it. Toggle SAT enable, drag again —
   still works (re-bind via `sat:rendered` event). (Patch 05)

## Rollback

Each patch has a Rollback section in its `PATCH.md`. To revert
everything in one shot:

```sh
git checkout HEAD -- data/
```

(assuming the patches were committed against `data/` and nothing else
in the same commits — Patch 04's ECharts build doesn't get into the
repo if you forget to `git add` it, so check `git status` after.)

## Things explicitly out of scope

- Touching `index.js` / `graph.js` / `sat.js` beyond the two specific
  hooks called out in Patches 04 and 05. Behavior changes belong in a
  separate PR.
- The Settings page form layout. The "Settings" button being nearly
  invisible in the SAT screenshot is fixed by Patch 02 (visible
  `[disabled]` state); the page itself isn't redesigned.
- The Log Viewer. Already shipped in the previous handoff.
- Renaming files in `src/`. The patches only touch `data/`.

## Open questions for the firmware side

1. Does `sat.js`'s render path already dispatch a custom event after
   it rewrites the panel? If yes, name it in PATCH 05 instead of
   `sat:rendered`.
2. Is anyone consuming the old `.sat-card` / `.sat-card-target` class
   names from outside `data/` (e.g. a docs page, a screenshot in the
   wiki)? If yes, leave the old classes as aliases in `sat.css` for
   one release.
3. Build host — who runs the ECharts custom build and where does the
   pinned `echarts.min.js` live between flashes? Probably worth
   committing it under `data/` since it changes ~once a year.

— design
