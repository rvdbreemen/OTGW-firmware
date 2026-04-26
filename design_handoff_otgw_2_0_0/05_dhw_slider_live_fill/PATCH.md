# Patch 05 — DHW slider live fill

## Why

In the 2.0.0 build the DHW setpoint slider has no live track fill —
the blue portion doesn't grow as you drag. Cosmetic, but it's the kind
of detail that makes the build feel half-finished. Patch 02's CSS
already supports it (the `.sat-dhw-slider` rule reads
`--sat-slider-pct`); we just need a JS hook to set the variable on
`input` events.

## What changes

1. Add `data/sat-slider.js` (file in this folder).
2. In `data/index.html`, link it AFTER `sat.js`:

   ```html
   <script src="sat.js"></script>
   <script src="sat-slider.js"></script>           <!-- NEW -->
   ```

3. **Optional but recommended:** in the SAT render path inside
   `sat.js`, after the partial's innerHTML is written, dispatch:

   ```js
   document.dispatchEvent(new Event('sat:rendered'));
   ```

   The hook listens for this and re-binds. Without it the live fill
   still works on first load, but stops working after the websocket
   re-renders the panel.

## Markup contract

The hook expects:

```html
<input type="range" id="dhw-setpoint-slider"
       class="sat-dhw-slider"
       min="40" max="65" step="0.5" value="55">

<span id="dhw-setpoint-readout">55.0 °C</span>
```

The readout id is derived from the slider id by replacing `-slider`
with `-readout` — if the readout span has a different id in the 2.0.0
build, either rename it or extend the convention in `sat-slider.js`.

## Verification

1. Drag the slider — the blue track grows/shrinks in real time, and
   the °C readout updates with it.
2. Set value via JS:
   ```js
   var s = document.getElementById('dhw-setpoint-slider');
   s.value = 60; s.dispatchEvent(new Event('input'));
   ```
   Track fill should jump to ~80%.
3. Trigger a re-render (toggle SAT enable, or whatever path rewrites
   the panel). Confirm the slider still has live fill afterwards. If
   not, the `sat:rendered` event isn't being dispatched — add it.

## Rollback

Delete `data/sat-slider.js`, remove the `<script>` tag, remove the
`sat:rendered` dispatch from `sat.js`. The slider falls back to a
solid blue track (the CSS variable defaults to `50%`).
