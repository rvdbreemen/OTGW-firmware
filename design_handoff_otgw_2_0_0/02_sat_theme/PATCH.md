# Patch 02 — SAT panel theme corrections

## Why

In the 2.0.0 build the Smart Autotune (SAT) panel reads as a separate
visual system from the rest of the UI:

- All four temperature tiles (Room / Outside / Boiler / Target) use a
  saturated dark-teal fill with cyan numbers. The kit's neutral surface
  is `#2c2c2e`; cyan is reserved for the nav bar.
- The Target Temperature tile is rendered lighter than its siblings.
  Reads as accidental, not deliberate.
- "Controls" section header is white while sibling headers ("DHW",
  "Temperature History") are cyan — inconsistent.
- The "Disabled" state pill and the SAT label next to the toggle have
  poor contrast against `#1a1a1a`.
- Chart gridlines and axis labels are barely visible in dark mode.
- The DHW slider has no live-fill — the blue portion doesn't grow as
  you drag (cosmetic, but it's the kind of thing that makes the build
  feel unfinished).

## What changes

1. Add `data/sat.css` (file in this folder).
2. In `data/index.html`, link it AFTER `index_dark.css`:

   ```html
   <link rel="stylesheet" href="ds-tokens.css">
   <link rel="stylesheet" href="index.css">
   <link rel="stylesheet" href="index_dark.css">
   <link rel="stylesheet" href="sat.css">           <!-- NEW -->
   ```

3. In `data/sat.js`, on the DHW slider's `input` event, set
   `--sat-slider-pct` so the track fills live. See Patch 05.

## Markup contract

The CSS expects this structure (matches what the 2.0.0 SAT panel
already renders, minus the new class names — rename in `sat.html` /
the partial that emits the panel):

```html
<section class="sat-panel">
  <div>
    <div class="sat-header">
      <div class="sat-header-title">SAT — Smart Autotune Thermostat</div>
      <label class="switch">…</label>
      <span class="sat-state-pill is-ok">Enabled</span>
    </div>

    <div class="sat-tiles">
      <div class="sat-tile">
        <div class="sat-tile-label">Room</div>
        <div><span class="sat-tile-value">19.4</span><span class="sat-tile-unit">°C</span></div>
      </div>
      <div class="sat-tile">…Outside…</div>
      <div class="sat-tile">…Boiler…</div>
      <div class="sat-tile is-target">…Target…</div>   <!-- accent -->
    </div>

    <h3>Temperature History</h3>
    <div id="sat-chart" class="sat-chart"></div>

    <h3>Controls</h3>
    <div class="sat-controls">
      <button class="btn btn-secondary">Recalibrate</button>
      <button class="btn btn-secondary" disabled>Diagnostics</button>
    </div>
  </div>

  <div>
    <h3>DHW</h3>
    <div class="sat-dhw">
      <div class="sat-dhw-row">
        <span class="sat-dhw-label">DHW Setpoint</span>
        <span class="sat-dhw-value" id="dhw-setpoint-readout">55.0 °C</span>
      </div>
      <input type="range" min="40" max="65" step="0.5" value="55"
             class="sat-dhw-slider" id="dhw-setpoint-slider">
    </div>
  </div>
</section>
```

Class renames in the existing partial (find/replace, single commit):

| Old class                  | New class               |
|----------------------------|-------------------------|
| `.sat-card` (4 instances)  | `.sat-tile`             |
| `.sat-card-target`         | `.sat-tile.is-target`   |
| `.sat-value`               | `.sat-tile-value`       |
| `.sat-status-disabled` etc | `.sat-state-pill is-*`  |

## Then delete

After verification, the old `.sat-*` blocks in `index.css` and
`index_dark.css` can be removed. They're shadowed by `sat.css` until
then so nothing breaks if you forget.

## Verification

1. Toggle the theme switch — every SAT element should swap together,
   no orphaned cyan or teal.
2. The four temperature tiles should read at the same visual weight in
   both themes; only "Target" carries the blue accent.
3. Drag the DHW slider — the blue fill should grow/shrink in real time
   (requires Patch 05).
4. Tab through controls with keyboard — focus rings visible on every
   interactive element.

## Rollback

Delete `data/sat.css`, remove the `<link>` from `index.html`, revert
the partial's class renames.
